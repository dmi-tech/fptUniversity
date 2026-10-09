/**
 * @file driver_can.c
 * @brief CAN driver on top of the Zephyr CAN API, with optional TX/RX activity LEDs.
 */
#include <errno.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "driver_can.h"

#define CAN_STD_ID_MAX  0x7FFU
#define CAN_EXT_ID_MAX  0x1FFFFFFFU
#define SEND_TIMEOUT    K_MSEC(100)
#define LED_PULSE_MS    30

#if IS_ENABLED(CONFIG_CAN) && DT_HAS_CHOSEN(zephyr_canbus)

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static bool ready;

/* ---- Activity LEDs (nodes can_tx_led / can_rx_led, optional) ------------------------- */

struct blink {
	struct gpio_dt_spec spec; /* port == NULL: LED not present */
	struct k_work_delayable off;
};

static struct blink tx_led = {.spec = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(can_tx_led), gpios, {0})};
static struct blink rx_led = {.spec = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(can_rx_led), gpios, {0})};

static void blink_off(struct k_work *work)
{
	struct blink *b = CONTAINER_OF(k_work_delayable_from_work(work), struct blink, off);

	gpio_pin_set_dt(&b->spec, 0);
}

static void blink_init(struct blink *b)
{
	if (b->spec.port != NULL && gpio_is_ready_dt(&b->spec) &&
	    gpio_pin_configure_dt(&b->spec, GPIO_OUTPUT_INACTIVE) == 0) {
		k_work_init_delayable(&b->off, blink_off);
	} else {
		b->spec.port = NULL; /* unusable: behave as if absent */
	}
}

static void blink_pulse(struct blink *b)
{
	if (b->spec.port != NULL) {
		gpio_pin_set_dt(&b->spec, 1);
		k_work_reschedule(&b->off, K_MSEC(LED_PULSE_MS));
	}
}

/* ---- Init, send ----------------------------------------------------------------------- */

int driver_can_init(uint32_t bitrate, bool loopback)
{
	int ret;

	if (!device_is_ready(can_dev)) {
		return -ENODEV;
	}

	if (!ready) {
		blink_init(&tx_led);
		blink_init(&rx_led);
	}

	/* Bitrate and mode can only be changed while the controller is stopped */
	ret = can_stop(can_dev);
	if (ret < 0 && ret != -EALREADY) {
		return ret;
	}
	ret = can_set_bitrate(can_dev, bitrate);
	if (ret < 0) {
		return ret;
	}
	ret = can_set_mode(can_dev, loopback ? CAN_MODE_LOOPBACK : CAN_MODE_NORMAL);
	if (ret < 0) {
		return ret;
	}
	ret = can_start(can_dev);
	if (ret < 0) {
		return ret;
	}
	ready = true;
	return 0;
}

static int send_frame(uint32_t id, uint32_t flags, const uint8_t *data, uint8_t len)
{
	struct can_frame frame = {
		.id = id,
		.flags = flags,
		.dlc = len,
	};
	int ret;

	if (len > 8 || (data == NULL && len > 0)) {
		return -EINVAL;
	}
	if (!ready) {
		return -EACCES;
	}
	if (len > 0) {
		memcpy(frame.data, data, len);
	}

	ret = can_send(can_dev, &frame, SEND_TIMEOUT, NULL, NULL);
	if (ret == 0) {
		blink_pulse(&tx_led);
	}
	return ret;
}

int driver_can_send(uint32_t id, const uint8_t *data, uint8_t len)
{
	if (id > CAN_STD_ID_MAX) {
		return -EINVAL;
	}
	return send_frame(id, 0, data, len);
}

int driver_can_send_ext(uint32_t id, const uint8_t *data, uint8_t len)
{
	if (id > CAN_EXT_ID_MAX) {
		return -EINVAL;
	}
	return send_frame(id, CAN_FRAME_IDE, data, len);
}

/* ---- Receive -------------------------------------------------------------------------- */

static driver_can_rx_cb_t rx_cbs[DRIVER_CAN_MAX_FILTERS];
static uint8_t filter_count;

static void rx_isr(const struct device *dev, struct can_frame *frame, void *user_data)
{
	driver_can_rx_cb_t cb = *(driver_can_rx_cb_t *)user_data;

	ARG_UNUSED(dev);
	blink_pulse(&rx_led);
	cb(frame->id, frame->data, can_dlc_to_bytes(frame->dlc));
}

int driver_can_add_rx(uint32_t id, uint32_t mask, driver_can_rx_cb_t cb)
{
	struct can_filter filter = {.id = id, .mask = mask, .flags = 0};
	int ret;

	if (cb == NULL || id > CAN_STD_ID_MAX || mask > CAN_STD_ID_MAX) {
		return -EINVAL;
	}
	if (!ready) {
		return -EACCES;
	}
	if (filter_count >= DRIVER_CAN_MAX_FILTERS) {
		return -ENOSPC;
	}

	rx_cbs[filter_count] = cb;
	ret = can_add_rx_filter(can_dev, rx_isr, &rx_cbs[filter_count], &filter);
	if (ret < 0) {
		return ret;
	}
	filter_count++;
	return 0;
}

int driver_can_get_state(void)
{
	enum can_state state;
	int ret;

	if (!device_is_ready(can_dev)) {
		return -ENODEV;
	}
	ret = can_get_state(can_dev, &state, NULL);
	if (ret < 0) {
		return ret;
	}
	return (state == CAN_STATE_BUS_OFF || state == CAN_STATE_STOPPED) ? -ENETDOWN : 0;
}

/* ---- Heartbeat watch ------------------------------------------------------------------ */

enum hb_state { HB_OFF, HB_WAIT, HB_ALIVE, HB_LOST };

static struct {
	enum hb_state state;
	uint32_t timeout_ms;
	driver_can_hb_cb_t cb;
	struct k_work_delayable timeout;
	struct k_work up;
	struct k_spinlock lock;
} hb;

static void hb_timeout(struct k_work *work)
{
	k_spinlock_key_t key = k_spin_lock(&hb.lock);
	bool report = (hb.state == HB_WAIT || hb.state == HB_ALIVE);

	if (report) {
		hb.state = HB_LOST;
	}
	k_spin_unlock(&hb.lock, key);
	if (report) {
		hb.cb(false);
	}
}

static void hb_up(struct k_work *work)
{
	hb.cb(true);
}

/* Interrupt context */
static void hb_rx(uint32_t id, const uint8_t *data, uint8_t len)
{
	k_spinlock_key_t key = k_spin_lock(&hb.lock);
	bool was_alive = (hb.state == HB_ALIVE);

	hb.state = HB_ALIVE;
	k_spin_unlock(&hb.lock, key);

	k_work_reschedule(&hb.timeout, K_MSEC(hb.timeout_ms));
	if (!was_alive) {
		k_work_submit(&hb.up);
	}
}

int driver_can_heartbeat_watch(uint32_t id, uint32_t timeout_ms, driver_can_hb_cb_t cb)
{
	int ret;

	if (id > CAN_STD_ID_MAX || timeout_ms == 0 || cb == NULL) {
		return -EINVAL;
	}
	if (!ready) {
		return -EACCES;
	}
	if (hb.state != HB_OFF) {
		return -EALREADY;
	}

	hb.timeout_ms = timeout_ms;
	hb.cb = cb;
	hb.state = HB_WAIT;
	k_work_init_delayable(&hb.timeout, hb_timeout);
	k_work_init(&hb.up, hb_up);

	ret = driver_can_add_rx(id, CAN_STD_ID_MAX, hb_rx);
	if (ret < 0) {
		hb.state = HB_OFF;
		return ret;
	}
	k_work_reschedule(&hb.timeout, K_MSEC(timeout_ms));
	return 0;
}

int driver_can_heartbeat_alive(void)
{
	if (hb.state == HB_OFF) {
		return -EACCES;
	}
	return hb.state == HB_ALIVE ? 1 : 0;
}

#else /* no CAN controller */

int driver_can_init(uint32_t bitrate, bool loopback)
{
	ARG_UNUSED(bitrate);
	ARG_UNUSED(loopback);
	return -ENODEV;
}

int driver_can_send(uint32_t id, const uint8_t *data, uint8_t len)
{
	ARG_UNUSED(data);
	if (id > CAN_STD_ID_MAX || len > 8) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_can_send_ext(uint32_t id, const uint8_t *data, uint8_t len)
{
	ARG_UNUSED(data);
	if (id > CAN_EXT_ID_MAX || len > 8) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_can_add_rx(uint32_t id, uint32_t mask, driver_can_rx_cb_t cb)
{
	if (cb == NULL || id > CAN_STD_ID_MAX || mask > CAN_STD_ID_MAX) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_can_get_state(void)
{
	return -ENODEV;
}

int driver_can_heartbeat_watch(uint32_t id, uint32_t timeout_ms, driver_can_hb_cb_t cb)
{
	if (id > CAN_STD_ID_MAX || timeout_ms == 0 || cb == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_can_heartbeat_alive(void)
{
	return -ENODEV;
}

#endif

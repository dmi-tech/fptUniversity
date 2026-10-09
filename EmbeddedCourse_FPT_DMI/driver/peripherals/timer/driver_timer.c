/**
 * @file driver_timer.c
 * @brief Software timers, stopwatch and one-shot hardware timer.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "driver_timer.h"

#if IS_ENABLED(CONFIG_COUNTER) && DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(hwtimer0))
#define HW_TIMER_AVAILABLE 1
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#endif

/* ---- Software timers ---------------------------------------------------------------- */

/* Timer expiry runs in the system clock interrupt: hand over to the system workqueue. */
static void timer_expiry(struct k_timer *timer)
{
	struct driver_timer *t = CONTAINER_OF(timer, struct driver_timer, timer);

	k_work_submit(&t->work);
}

static void timer_work(struct k_work *work)
{
	struct driver_timer *t = CONTAINER_OF(work, struct driver_timer, work);
	driver_timer_cb_t cb = t->cb;

	if (cb != NULL) {
		cb(t->user_data);
	}
}

int driver_timer_start(struct driver_timer *t, uint32_t period_ms, bool periodic,
		       driver_timer_cb_t cb, void *user_data)
{
	if (t == NULL || cb == NULL || period_ms == 0) {
		return -EINVAL;
	}

	if (!t->initialised) {
		k_timer_init(&t->timer, timer_expiry, NULL);
		k_work_init(&t->work, timer_work);
		t->initialised = true;
	}

	t->cb = cb;
	t->user_data = user_data;
	k_timer_start(&t->timer, K_MSEC(period_ms), periodic ? K_MSEC(period_ms) : K_NO_WAIT);
	return 0;
}

int driver_timer_stop(struct driver_timer *t)
{
	if (t == NULL) {
		return -EINVAL;
	}
	if (t->initialised) {
		k_timer_stop(&t->timer);
	}
	return 0;
}

bool driver_timer_is_running(const struct driver_timer *t)
{
	if (t == NULL || !t->initialised) {
		return false;
	}
	return k_timer_remaining_get((struct k_timer *)&t->timer) != 0;
}

/* ---- Stopwatch ---------------------------------------------------------------------- */

void driver_timer_stopwatch_start(struct driver_stopwatch *sw)
{
	sw->start_cycles = k_cycle_get_32();
}

uint32_t driver_timer_stopwatch_us(const struct driver_stopwatch *sw)
{
	/* Unsigned subtraction stays correct across one wrap of the cycle counter. */
	return k_cyc_to_us_floor32(k_cycle_get_32() - sw->start_cycles);
}

/* ---- Hardware timer ----------------------------------------------------------------- */

#ifdef HW_TIMER_AVAILABLE

static const struct device *const hw_dev = DEVICE_DT_GET(DT_ALIAS(hwtimer0));
static bool hw_started;
static volatile bool hw_alarm_pending;
static driver_timer_cb_t hw_cb;
static void *hw_cb_data;

int driver_timer_hw_init(void)
{
	int ret;

	if (!device_is_ready(hw_dev)) {
		return -ENODEV;
	}
	if (hw_started) {
		return 0;
	}
	ret = counter_start(hw_dev);
	if (ret < 0) {
		return ret;
	}
	hw_started = true;
	return 0;
}

static void hw_alarm_isr(const struct device *dev, uint8_t chan, uint32_t ticks,
			 void *user_data)
{
	driver_timer_cb_t cb = hw_cb;
	void *data = hw_cb_data;

	ARG_UNUSED(dev);
	ARG_UNUSED(chan);
	ARG_UNUSED(ticks);
	ARG_UNUSED(user_data);

	hw_alarm_pending = false;
	if (cb != NULL) {
		cb(data);
	}
}

int driver_timer_hw_alarm_us(uint32_t us, driver_timer_cb_t cb, void *user_data)
{
	struct counter_alarm_cfg cfg;
	uint32_t ticks;
	int ret;

	if (cb == NULL) {
		return -EINVAL;
	}
	if (!hw_started) {
		return -ENODEV;
	}
	if (hw_alarm_pending) {
		return -EBUSY;
	}

	ticks = counter_us_to_ticks(hw_dev, us);
	if (ticks == 0 || ticks > counter_get_top_value(hw_dev)) {
		return -EINVAL;
	}

	hw_cb = cb;
	hw_cb_data = user_data;
	hw_alarm_pending = true;

	cfg.flags = 0; /* relative to the current counter value */
	cfg.ticks = ticks;
	cfg.callback = hw_alarm_isr;
	cfg.user_data = NULL;
	ret = counter_set_channel_alarm(hw_dev, 0, &cfg);
	if (ret < 0) {
		hw_alarm_pending = false;
	}
	return ret;
}

uint32_t driver_timer_hw_now_us(void)
{
	uint32_t ticks;

	if (!hw_started || counter_get_value(hw_dev, &ticks) < 0) {
		return 0;
	}
	return (uint32_t)counter_ticks_to_us(hw_dev, ticks);
}

#else /* !HW_TIMER_AVAILABLE */

int driver_timer_hw_init(void)
{
	return -ENODEV;
}

int driver_timer_hw_alarm_us(uint32_t us, driver_timer_cb_t cb, void *user_data)
{
	ARG_UNUSED(us);
	ARG_UNUSED(cb);
	ARG_UNUSED(user_data);
	return -ENODEV;
}

uint32_t driver_timer_hw_now_us(void)
{
	return 0;
}

#endif /* HW_TIMER_AVAILABLE */

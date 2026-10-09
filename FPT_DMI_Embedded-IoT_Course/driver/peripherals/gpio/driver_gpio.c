/**
 * @file driver_gpio.c
 * @brief GPIO driver: outputs out0..out3 and debounced inputs in0..in3.
 */
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "driver_gpio.h"

/* An input must stay unchanged this long before the callback runs. */
#define DRIVER_GPIO_DEBOUNCE_MS 50

/* gpio_dt_spec of alias <name>, or an empty spec (port == NULL) if the alias is absent. */
#define GPIO_SPEC_OR_NONE(name) GPIO_DT_SPEC_GET_OR(DT_ALIAS(name), gpios, {0})

static const struct gpio_dt_spec out_spec[DRIVER_GPIO_MAX_OUT] = {
	GPIO_SPEC_OR_NONE(out0), GPIO_SPEC_OR_NONE(out1),
	GPIO_SPEC_OR_NONE(out2), GPIO_SPEC_OR_NONE(out3),
};

static bool out_ready[DRIVER_GPIO_MAX_OUT];

struct gpio_input {
	struct gpio_dt_spec spec;
	driver_gpio_in_cb_t cb;
	struct gpio_callback gpio_cb;
	struct k_work_delayable work;
	uint8_t id;
	bool ready;
	bool last_reported;
};

static struct gpio_input inputs[DRIVER_GPIO_MAX_IN] = {
	{.spec = GPIO_SPEC_OR_NONE(in0), .id = 0},
	{.spec = GPIO_SPEC_OR_NONE(in1), .id = 1},
	{.spec = GPIO_SPEC_OR_NONE(in2), .id = 2},
	{.spec = GPIO_SPEC_OR_NONE(in3), .id = 3},
};

static int out_check(uint8_t id)
{
	if (id >= DRIVER_GPIO_MAX_OUT) {
		return -EINVAL;
	}
	if (out_spec[id].port == NULL) {
		return -ENODEV;
	}
	if (!out_ready[id]) {
		return -EACCES;
	}
	return 0;
}

int driver_gpio_out_init(uint8_t id)
{
	int ret;

	if (id >= DRIVER_GPIO_MAX_OUT) {
		return -EINVAL;
	}
	if (out_spec[id].port == NULL || !gpio_is_ready_dt(&out_spec[id])) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&out_spec[id], GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}
	out_ready[id] = true;
	return 0;
}

int driver_gpio_set(uint8_t id, bool on)
{
	int ret = out_check(id);

	if (ret < 0) {
		return ret;
	}
	return gpio_pin_set_dt(&out_spec[id], on ? 1 : 0);
}

int driver_gpio_toggle(uint8_t id)
{
	int ret = out_check(id);

	if (ret < 0) {
		return ret;
	}
	return gpio_pin_toggle_dt(&out_spec[id]);
}

/* Runs in the system workqueue once the input has been stable for the debounce time. */
static void input_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct gpio_input *in = CONTAINER_OF(dwork, struct gpio_input, work);
	int level = gpio_pin_get_dt(&in->spec);

	if (level < 0) {
		return;
	}
	if ((bool)level != in->last_reported) {
		in->last_reported = level;
		if (in->cb != NULL) {
			in->cb(in->id, in->last_reported);
		}
	}
}

/* Interrupt context: only (re)start the debounce timer. */
static void input_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	struct gpio_input *in = CONTAINER_OF(cb, struct gpio_input, gpio_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);
	k_work_reschedule(&in->work, K_MSEC(DRIVER_GPIO_DEBOUNCE_MS));
}

int driver_gpio_in_init(uint8_t id, driver_gpio_in_cb_t cb)
{
	struct gpio_input *in;
	int ret;

	if (id >= DRIVER_GPIO_MAX_IN) {
		return -EINVAL;
	}
	in = &inputs[id];
	if (in->spec.port == NULL || !gpio_is_ready_dt(&in->spec)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&in->spec, GPIO_INPUT);
	if (ret < 0) {
		return ret;
	}

	in->cb = cb;
	k_work_init_delayable(&in->work, input_work_handler);
	ret = gpio_pin_get_dt(&in->spec);
	in->last_reported = ret > 0;

	if (cb != NULL) {
		gpio_init_callback(&in->gpio_cb, input_isr, BIT(in->spec.pin));
		ret = gpio_add_callback(in->spec.port, &in->gpio_cb);
		if (ret < 0) {
			return ret;
		}
		ret = gpio_pin_interrupt_configure_dt(&in->spec, GPIO_INT_EDGE_BOTH);
		if (ret < 0) {
			gpio_remove_callback(in->spec.port, &in->gpio_cb);
			return ret;
		}
	}

	in->ready = true;
	return 0;
}

int driver_gpio_get(uint8_t id)
{
	if (id >= DRIVER_GPIO_MAX_IN) {
		return -EINVAL;
	}
	if (inputs[id].spec.port == NULL) {
		return -ENODEV;
	}
	if (!inputs[id].ready) {
		return -EACCES;
	}
	return gpio_pin_get_dt(&inputs[id].spec);
}

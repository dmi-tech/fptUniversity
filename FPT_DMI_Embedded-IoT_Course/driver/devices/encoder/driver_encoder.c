/**
 * @file driver_encoder.c
 * @brief Encoder driver: listens to input events of the gpio-qdec and gpio-keys devices.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>

#include "driver_encoder.h"

static struct k_spinlock lock;
static int32_t position;
static int32_t limit_min = INT32_MIN;
static int32_t limit_max = INT32_MAX;
static driver_encoder_cb_t step_cb;
static driver_encoder_btn_cb_t btn_cb;
static bool btn_pressed;

static int32_t clamp(int32_t v)
{
	return CLAMP(v, limit_min, limit_max);
}

void driver_encoder_set_position(int32_t pos)
{
	K_SPINLOCK(&lock) {
		position = clamp(pos);
	}
}

void driver_encoder_set_limits(int32_t min, int32_t max)
{
	if (min > max) {
		return;
	}
	K_SPINLOCK(&lock) {
		limit_min = min;
		limit_max = max;
		position = clamp(position);
	}
}

void driver_encoder_set_callback(driver_encoder_cb_t cb)
{
	step_cb = cb;
}

void driver_encoder_set_button_callback(driver_encoder_btn_cb_t cb)
{
	btn_cb = cb;
}

int32_t driver_encoder_get_position(void)
{
	int32_t pos;

	K_SPINLOCK(&lock) {
		pos = position;
	}
	return pos;
}

bool driver_encoder_button_is_pressed(void)
{
	return btn_pressed;
}

#if IS_ENABLED(CONFIG_INPUT) && DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(encoder0))

#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>

static const struct device *const enc_dev = DEVICE_DT_GET(DT_ALIAS(encoder0));
static bool started;

/* One callback for every input device (NULL device): the events are told apart by code */
static void input_event(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (!started) {
		return;
	}

	if (evt->type == INPUT_EV_REL && evt->code == INPUT_REL_WHEEL) {
		int32_t pos;
		int8_t step = evt->value > 0 ? 1 : -1;

		K_SPINLOCK(&lock) {
			position = clamp(position + evt->value);
			pos = position;
		}
		if (step_cb != NULL) {
			step_cb(pos, step);
		}
	} else if (evt->type == INPUT_EV_KEY && evt->code == INPUT_KEY_ENTER) {
		btn_pressed = evt->value != 0;
		if (btn_cb != NULL) {
			btn_cb(btn_pressed);
		}
	}
}
INPUT_CALLBACK_DEFINE(NULL, input_event, NULL);

int driver_encoder_init(void)
{
	if (!device_is_ready(enc_dev)) {
		return -ENODEV;
	}
	driver_encoder_set_position(0);
	started = true;
	return 0;
}

#else /* input subsystem or encoder not available */

int driver_encoder_init(void)
{
	return -ENODEV;
}

#endif

/**
 * @file driver_servo.c
 * @brief Servo driver: 50 Hz PWM through driver_pwm.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "driver_pwm.h"
#include "driver_servo.h"

#define SWEEP_STEP_MS 20 /* one new position per PWM period */

#define SERVO_SPEC_OR_NONE(name)                                                            \
	COND_CODE_1(DT_NODE_HAS_PROP(DT_ALIAS(name), pwms),                                 \
		    (PWM_DT_SPEC_GET(DT_ALIAS(name))), ({0}))

static const struct pwm_dt_spec spec[DRIVER_SERVO_MAX] = {
	SERVO_SPEC_OR_NONE(servo0),
	SERVO_SPEC_OR_NONE(servo1),
};

static struct driver_servo_cfg cfg_of[DRIVER_SERVO_MAX];
static bool ready[DRIVER_SERVO_MAX];

static int check(uint8_t id)
{
	if (id >= DRIVER_SERVO_MAX) {
		return -EINVAL;
	}
	if (spec[id].dev == NULL) {
		return -ENODEV;
	}
	return ready[id] ? 0 : -EACCES;
}

int driver_servo_init(uint8_t id, const struct driver_servo_cfg *cfg)
{
	const struct driver_servo_cfg def = DRIVER_SERVO_CFG_DEFAULT;

	if (id >= DRIVER_SERVO_MAX) {
		return -EINVAL;
	}
	if (cfg == NULL) {
		cfg = &def;
	}
	if (spec[id].dev == NULL || !pwm_is_ready_dt(&spec[id])) {
		return -ENODEV;
	}
	if (cfg->min_pulse_us >= cfg->max_pulse_us || cfg->max_angle == 0 ||
	    (uint32_t)cfg->max_pulse_us * 1000 > spec[id].period) {
		return -EINVAL;
	}

	cfg_of[id] = *cfg;
	ready[id] = true;
	return 0;
}

int driver_servo_set_pulse_us(uint8_t id, uint16_t us)
{
	int ret = check(id);

	if (ret < 0) {
		return ret;
	}
	if ((uint32_t)us * 1000 > spec[id].period) {
		return -EINVAL;
	}
	return driver_pwm_spec_set(&spec[id], spec[id].period, (uint32_t)us * 1000);
}

/* Pulse width in us for an angle (angle already checked) */
static uint32_t pulse_for(uint8_t id, uint16_t deg)
{
	const struct driver_servo_cfg *c = &cfg_of[id];

	return c->min_pulse_us +
	       ((uint32_t)(c->max_pulse_us - c->min_pulse_us) * deg) / c->max_angle;
}

int driver_servo_set_angle(uint8_t id, uint16_t deg)
{
	int ret = check(id);

	if (ret < 0) {
		return ret;
	}
	if (deg > cfg_of[id].max_angle) {
		return -EINVAL;
	}
	return driver_servo_set_pulse_us(id, (uint16_t)pulse_for(id, deg));
}

int driver_servo_sweep(uint8_t id, uint16_t from, uint16_t to, uint32_t ms)
{
	int ret = check(id);
	uint32_t steps;

	if (ret < 0) {
		return ret;
	}
	if (from > cfg_of[id].max_angle || to > cfg_of[id].max_angle) {
		return -EINVAL;
	}

	steps = MAX(ms / SWEEP_STEP_MS, 1U);
	for (uint32_t i = 0; i <= steps && ret == 0; i++) {
		int32_t deg = (int32_t)from + ((int32_t)to - (int32_t)from) * (int32_t)i / (int32_t)steps;

		ret = driver_servo_set_angle(id, (uint16_t)deg);
		if (i < steps) {
			k_msleep(SWEEP_STEP_MS);
		}
	}
	return ret;
}

int driver_servo_release(uint8_t id)
{
	int ret = check(id);

	if (ret < 0) {
		return ret;
	}
	return driver_pwm_spec_set(&spec[id], spec[id].period, 0);
}

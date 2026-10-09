/**
 * @file driver_pwm.c
 * @brief PWM driver on top of the Zephyr PWM API.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "driver_pwm.h"

/* pwm_dt_spec of alias <name>, or an empty spec (dev == NULL) when the alias is absent. */
#define PWM_SPEC_OR_NONE(name)                                                                \
	COND_CODE_1(DT_NODE_HAS_PROP(DT_ALIAS(name), pwms),                                  \
		    (PWM_DT_SPEC_GET(DT_ALIAS(name))), ({0}))

static const struct pwm_dt_spec pwm_spec[DRIVER_PWM_MAX] = {
	PWM_SPEC_OR_NONE(pwm0), PWM_SPEC_OR_NONE(pwm1),
	PWM_SPEC_OR_NONE(pwm2), PWM_SPEC_OR_NONE(pwm3),
};

static bool pwm_ready[DRIVER_PWM_MAX];

int driver_pwm_spec_set(const struct pwm_dt_spec *spec, uint32_t period_ns, uint32_t pulse_ns)
{
	if (spec == NULL || spec->dev == NULL || pulse_ns > period_ns) {
		return -EINVAL;
	}
	if (!pwm_is_ready_dt(spec)) {
		return -ENODEV;
	}
	return pwm_set(spec->dev, spec->channel, period_ns, pulse_ns, spec->flags);
}

static int check(uint8_t id)
{
	if (id >= DRIVER_PWM_MAX) {
		return -EINVAL;
	}
	if (pwm_spec[id].dev == NULL) {
		return -ENODEV;
	}
	if (!pwm_ready[id]) {
		return -EACCES;
	}
	return 0;
}

int driver_pwm_init(uint8_t id)
{
	int ret;

	if (id >= DRIVER_PWM_MAX) {
		return -EINVAL;
	}
	if (pwm_spec[id].dev == NULL || !pwm_is_ready_dt(&pwm_spec[id])) {
		return -ENODEV;
	}

	ret = driver_pwm_spec_set(&pwm_spec[id], pwm_spec[id].period, 0);
	if (ret < 0) {
		return ret;
	}
	pwm_ready[id] = true;
	return 0;
}

int driver_pwm_set_pulse(uint8_t id, uint32_t period_ns, uint32_t pulse_ns)
{
	int ret = check(id);

	if (ret < 0) {
		return ret;
	}
	return driver_pwm_spec_set(&pwm_spec[id], period_ns, pulse_ns);
}

int driver_pwm_set_freq_duty(uint8_t id, uint32_t freq_hz, uint8_t duty_percent)
{
	uint32_t period_ns;

	if (freq_hz == 0 || duty_percent > 100) {
		return -EINVAL;
	}
	period_ns = (uint32_t)(NSEC_PER_SEC / freq_hz);
	if (period_ns == 0) {
		return -EINVAL;
	}
	return driver_pwm_set_pulse(id, period_ns, (uint32_t)((uint64_t)period_ns * duty_percent / 100));
}

int driver_pwm_stop(uint8_t id)
{
	int ret = check(id);

	if (ret < 0) {
		return ret;
	}
	return driver_pwm_spec_set(&pwm_spec[id], pwm_spec[id].period, 0);
}

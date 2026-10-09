/**
 * @file driver_motor.c
 * @brief Motor driver with two compile-time modes chosen by the devicetree alias motor0.
 *
 * PWM mode needs driver_pwm; GPIO mode does not reference it at all.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "driver_motor.h"

#define MOTOR_NODE DT_ALIAS(motor0)

#if DT_NODE_HAS_PROP(MOTOR_NODE, pwms)
#define MOTOR_PWM_MODE 1
#elif DT_NODE_HAS_PROP(MOTOR_NODE, gpios)
#define MOTOR_GPIO_MODE 1
#endif

#ifdef MOTOR_PWM_MODE

#include <zephyr/drivers/pwm.h>

#include "driver_pwm.h"

#define RAMP_STEP_MS 20

static const struct pwm_dt_spec motor_pwm = PWM_DT_SPEC_GET(MOTOR_NODE);
static uint8_t speed = 100;    /* wanted speed, percent */
static uint8_t min_duty;       /* duty of speed 1 % */
static bool running;
static bool ready;
static K_MUTEX_DEFINE(lock);

/* Duty (percent) for a speed: 0 -> 0, 1 -> min_duty, 100 -> 100, linear in between */
static uint32_t duty_for(uint8_t percent)
{
	if (percent == 0) {
		return 0;
	}
	return min_duty + ((uint32_t)(percent - 1) * (100 - min_duty)) / 99;
}

/* Lock must be held */
static int apply(bool on, uint8_t percent)
{
	uint32_t pulse = on ? (uint32_t)((uint64_t)motor_pwm.period * duty_for(percent) / 100) : 0;

	return driver_pwm_spec_set(&motor_pwm, motor_pwm.period, pulse);
}

int driver_motor_init(void)
{
	int ret;

	if (!pwm_is_ready_dt(&motor_pwm)) {
		return -ENODEV;
	}
	k_mutex_lock(&lock, K_FOREVER);
	speed = 100;
	running = false;
	ret = apply(false, 0);
	ready = (ret == 0);
	k_mutex_unlock(&lock);
	return ret;
}

int driver_motor_on(void)
{
	int ret;

	if (!ready) {
		return -EACCES;
	}
	k_mutex_lock(&lock, K_FOREVER);
	ret = apply(true, speed);
	running = (ret == 0) || running;
	k_mutex_unlock(&lock);
	return ret;
}

int driver_motor_off(void)
{
	int ret;

	if (!ready) {
		return -EACCES;
	}
	k_mutex_lock(&lock, K_FOREVER);
	ret = apply(false, 0);
	if (ret == 0) {
		running = false;
	}
	k_mutex_unlock(&lock);
	return ret;
}

bool driver_motor_is_on(void)
{
	return running;
}

int driver_motor_set_speed(uint8_t percent)
{
	int ret;

	if (percent > 100) {
		return -EINVAL;
	}
	if (!ready) {
		return -EACCES;
	}
	k_mutex_lock(&lock, K_FOREVER);
	if (percent == 0) {
		ret = apply(false, 0);
		running = (ret == 0) ? false : running;
	} else {
		speed = percent;
		ret = apply(true, percent);
		running = (ret == 0) || running;
	}
	k_mutex_unlock(&lock);
	return ret;
}

uint8_t driver_motor_get_speed(void)
{
	return running ? speed : 0;
}

int driver_motor_set_min_duty(uint8_t percent)
{
	int ret = 0;

	if (percent > 99) {
		return -EINVAL;
	}
	k_mutex_lock(&lock, K_FOREVER);
	min_duty = percent;
	if (running) {
		ret = apply(true, speed); /* new mapping takes effect at once */
	}
	k_mutex_unlock(&lock);
	return ret;
}

int driver_motor_ramp_to(uint8_t percent, uint32_t ms)
{
	int from;
	int steps;
	int ret = 0;

	if (percent > 100) {
		return -EINVAL;
	}
	if (!ready) {
		return -EACCES;
	}

	from = driver_motor_get_speed();
	if (from == percent) {
		return 0;
	}
	steps = ms / RAMP_STEP_MS;
	for (int i = 1; i < steps && ret == 0; i++) {
		int p = from + ((int)percent - from) * i / steps;

		/* A ramp from stop or to stop skips speed 0 until the end */
		ret = driver_motor_set_speed(p > 0 ? (uint8_t)p : 1);
		k_msleep(RAMP_STEP_MS);
	}
	return ret < 0 ? ret : driver_motor_set_speed(percent);
}

#elif defined(MOTOR_GPIO_MODE)

#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec motor_pin = GPIO_DT_SPEC_GET(MOTOR_NODE, gpios);
static bool running;
static bool ready;

int driver_motor_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&motor_pin)) {
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&motor_pin, GPIO_OUTPUT_INACTIVE);
	if (ret == 0) {
		running = false;
		ready = true;
	}
	return ret;
}

static int set_pin(bool on)
{
	int ret;

	if (!ready) {
		return -EACCES;
	}
	ret = gpio_pin_set_dt(&motor_pin, on ? 1 : 0);
	if (ret == 0) {
		running = on;
	}
	return ret;
}

int driver_motor_on(void)
{
	return set_pin(true);
}

int driver_motor_off(void)
{
	return set_pin(false);
}

bool driver_motor_is_on(void)
{
	return running;
}

int driver_motor_set_speed(uint8_t percent)
{
	ARG_UNUSED(percent);
	return -ENOTSUP;
}

uint8_t driver_motor_get_speed(void)
{
	return running ? 100 : 0;
}

int driver_motor_set_min_duty(uint8_t percent)
{
	ARG_UNUSED(percent);
	return -ENOTSUP;
}

int driver_motor_ramp_to(uint8_t percent, uint32_t ms)
{
	ARG_UNUSED(percent);
	ARG_UNUSED(ms);
	return -ENOTSUP;
}

#else /* no motor0 alias */

int driver_motor_init(void)
{
	return -ENODEV;
}

int driver_motor_on(void)
{
	return -ENODEV;
}

int driver_motor_off(void)
{
	return -ENODEV;
}

bool driver_motor_is_on(void)
{
	return false;
}

int driver_motor_set_speed(uint8_t percent)
{
	return percent > 100 ? -EINVAL : -ENODEV;
}

uint8_t driver_motor_get_speed(void)
{
	return 0;
}

int driver_motor_set_min_duty(uint8_t percent)
{
	return percent > 99 ? -EINVAL : -ENODEV;
}

int driver_motor_ramp_to(uint8_t percent, uint32_t ms)
{
	ARG_UNUSED(ms);
	return percent > 100 ? -EINVAL : -ENODEV;
}

#endif

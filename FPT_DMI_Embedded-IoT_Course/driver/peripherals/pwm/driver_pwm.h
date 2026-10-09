/**
 * @file driver_pwm.h
 * @brief PWM outputs found through the devicetree aliases pwm0..pwm3.
 *
 * Every alias points to a "pwm-leds" style node with a "pwms" property. All channels of
 * one hardware timer share the same frequency.
 */
#ifndef DRIVER_PWM_H_
#define DRIVER_PWM_H_

#include <stdint.h>

#include <zephyr/drivers/pwm.h>

/** Number of PWM ids (pwm0..pwm3). */
#define DRIVER_PWM_MAX 4

/**
 * @brief Initialise channel pwm<id>, initially off (duty 0).
 *
 * @retval 0       Success
 * @retval -EINVAL id out of range
 * @retval -ENODEV alias pwm<id> missing, or the PWM controller is not ready
 */
int driver_pwm_init(uint8_t id);

/**
 * @brief Set frequency and duty cycle.
 *
 * @param id           PWM id
 * @param freq_hz      Frequency in Hz, at least 1
 * @param duty_percent 0..100
 *
 * @retval 0        Success
 * @retval -EINVAL  freq_hz is 0, duty above 100, or frequency not possible for the timer
 * @retval -ENODEV  alias missing
 * @retval -EACCES  driver_pwm_init() not called for this id
 */
int driver_pwm_set_freq_duty(uint8_t id, uint32_t freq_hz, uint8_t duty_percent);

/**
 * @brief Set period and pulse width in nanoseconds.
 * @retval as driver_pwm_set_freq_duty(); also -EINVAL when pulse_ns > period_ns
 */
int driver_pwm_set_pulse(uint8_t id, uint32_t period_ns, uint32_t pulse_ns);

/**
 * @brief Stop the signal (pin held at the inactive level).
 * @retval as driver_pwm_set_freq_duty()
 */
int driver_pwm_stop(uint8_t id);

/**
 * @brief Set period and pulse on a PWM specification. For other drivers (motor, servo,
 *        servo); students normally do not call it.
 *
 * @retval 0       Success
 * @retval -EINVAL NULL spec or pulse_ns > period_ns
 * @retval -ENODEV PWM controller not ready
 */
int driver_pwm_spec_set(const struct pwm_dt_spec *spec, uint32_t period_ns, uint32_t pulse_ns);

#endif /* DRIVER_PWM_H_ */

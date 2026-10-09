/**
 * @file driver_motor.h
 * @brief DC motor on one pin (alias "motor0"): speed control with PWM, or on/off with GPIO.
 *
 * The mode follows the devicetree node of the alias: a "pwms" property selects PWM mode
 * (uses driver_pwm), a "gpios" property selects GPIO mode. In GPIO mode the speed
 * functions return -ENOTSUP and the motor always runs at full speed.
 */
#ifndef DRIVER_MOTOR_H_
#define DRIVER_MOTOR_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Prepare the motor, stopped.
 * @retval 0       Success
 * @retval -ENODEV Alias motor0 missing, or its controller is not ready
 */
int driver_motor_init(void);

/**
 * @brief Run at the speed set before (100 % at start). GPIO mode: full speed.
 * @retval -EACCES driver_motor_init() not called
 */
int driver_motor_on(void);

/** @brief Stop. @retval -EACCES driver_motor_init() not called */
int driver_motor_off(void);

/** @brief true while the motor is running. */
bool driver_motor_is_on(void);

/**
 * @brief Set the speed, 0..100 %. 0 stops the motor; a value above 0 starts it.
 * @retval -ENOTSUP GPIO mode
 * @retval -EINVAL  percent above 100
 */
int driver_motor_set_speed(uint8_t percent);

/** @brief Speed in percent that is set (0 when stopped). GPIO mode: 0 or 100. */
uint8_t driver_motor_get_speed(void);

/**
 * @brief Smallest duty (percent) at which the motor starts to turn, typically 20..30.
 *        A speed of 1 % is mapped to it and 100 % stays 100 %, in between linearly.
 * @retval -ENOTSUP GPIO mode
 * @retval -EINVAL  percent above 99
 */
int driver_motor_set_min_duty(uint8_t percent);

/**
 * @brief Change speed gradually over @p ms milliseconds (blocks the calling thread).
 * @retval -ENOTSUP GPIO mode
 * @retval -EINVAL  percent above 100
 */
int driver_motor_ramp_to(uint8_t percent, uint32_t ms);

#endif /* DRIVER_MOTOR_H_ */

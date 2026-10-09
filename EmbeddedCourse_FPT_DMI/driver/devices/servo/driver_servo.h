/**
 * @file driver_servo.h
 * @brief Hobby servos on PWM (aliases servo0, servo1), 50 Hz, pulse width sets the angle.
 *
 * Uses driver_pwm. Both servos share TIM12 (PB14, PB15) and therefore the 20 ms period.
 */
#ifndef DRIVER_SERVO_H_
#define DRIVER_SERVO_H_

#include <stdint.h>

/** Number of servos (alias servo0 and servo1). */
#define DRIVER_SERVO_MAX 2

/** Pulse widths and range of a servo. */
struct driver_servo_cfg {
	uint16_t min_pulse_us; /**< Pulse for 0 degrees, default 500 */
	uint16_t max_pulse_us; /**< Pulse for max_angle, default 2500 */
	uint16_t max_angle;    /**< 180 for an SG90, 270 for 270 degree servos */
};

/** @brief SG90 style defaults: 500..2500 us over 180 degrees. */
#define DRIVER_SERVO_CFG_DEFAULT {.min_pulse_us = 500, .max_pulse_us = 2500, .max_angle = 180}

/**
 * @brief Prepare servo<id>. No pulse is sent until the first set call, so the servo does
 *        not jump.
 *
 * @param id  Servo number (alias servo<id>)
 * @param cfg Range, or NULL for DRIVER_SERVO_CFG_DEFAULT
 *
 * @retval 0       Success
 * @retval -EINVAL id out of range, min >= max pulse, max_angle 0, or pulse above the period
 * @retval -ENODEV Alias servo<id> missing, or PWM controller not ready
 */
int driver_servo_init(uint8_t id, const struct driver_servo_cfg *cfg);

/**
 * @brief Turn to an angle.
 * @retval -EINVAL deg above max_angle
 * @retval -EACCES driver_servo_init() not called
 */
int driver_servo_set_angle(uint8_t id, uint16_t deg);

/** @brief Send a given pulse width (for calibration). @retval -EINVAL us above the period */
int driver_servo_set_pulse_us(uint8_t id, uint16_t us);

/**
 * @brief Turn slowly from one angle to another in @p ms milliseconds (blocks).
 * @retval -EINVAL An angle above max_angle
 */
int driver_servo_sweep(uint8_t id, uint16_t from, uint16_t to, uint32_t ms);

/** @brief Stop the pulses: the servo goes limp and holds no torque. */
int driver_servo_release(uint8_t id);

#endif /* DRIVER_SERVO_H_ */

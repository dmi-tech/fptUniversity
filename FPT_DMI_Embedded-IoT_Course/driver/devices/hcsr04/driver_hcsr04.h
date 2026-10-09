/**
 * @file driver_hcsr04.h
 * @brief HC-SR04 ultrasonic distance sensor (alias "distance0").
 *
 * TRIG on PC0, ECHO on PC2 (through a 1 k / 2 k divider, the module outputs 5 V).
 */
#ifndef DRIVER_HCSR04_H_
#define DRIVER_HCSR04_H_

#include <stdint.h>

/** Shortest distance reported, in millimetres. Nearer objects give -ERANGE. */
#define DRIVER_HCSR04_MIN_MM 20
/** Longest distance reported, in millimetres. Farther objects give -ERANGE. */
#define DRIVER_HCSR04_MAX_MM 4000
/** Largest number of samples for driver_hcsr04_read_avg_mm(). */
#define DRIVER_HCSR04_MAX_SAMPLES 10

/**
 * @brief Check that the sensor is ready.
 * @retval 0       Ready
 * @retval -ENODEV Alias distance0 missing, CONFIG_SENSOR off, or sensor not ready
 */
int driver_hcsr04_init(void);

/**
 * @brief Measure once (takes up to about 60 ms). At least 60 ms are kept between two
 *        measurements: a call that comes too early waits.
 *
 * @param[out] mm Distance in millimetres
 * @retval 0       Success
 * @retval -EINVAL mm is NULL
 * @retval -ENODEV Sensor not available
 * @retval -ERANGE Result outside 20..4000 mm (nothing in front, or too near)
 * @retval -EIO    No ECHO pulse
 */
int driver_hcsr04_read_mm(uint32_t *mm);

/**
 * @brief Average of several measurements. With 3 or more samples the largest and the
 *        smallest are dropped first.
 *
 * @param samples Number of measurements, 1..DRIVER_HCSR04_MAX_SAMPLES
 * @param[out] mm Distance in millimetres
 * @retval -EINVAL samples out of range or mm NULL
 * @retval other   Errors of driver_hcsr04_read_mm(); the first failing measurement ends the call
 */
int driver_hcsr04_read_avg_mm(uint8_t samples, uint32_t *mm);

#endif /* DRIVER_HCSR04_H_ */

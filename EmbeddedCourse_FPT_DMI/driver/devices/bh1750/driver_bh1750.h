/**
 * @file driver_bh1750.h
 * @brief BH1750 ambient light sensor on I2C1 (alias "light0").
 */
#ifndef DRIVER_BH1750_H_
#define DRIVER_BH1750_H_

/**
 * @brief Check that the sensor is ready.
 * @retval 0       Ready
 * @retval -ENODEV Alias light0 missing, CONFIG_SENSOR off, or sensor not ready
 */
int driver_bh1750_init(void);

/**
 * @brief Measure the illuminance once (takes 120..180 ms in the default resolution).
 *
 * @param[out] lux Illuminance in lux
 * @retval 0       Success
 * @retval -EINVAL lux is NULL
 * @retval -ENODEV Sensor not available
 * @retval other   Negative errno from the sensor driver
 */
int driver_bh1750_read_lux(float *lux);

#endif /* DRIVER_BH1750_H_ */

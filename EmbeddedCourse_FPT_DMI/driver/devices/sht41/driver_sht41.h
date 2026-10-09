/**
 * @file driver_sht41.h
 * @brief SHT41 temperature and humidity sensor on I2C2 (node label "sht41", on the board).
 */
#ifndef DRIVER_SHT41_H_
#define DRIVER_SHT41_H_

/**
 * @brief Check that the sensor is ready.
 * @retval 0       Ready
 * @retval -ENODEV Node "sht41" missing, CONFIG_SENSOR off, or sensor not ready
 */
int driver_sht41_init(void);

/**
 * @brief Measure once (takes about 10 ms).
 *
 * @param[out] temp_c   Temperature in degrees Celsius
 * @param[out] humidity Relative humidity in percent
 *
 * @retval 0       Success
 * @retval -EINVAL A pointer is NULL
 * @retval -ENODEV Sensor not available
 * @retval other   Negative errno from the sensor driver (for example -EIO)
 */
int driver_sht41_read(float *temp_c, float *humidity);

#endif /* DRIVER_SHT41_H_ */

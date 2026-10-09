/**
 * @file driver_dht11.h
 * @brief DHT11 temperature / humidity sensors on one GPIO each (aliases dht0, dht1).
 */
#ifndef DRIVER_DHT11_H_
#define DRIVER_DHT11_H_

#include <stdint.h>

/** Number of sensors (alias dht0 and dht1). */
#define DRIVER_DHT11_MAX 2

/** Shortest time between two readings of one sensor, in milliseconds. */
#define DRIVER_DHT11_MIN_INTERVAL_MS 2000

/**
 * @brief Prepare sensor dht<id>.
 * @retval 0       Ready
 * @retval -EINVAL id out of range
 * @retval -ENODEV Alias dht<id> missing, CONFIG_SENSOR off, or sensor not ready
 */
int driver_dht11_init(uint8_t id);

/**
 * @brief Read temperature and humidity (whole degrees and percent).
 *
 * @param id            Sensor id
 * @param[out] temp_c   Temperature in degrees Celsius
 * @param[out] humidity Relative humidity in percent
 *
 * @retval 0       Success
 * @retval -EINVAL Bad id or NULL pointer
 * @retval -ENODEV Sensor not available
 * @retval -EAGAIN Less than DRIVER_DHT11_MIN_INTERVAL_MS since the last reading
 * @retval other   Negative errno from the sensor driver (for example -EIO for a bad checksum)
 */
int driver_dht11_read(uint8_t id, int *temp_c, int *humidity);

#endif /* DRIVER_DHT11_H_ */

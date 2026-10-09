/**
 * @file driver_sht41.c
 * @brief SHT41 driver on top of the Zephyr sensor API.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "driver_sht41.h"

#if IS_ENABLED(CONFIG_SENSOR) && DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(sht41))

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

static const struct device *const sht_dev = DEVICE_DT_GET(DT_NODELABEL(sht41));

int driver_sht41_init(void)
{
	return device_is_ready(sht_dev) ? 0 : -ENODEV;
}

int driver_sht41_read(float *temp_c, float *humidity)
{
	struct sensor_value t;
	struct sensor_value h;
	int ret;

	if (temp_c == NULL || humidity == NULL) {
		return -EINVAL;
	}
	if (!device_is_ready(sht_dev)) {
		return -ENODEV;
	}

	ret = sensor_sample_fetch(sht_dev);
	if (ret == 0) {
		ret = sensor_channel_get(sht_dev, SENSOR_CHAN_AMBIENT_TEMP, &t);
	}
	if (ret == 0) {
		ret = sensor_channel_get(sht_dev, SENSOR_CHAN_HUMIDITY, &h);
	}
	if (ret < 0) {
		return ret;
	}

	*temp_c = (float)sensor_value_to_double(&t);
	*humidity = (float)sensor_value_to_double(&h);
	return 0;
}

#else /* sensor not available */

int driver_sht41_init(void)
{
	return -ENODEV;
}

int driver_sht41_read(float *temp_c, float *humidity)
{
	if (temp_c == NULL || humidity == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

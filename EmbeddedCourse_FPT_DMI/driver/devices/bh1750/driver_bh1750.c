/**
 * @file driver_bh1750.c
 * @brief BH1750 driver on top of the Zephyr sensor API.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "driver_bh1750.h"

#if IS_ENABLED(CONFIG_SENSOR) && DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(light0))

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

static const struct device *const light_dev = DEVICE_DT_GET(DT_ALIAS(light0));

int driver_bh1750_init(void)
{
	return device_is_ready(light_dev) ? 0 : -ENODEV;
}

int driver_bh1750_read_lux(float *lux)
{
	struct sensor_value val;
	int ret;

	if (lux == NULL) {
		return -EINVAL;
	}
	if (!device_is_ready(light_dev)) {
		return -ENODEV;
	}

	ret = sensor_sample_fetch(light_dev);
	if (ret == 0) {
		ret = sensor_channel_get(light_dev, SENSOR_CHAN_LIGHT, &val);
	}
	if (ret < 0) {
		return ret;
	}
	*lux = (float)sensor_value_to_double(&val);
	return 0;
}

#else /* sensor not available */

int driver_bh1750_init(void)
{
	return -ENODEV;
}

int driver_bh1750_read_lux(float *lux)
{
	if (lux == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

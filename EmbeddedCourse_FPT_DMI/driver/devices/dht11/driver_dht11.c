/**
 * @file driver_dht11.c
 * @brief DHT11 driver on top of the Zephyr "aosong,dht" sensor driver.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "driver_dht11.h"

#if IS_ENABLED(CONFIG_SENSOR)

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

/* NULL when the alias is absent */
#define DHT_DEV_OR_NULL(name) \
	COND_CODE_1(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(name)), (DEVICE_DT_GET(DT_ALIAS(name))), (NULL))

static const struct device *const dev_of[DRIVER_DHT11_MAX] = {
	DHT_DEV_OR_NULL(dht0),
	DHT_DEV_OR_NULL(dht1),
};

static int64_t last_read_ms[DRIVER_DHT11_MAX];
static bool has_read[DRIVER_DHT11_MAX];

int driver_dht11_init(uint8_t id)
{
	if (id >= DRIVER_DHT11_MAX) {
		return -EINVAL;
	}
	if (dev_of[id] == NULL || !device_is_ready(dev_of[id])) {
		return -ENODEV;
	}
	return 0;
}

int driver_dht11_read(uint8_t id, int *temp_c, int *humidity)
{
	struct sensor_value t;
	struct sensor_value h;
	int64_t now;
	int ret;

	if (id >= DRIVER_DHT11_MAX || temp_c == NULL || humidity == NULL) {
		return -EINVAL;
	}
	if (dev_of[id] == NULL || !device_is_ready(dev_of[id])) {
		return -ENODEV;
	}

	now = k_uptime_get();
	if (has_read[id] && now - last_read_ms[id] < DRIVER_DHT11_MIN_INTERVAL_MS) {
		return -EAGAIN;
	}
	last_read_ms[id] = now;
	has_read[id] = true;

	ret = sensor_sample_fetch(dev_of[id]);
	if (ret == 0) {
		ret = sensor_channel_get(dev_of[id], SENSOR_CHAN_AMBIENT_TEMP, &t);
	}
	if (ret == 0) {
		ret = sensor_channel_get(dev_of[id], SENSOR_CHAN_HUMIDITY, &h);
	}
	if (ret < 0) {
		return ret;
	}

	*temp_c = t.val1;
	*humidity = h.val1;
	return 0;
}

#else /* CONFIG_SENSOR off */

int driver_dht11_init(uint8_t id)
{
	return id >= DRIVER_DHT11_MAX ? -EINVAL : -ENODEV;
}

int driver_dht11_read(uint8_t id, int *temp_c, int *humidity)
{
	if (id >= DRIVER_DHT11_MAX || temp_c == NULL || humidity == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

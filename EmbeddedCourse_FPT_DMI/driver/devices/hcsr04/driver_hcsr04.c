/**
 * @file driver_hcsr04.c
 * @brief HC-SR04 driver on top of the Zephyr "hc-sr04" sensor driver.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "driver_hcsr04.h"

#define MEASURE_GAP_MS 60

#if IS_ENABLED(CONFIG_SENSOR) && DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(distance0))

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

static const struct device *const dist_dev = DEVICE_DT_GET(DT_ALIAS(distance0));
static int64_t last_ms;
static bool measured;
static K_MUTEX_DEFINE(lock);

int driver_hcsr04_init(void)
{
	return device_is_ready(dist_dev) ? 0 : -ENODEV;
}

int driver_hcsr04_read_mm(uint32_t *mm)
{
	struct sensor_value val;
	int64_t value_mm;
	int ret;

	if (mm == NULL) {
		return -EINVAL;
	}
	if (!device_is_ready(dist_dev)) {
		return -ENODEV;
	}

	k_mutex_lock(&lock, K_FOREVER);
	if (measured) {
		int64_t wait = MEASURE_GAP_MS - (k_uptime_get() - last_ms);

		if (wait > 0) {
			k_msleep((int32_t)wait); /* let the previous echo die away */
		}
	}
	ret = sensor_sample_fetch(dist_dev);
	last_ms = k_uptime_get();
	measured = true;
	k_mutex_unlock(&lock);

	if (ret < 0) {
		return -EIO;
	}
	ret = sensor_channel_get(dist_dev, SENSOR_CHAN_DISTANCE, &val);
	if (ret < 0) {
		return ret;
	}

	/* The sensor reports metres as val1 + val2 / 1e6 */
	value_mm = (int64_t)val.val1 * 1000 + val.val2 / 1000;
	if (value_mm < DRIVER_HCSR04_MIN_MM || value_mm > DRIVER_HCSR04_MAX_MM) {
		return -ERANGE;
	}
	*mm = (uint32_t)value_mm;
	return 0;
}

int driver_hcsr04_read_avg_mm(uint8_t samples, uint32_t *mm)
{
	uint32_t sum = 0;
	uint32_t min = UINT32_MAX;
	uint32_t max = 0;

	if (mm == NULL || samples == 0 || samples > DRIVER_HCSR04_MAX_SAMPLES) {
		return -EINVAL;
	}

	for (uint8_t i = 0; i < samples; i++) {
		uint32_t v;
		int ret = driver_hcsr04_read_mm(&v);

		if (ret < 0) {
			return ret;
		}
		sum += v;
		min = MIN(min, v);
		max = MAX(max, v);
	}

	if (samples >= 3) {
		sum -= min + max;
		samples -= 2;
	}
	*mm = (sum + samples / 2) / samples; /* rounded */
	return 0;
}

#else /* sensor not available */

int driver_hcsr04_init(void)
{
	return -ENODEV;
}

int driver_hcsr04_read_mm(uint32_t *mm)
{
	if (mm == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_hcsr04_read_avg_mm(uint8_t samples, uint32_t *mm)
{
	if (mm == NULL || samples == 0 || samples > DRIVER_HCSR04_MAX_SAMPLES) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

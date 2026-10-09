/**
 * @file driver_adc.c
 * @brief ADC driver: Zephyr ADC API for the external pin, sensor API for internal channels.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>

#include "driver_adc.h"

#define USER_NODE DT_PATH(zephyr_user)

/* The external channel exists when zephyr,user has an io-channels property. */
#define HAS_EXT_CHANNEL (DT_NODE_EXISTS(USER_NODE) && DT_NODE_HAS_PROP(USER_NODE, io_channels))

#define HAS_DIE_TEMP (IS_ENABLED(CONFIG_SENSOR) && DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(die_temp)))
#define HAS_VREF     (IS_ENABLED(CONFIG_SENSOR) && DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(vref)))

#if HAS_DIE_TEMP || HAS_VREF
#include <zephyr/drivers/sensor.h>
#endif

static K_MUTEX_DEFINE(adc_lock);

/* ---- External channel --------------------------------------------------------------- */

#if HAS_EXT_CHANNEL

static const struct adc_dt_spec ext_ch = ADC_DT_SPEC_GET_BY_IDX(USER_NODE, 0);
static bool ext_ready;

int driver_adc_init(void)
{
	int ret;

	if (!adc_is_ready_dt(&ext_ch)) {
		return -ENODEV;
	}
	ret = adc_channel_setup_dt(&ext_ch);
	if (ret < 0) {
		return ret;
	}
	ext_ready = true;
	return 0;
}

int driver_adc_read_raw(int16_t *raw)
{
	int16_t buf;
	struct adc_sequence seq = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};
	int ret;

	if (raw == NULL) {
		return -EINVAL;
	}
	if (!ext_ready) {
		return -ENODEV;
	}

	k_mutex_lock(&adc_lock, K_FOREVER);
	ret = adc_sequence_init_dt(&ext_ch, &seq);
	if (ret == 0) {
		ret = adc_read_dt(&ext_ch, &seq);
	}
	k_mutex_unlock(&adc_lock);
	if (ret < 0) {
		return ret;
	}

	*raw = buf < 0 ? 0 : buf; /* single-ended: a negative value is noise around 0 */
	return 0;
}

#else /* no external channel */

int driver_adc_init(void)
{
	return 0;
}

int driver_adc_read_raw(int16_t *raw)
{
	if (raw == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

static int32_t raw_to_mv(int32_t raw)
{
	return raw * DRIVER_ADC_VREF_MV / DRIVER_ADC_RAW_MAX;
}

int driver_adc_read_mv(int32_t *mv)
{
	int16_t raw;
	int ret;

	if (mv == NULL) {
		return -EINVAL;
	}
	ret = driver_adc_read_raw(&raw);
	if (ret < 0) {
		return ret;
	}
	*mv = raw_to_mv(raw);
	return 0;
}

int driver_adc_read_avg_mv(uint8_t samples, int32_t *mv)
{
	int32_t sum = 0;

	if (mv == NULL || samples == 0 || samples > DRIVER_ADC_MAX_AVG) {
		return -EINVAL;
	}
	for (uint8_t i = 0; i < samples; i++) {
		int16_t raw;
		int ret = driver_adc_read_raw(&raw);

		if (ret < 0) {
			return ret;
		}
		sum += raw;
	}
	*mv = raw_to_mv(sum / samples);
	return 0;
}

/* ---- Internal channels (sensor API) ------------------------------------------------- */

#if HAS_DIE_TEMP || HAS_VREF
/* Read one value of a sensor and convert it to thousandths (milli-C or millivolt). */
static int sensor_read_milli(const struct device *dev, enum sensor_channel chan, int32_t *out)
{
	struct sensor_value val;
	int ret;

	if (!device_is_ready(dev)) {
		return -ENODEV;
	}
	k_mutex_lock(&adc_lock, K_FOREVER);
	ret = sensor_sample_fetch(dev);
	if (ret == 0) {
		ret = sensor_channel_get(dev, chan, &val);
	}
	k_mutex_unlock(&adc_lock);
	if (ret < 0) {
		return ret;
	}
	*out = val.val1 * 1000 + val.val2 / 1000;
	return 0;
}
#endif

int driver_adc_read_die_temp(int32_t *milli_c)
{
	if (milli_c == NULL) {
		return -EINVAL;
	}
#if HAS_DIE_TEMP
	return sensor_read_milli(DEVICE_DT_GET(DT_NODELABEL(die_temp)), SENSOR_CHAN_DIE_TEMP,
				 milli_c);
#else
	return -ENODEV;
#endif
}

int driver_adc_read_vdda(int32_t *mv)
{
	if (mv == NULL) {
		return -EINVAL;
	}
#if HAS_VREF
	return sensor_read_milli(DEVICE_DT_GET(DT_NODELABEL(vref)), SENSOR_CHAN_VOLTAGE, mv);
#else
	return -ENODEV;
#endif
}

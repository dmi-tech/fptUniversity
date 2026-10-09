/**
 * @file driver_adc.h
 * @brief ADC: one external channel (first entry of zephyr,user io-channels) plus the
 *        internal die temperature and VREFINT channels.
 */
#ifndef DRIVER_ADC_H_
#define DRIVER_ADC_H_

#include <stdint.h>

/** Reference voltage assumed for raw to millivolt conversion (3.3 V analog supply). */
#define DRIVER_ADC_VREF_MV   3300
/** Largest raw value of the 12 bit converter. */
#define DRIVER_ADC_RAW_MAX   4095
/** Largest number of samples accepted by driver_adc_read_avg_mv(). */
#define DRIVER_ADC_MAX_AVG   64

/**
 * @brief Initialise the external channel (when declared) and the internal sensors.
 *
 * Works with only the internal channels: a missing external channel is not an error here,
 * the external read functions then return -ENODEV.
 *
 * @retval 0       Success
 * @retval -ENODEV ADC controller not ready
 */
int driver_adc_init(void);

/**
 * @brief Raw value of the external channel.
 * @param[out] raw 0..4095
 * @retval 0       Success
 * @retval -EINVAL raw is NULL
 * @retval -ENODEV No external channel in zephyr,user io-channels
 */
int driver_adc_read_raw(int16_t *raw);

/**
 * @brief Voltage on the external channel pin, in millivolts (raw * 3300 / 4095).
 * @retval 0 or negative errno, as driver_adc_read_raw()
 */
int driver_adc_read_mv(int32_t *mv);

/**
 * @brief Average of several reads of the external channel, in millivolts.
 * @param samples Number of reads, 1..DRIVER_ADC_MAX_AVG
 * @param[out] mv Result
 * @retval -EINVAL samples out of range or mv is NULL
 */
int driver_adc_read_avg_mv(uint8_t samples, int32_t *mv);

/**
 * @brief Chip (die) temperature in milli-degrees Celsius (25000 = 25.000 C).
 * @retval -ENODEV die_temp sensor missing (needs CONFIG_SENSOR and &die_temp okay)
 */
int driver_adc_read_die_temp(int32_t *milli_c);

/**
 * @brief Real analog supply voltage VDDA in millivolts, measured with VREFINT.
 * @retval -ENODEV vref sensor missing (needs CONFIG_SENSOR and &vref okay)
 */
int driver_adc_read_vdda(int32_t *mv);

#endif /* DRIVER_ADC_H_ */

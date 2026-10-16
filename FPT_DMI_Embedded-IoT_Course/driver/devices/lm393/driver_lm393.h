/**
 * @file driver_lm393.h
 * @brief LM393 comparator modules (light, sound, soil moisture, IR obstacle, flame...):
 *        digital output DO with a threshold set by the module trimmer, optional analog
 *        output AO.
 *
 * Both come from the zephyr,user node:
 *  - DO of module <id>: entry <id> of the property "lm393-gpios". The GPIO flags set what
 *    "detected" means (GPIO_ACTIVE_LOW: DO = 0 is a detection, the usual case).
 *  - AO of module <id> (optional): the io-channels entry named "lm393_<id>".
 */
#ifndef DRIVER_LM393_H_
#define DRIVER_LM393_H_

#include <stdbool.h>
#include <stdint.h>

/** Number of modules (ids 0 and 1). */
#define DRIVER_LM393_MAX 2

/** Default hold time, see driver_lm393_set_hold(). */
#define DRIVER_LM393_HOLD_MS_DEFAULT 100

/** Largest number of samples for driver_lm393_read_avg_mv(). */
#define DRIVER_LM393_MAX_AVG 64

/**
 * @brief Detection callback.
 *
 * Called from the system workqueue (not from an interrupt). @p detected becomes true as soon
 * as DO becomes active, even for a pulse of a few microseconds (a clap on a sound module).
 * It becomes false once DO has stayed inactive for the hold time: the chatter of the
 * comparator around the threshold gives one detection, not many.
 *
 * @param id       Module id
 * @param detected true when the module detects (DO active)
 */
typedef void (*driver_lm393_cb_t)(uint8_t id, bool detected);

/**
 * @brief Initialise the DO input of module <id>.
 *
 * @param id Module id, 0..DRIVER_LM393_MAX-1
 * @param cb Callback on a detection change, or NULL to read only with driver_lm393_get()
 *
 * @retval 0       Success
 * @retval -EINVAL id out of range
 * @retval -ENODEV lm393-gpios entry <id> missing, or its GPIO controller is not ready
 * @retval other   GPIO error (-EBUSY: interrupt line already used by a pin with the same number)
 */
int driver_lm393_init(uint8_t id, driver_lm393_cb_t cb);

/**
 * @brief Read DO now (no hold time).
 *
 * @retval 1       Detected
 * @retval 0       Not detected
 * @retval -EINVAL id out of range
 * @retval -ENODEV lm393-gpios entry <id> missing
 * @retval -EACCES driver_lm393_init() not called for this id
 */
int driver_lm393_get(uint8_t id);

/**
 * @brief Set the hold time of the callback: how long DO must stay inactive before
 *        "not detected" is reported. Can be called before or after driver_lm393_init().
 *
 * @param id Module id
 * @param ms Hold time in milliseconds, 1..60000 (default DRIVER_LM393_HOLD_MS_DEFAULT)
 * @retval 0       Success
 * @retval -EINVAL id or ms out of range
 */
int driver_lm393_set_hold(uint8_t id, uint32_t ms);

/**
 * @brief Raw value of AO, 0..4095 (12 bit). Initialises the ADC channel on the first call.
 *
 * @retval 0       Success
 * @retval -EINVAL id out of range or raw NULL
 * @retval -ENODEV No io-channels entry named "lm393_<id>", CONFIG_ADC off, or ADC not ready
 * @retval other   ADC error
 */
int driver_lm393_read_raw(uint8_t id, int16_t *raw);

/**
 * @brief Voltage on the AO pin in millivolts (raw * 3300 / 4095). Same errors as
 *        driver_lm393_read_raw().
 */
int driver_lm393_read_mv(uint8_t id, int32_t *mv);

/**
 * @brief Average of @p samples AO reads (1..DRIVER_LM393_MAX_AVG), in millivolts.
 *
 * @retval -EINVAL samples out of range or mv NULL
 * @retval other   Errors of driver_lm393_read_raw()
 */
int driver_lm393_read_avg_mv(uint8_t id, uint8_t samples, int32_t *mv);

/**
 * @brief AO as a percentage of full scale, 0..100. On most modules the value goes UP when
 *        the measured quantity goes DOWN (darker, drier); use 100 - percent if needed.
 *        Same errors as driver_lm393_read_raw().
 */
int driver_lm393_read_percent(uint8_t id, uint8_t *percent);

#endif /* DRIVER_LM393_H_ */

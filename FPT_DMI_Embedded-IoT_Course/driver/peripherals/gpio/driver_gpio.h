/**
 * @file driver_gpio.h
 * @brief Digital outputs and debounced digital inputs, found through devicetree aliases.
 *
 * Outputs use the aliases out0..out3 (gpio-leds style nodes with a "gpios" property),
 * inputs use in0..in3. The number in the alias is the @p id given to the functions.
 */
#ifndef DRIVER_GPIO_H_
#define DRIVER_GPIO_H_

#include <stdbool.h>
#include <stdint.h>

/** Number of output ids (out0..out3) and of input ids (in0..in3). */
#define DRIVER_GPIO_MAX_OUT 4
#define DRIVER_GPIO_MAX_IN  4

/**
 * @brief Input callback.
 *
 * Called from the system workqueue (not from an interrupt) once the input has been
 * stable for DRIVER_GPIO_DEBOUNCE_MS after a change.
 *
 * @param id     Input id (the n of alias in<n>)
 * @param active true when the input is active (for example button pressed)
 */
typedef void (*driver_gpio_in_cb_t)(uint8_t id, bool active);

/**
 * @brief Initialise output out<id>, initially off.
 *
 * @retval 0       Success
 * @retval -EINVAL id out of range
 * @retval -ENODEV alias out<id> missing, or its GPIO controller is not ready
 */
int driver_gpio_out_init(uint8_t id);

/**
 * @brief Switch an output on or off (logical level, active-low pins are handled).
 *
 * @retval 0        Success
 * @retval -EINVAL  id out of range
 * @retval -ENODEV  alias missing
 * @retval -EACCES  driver_gpio_out_init() not called for this id
 */
int driver_gpio_set(uint8_t id, bool on);

/**
 * @brief Toggle an output. Same return values as driver_gpio_set().
 */
int driver_gpio_toggle(uint8_t id);

/**
 * @brief Initialise input in<id>.
 *
 * @param id Input id
 * @param cb Callback on a debounced change, or NULL to read only with driver_gpio_get()
 *
 * @retval 0       Success
 * @retval -EINVAL id out of range
 * @retval -ENODEV alias in<id> missing, or its GPIO controller is not ready
 * @retval -EBUSY  The interrupt cannot be configured (EXTI line already used)
 */
int driver_gpio_in_init(uint8_t id, driver_gpio_in_cb_t cb);

/**
 * @brief Read an input.
 *
 * @return 1 when active (for example button pressed), 0 when not, negative errno on error
 *         (-EINVAL, -ENODEV, -EACCES when not initialised)
 */
int driver_gpio_get(uint8_t id);

#endif /* DRIVER_GPIO_H_ */

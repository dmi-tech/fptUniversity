/**
 * @file driver_encoder.h
 * @brief Rotary encoder with push button (KY-040), through the Zephyr input subsystem.
 *
 * The encoder A/B lines are the "gpio-qdec" node aliased "encoder0" (PC6, PC7); the push
 * button is the board node user_btn (PA10) reporting INPUT_KEY_ENTER.
 */
#ifndef DRIVER_ENCODER_H_
#define DRIVER_ENCODER_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Called for every detent turned. Runs in the input thread (printk is allowed).
 * @param position New position (after limits are applied)
 * @param step     +1 or -1
 */
typedef void (*driver_encoder_cb_t)(int32_t position, int8_t step);

/** @brief Called when the button is pressed (true) or released (false). Input thread. */
typedef void (*driver_encoder_btn_cb_t)(bool pressed);

/**
 * @brief Start the encoder with position 0.
 * @retval 0       Success
 * @retval -ENODEV Alias encoder0 missing, CONFIG_INPUT off, or device not ready
 */
int driver_encoder_init(void);

/** @brief Position: the number of detents turned, signed. */
int32_t driver_encoder_get_position(void);

/** @brief Set the position (clamped to the limits). */
void driver_encoder_set_position(int32_t pos);

/** @brief Keep the position inside [min, max], for example a volume 0..100. */
void driver_encoder_set_limits(int32_t min, int32_t max);

/** @brief Callback for every detent; NULL removes it. */
void driver_encoder_set_callback(driver_encoder_cb_t cb);

/** @brief Callback for button press and release; NULL removes it. */
void driver_encoder_set_button_callback(driver_encoder_btn_cb_t cb);

/** @brief true while the button is held down. */
bool driver_encoder_button_is_pressed(void);

#endif /* DRIVER_ENCODER_H_ */

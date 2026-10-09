/**
 * @file driver_lcd.h
 * @brief HD44780 character LCD in 4-bit mode behind a PCF8574 I2C backpack (on I2C1).
 *
 * Supports 8, 16, 20, 24 or 40 columns by 1, 2 or 4 rows (at most 80 characters in all).
 * Uses driver_i2c. Several displays can be used: one struct driver_lcd for each.
 */
#ifndef DRIVER_LCD_H_
#define DRIVER_LCD_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

/* PCF8574 output bit of each LCD signal (common backpack wiring) */
#define LCD_PIN_RS BIT(0)
#define LCD_PIN_RW BIT(1)
#define LCD_PIN_E  BIT(2)
#define LCD_PIN_BL BIT(3)
#define LCD_PIN_D4 BIT(4)
#define LCD_PIN_D5 BIT(5)
#define LCD_PIN_D6 BIT(6)
#define LCD_PIN_D7 BIT(7)

/** Display description given to driver_lcd_init(). */
struct driver_lcd_cfg {
	uint8_t addr;     /**< 7-bit I2C address: 0x27, 0x3F, ... */
	uint8_t cols;     /**< 8, 16, 20, 24 or 40 */
	uint8_t rows;     /**< 1, 2 or 4 */
	bool split_16x1;  /**< 16x1 modules wired like an 8x2 (right half not shown otherwise) */
	bool font_5x10;   /**< 5x10 font, only for 1-row displays */
};

/** @brief Config for any size. */
#define DRIVER_LCD_CFG(_addr, _cols, _rows) {.addr = (_addr), .cols = (_cols), .rows = (_rows)}
/** @brief Config for a 16x2 display (LCD1602). */
#define DRIVER_LCD_CFG_16X2(_addr) DRIVER_LCD_CFG(_addr, 16, 2)
/** @brief Config for a 20x4 display (LCD2004). */
#define DRIVER_LCD_CFG_20X4(_addr) DRIVER_LCD_CFG(_addr, 20, 4)

/** @brief One display. The fields are private; a zeroed variable is ready for init. */
struct driver_lcd {
	struct driver_lcd_cfg cfg;
	struct k_mutex lock;
	uint8_t row_addr[4]; /* DDRAM address of the start of each row */
	uint8_t col;         /* cursor position tracked for print() */
	uint8_t row;
	uint8_t display_ctl; /* display on/off, cursor, blink bits */
	bool backlight;
	bool ready;
};

/**
 * @brief Initialise the display (does the HD44780 4-bit reset sequence), backlight on.
 *
 * @retval 0        Success
 * @retval -EINVAL  NULL argument, or size not in {8,16,20,24,40} x {1,2,4}
 * @retval -ENOTSUP cols * rows above 80
 * @retval -EIO     The backpack does not answer on its I2C address
 * @retval -ENODEV  I2C1 not available
 */
int driver_lcd_init(struct driver_lcd *lcd, const struct driver_lcd_cfg *cfg);

/** @brief Clear the display and move the cursor to the first position. */
int driver_lcd_clear(struct driver_lcd *lcd);

/** @brief Move the cursor to the first position without clearing. */
int driver_lcd_home(struct driver_lcd *lcd);

/** @brief Move the cursor. @retval -EINVAL col or row outside the display */
int driver_lcd_set_cursor(struct driver_lcd *lcd, uint8_t col, uint8_t row);

/**
 * @brief Print a string at the cursor. Text is cut at the end of the row; '\n' goes to the
 *        start of the next row (and the rest is dropped after the last row).
 */
int driver_lcd_print(struct driver_lcd *lcd, const char *str);

/** @brief Move the cursor, then print. */
int driver_lcd_print_at(struct driver_lcd *lcd, uint8_t col, uint8_t row, const char *str);

/** @brief printf at the cursor (output limited to 80 characters). */
int driver_lcd_printf(struct driver_lcd *lcd, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/** @brief Fill a row with spaces; the cursor ends at the start of that row. */
int driver_lcd_clear_row(struct driver_lcd *lcd, uint8_t row);

/** @brief Backlight on or off. */
int driver_lcd_backlight(struct driver_lcd *lcd, bool on);

/** @brief Show the underline cursor and/or the blinking block. */
int driver_lcd_cursor(struct driver_lcd *lcd, bool show, bool blink);

/**
 * @brief Define a custom 5x8 character.
 * @param slot   0..7 (print it with character code 0..7)
 * @param bitmap 8 rows, the low 5 bits of each byte are the pixels
 */
int driver_lcd_create_char(struct driver_lcd *lcd, uint8_t slot, const uint8_t bitmap[8]);

/** @brief Write one character at the cursor ('\n' goes to the next row). */
int driver_lcd_putc(struct driver_lcd *lcd, char c);

/** @brief Number of columns and rows of the display. */
int driver_lcd_get_size(const struct driver_lcd *lcd, uint8_t *cols, uint8_t *rows);

#endif /* DRIVER_LCD_H_ */

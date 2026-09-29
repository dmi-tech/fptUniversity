#ifndef LCD_PCF8574_H
#define LCD_PCF8574_H
#include <stdbool.h>
#include <stdint.h>

/* HD44780 20x4 character LCD behind a PCF8574 / PCF8574A I2C backpack on
 * I2C1 (PB6 SCL = U16-10, PB9 SDA = U16-9). */
#define LCD_COLS 20
#define LCD_ROWS 4

/* Probe the backpack (0x27/0x3F first, then 0x20-0x27, 0x38-0x3F) and
 * initialise the controller. Returns 0 or a negative errno. */
int  lcd_init(void);
bool lcd_is_ready(void);
/* I2C address found by lcd_init(), 0 when none */
uint8_t lcd_addr(void);

void lcd_clear(void);
/* Write one full row, space-padded or truncated to LCD_COLS. A bus error
 * marks the LCD not ready so the caller can retry lcd_init() later. */
int  lcd_write_line(uint8_t row, const char *text);

#endif /* LCD_PCF8574_H */

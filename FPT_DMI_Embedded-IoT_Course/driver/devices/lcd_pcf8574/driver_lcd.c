/**
 * @file driver_lcd.c
 * @brief HD44780 LCD through a PCF8574 backpack, 4-bit mode, write only (RW held low).
 *
 * The busy flag is never read: every command uses the fixed HD44780 execution time. One
 * I2C byte takes about 90 us at 100 kHz, longer than the 37 us of most commands, so only
 * "clear" and "home" need an explicit delay.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/sys/util.h>

#include "driver_i2c.h"
#include "driver_lcd.h"

#define LCD_BUS DRIVER_I2C1

#define CMD_CLEAR       0x01
#define CMD_HOME        0x02
#define CMD_ENTRY_INC   0x06
#define CMD_DISPLAY     0x08 /* + display / cursor / blink bits */
#define CMD_FUNCTION    0x20 /* + 4-bit interface bits */
#define CMD_SET_CGRAM   0x40
#define CMD_SET_DDRAM   0x80

#define DISPLAY_ON      0x04
#define CURSOR_ON       0x02
#define BLINK_ON        0x01

#define FUNC_2LINE      0x08
#define FUNC_5X10       0x04

#define MAX_CHARS       80

static bool size_ok(uint8_t cols, uint8_t rows)
{
	bool cols_ok = cols == 8 || cols == 16 || cols == 20 || cols == 24 || cols == 40;

	return cols_ok && (rows == 1 || rows == 2 || rows == 4);
}

/* Every I2C problem is reported as -EIO, except a missing bus */
static int map_err(int ret)
{
	return (ret < 0 && ret != -ENODEV) ? -EIO : ret;
}

static int write_nibble(struct driver_lcd *lcd, uint8_t nibble, uint8_t flags)
{
	uint8_t v = (nibble & 0xF0) | flags | (lcd->backlight ? LCD_PIN_BL : 0);
	/* RS and data first, then E high, then E low: the LCD latches on the falling edge */
	const uint8_t seq[] = {v, v | LCD_PIN_E, v};

	return map_err(driver_i2c_write(LCD_BUS, lcd->cfg.addr, seq, sizeof(seq)));
}

static int write_byte(struct driver_lcd *lcd, uint8_t value, uint8_t flags)
{
	int ret = write_nibble(lcd, value & 0xF0, flags);

	if (ret == 0) {
		ret = write_nibble(lcd, (uint8_t)(value << 4), flags);
	}
	return ret;
}

static int command(struct driver_lcd *lcd, uint8_t cmd)
{
	int ret = write_byte(lcd, cmd, 0);

	if (ret == 0 && (cmd == CMD_CLEAR || cmd == CMD_HOME)) {
		k_msleep(2); /* 1.52 ms */
	}
	return ret;
}

static int data(struct driver_lcd *lcd, uint8_t c)
{
	return write_byte(lcd, c, LCD_PIN_RS);
}

/* DDRAM address of a position. Callers have checked col and row. */
static uint8_t ddram_addr(const struct driver_lcd *lcd, uint8_t col, uint8_t row)
{
	if (lcd->cfg.rows == 1 && lcd->cfg.split_16x1 && col >= 8) {
		return 0x40 + (col - 8); /* second half of a 16x1 wired as 8x2 */
	}
	return lcd->row_addr[row] + col;
}

static int move_cursor(struct driver_lcd *lcd, uint8_t col, uint8_t row)
{
	int ret = command(lcd, CMD_SET_DDRAM | ddram_addr(lcd, col, row));

	if (ret == 0) {
		lcd->col = col;
		lcd->row = row;
	}
	return ret;
}

static int check_ready(const struct driver_lcd *lcd)
{
	if (lcd == NULL) {
		return -EINVAL;
	}
	return lcd->ready ? 0 : -EACCES;
}

int driver_lcd_init(struct driver_lcd *lcd, const struct driver_lcd_cfg *cfg)
{
	uint8_t func;
	int ret;

	if (lcd == NULL || cfg == NULL || !size_ok(cfg->cols, cfg->rows)) {
		return -EINVAL;
	}
	if (cfg->cols * cfg->rows > MAX_CHARS) {
		return -ENOTSUP;
	}

	/* A first call on a zeroed variable creates the mutex; re-init keeps it */
	if (!lcd->ready) {
		k_mutex_init(&lcd->lock);
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	lcd->ready = false;
	lcd->cfg = *cfg;
	lcd->backlight = true;
	lcd->col = 0;
	lcd->row = 0;
	lcd->display_ctl = DISPLAY_ON;

	/* DDRAM start of each row (20x4: 0x00, 0x40, 0x14, 0x54) */
	lcd->row_addr[0] = 0x00;
	lcd->row_addr[1] = 0x40;
	lcd->row_addr[2] = cfg->cols;
	lcd->row_addr[3] = 0x40 + cfg->cols;

	/* A backpack answers a one-byte write: this also lights the backlight */
	ret = map_err(driver_i2c_write(LCD_BUS, cfg->addr, (const uint8_t[]){LCD_PIN_BL}, 1));
	if (ret < 0) {
		goto out;
	}

	/* HD44780 reset sequence for 4-bit mode (works from any state) */
	k_msleep(50);
	write_nibble(lcd, 0x30, 0);
	k_msleep(5);
	write_nibble(lcd, 0x30, 0);
	k_msleep(1);
	write_nibble(lcd, 0x30, 0);
	k_msleep(1);
	ret = write_nibble(lcd, 0x20, 0);
	if (ret < 0) {
		goto out;
	}
	k_msleep(1);

	/* 2-line mode drives all rows of 2 and 4 row displays and the split 16x1 */
	func = CMD_FUNCTION;
	if (cfg->rows > 1 || cfg->split_16x1) {
		func |= FUNC_2LINE;
	} else if (cfg->font_5x10) {
		func |= FUNC_5X10;
	}
	ret = command(lcd, func);
	if (ret == 0) {
		ret = command(lcd, CMD_DISPLAY);
	}
	if (ret == 0) {
		ret = command(lcd, CMD_CLEAR);
	}
	if (ret == 0) {
		ret = command(lcd, CMD_ENTRY_INC);
	}
	if (ret == 0) {
		ret = command(lcd, CMD_DISPLAY | lcd->display_ctl);
	}
	lcd->ready = (ret == 0);
out:
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_clear(struct driver_lcd *lcd)
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	ret = command(lcd, CMD_CLEAR);
	if (ret == 0) {
		lcd->col = 0;
		lcd->row = 0;
	}
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_home(struct driver_lcd *lcd)
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	ret = command(lcd, CMD_HOME);
	if (ret == 0) {
		lcd->col = 0;
		lcd->row = 0;
	}
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_set_cursor(struct driver_lcd *lcd, uint8_t col, uint8_t row)
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	if (col >= lcd->cfg.cols || row >= lcd->cfg.rows) {
		return -EINVAL;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	ret = move_cursor(lcd, col, row);
	k_mutex_unlock(&lcd->lock);
	return ret;
}

/* Lock must be held. Position is tracked so text never runs into the hidden DDRAM area. */
static int put_locked(struct driver_lcd *lcd, char c)
{
	int ret = 0;

	if (c == '\n') {
		if (lcd->row + 1 < lcd->cfg.rows) {
			ret = move_cursor(lcd, 0, lcd->row + 1);
		} else {
			lcd->col = lcd->cfg.cols; /* past the last row: drop the rest */
		}
		return ret;
	}
	if (lcd->col >= lcd->cfg.cols) {
		return 0; /* cut at the end of the row */
	}
	if (lcd->cfg.rows == 1 && lcd->cfg.split_16x1 && lcd->col == 8) {
		ret = command(lcd, CMD_SET_DDRAM | 0x40); /* jump to the second half */
		if (ret < 0) {
			return ret;
		}
	}
	ret = data(lcd, (uint8_t)c);
	if (ret == 0) {
		lcd->col++;
	}
	return ret;
}

int driver_lcd_putc(struct driver_lcd *lcd, char c)
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	ret = put_locked(lcd, c);
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_print(struct driver_lcd *lcd, const char *str)
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	if (str == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	for (; *str != '\0' && ret == 0; str++) {
		ret = put_locked(lcd, *str);
	}
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_print_at(struct driver_lcd *lcd, uint8_t col, uint8_t row, const char *str)
{
	int ret = driver_lcd_set_cursor(lcd, col, row);

	return ret < 0 ? ret : driver_lcd_print(lcd, str);
}

int driver_lcd_printf(struct driver_lcd *lcd, const char *fmt, ...)
{
	char buf[MAX_CHARS + 1];
	va_list ap;
	int n;

	if (fmt == NULL) {
		return -EINVAL;
	}
	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0) {
		return -EINVAL;
	}
	return driver_lcd_print(lcd, buf);
}

int driver_lcd_clear_row(struct driver_lcd *lcd, uint8_t row)
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	if (row >= lcd->cfg.rows) {
		return -EINVAL;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	ret = move_cursor(lcd, 0, row);
	for (uint8_t i = 0; i < lcd->cfg.cols && ret == 0; i++) {
		ret = put_locked(lcd, ' ');
	}
	if (ret == 0) {
		ret = move_cursor(lcd, 0, row);
	}
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_backlight(struct driver_lcd *lcd, bool on)
{
	int ret = check_ready(lcd);
	uint8_t v;

	if (ret < 0) {
		return ret;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	lcd->backlight = on;
	v = on ? LCD_PIN_BL : 0;
	ret = map_err(driver_i2c_write(LCD_BUS, lcd->cfg.addr, &v, 1));
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_cursor(struct driver_lcd *lcd, bool show, bool blink)
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	lcd->display_ctl = DISPLAY_ON | (show ? CURSOR_ON : 0) | (blink ? BLINK_ON : 0);
	ret = command(lcd, CMD_DISPLAY | lcd->display_ctl);
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_create_char(struct driver_lcd *lcd, uint8_t slot, const uint8_t bitmap[8])
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	if (slot > 7 || bitmap == NULL) {
		return -EINVAL;
	}
	k_mutex_lock(&lcd->lock, K_FOREVER);
	ret = command(lcd, CMD_SET_CGRAM | (slot << 3));
	for (int i = 0; i < 8 && ret == 0; i++) {
		ret = data(lcd, bitmap[i] & 0x1F);
	}
	if (ret == 0) {
		/* Writing to CGRAM moved the address counter: go back to the display */
		ret = move_cursor(lcd, MIN(lcd->col, lcd->cfg.cols - 1), lcd->row);
	}
	k_mutex_unlock(&lcd->lock);
	return ret;
}

int driver_lcd_get_size(const struct driver_lcd *lcd, uint8_t *cols, uint8_t *rows)
{
	int ret = check_ready(lcd);

	if (ret < 0) {
		return ret;
	}
	if (cols == NULL || rows == NULL) {
		return -EINVAL;
	}
	*cols = lcd->cfg.cols;
	*rows = lcd->cfg.rows;
	return 0;
}

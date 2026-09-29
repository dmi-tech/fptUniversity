/* lcd_pcf8574.c — HD44780 20x4 LCD in 4-bit mode through a PCF8574 backpack.
 *
 * Common backpack wiring: P0=RS, P1=RW, P2=EN, P3=backlight, P4..P7=D4..D7.
 * RW is held low (write only), so the busy flag is never read and every
 * command relies on the fixed HD44780 execution times instead.
 */
#include "lcd_pcf8574.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define PIN_RS BIT(0)
#define PIN_EN BIT(2)
#define PIN_BL BIT(3)

#define CMD_CLEAR        0x01
#define CMD_ENTRY_INC    0x06
#define CMD_DISPLAY_OFF  0x08
#define CMD_DISPLAY_ON   0x0C
#define CMD_4BIT_2LINE   0x28   /* "2-line" mode also drives rows 3/4 of a 20x4 */
#define CMD_SET_DDRAM    0x80

/* 20x4 modules: rows 3/4 continue rows 1/2 at +20 characters */
static const uint8_t row_offset[LCD_ROWS] = { 0x00, 0x40, 0x14, 0x54 };

static const struct device *const s_bus = DEVICE_DT_GET(DT_NODELABEL(i2c1));
static uint8_t s_addr;
static bool    s_ready;
static K_MUTEX_DEFINE(s_lock);

bool    lcd_is_ready(void) { return s_ready; }
uint8_t lcd_addr(void)     { return s_addr; }

/* One I2C byte takes ~90 us at 100 kHz, which already exceeds the enable
 * pulse width and the 37 us command time. RS is presented one byte before
 * EN rises to honour the address setup time. */
static int write_nibble(uint8_t nibble, uint8_t flags)
{
    uint8_t v = (nibble & 0xF0) | flags | PIN_BL;
    uint8_t seq[] = { v, v | PIN_EN, v };

    return i2c_write(s_bus, seq, sizeof(seq), s_addr);
}

static int write_byte(uint8_t value, uint8_t flags)
{
    int ret = write_nibble(value & 0xF0, flags);

    if (ret == 0) ret = write_nibble((uint8_t)(value << 4), flags);
    return ret;
}

static int command(uint8_t cmd)
{
    int ret = write_byte(cmd, 0);

    if (ret == 0 && cmd == CMD_CLEAR) k_msleep(2);   /* 1.52 ms */
    return ret;
}

/* A PCF8574 acknowledges a 1-byte write; 0x08 only turns the backlight on */
static bool probe(uint8_t addr)
{
    uint8_t v = PIN_BL;
    return i2c_write(s_bus, &v, 1, addr) == 0;
}

static uint8_t find_backpack(void)
{
    static const uint8_t preferred[] = { 0x27, 0x3F };

    ARRAY_FOR_EACH(preferred, i) {
        if (probe(preferred[i])) return preferred[i];
    }
    for (uint8_t a = 0x20; a <= 0x3F; a++) {
        if ((a <= 0x27 || a >= 0x38) && probe(a)) return a;
    }
    return 0;
}

static int controller_init(void)
{
    /* Datasheet 4-bit reset sequence (the controller may be in 8-bit mode
     * or half-way through a nibble after an MCU-only reset). */
    k_msleep(50);
    int ret = write_nibble(0x30, 0);
    if (ret) return ret;
    k_msleep(5);
    write_nibble(0x30, 0);
    k_msleep(1);
    write_nibble(0x30, 0);
    k_msleep(1);
    ret = write_nibble(0x20, 0);
    if (ret) return ret;
    k_msleep(1);

    static const uint8_t setup[] = { CMD_4BIT_2LINE, CMD_DISPLAY_OFF, CMD_CLEAR,
                                     CMD_ENTRY_INC, CMD_DISPLAY_ON };
    ARRAY_FOR_EACH(setup, i) {
        ret = command(setup[i]);
        if (ret) return ret;
    }
    return 0;
}

int lcd_init(void)
{
    int ret;

    if (!device_is_ready(s_bus)) {
        printk("[LCD] I2C1 not ready\n");
        return -ENODEV;
    }

    k_mutex_lock(&s_lock, K_FOREVER);
    s_ready = false;
    s_addr = find_backpack();
    if (s_addr == 0) {
        ret = -ENXIO;
    } else {
        ret = controller_init();
        s_ready = (ret == 0);
    }
    k_mutex_unlock(&s_lock);

    if (s_ready) printk("[LCD] PCF8574 20x4 at 0x%02X OK\n", s_addr);
    else if (s_addr) printk("[LCD] init at 0x%02X failed (%d)\n", s_addr, ret);
    else printk("[LCD] no PCF8574 on I2C1 (PB6/PB9)\n");
    return ret;
}

void lcd_clear(void)
{
    if (!s_ready) return;
    k_mutex_lock(&s_lock, K_FOREVER);
    if (command(CMD_CLEAR)) s_ready = false;
    k_mutex_unlock(&s_lock);
}

int lcd_write_line(uint8_t row, const char *text)
{
    if (row >= LCD_ROWS) return -EINVAL;
    if (!s_ready) return -ENODEV;

    k_mutex_lock(&s_lock, K_FOREVER);
    int ret = command(CMD_SET_DDRAM | row_offset[row]);
    bool end = false;

    for (int col = 0; col < LCD_COLS && ret == 0; col++) {
        if (text[col] == '\0') end = true;
        ret = write_byte(end ? ' ' : (uint8_t)text[col], PIN_RS);
    }
    if (ret) s_ready = false;
    k_mutex_unlock(&s_lock);
    return ret;
}

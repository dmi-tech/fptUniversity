#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_lcd.h"
#include "driver_lm393.h"

/* Cấu hình LCD: chọn loại và địa chỉ I2C theo module của bạn */
#define LCD_TYPE 1602   /* 1602 (16x2) hoặc 2004 (20x4) */
#define LCD_ADDR 0x27   /* PCF8574: 0x27, PCF8574A: 0x3F */

#define SENSOR 0        /* phần tử 0 của lm393-gpios, kênh "lm393_0" */

#if LCD_TYPE == 2004
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_20X4(LCD_ADDR);
#else
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_16X2(LCD_ADDR);
#endif

static struct driver_lcd lcd;

int main(void)
{
	printk("Lab 6: MQ gas -> LCD %d\n", LCD_TYPE);
	if (driver_lcd_init(&lcd, &lcd_cfg)) {
		printk("LCD initialization error\n");
		return 0;
	}

	if (driver_lm393_init(SENSOR, NULL)) {   /* NULL: chỉ đọc DO bằng driver_lm393_get */
		printk("MQ initialization error\n");
		driver_lcd_print_at(&lcd, 0, 0, "MQ init error");
		return 0;
	}

	while (1) {
		uint8_t gas;

		/* AO của module MQ tăng khi nồng độ khí tăng */
		if (driver_lm393_read_percent(SENSOR, &gas)) {
			printk("MQ read error\n");
			driver_lcd_clear(&lcd);
			driver_lcd_print_at(&lcd, 0, 0, "MQ read error");
			k_msleep(1000);
			continue;
		}
		bool alarm = driver_lm393_get(SENSOR) == 1;

		printk("MQ: gas %u %%, alarm %s\n", gas, alarm ? "YES" : "NO");

		driver_lcd_clear(&lcd);
		driver_lcd_set_cursor(&lcd, 0, 0);
		driver_lcd_printf(&lcd, "Gas: %u %%", gas);
		driver_lcd_set_cursor(&lcd, 0, 1);
		driver_lcd_printf(&lcd, "Alarm: %s", alarm ? "YES" : "NO");
		k_msleep(1000);
	}
	return 0;
}

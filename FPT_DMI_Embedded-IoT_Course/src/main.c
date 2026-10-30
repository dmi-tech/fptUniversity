#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_lcd.h"
#include "driver_dht11.h"

/* Cấu hình LCD: chọn loại và địa chỉ I2C theo module của bạn */
#define LCD_TYPE 1602   /* 1602 (16x2) hoặc 2004 (20x4) */
#define LCD_ADDR 0x27   /* PCF8574: 0x27, PCF8574A: 0x3F */

#define DHT 0           /* alias dht0 */

#if LCD_TYPE == 2004
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_20X4(LCD_ADDR);
#else
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_16X2(LCD_ADDR);
#endif

static struct driver_lcd lcd;

int main(void)
{
	printk("Lab 4: DHT11 -> LCD %d\n", LCD_TYPE);
	if (driver_lcd_init(&lcd, &lcd_cfg)) {
		printk("LCD initialization error\n");
		return 0;
	}

	if (driver_dht11_init(DHT)) {
		printk("DHT11 initialization error\n");
		driver_lcd_print_at(&lcd, 0, 0, "DHT11 init error");
		return 0;
	}
	driver_lcd_print_at(&lcd, 0, 0, "Reading...");

	while (1) {
		int temp, hum;

		k_msleep(2000);   /* DHT11 chỉ đọc được mỗi 2 giây */
		if (driver_dht11_read(DHT, &temp, &hum)) {
			printk("DHT11 read error\n");
			driver_lcd_clear(&lcd);
			driver_lcd_print_at(&lcd, 0, 0, "DHT11 read error");
			continue;
		}
		printk("DHT11: %d C, %d %%\n", temp, hum);

		driver_lcd_clear(&lcd);
		driver_lcd_set_cursor(&lcd, 0, 0);
		driver_lcd_printf(&lcd, "Temp: %d C", temp);
		driver_lcd_set_cursor(&lcd, 0, 1);
		driver_lcd_printf(&lcd, "Humi: %d %%", hum);
	}
	return 0;
}

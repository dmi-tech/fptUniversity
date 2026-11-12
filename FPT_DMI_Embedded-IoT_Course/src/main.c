#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_adc.h"
#include "driver_gpio.h"
#include "driver_lcd.h"

#define LCD_ADDR      0x27
#define BUZZER        0
#define ADC_SAMPLES   16
#define TEMP_ON_DC    350
#define TEMP_OFF_DC   330
#define LOOP_MS       250
#define LCD_EVERY     4

static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_16X2(LCD_ADDR);
static struct driver_lcd lcd;

static int lm35_read_dc(int32_t *temp_dc)
{
	int32_t mv;
	int ret = driver_adc_read_avg_mv(ADC_SAMPLES, &mv);

	if (ret < 0) {
		return ret;
	}
	*temp_dc = mv;
	return 0;
}

int main(void)
{
	bool alarm = false;
	bool beep = false;
	unsigned int tick = 0;

	printk("Group6_SE2024: LM35 + Buzzer + LCD\n");

	if (driver_lcd_init(&lcd, &lcd_cfg)) {
		printk("LCD init error\n");
		return 0;
	}
	if (driver_adc_init() < 0) {
		printk("ADC init error\n");
		driver_lcd_print_at(&lcd, 0, 0, "ADC init error");
		return 0;
	}
	if (driver_gpio_out_init(BUZZER) < 0) {
		printk("Buzzer init error\n");
		driver_lcd_print_at(&lcd, 0, 0, "Buzzer init err");
		return 0;
	}

	driver_lcd_clear(&lcd);
	driver_lcd_print_at(&lcd, 0, 0, "Group6 SE2024");
	driver_lcd_print_at(&lcd, 0, 1, "LM35 starting..");
	k_msleep(1000);
	driver_lcd_clear(&lcd);

	while (1) {
		int32_t t;

		if (lm35_read_dc(&t) < 0) {
			driver_gpio_set(BUZZER, false);
			driver_lcd_print_at(&lcd, 0, 0, "LM35 read error ");
			printk("LM35 read error\n");
			k_msleep(LOOP_MS);
			continue;
		}

		if (!alarm && t >= TEMP_ON_DC) {
			alarm = true;
		} else if (alarm && t < TEMP_OFF_DC) {
			alarm = false;
		}

		beep = alarm ? !beep : false;
		driver_gpio_set(BUZZER, beep);

		if (tick % LCD_EVERY == 0) {
			driver_lcd_set_cursor(&lcd, 0, 0);
			driver_lcd_printf(&lcd, "Temp: %3d.%d C   ", t / 10, t % 10);
			driver_lcd_set_cursor(&lcd, 0, 1);
			driver_lcd_printf(&lcd, alarm ? "ALARM! >=%d.%dC  " : "Normal <%d.%dC   ",
					  TEMP_ON_DC / 10, TEMP_ON_DC % 10);
			printk("LM35: %d.%d C alarm=%d\n", t / 10, t % 10, alarm);
		}
		tick++;
		k_msleep(LOOP_MS);
	}
	return 0;
}

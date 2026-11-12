#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_gpio.h"
#include "driver_lcd.h"
#include "driver_lm393.h"

#define LCD_ADDR      0x27
#define MQ3           0
#define BUZZER        0
#define ADC_SAMPLES   16
#define WARMUP_S      20
#define LEVEL_ON_PCT  40
#define LEVEL_OFF_PCT 35
#define LOOP_MS       250
#define LCD_EVERY     4

static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_16X2(LCD_ADDR);
static struct driver_lcd lcd;

static void fatal(const char *msg)
{
	printk("%s\n", msg);
	driver_lcd_clear(&lcd);
	driver_lcd_print_at(&lcd, 0, 0, msg);
}

static void warm_up(void)
{
	for (int s = WARMUP_S; s > 0; s--) {
		driver_lcd_print_at(&lcd, 0, 0, "MQ-3 warming up ");
		driver_lcd_set_cursor(&lcd, 0, 1);
		driver_lcd_printf(&lcd, "Please wait %2ds ", s);
		printk("MQ-3 warming up: %d s\n", s);
		k_msleep(1000);
	}
	driver_lcd_clear(&lcd);
}

int main(void)
{
	bool alarm = false;
	bool beep = false;
	unsigned int tick = 0;

	printk("Group2_SE2023: MQ-3 + Buzzer + LCD\n");

	if (driver_lcd_init(&lcd, &lcd_cfg)) {
		printk("LCD init error\n");
		return 0;
	}
	if (driver_lm393_init(MQ3, NULL)) {
		fatal("MQ-3 init error");
		return 0;
	}
	if (driver_gpio_out_init(BUZZER) < 0) {
		fatal("Buzzer init err");
		return 0;
	}

	driver_lcd_clear(&lcd);
	driver_lcd_print_at(&lcd, 0, 0, "Group2 SE2023");
	driver_lcd_print_at(&lcd, 0, 1, "Alcohol monitor");
	k_msleep(1500);
	warm_up();

	while (1) {
		int32_t mv;
		uint8_t pct;
		int d_out;

		if (driver_lm393_read_avg_mv(MQ3, ADC_SAMPLES, &mv) ||
		    driver_lm393_read_percent(MQ3, &pct)) {
			driver_gpio_set(BUZZER, false);
			driver_lcd_print_at(&lcd, 0, 0, "MQ-3 read error ");
			printk("MQ-3 read error\n");
			k_msleep(LOOP_MS);
			continue;
		}
		d_out = driver_lm393_get(MQ3);

		if (!alarm && (pct >= LEVEL_ON_PCT || d_out == 1)) {
			alarm = true;
		} else if (alarm && pct < LEVEL_OFF_PCT && d_out != 1) {
			alarm = false;
		}

		beep = alarm ? !beep : false;
		driver_gpio_set(BUZZER, beep);

		if (tick % LCD_EVERY == 0) {
			driver_lcd_set_cursor(&lcd, 0, 0);
			driver_lcd_printf(&lcd, "MQ3:%3u%% %4dmV ", pct, mv);
			driver_lcd_print_at(&lcd, 0, 1, alarm ? "ALCOHOL DETECTED" : "Status: NORMAL  ");
			printk("MQ-3: %u %% %d mV DO=%d alarm=%d\n", pct, mv, d_out, alarm);
		}
		tick++;
		k_msleep(LOOP_MS);
	}
	return 0;
}

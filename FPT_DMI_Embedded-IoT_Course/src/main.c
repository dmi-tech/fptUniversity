#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_dht11.h"
#include "driver_gpio.h"
#include "driver_lcd.h"
#include "driver_lm393.h"

#define LCD_ADDR      0x27
#define DHT           0
#define MQ2           0
#define BUZZER        0
#define ADC_SAMPLES   16
#define WARMUP_S      20
#define GAS_ON_PCT    40
#define GAS_OFF_PCT   35
#define TEMP_ON_C     40
#define TEMP_OFF_C    38
#define LOOP_MS       250
#define LCD_EVERY     4
#define DHT_EVERY     8

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
		driver_lcd_print_at(&lcd, 0, 0, "MQ-2 warming up ");
		driver_lcd_set_cursor(&lcd, 0, 1);
		driver_lcd_printf(&lcd, "Please wait %2ds ", s);
		printk("MQ-2 warming up: %d s\n", s);
		k_msleep(1000);
	}
	driver_lcd_clear(&lcd);
}

static const char *status_text(bool gas, bool hot)
{
	if (gas && hot) {
		return "GAS+HOT";
	}
	if (gas) {
		return "GAS!";
	}
	if (hot) {
		return "HOT!";
	}
	return "NORMAL";
}

int main(void)
{
	bool gas_alarm = false;
	bool hot_alarm = false;
	bool dht_ok = false;
	int temp = 0;
	int hum = 0;
	uint8_t gas = 0;
	unsigned int tick = 0;

	printk("Group3_SE2023: DHT11 + MQ-2 + Buzzer + LCD\n");

	if (driver_lcd_init(&lcd, &lcd_cfg)) {
		printk("LCD init error\n");
		return 0;
	}
	if (driver_dht11_init(DHT)) {
		fatal("DHT11 init error");
		return 0;
	}
	if (driver_lm393_init(MQ2, NULL)) {
		fatal("MQ-2 init error");
		return 0;
	}
	if (driver_gpio_out_init(BUZZER) < 0) {
		fatal("Buzzer init err");
		return 0;
	}

	driver_lcd_clear(&lcd);
	driver_lcd_print_at(&lcd, 0, 0, "Group3 SE2023");
	driver_lcd_print_at(&lcd, 0, 1, "DHT11 + MQ-2");
	k_msleep(1500);
	warm_up();

	while (1) {
		int32_t mv;
		int d_out;
		bool buzz;

		if (tick % DHT_EVERY == 0) {
			int t, h;

			if (driver_dht11_read(DHT, &t, &h) == 0) {
				temp = t;
				hum = h;
				dht_ok = true;
			} else {
				dht_ok = false;
				printk("DHT11 read error\n");
			}
		}

		if (driver_lm393_read_avg_mv(MQ2, ADC_SAMPLES, &mv) == 0 &&
		    driver_lm393_read_percent(MQ2, &gas) == 0) {
			d_out = driver_lm393_get(MQ2);
		} else {
			mv = -1;
			d_out = -1;
			printk("MQ-2 read error\n");
		}

		if (!gas_alarm && (gas >= GAS_ON_PCT || d_out == 1)) {
			gas_alarm = true;
		} else if (gas_alarm && gas < GAS_OFF_PCT && d_out != 1) {
			gas_alarm = false;
		}

		if (dht_ok) {
			if (!hot_alarm && temp >= TEMP_ON_C) {
				hot_alarm = true;
			} else if (hot_alarm && temp < TEMP_OFF_C) {
				hot_alarm = false;
			}
		}

		if (gas_alarm) {
			buzz = (tick % 2) == 0;
		} else if (hot_alarm) {
			buzz = (tick % 4) == 0;
		} else {
			buzz = false;
		}
		driver_gpio_set(BUZZER, buzz);

		if (tick % LCD_EVERY == 0) {
			driver_lcd_set_cursor(&lcd, 0, 0);
			if (dht_ok) {
				driver_lcd_printf(&lcd, "T:%2dC  H:%2d%%    ", temp, hum);
			} else {
				driver_lcd_printf(&lcd, "T:--C  H:--%%    ");
			}
			driver_lcd_set_cursor(&lcd, 0, 1);
			driver_lcd_printf(&lcd, "Gas:%3u%% %-7s", gas, status_text(gas_alarm, hot_alarm));
			printk("T=%d C H=%d %% dht_ok=%d | gas=%u %% %d mV DO=%d | gas_alarm=%d hot_alarm=%d\n",
			       temp, hum, dht_ok, gas, mv, d_out, gas_alarm, hot_alarm);
		}
		tick++;
		k_msleep(LOOP_MS);
	}
	return 0;
}

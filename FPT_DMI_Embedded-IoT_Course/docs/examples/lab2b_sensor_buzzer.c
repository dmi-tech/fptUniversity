/* Lab 2B – Sensor -> Buzzer: moving average + threshold with hysteresis.
 * The buzzer is a plain on/off output (alias out2). The sensor here is the on-chip
 * temperature through driver_adc; replace read_sensor() with your own input.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_adc.h"
#include "driver_gpio.h"

#define BUZZER      2
#define WINDOW      8
#define TH_ON_MC    35000 /* buzzer on above 35.0 C   */
#define TH_OFF_MC   33000 /* buzzer off below 33.0 C  (hysteresis = 2.0 C) */

static int read_sensor(int32_t *milli_c)
{
	return driver_adc_read_die_temp(milli_c);
}

int main(void)
{
	int32_t samples[WINDOW] = {0};
	int64_t sum = 0;
	unsigned int n = 0;
	bool alarm = false;

	if (driver_adc_init() < 0 || driver_gpio_out_init(BUZZER) < 0) {
		printk("init failed\n");
		return 0;
	}

	while (1) {
		int32_t v;

		if (read_sensor(&v) == 0) {
			sum += v - samples[n % WINDOW];
			samples[n % WINDOW] = v;
			n++;
			if (n >= WINDOW) {
				int32_t avg = (int32_t)(sum / WINDOW);

				if (!alarm && avg > TH_ON_MC) {
					alarm = true;
				} else if (alarm && avg < TH_OFF_MC) {
					alarm = false;
				}
				driver_gpio_set(BUZZER, alarm);
				printk("avg=%d mC alarm=%d\n", avg, alarm);
			}
		}
		k_msleep(250);
	}
	return 0;
}

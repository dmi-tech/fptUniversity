#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_gpio.h"
#include "driver_dht11.h"

#define RELAY_1    0    /* alias out0 */
#define DHT        0    /* alias dht0 */
#define TEMP_ON_C  30   /* relay bật khi nhiệt độ >= 30 C */
#define TEMP_OFF_C 29   /* relay tắt khi nhiệt độ <= 29 C */

int main(void)
{
	bool relay_on = false;

	printk("Lab 2: DHT11 -> Relay\n");
	if (driver_gpio_out_init(RELAY_1)) {
		printk("Relay initialization error\n");
		return 0;
	}

	if (driver_dht11_init(DHT)) {
		printk("DHT11 initialization error\n");
		return 0;
	}

	while (1) {
		int temp, hum;

		k_msleep(2000);   /* DHT11 chỉ đọc được mỗi 2 giây */
		if (driver_dht11_read(DHT, &temp, &hum)) {
			printk("DHT11 read error\n");
			continue;
		}

		if (!relay_on && temp >= TEMP_ON_C) {
			relay_on = true;
		} else if (relay_on && temp <= TEMP_OFF_C) {
			relay_on = false;
		}
		driver_gpio_set(RELAY_1, relay_on);
		printk("DHT11: %d C, %d %% -> Relay 1 %s\n", temp, hum, relay_on ? "On" : "Off");
	}
	return 0;
}

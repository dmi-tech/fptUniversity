#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_gpio.h"
#include "driver_lm393.h"

#define RELAY_1 0   /* alias out0 */
#define LDR     0   /* phần tử 0 của lm393-gpios */

static void on_light(uint8_t id, bool dark)
{
	driver_gpio_set(RELAY_1, dark);
	printk("LDR: %s -> Relay 1 %s\n", dark ? "Dark" : "Bright", dark ? "On" : "Off");
}

int main(void)
{
	printk("Lab 3: LDR -> Relay\n");
	if (driver_gpio_out_init(RELAY_1)) {
		printk("Relay initialization error\n");
		return 0;
	}

	if (driver_lm393_init(LDR, on_light)) {
		printk("LDR initialization error\n");
		return 0;
	}
	driver_lm393_set_hold(LDR, 500);   /* chống rung khi ánh sáng sát ngưỡng */

	while (1) {
		k_msleep(1000);
	}
	return 0;
}

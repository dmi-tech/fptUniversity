/* Lab 2A – Button -> Relay with debounce. The relay is an output (alias out1), the button an
 * input (alias in0). driver_gpio already debounces (50 ms) and calls back from a workqueue.
 * The relay starts OFF (driver_gpio_out_init -> inactive): the safe state after reset.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_gpio.h"

#define RELAY  1
#define BUTTON 0

static unsigned int presses;

static void on_button(uint8_t id, bool active)
{
	if (active) { /* pressed: toggle on the press only, not on release */
		presses++;
		driver_gpio_toggle(RELAY);
		printk("press #%u\n", presses);
	}
}

int main(void)
{
	int ret = driver_gpio_out_init(RELAY);

	if (ret == 0) {
		ret = driver_gpio_in_init(BUTTON, on_button);
	}
	printk("Lab 2A ready (%d)\n", ret);
	return 0;
}

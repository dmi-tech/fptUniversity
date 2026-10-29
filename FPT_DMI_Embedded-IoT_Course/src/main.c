#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_gpio.h"

#define RELAY_1 0
#define BUTTON 0

static void on_button(uint8_t id, bool pressed){
	static bool relay_on;
	
	if(!pressed) return;
	relay_on = !relay_on;
	driver_gpio_set(RELAY_1, relay_on);
	printk("Button is pressed -> Relay 1 %s\n", relay_on ? "On" : "Off");
}
int main(void)
{
	printk("Lab 1: Button -> Relay\n");
	if (driver_gpio_out_init(RELAY_1)){
		printk("Relay initialization error\n");
		return 0;
	}

	if (driver_gpio_in_init(BUTTON, on_button)){
		printk("Button initialization error\n");
		return 0;
	}

	while (1) {
		k_msleep(1000);
	}
	return 0;
}

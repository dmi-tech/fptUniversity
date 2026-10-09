/* Servo held at 90 degrees (1450 us pulse every 20 ms) for wiring checks with a multimeter. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_servo.h"

int main(void)
{
	const struct driver_servo_cfg cfg = {
		.min_pulse_us = 500, .max_pulse_us = 2400, .max_angle = 180,
	};
	int ret = driver_servo_init(0, &cfg);

	if (ret == 0) {
		ret = driver_servo_set_angle(0, 90);
	}
	printk("servo fixed at 90 deg, ret=%d\n", ret);
	while (1) {
		k_msleep(1000);
	}
	return 0;
}

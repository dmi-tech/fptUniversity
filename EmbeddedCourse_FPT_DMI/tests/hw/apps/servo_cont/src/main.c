/* Servo: continuous angle adjustment. Alternates a 1-degree step sweep (set_angle every 20 ms)
 * and a driver_servo_sweep() ramp, forever. Prints the angle about twice per second.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_servo.h"

int main(void)
{
	const struct driver_servo_cfg cfg = {
		.min_pulse_us = 500, .max_pulse_us = 2400, .max_angle = 180,
	};
	int ret = driver_servo_init(0, &cfg);

	printk("servo init=%d\n", ret);
	for (int round = 0;; round++) {
		printk("round %d: 1-degree steps 0 -> 180 -> 0 (20 ms/step)\n", round);
		for (int a = 0; a <= 180; a++) {
			driver_servo_set_angle(0, a);
			if (a % 30 == 0) {
				printk("  angle %d\n", a);
			}
			k_msleep(20);
		}
		for (int a = 180; a >= 0; a--) {
			driver_servo_set_angle(0, a);
			if (a % 30 == 0) {
				printk("  angle %d\n", a);
			}
			k_msleep(20);
		}
		printk("round %d: ramp 0 -> 180 -> 0 in 2 s each\n", round);
		ret = driver_servo_sweep(0, 0, 180, 2000);
		ret |= driver_servo_sweep(0, 180, 0, 2000);
		printk("  sweep ret=%d\n", ret);
	}
	return 0;
}

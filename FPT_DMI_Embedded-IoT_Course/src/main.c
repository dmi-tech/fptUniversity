#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_gpio.h"
#include "driver_hcsr04.h"
#include "driver_servo.h"

#define SERVO         0
#define LED           0
#define LID_CLOSED    0
#define LID_OPEN      90
#define OPEN_MS       500
#define CLOSE_MS      1000
#define NEAR_MM       300
#define FAR_MM        450
#define NEAR_COUNT    2
#define CLOSE_DELAY_MS 3000
#define LOOP_MS       100
#define SAMPLES       3

static bool lid_open;

static void open_lid(void)
{
	driver_gpio_set(LED, true);
	driver_servo_sweep(SERVO, LID_CLOSED, LID_OPEN, OPEN_MS);
	lid_open = true;
	printk("Lid OPEN\n");
}

static void close_lid(void)
{
	driver_servo_sweep(SERVO, LID_OPEN, LID_CLOSED, CLOSE_MS);
	driver_gpio_set(LED, false);
	lid_open = false;
	printk("Lid CLOSED\n");
}

static int read_distance(uint32_t *mm)
{
	int ret = driver_hcsr04_read_avg_mm(SAMPLES, mm);

	if (ret == -ERANGE || ret == -EIO) {
		*mm = DRIVER_HCSR04_MAX_MM;
		return 0;
	}
	return ret;
}

int main(void)
{
	unsigned int near_cnt = 0;
	int64_t last_seen = 0;

	printk("Group6_SE2023: Smart trash bin (HC-SR04 + Servo + LED)\n");

	if (driver_hcsr04_init()) {
		printk("HC-SR04 init error\n");
		return 0;
	}
	if (driver_servo_init(SERVO, NULL)) {
		printk("Servo init error\n");
		return 0;
	}
	if (driver_gpio_out_init(LED) < 0) {
		printk("LED init error\n");
		return 0;
	}

	driver_servo_set_angle(SERVO, LID_CLOSED);
	lid_open = false;

	while (1) {
		uint32_t mm;
		int64_t now = k_uptime_get();

		if (read_distance(&mm)) {
			printk("HC-SR04 read error\n");
			k_msleep(LOOP_MS);
			continue;
		}

		if (!lid_open) {
			near_cnt = (mm <= NEAR_MM) ? near_cnt + 1 : 0;
			if (near_cnt >= NEAR_COUNT) {
				printk("Person detected at %u mm\n", mm);
				open_lid();
				last_seen = k_uptime_get();
				near_cnt = 0;
			}
		} else {
			if (mm <= FAR_MM) {
				last_seen = now;
			} else if (now - last_seen >= CLOSE_DELAY_MS) {
				printk("Person left, last distance %u mm\n", mm);
				close_lid();
			}
		}

		k_msleep(LOOP_MS);
	}
	return 0;
}

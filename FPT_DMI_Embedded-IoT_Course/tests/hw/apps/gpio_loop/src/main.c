/* A1 by loopback: no wires, no finger. The button pin PA10 and the LED pin PA8 are put in
 * input+output mode, so software drives the "button" and reads back the "LED".
 * Checks: 20 clean presses -> 20 NHAN + 20 NHA callbacks; 20 bouncy presses (chatter of a few
 * hundred microseconds before settling) -> still exactly 20 + 20; outputs read back correctly.
 */
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_gpio.h"

static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(in0), gpios);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(out0), gpios);

static volatile int n_press, n_release;

static void on_btn(uint8_t id, bool active)
{
	if (active) {
		n_press++;
	} else {
		n_release++;
	}
}

static void press(bool bounce)
{
	if (bounce) {
		for (int i = 0; i < 3; i++) { /* chatter: 3 x (active, inactive) within ~3 ms */
			gpio_pin_set_dt(&btn, 1);
			k_busy_wait(500);
			gpio_pin_set_dt(&btn, 0);
			k_busy_wait(500);
		}
	}
	gpio_pin_set_dt(&btn, 1);
	k_msleep(150);
	if (bounce) { /* bounce on release too */
		gpio_pin_set_dt(&btn, 0);
		k_busy_wait(700);
		gpio_pin_set_dt(&btn, 1);
		k_busy_wait(700);
	}
	gpio_pin_set_dt(&btn, 0);
	k_msleep(150);
}

int main(void)
{
	int ret = driver_gpio_out_init(0);

	ret |= driver_gpio_in_init(0, on_btn);
	printk("init=%d\n", ret);

	/* Loopback: the pins also read themselves. The driver's callback stays registered. */
	ret = gpio_pin_configure_dt(&btn, GPIO_INPUT | GPIO_OUTPUT_INACTIVE);
	ret |= gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_BOTH);
	ret |= gpio_pin_configure_dt(&led, GPIO_INPUT | GPIO_OUTPUT_INACTIVE);
	printk("loopback config=%d\n", ret);

	int ok = 1;

	for (int i = 0; i < 4; i++) { /* output read back */
		driver_gpio_set(0, i & 1);
		if (gpio_pin_get_dt(&led) != (i & 1)) {
			ok = 0;
		}
	}
	driver_gpio_toggle(0);
	driver_gpio_toggle(0);
	printk("output readback %s\n", ok ? "OK" : "WRONG");

	for (int i = 0; i < 20; i++) {
		press(false);
	}
	printk("clean: press=%d release=%d (want 20/20) get=%d\n", n_press, n_release,
	       driver_gpio_get(0));
	int clean_ok = n_press == 20 && n_release == 20;

	n_press = n_release = 0;
	for (int i = 0; i < 20; i++) {
		press(true);
	}
	printk("bounce: press=%d release=%d (want 20/20)\n", n_press, n_release);
	int bounce_ok = n_press == 20 && n_release == 20;

	printk("range: out id 9 -> %d (want -22), in id 9 -> %d (want -22)\n", driver_gpio_set(9, 1),
	       driver_gpio_get(9));
	printk("A1 %s\n", (ok && clean_ok && bounce_ok) ? "PASS" : "FAIL");
	while (1) {
		k_msleep(1000);
	}
}

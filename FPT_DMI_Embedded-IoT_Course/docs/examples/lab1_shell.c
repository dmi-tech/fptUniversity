/* Lab 1 – First Zephyr Application: boot log with version/build ID, LED + Zephyr shell.
 * Needs: driver/peripherals/gpio, alias out0 = &user_led, prj.conf of docs/labs.md (Lab 1).
 */
#include <stdlib.h>
#include <string.h>
#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>

#include "driver_gpio.h"

static int cmd_led(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	if (strcmp(argv[1], "on") == 0) {
		ret = driver_gpio_set(0, true);
	} else if (strcmp(argv[1], "off") == 0) {
		ret = driver_gpio_set(0, false);
	} else if (strcmp(argv[1], "toggle") == 0) {
		ret = driver_gpio_toggle(0);
	} else {
		shell_error(sh, "usage: led on|off|toggle");
		return -EINVAL;
	}
	if (ret < 0) {
		shell_error(sh, "led failed: %d", ret);
	}
	return ret;
}
SHELL_CMD_ARG_REGISTER(led, NULL, "led on|off|toggle", cmd_led, 2, 0);

int main(void)
{
	int ret = driver_gpio_out_init(0);

	printk("\n== FPT_DMI_Embedded-IoT_Course v%s (build %s %s) ==\n", APP_VERSION_STRING,
	       __DATE__, __TIME__);
	if (ret < 0) {
		printk("led init failed: %d\n", ret);
	}

	while (1) { /* heartbeat blink; the shell runs in its own thread */
		driver_gpio_toggle(0);
		k_msleep(500);
	}
	return 0;
}

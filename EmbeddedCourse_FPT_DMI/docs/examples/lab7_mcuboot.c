/* Lab 7 – MCUboot lifecycle: self-test, confirm, rollback.
 * Boot: print the version, run the self-test.
 *   pass -> boot_write_img_confirmed(): the image becomes permanent.
 *   fail -> do NOT confirm and reboot: MCUboot reverts to the previous image.
 * To build the broken V3, set SELFTEST_FORCE_FAIL to 1.
 */
#include <zephyr/app_version.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>

#define SELFTEST_FORCE_FAIL 0

static bool self_test(void)
{
	/* Put real checks here: sensors answer, network up, config valid... */
	return !SELFTEST_FORCE_FAIL;
}

int main(void)
{
	printk("\n== App version %s, image %s ==\n", APP_VERSION_STRING,
	       boot_is_img_confirmed() ? "confirmed" : "TEST (not confirmed)");

	if (boot_is_img_confirmed()) {
		printk("Already confirmed, nothing to do\n");
	} else if (self_test()) {
		int ret = boot_write_img_confirmed();

		printk("Self-test PASS, confirm: %d\n", ret);
	} else {
		printk("Self-test FAIL, rebooting without confirm -> rollback\n");
		k_msleep(200);
		sys_reboot(SYS_REBOOT_COLD);
	}

	while (1) {
		k_msleep(1000);
	}
	return 0;
}

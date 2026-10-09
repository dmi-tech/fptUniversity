#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
	printk("\n============================================\n");
	printk("   FPT_DMI_Embedded-IoT_Course - Zephyr RTOS\n");
	printk("   Target: %s\n", CONFIG_BOARD_TARGET);
	printk("   Hello, World !!!\n");
	printk("============================================\n\n");

	while (1) {
		k_msleep(1000);
	}
	return 0;
}

/* RS485 beacon + echo: sends "BEACON n" twice a second and echoes what it receives, so the PC side
 * can check TX and RX with no SWD (24 V on breaks SWD on this setup).
 */
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_rs485.h"

int main(void)
{
	uint8_t buf[64];
	char msg[32];
	unsigned int n = 0;
	int ret = driver_rs485_init(9600);

	printk("rs485 init=%d\n", ret);
	if (ret < 0) {
		return 0;
	}
	while (1) {
		int len = driver_rs485_receive_frame(buf, sizeof(buf), 20, K_MSEC(500));

		if (len > 0) {
			driver_rs485_print("ECHO: ");
			driver_rs485_send(buf, len);
		}
		snprintf(msg, sizeof(msg), "BEACON %u\r\n", n++);
		driver_rs485_print(msg);
	}
	return 0;
}

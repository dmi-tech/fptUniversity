/* UART debug: same as the README example, with RTT markers around every step. */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_uart.h"

int main(void)
{
	char line[64];
	int ret = driver_uart_init(DRIVER_UART6, 115200);
	unsigned int n_lines = 0;

	printk("uart init=%d\n", ret);
	if (ret < 0) {
		return 0;
	}
	driver_uart_print(DRIVER_UART6, "Xin chao! Go 'led on' hoac bat ky chu nao:\r\n");
	printk("greeting sent\n");

	while (1) {
		printk("A: waiting for a line (#%u)\n", n_lines);
		int n = driver_uart_read_line(DRIVER_UART6, line, sizeof(line), K_FOREVER);

		printk("B: read_line returned %d avail=%d\n", n, driver_uart_available(DRIVER_UART6));
		if (n <= 0) {
			continue;
		}
		n_lines++;
		printk("C: got '%s' (%d chars)\n", line, n);
		ret = driver_uart_printf(DRIVER_UART6, "Ban vua go: %s (%d ky tu)\r\n", line, n);
		printk("D: reply ret=%d\n", ret);
	}
	return 0;
}

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "driver_gpio.h"
#include "driver_rs485.h"
#include "driver_sht41.h"

#define RELAY_1  0      /* alias out0 */
#define BAUD     9600
#define GAP_MS   20     /* lệnh kết thúc khi đường truyền im lặng 20 ms */

static bool relay_on;

static void reply(const char *str)
{
	driver_rs485_print(str);
	driver_rs485_print("\r\n");
	printk("TX: %s\n", str);
}

static void send_status(void)
{
	char msg[64];
	float t, h;

	if (driver_sht41_read(&t, &h)) {
		snprintf(msg, sizeof(msg), "Relay 1: %s, SHT41 read error", relay_on ? "ON" : "OFF");
		reply(msg);
		return;
	}
	int temp = (int)(t * 10);   /* 28.5 C -> 285 */
	int hum = (int)(h * 10);

	snprintf(msg, sizeof(msg), "Relay 1: %s, Temp: %d.%d C, Humi: %d.%d %%",
		 relay_on ? "ON" : "OFF", temp / 10, temp % 10, hum / 10, hum % 10);
	reply(msg);
}

int main(void)
{
	char cmd[32];

	printk("Lab 8: PC -> RS485 -> Relay\n");
	if (driver_gpio_out_init(RELAY_1)) {
		printk("Relay initialization error\n");
		return 0;
	}

	if (driver_sht41_init()) {
		printk("SHT41 initialization error\n");
		return 0;
	}

	if (driver_rs485_init(BAUD)) {
		printk("RS485 initialization error\n");
		return 0;
	}
	reply("Board ready. Commands: ON, OFF, STATUS");

	while (1) {
		/* Phần mềm trên PC không gửi CR/LF: một lệnh là một cụm byte liền nhau */
		int n = driver_rs485_receive_frame((uint8_t *)cmd, sizeof(cmd) - 1, GAP_MS, K_FOREVER);

		if (n <= 0) {
			continue;
		}
		/* Bỏ CR, LF, dấu cách ở cuối (nếu PC có gửi kèm) */
		while (n > 0 && (cmd[n - 1] == '\r' || cmd[n - 1] == '\n' || cmd[n - 1] == ' ')) {
			n--;
		}
		cmd[n] = '\0';
		printk("RX: %s\n", cmd);

		if (strcasecmp(cmd, "ON") == 0) {
			relay_on = true;
			driver_gpio_set(RELAY_1, relay_on);
			reply("OK Relay 1 ON");
		} else if (strcasecmp(cmd, "OFF") == 0) {
			relay_on = false;
			driver_gpio_set(RELAY_1, relay_on);
			reply("OK Relay 1 OFF");
		} else if (strcasecmp(cmd, "STATUS") == 0) {
			send_status();
		} else if (n > 0) {
			char msg[64];

			snprintf(msg, sizeof(msg), "ERR unknown command: %s", cmd);
			reply(msg);
		}
	}
	return 0;
}

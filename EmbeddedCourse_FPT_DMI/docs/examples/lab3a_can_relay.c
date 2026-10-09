/* Lab 3A – CAN -> Relay with heartbeat and timeout (fail-safe).
 * Frames: 0x100 heartbeat (master->slave), 0x200 command (byte0 = relay 0/1),
 *         0x201 status (slave->master: byte0 = relay, byte1 = heartbeat alive).
 * Relay = alias out1, off after reset and whenever the heartbeat is lost.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_can.h"
#include "driver_gpio.h"

#define RELAY         1
#define ID_HEARTBEAT  0x100
#define ID_COMMAND    0x200
#define ID_STATUS     0x201
#define HB_TIMEOUT_MS 1000

static volatile bool relay_on;

static void on_heartbeat(bool alive)
{
	if (!alive) {
		relay_on = false;
		driver_gpio_set(RELAY, false); /* safe state */
	}
	printk("heartbeat %s\n", alive ? "OK" : "LOST -> relay OFF");
}

static void on_command(uint32_t id, const uint8_t *data, uint8_t len)
{
	/* Interrupt context: no printk here. Accept commands only while the master is alive. */
	if (len >= 1 && driver_can_heartbeat_alive() == 1) {
		relay_on = data[0] != 0;
		driver_gpio_set(RELAY, relay_on);
	}
}

int main(void)
{
	int ret = driver_gpio_out_init(RELAY);

	if (ret == 0) {
		ret = driver_can_init(500000, false);
	}
	if (ret == 0) {
		ret = driver_can_heartbeat_watch(ID_HEARTBEAT, HB_TIMEOUT_MS, on_heartbeat);
	}
	if (ret == 0) {
		ret = driver_can_add_rx(ID_COMMAND, 0x7FF, on_command);
	}
	printk("Lab 3A ready (%d)\n", ret);

	while (ret == 0) {
		uint8_t status[2] = {relay_on, driver_can_heartbeat_alive() == 1};

		driver_can_send(ID_STATUS, status, sizeof(status));
		k_msleep(500);
	}
	return 0;
}

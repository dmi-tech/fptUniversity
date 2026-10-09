/* C5/C6: CAN on the real bus (500 kbit/s). Needs 24 V on the isolated side.
 *  - sends a beacon 0x321 [counter u32, 0xA5, 0x5A] every 500 ms
 *  - echoes every frame 0x200..0x2FF back as id+0x100 (0x300..0x3FF) with the same payload
 *  - heartbeat watch on 0x100 (timeout 1 s)
 * Prints a status line every second: tx/rx/echo counters, controller state.
 */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_can.h"

#define HB_ID 0x100

struct rx_msg {
	uint32_t id;
	uint8_t len;
	uint8_t data[8];
};

K_MSGQ_DEFINE(rxq, sizeof(struct rx_msg), 32, 4);

static volatile uint32_t rx_total, rx_dropped;
static volatile bool hb_alive;

static void on_rx(uint32_t id, const uint8_t *data, uint8_t len)
{
	struct rx_msg m = {.id = id, .len = len};

	memcpy(m.data, data, len);
	rx_total++;
	if (k_msgq_put(&rxq, &m, K_NO_WAIT) != 0) {
		rx_dropped++;
	}
}

static void on_hb(bool alive)
{
	hb_alive = alive;
	printk("HEARTBEAT %s\n", alive ? "OK" : "LOST");
}

int main(void)
{
	uint32_t beacon = 0, echoed = 0, tx_err = 0;
	int ret = driver_can_init(500000, false);

	ret |= driver_can_add_rx(0x200, 0x700, on_rx);
	printk("can init+filters=%d\n", ret);
	if (ret < 0) {
		return 0;
	}
	printk("heartbeat watch=%d\n", driver_can_heartbeat_watch(HB_ID, 1000, on_hb));

	int64_t next_beacon = k_uptime_get(), next_status = next_beacon + 1000;

	while (1) {
		struct rx_msg m;

		while (k_msgq_get(&rxq, &m, K_MSEC(20)) == 0) {
			if (driver_can_send(m.id + 0x100, m.data, m.len) == 0) {
				echoed++;
			} else {
				tx_err++;
			}
		}
		int64_t now = k_uptime_get();

		if (now >= next_beacon) {
			uint8_t d[6] = {beacon, beacon >> 8, beacon >> 16, beacon >> 24, 0xA5, 0x5A};

			if (driver_can_send(0x321, d, sizeof(d)) != 0) {
				tx_err++;
			}
			beacon++;
			next_beacon += 500;
		}
		if (now >= next_status) {
			printk("beacon=%u rx=%u echo=%u dropped=%u tx_err=%u state=%d\n", beacon,
			       rx_total, echoed, rx_dropped, tx_err, driver_can_get_state());
			next_status += 1000;
		}
	}
}

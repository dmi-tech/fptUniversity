/* A8/A9: CAN loopback - mask filter and heartbeat watch. Prints PASS/FAIL lines. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_can.h"

#define HB_ID 0x100
#define HB_TIMEOUT 1000

static volatile uint32_t exact_rx, range_rx, hb_frames;
static volatile int64_t t_lost, t_back, t_last_hb_sent;
static volatile int lost_count, back_count;

static void on_exact(uint32_t id, const uint8_t *d, uint8_t len) { exact_rx++; }
static void on_range(uint32_t id, const uint8_t *d, uint8_t len) { range_rx++; }

static void on_hb(bool alive)
{
	if (alive) {
		back_count++;
		t_back = k_uptime_get();
	} else {
		lost_count++;
		t_lost = k_uptime_get();
	}
}

static void send_hb_for(int ms)
{
	int64_t end = k_uptime_get() + ms;

	while (k_uptime_get() < end) {
		driver_can_send(HB_ID, NULL, 0);
		t_last_hb_sent = k_uptime_get();
		hb_frames++;
		k_msleep(250);
	}
}

int main(void)
{
	uint8_t d[1] = {0};
	int ret = driver_can_init(500000, true);

	printk("init=%d\n", ret);
	/* --- A8: filters. exact 0x123 (mask 0x7FF), range 0x200..0x20F (mask 0x7F0) */
	ret = driver_can_add_rx(0x123, 0x7FF, on_exact);
	ret |= driver_can_add_rx(0x200, 0x7F0, on_range);
	printk("add_rx=%d\n", ret);
	driver_can_send(0x123, d, 1);
	driver_can_send(0x124, d, 1);  /* outside exact */
	driver_can_send(0x205, d, 1);  /* inside range */
	driver_can_send(0x215, d, 1);  /* outside range */
	driver_can_send(0x7FF, d, 0);  /* nobody */
	k_msleep(200);
	printk("exact_rx=%u (want 1) range_rx=%u (want 1)\n", exact_rx, range_rx);
	printk("A8 %s\n", (exact_rx == 1 && range_rx == 1) ? "PASS" : "FAIL");
	printk("bad id: %d (want -22)  state=%d (want 0)\n", driver_can_send(0x800, d, 1),
	       driver_can_get_state());

	/* --- A9: heartbeat */
	ret = driver_can_heartbeat_watch(HB_ID, HB_TIMEOUT, on_hb);
	printk("watch=%d  again=%d (want -114)\n", ret,
	       driver_can_heartbeat_watch(HB_ID, HB_TIMEOUT, on_hb));
	send_hb_for(1500);
	printk("alive=%d (want 1) back=%d lost=%d\n", driver_can_heartbeat_alive(), back_count, lost_count);
	int64_t stop = t_last_hb_sent;
	k_msleep(2000);
	int64_t dt = t_lost - stop;
	printk("lost=%d after %lld ms (want 1, 1000..1300)\n", lost_count, (long long)dt);
	printk("alive=%d (want 0)\n", driver_can_heartbeat_alive());
	send_hb_for(500);
	printk("back=%d (want 2) alive=%d (want 1)\n", back_count, driver_can_heartbeat_alive());
	bool ok = back_count == 2 && lost_count == 1 && dt >= 950 && dt <= 1400;
	printk("A9 %s\n", ok ? "PASS" : "FAIL");
	while (1) {
		k_msleep(1000);
	}
}

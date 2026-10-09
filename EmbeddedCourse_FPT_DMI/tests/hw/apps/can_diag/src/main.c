/* CAN diagnostic. The status is printed on RS485 (9600 8N1) once a second, because SWD does not
 * work while the 24 V supply is on. Needs the 24 V supply (isolated CAN side).
 *
 * Cycles the bitrate (500k, 250k, 125k, 1M, 12 s each), sends 0x321 every 100 ms in normal mode
 * and counts: frames ACKed by a peer (tx callback without error), tx failures, frames received,
 * controller state and error counters. A peer that ACKs makes "txok" grow: that is the bitrate
 * the peer is using.
 */
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_io.h>

#include "driver_rs485.h"

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static const uint32_t rates[] = {500000, 250000, 125000, 1000000};

static volatile uint32_t txok, txfail, rx, rx_last_id;
static volatile int last_cb_err, last_send_ret;
static volatile uint32_t send_fail;

static void tx_done(const struct device *dev, int error, void *user_data)
{
	if (error == 0) {
		txok++;
	} else {
		txfail++;
		last_cb_err = error;
	}
}

static void rx_cb(const struct device *dev, struct can_frame *f, void *user_data)
{
	rx++;
	rx_last_id = f->id;
}

/* Transceiver check without the CAN controller: PB7 (TXD) as GPIO output, PB8 (RXD) as input.
 * A powered transceiver (24 V on, standby released) loops TXD back to RXD when nobody else drives
 * the bus. The standby pin is PB0 on the schematic (PB0-FDCAN1-S); the DMI_CAN_01 sketch uses PB5
 * (which is RL-OUT3 on the schematic), so every combination is tried:
 *   pb0: 0 = PB0 driven low (normal), 1 = PB0 driven high (standby)
 *   pb5: 0 = PB5 driven low,          -1 = PB5 left alone
 */
static void txd_rxd_test_one(int pb0, int pb5)
{
	const uint32_t gpiob = 0x42020400;
	uint32_t moder = sys_read32(gpiob);
	uint32_t m = moder & ~((3U << 14) | (3U << 16));
	char line[130];
	uint32_t r1, r0, r0b;

	m |= (1U << 14);                                 /* PB7 out, PB8 in */
	if (pb5 >= 0) {
		m = (m & ~(3U << 10)) | (1U << 10);      /* PB5 out */
		sys_write32(1U << (5 + 16), gpiob + 0x18); /* PB5 = 0 */
	}
	sys_write32(pb0 ? (1U << 0) : (1U << 16), gpiob + 0x18);
	sys_write32(m, gpiob);
	sys_write32(1U << 7, gpiob + 0x18);
	k_busy_wait(3000);
	r1 = (sys_read32(gpiob + 0x10) >> 8) & 1;
	sys_write32(1U << (7 + 16), gpiob + 0x18);
	k_busy_wait(3000);
	r0 = (sys_read32(gpiob + 0x10) >> 8) & 1;
	sys_write32(1U << 7, gpiob + 0x18);
	k_busy_wait(3000);
	r0b = (sys_read32(gpiob + 0x10) >> 8) & 1;
	sys_write32(moder, gpiob); /* PB7/PB8 back to AF, PB5 back to its previous mode */
	snprintf(line, sizeof(line), "TXD/RXD test PB0=%s PB5=%s: TXD=1->RXD=%u TXD=0->RXD=%u TXD=1->RXD=%u (follow: 1,0,1)\r\n",
		 pb0 ? "HIGH" : "low", pb5 >= 0 ? "low" : "untouched", r1, r0, r0b);
	driver_rs485_print(line);
	printk("%s", line);
}

static void txd_rxd_test(void)
{
	const uint32_t gpiob = 0x42020400;
	uint32_t stb_before = sys_read32(gpiob + 0x14) & 1;

	txd_rxd_test_one(0, -1);
	txd_rxd_test_one(0, 0);
	txd_rxd_test_one(1, 0);
	txd_rxd_test_one(1, -1);
	/* restore standby as the CAN driver left it */
	sys_write32(stb_before ? (1U << 0) : (1U << 16), gpiob + 0x18);
}

/* Holds TXD (PB7) low/high for several seconds with the transceiver forced out of standby, so that
 * the voltages can be measured with a multimeter (U7 pin 14, U15 pin 1/4/6/7, CAN bus, U7 pin 5).
 */
static void hold_txd(bool low, int seconds)
{
	const uint32_t gpiob = 0x42020400;
	uint32_t moder = sys_read32(gpiob);
	char line[100];

	sys_write32(1U << (0 + 16), gpiob + 0x18);                              /* PB0 = 0: normal mode */
	sys_write32(1U << (5 + 16), gpiob + 0x18);                              /* PB5 = 0 as well (sketch's S) */
	sys_write32(((moder & ~((3U << 14) | (3U << 16) | (3U << 10))) | (1U << 14) | (1U << 10)), gpiob);
	sys_write32(low ? (1U << (7 + 16)) : (1U << 7), gpiob + 0x18);
	for (int i = 0; i < seconds; i++) {
		snprintf(line, sizeof(line), "HOLD TXD=%s  RXD(PB8)=%u  (%d/%d s)\r\n", low ? "LOW(dominant)" : "HIGH",
			 (unsigned int)((sys_read32(gpiob + 0x10) >> 8) & 1), i + 1, seconds);
		driver_rs485_print(line);
		printk("%s", line);
		k_msleep(1000);
	}
	sys_write32(1U << 7, gpiob + 0x18);
	sys_write32(moder, gpiob);
	sys_write32(1U << 0, gpiob + 0x18); /* standby again, as the CAN driver leaves it */
}

static void say(const char *s)
{
	driver_rs485_print(s);
	printk("%s", s);
}

int main(void)
{
	char line[160];
	struct can_filter any = {.id = 0, .mask = 0, .flags = 0};
	uint32_t beacon = 0;
	int ret = driver_rs485_init(9600);

	if (ret < 0 || !device_is_ready(can_dev)) {
		printk("init failed rs485=%d can_ready=%d\n", ret, device_is_ready(can_dev));
		return 0;
	}
	can_add_rx_filter(can_dev, rx_cb, NULL, &any);
	say("\r\nCAN DIAG start\r\n");
	can_stop(can_dev);
	say("MEASURE NOW: TXD low (dominant) for 10 s\r\n");
	hold_txd(true, 10);
	say("MEASURE NOW: TXD high (recessive) for 6 s\r\n");
	hold_txd(false, 6);

	for (int r = 0;; r = (r + 1) % ARRAY_SIZE(rates)) {
		can_stop(can_dev);
		txd_rxd_test();
		ret = can_set_bitrate(can_dev, rates[r]);
		ret |= can_set_mode(can_dev, CAN_MODE_NORMAL);
		ret |= can_start(can_dev);
		txok = txfail = rx = 0;
		send_fail = 0;
		snprintf(line, sizeof(line), "--- bitrate %u set=%d ---\r\n", rates[r], ret);
		say(line);

		for (int sec = 0; sec < 12; sec++) {
			for (int i = 0; i < 10; i++) {
				struct can_frame f = {.id = 0x321, .dlc = 4};

				f.data[0] = beacon;
				f.data[1] = beacon >> 8;
				f.data[2] = 0xA5;
				f.data[3] = 0x5A;
				beacon++;
				int sret = can_send(can_dev, &f, K_NO_WAIT, tx_done, NULL);

				if (sret != 0) {
					send_fail++;
					last_send_ret = sret;
				}
				k_msleep(100);
			}
			enum can_state st;
			struct can_bus_err_cnt cnt;

			can_get_state(can_dev, &st, &cnt);
			uint32_t psr = sys_read32(0x4000A444);
			uint32_t gpiob_odr = sys_read32(0x42020400 + 0x14);
			uint32_t gpiob_idr = sys_read32(0x42020400 + 0x10);
			uint32_t cccr = sys_read32(0x4000A418);
			uint32_t txbrp = sys_read32(0x4000A4C8);
			uint32_t ir = sys_read32(0x4000A450);

			snprintf(line, sizeof(line),
				 "br=%u st=%d tec=%u rec=%u lec=%u txok=%u txfail=%u rx=%u id=%03X STB=%u\r\n",
				 rates[r], st, cnt.tx_err_cnt, cnt.rx_err_cnt, psr & 7, txok, txfail, rx,
				 rx_last_id, gpiob_odr & 1);
			say(line);
			snprintf(line, sizeof(line),
				 "   PSR=%08X act=%u CCCR=%X TXBRP=%X IR=%X RXpin(PB8)=%u TXpin(PB7)=%u cb_err=%d send_fail=%u send_ret=%d\r\n",
				 psr, (psr >> 3) & 3, cccr, txbrp, ir, (gpiob_idr >> 8) & 1,
				 (gpiob_idr >> 7) & 1, last_cb_err, send_fail, last_send_ret);
			say(line);
		}
	}
}

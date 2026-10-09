/* Static signal levels for multimeter checks of the CAN isolation + transceiver chain.
 * TXD (PB7) is held low for 30 s, then high for 30 s, forever. PB0 (S) is held low (normal mode).
 * The CAN controller is stopped and not used. Status every second on RTT (no RS485).
 */
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_io.h>


#define GPIOB 0x42020400U
#define PHASE_S 30

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

int main(void)
{
	char line[100];

	can_stop(can_dev);
	uint32_t moder = sys_read32(GPIOB);

	sys_write32(1U << 16, GPIOB + 0x18); /* PB0 = 0: transceiver normal mode */
	sys_write32((moder & ~((3U << 14) | (3U << 16))) | (1U << 14), GPIOB); /* PB7 out, PB8 in */

	for (;;) {
		for (int phase = 0; phase < 2; phase++) {
			bool low = (phase == 0);

			sys_write32(low ? (1U << 23) : (1U << 7), GPIOB + 0x18);
			for (int i = 1; i <= PHASE_S; i++) {
				snprintf(line, sizeof(line),
					 "TXD=%s (PB7)  RXD(PB8)=%u  PB0(S)=%u  %d/%d s\r\n",
					 low ? "LOW(dominant)" : "HIGH(recessive)",
					 (unsigned int)((sys_read32(GPIOB + 0x10) >> 8) & 1),
					 (unsigned int)(sys_read32(GPIOB + 0x14) & 1), i, PHASE_S);
				printk("%s", line);
				k_msleep(1000);
			}
		}
	}
}

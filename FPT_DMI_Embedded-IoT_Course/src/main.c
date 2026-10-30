#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>
#include "driver_gpio.h"
#include "driver_can.h"
#include "driver_lcd.h"
#include "driver_sht41.h"

/* Mỗi board một ID riêng (0x001..0x00F), cũng là CAN ID của khung board gửi */
#define NODE_ID  0x001

/* Cấu hình LCD: chọn loại và địa chỉ I2C theo module của bạn */
#define LCD_TYPE 1602   /* 1602 (16x2) hoặc 2004 (20x4) */
#define LCD_ADDR 0x27   /* PCF8574: 0x27, PCF8574A: 0x3F */

#define BUTTON   0      /* alias in0 */

#if LCD_TYPE == 2004
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_20X4(LCD_ADDR);
#else
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_16X2(LCD_ADDR);
#endif

static struct driver_lcd lcd;

struct can_msg {
	uint32_t id;
	uint8_t len;
	uint8_t data[8];
};

K_MSGQ_DEFINE(rx_q, sizeof(struct can_msg), 8, 4);
static atomic_t send_request;

/* Chạy trong ngắt: chỉ chép khung vào hàng đợi, xử lý ở main */
static void on_can_rx(uint32_t id, const uint8_t *data, uint8_t len)
{
	struct can_msg msg = {.id = id, .len = len};

	memcpy(msg.data, data, len);
	k_msgq_put(&rx_q, &msg, K_NO_WAIT);
}

static void on_button(uint8_t id, bool pressed)
{
	if (pressed) {
		atomic_set(&send_request, 1);   /* main sẽ gửi khung */
	}
}

/* Dòng 0: "Sent:"/"Received:" + ID, dòng 1: các byte dữ liệu (tối đa 5 byte vừa LCD 16 cột) */
static void show_frame(const char *title, const struct can_msg *msg)
{
	char line[20] = "";
	int pos = 0;

	for (int i = 0; i < msg->len && i < 5; i++) {
		pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", msg->data[i]);
	}
	driver_lcd_clear(&lcd);
	driver_lcd_set_cursor(&lcd, 0, 0);
	driver_lcd_printf(&lcd, "%s 0x%03X", title, msg->id);
	driver_lcd_print_at(&lcd, 0, 1, line);
}

static void send_sht41(void)
{
	float t, h;
	struct can_msg msg = {.id = NODE_ID, .len = 4};

	if (driver_sht41_read(&t, &h)) {
		printk("SHT41 read error\n");
		return;
	}
	int16_t temp = (int16_t)(t * 10);    /* 28.5 C -> 285 */
	uint16_t hum = (uint16_t)(h * 10);   /* 61.2 % -> 612 */

	msg.data[0] = temp >> 8;
	msg.data[1] = temp & 0xFF;
	msg.data[2] = hum >> 8;
	msg.data[3] = hum & 0xFF;

	if (driver_can_send(msg.id, msg.data, msg.len)) {
		printk("CAN send error\n");
		driver_lcd_clear(&lcd);
		driver_lcd_print_at(&lcd, 0, 0, "CAN send error");
		return;
	}
	printk("TX 0x%03X [4] %02X %02X %02X %02X -> %d.%d C, %d.%d %%\n", msg.id,
	       msg.data[0], msg.data[1], msg.data[2], msg.data[3],
	       temp / 10, temp % 10, hum / 10, hum % 10);
	show_frame("Sent:", &msg);
}

int main(void)
{
	printk("Lab 7: CAN network, node 0x%03X\n", NODE_ID);
	if (driver_lcd_init(&lcd, &lcd_cfg)) {
		printk("LCD initialization error\n");
		return 0;
	}

	if (driver_sht41_init()) {
		printk("SHT41 initialization error\n");
		driver_lcd_print_at(&lcd, 0, 0, "SHT41 init error");
		return 0;
	}

	if (driver_can_init(500000, false)) {
		printk("CAN initialization error\n");
		driver_lcd_print_at(&lcd, 0, 0, "CAN init error");
		return 0;
	}
	/* Nhận mọi khung có ID 0x000..0x00F */
	if (driver_can_add_rx(0x000, 0x7F0, on_can_rx)) {
		printk("CAN filter error\n");
		return 0;
	}

	if (driver_gpio_in_init(BUTTON, on_button)) {
		printk("Button initialization error\n");
		return 0;
	}

	driver_lcd_set_cursor(&lcd, 0, 0);
	driver_lcd_printf(&lcd, "Node 0x%03X", NODE_ID);
	driver_lcd_print_at(&lcd, 0, 1, "Press button");

	while (1) {
		struct can_msg msg;

		if (k_msgq_get(&rx_q, &msg, K_MSEC(50)) == 0) {
			printk("RX 0x%03X [%u]", msg.id, msg.len);
			for (int i = 0; i < msg.len; i++) {
				printk(" %02X", msg.data[i]);
			}
			if (msg.len == 4) {
				int16_t temp = (msg.data[0] << 8) | msg.data[1];
				uint16_t hum = (msg.data[2] << 8) | msg.data[3];

				printk(" -> %d.%d C, %d.%d %%", temp / 10, temp % 10, hum / 10, hum % 10);
			}
			printk("\n");
			show_frame("Received:", &msg);
		}

		if (atomic_cas(&send_request, 1, 0)) {
			send_sht41();
		}
	}
	return 0;
}

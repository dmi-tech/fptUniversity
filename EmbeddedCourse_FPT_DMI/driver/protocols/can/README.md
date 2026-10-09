# Driver CAN – Bus CAN FD cách ly qua CN5

> Gửi/nhận khung CAN với board khác hoặc bộ chuyển USB-CAN. Có **chế độ loopback** để học CAN
> **không cần** board thứ hai, không cần 24 V.

---

## 1. Phần cứng

### 1.1 Trên board
| Khối | Chân MCU | Ghi chú |
| --- | --- | --- |
| FDCAN1 TX | PB7 | qua cách ly ISO7731 → TCAN1057 |
| FDCAN1 RX | PB8 | |
| S (standby) | PB0 | Zephyr tự điều khiển, **không đụng tới** |
| LED6 TX / RX | PD11 / PD12 | Mức thấp = sáng; driver tự nháy |
| Điện trở đầu cuối | R77 + R78 (2 × 60.4 Ω) | Có sẵn = 120 Ω |

### 1.2 Cổng CN5 (khi nối bus thật)
| CN5 | Tín hiệu | Nối đến board/thiết bị kia |
| --- | --- | --- |
| 1 | CANH | CANH |
| 2 | CANL | CANL |
| 3 | GND-ISO | GND-ISO / GND của thiết bị kia |
| 4 | +24V | Không nối (trừ khi cấp nguồn qua cáp) |

⚠ Nối bus thật cần **24 V** (phía cách ly). Chế độ loopback chỉ dùng phần trong chip, **không cần**.

## 2. Xung đột tài nguyên
Không xung đột với U16.

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_CAN=y
CONFIG_CAN_FD_MODE=y
CONFIG_GPIO=y
```
- `CONFIG_CAN_FD_MODE`: cho phép khung CAN FD (tới 64 byte). Không bắt buộc cho CAN thường.

### Bước 2 – `app.overlay`
FDCAN1 (500 kbit/s) và transceiver đã có trong board DTS. Chỉ thêm LED báo:
```dts
/ {
	can_leds {
		compatible = "gpio-leds";
		can_tx_led: can_tx_led { gpios = <&gpiod 11 GPIO_ACTIVE_LOW>; label = "CAN TX"; };
		can_rx_led: can_rx_led { gpios = <&gpiod 12 GPIO_ACTIVE_LOW>; label = "CAN RX"; };
	};
};
```
(Bỏ khối này cũng được – driver chỉ không nháy LED.)

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/protocols/can
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/protocols/can/driver_can.c
)
```

---

## 4. API (`driver_can.h`)

```c
typedef void (*driver_can_rx_cb_t)(uint32_t id, const uint8_t *data, uint8_t len);
```

| Hàm | Mô tả |
| --- | --- |
| `int driver_can_init(uint32_t bitrate, bool loopback)` | `bitrate`: 125000, 250000, 500000, 1000000. `loopback = true` → tự nhận khung mình gửi, không ra dây |
| `int driver_can_send(uint32_t id, const uint8_t *data, uint8_t len)` | Gửi khung chuẩn (ID 11 bit, ≤ 0x7FF), `len` 0..8. Chờ tối đa 100 ms |
| `int driver_can_send_ext(uint32_t id, const uint8_t *data, uint8_t len)` | Khung mở rộng (ID 29 bit) |
| `int driver_can_add_rx(uint32_t id, uint32_t mask, driver_can_rx_cb_t cb)` | Nhận khung có `(rx_id & mask) == (id & mask)`. `mask = 0x7FF` → đúng 1 ID; `mask = 0` → mọi ID. Tối đa 8 bộ lọc |
| `int driver_can_get_state(void)` | 0 = bình thường; `-ENETDOWN` = bus-off |
| `int driver_can_heartbeat_watch(uint32_t id, uint32_t timeout_ms, driver_can_hb_cb_t cb)` | Theo dõi khung heartbeat ID `id` (dùng 1 bộ lọc). Mỗi khung nhận được làm lại bộ đếm `timeout_ms`. `cb(false)` khi quá hạn (kể cả khi chưa nhận được khung nào), `cb(true)` khi heartbeat trở lại. `cb` chạy trong system workqueue. Chỉ 1 watch |
| `int driver_can_heartbeat_alive(void)` | 1 = còn sống, 0 = mất hoặc chưa thấy khung nào, `-EACCES` = chưa gọi watch |

Callback chạy **trong ngắt**: chỉ copy dữ liệu, đặt cờ, `k_sem_give`. In log/ xử lý nặng ở `main`.

Ý nghĩa mask, ví dụ `id = 0x120, mask = 0x7F0`: nhận các ID 0x120..0x12F.

---

## 5. Ví dụ `src/main.c` – loopback, không cần phần cứng

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include "driver_can.h"

K_MSGQ_DEFINE(rx_q, sizeof(uint8_t[8]), 4, 4);

static void on_rx(uint32_t id, const uint8_t *data, uint8_t len)
{
	uint8_t copy[8] = {0};

	memcpy(copy, data, len);
	k_msgq_put(&rx_q, copy, K_NO_WAIT);   /* chỉ đẩy vào hàng đợi, không printk trong ngắt */
}

int main(void)
{
	uint8_t tx[8] = {0x11, 0x22, 0x33, 0x44, 0, 0, 0, 0};
	uint8_t rx[8];
	int ret = driver_can_init(500000, true);   /* true = loopback */

	if (ret < 0) {
		printk("Loi CAN: %d\n", ret);
		return 0;
	}
	driver_can_add_rx(0x123, 0x7FF, on_rx);

	while (1) {
		tx[4]++;
		ret = driver_can_send(0x123, tx, 8);
		printk("Gui ID 0x123, byte4=%u, ket qua %d\n", tx[4], ret);

		if (k_msgq_get(&rx_q, rx, K_MSEC(100)) == 0) {
			printk("  Nhan lai: %02x %02x %02x %02x %02x\n", rx[0], rx[1], rx[2], rx[3], rx[4]);
		}
		k_msleep(1000);
	}
	return 0;
}
```

Chuyển sang bus thật: đổi `driver_can_init(500000, true)` thành `false`, nối 2 board qua CN5,
cấp 24 V. Board kia nhận bằng `driver_can_add_rx`.

### Heartbeat và timeout (Lab 3A)

Mô hình an toàn khi mất master: board là slave, master gửi heartbeat đều đặn; mất heartbeat thì
đưa cơ cấu chấp hành về trạng thái an toàn.

```c
static void on_hb(bool alive)
{
	if (!alive) {
		driver_gpio_set(0, false);   /* relay ngắt = trạng thái an toàn */
		printk("heartbeat LOST -> relay OFF\n");
	} else {
		printk("heartbeat OK\n");
	}
}

static void on_cmd(uint32_t id, const uint8_t *d, uint8_t len)
{
	if (len >= 1 && driver_can_heartbeat_alive() == 1) { /* chỉ nghe lệnh khi master còn sống */
		driver_gpio_set(0, d[0] != 0);
	}
}

/* sau driver_can_init(500000, false): */
driver_can_heartbeat_watch(0x100, 1000, on_hb);   /* timeout 1 s */
driver_can_add_rx(0x200, 0x7FF, on_cmd);          /* khung lệnh */
```

Quy ước khung dùng trong lớp: `0x100` heartbeat (master → slave), `0x200` lệnh relay
(byte 0 = 0/1), `0x201` trạng thái (slave → master, byte 0 = relay, byte 1 = heartbeat còn sống).
Relay khởi động ở trạng thái ngắt (`driver_gpio_out_init` luôn bắt đầu ở mức tắt).

## 6. Kết quả mong đợi (RTT)
```text
Gui ID 0x123, byte4=1, ket qua 0
  Nhan lai: 11 22 33 44 01
```
LED6 nháy khi gửi/nhận.

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Bus thật: gửi trả `-EAGAIN`/timeout | Không có thiết bị nào ACK (chỉ 1 nút trên bus) | CAN cần ít nhất 2 nút. Dùng loopback để tự thử |
| Bus thật: không có gì xảy ra | Chưa cấp 24 V | Cấp 24 V |
| `-ENETDOWN` (bus-off) | Sai bitrate 2 bên, đảo CANH/CANL, thiếu điện trở cuối | Cùng bitrate; kiểm tra dây |
| Không nhận được dù bên kia gửi | Bộ lọc sai | Thử `mask = 0` để nhận mọi ID |
| Hard fault khi nhận | `printk`/hàm chặn trong callback | Dùng hàng đợi như ví dụ |

## 8. Tham khảo
- Zephyr CAN API: https://docs.zephyrproject.org/latest/hardware/peripherals/can/index.html
- Datasheet TI TCAN1057, ISO7731

# Driver RS485 – Truyền thông công nghiệp qua CN1

> Gửi/nhận dữ liệu trên bus RS485 cách ly (cổng CN1) – chuẩn phổ biến trong nhà máy, kết nối được
> nhiều thiết bị trên một đôi dây, xa tới 1200 m. Nền tảng để học Modbus RTU.

**Cần thêm driver:** `protocols/uart`.

---

## 1. Phần cứng

### 1.1 Trên board
| Khối | Chân MCU | Ghi chú |
| --- | --- | --- |
| UART4 TX → ISO3088 D | PA0 | |
| UART4 RX ← ISO3088 R | PA1 | |
| DE/RE (hướng) | **PC3** khi lắp R88; mạch 555 tự đảo khi lắp R87 | **[CẦN KIỂM TRA]** board của bạn lắp điện trở nào |
| LED1 | – | Tự nháy khi có TX/RX (không cần code) |
| Điện trở đầu cuối 120 Ω | R21 | Có sẵn |

### 1.2 Cổng CN1 (terminal 3.81 mm)
| CN1 | Tín hiệu | Nối đến bộ chuyển USB-RS485 |
| --- | --- | --- |
| 1 | RS485A | A (hoặc D+, 485+) |
| 2 | RS485B | B (hoặc D-, 485-) |
| 3 | +24V | **Không nối** |
| 4 | GND-ISO | GND (khuyến nghị) |

⚠ **Bắt buộc cấp 24 V** (jack DC1 hoặc CN4): phía RS485 dùng nguồn cách ly lấy từ 24 V. Chỉ cắm
USB thì RS485 **không hoạt động**.

```text
 Máy tính ── USB ── [USB-RS485] ── A ──── CN1-1
                                 ── B ──── CN1-2
                                 ── GND ── CN1-4
 Nguồn 24 V ── jack DC1 của board
```

## 2. Xung đột tài nguyên
Không xung đột với U16. UART4 chỉ dùng cho RS485.

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_SERIAL=y
CONFIG_UART_INTERRUPT_DRIVEN=y
CONFIG_RING_BUFFER=y
CONFIG_GPIO=y
```

### Bước 2 – `app.overlay`
UART4 (9600 8N1) đã bật trong board DTS. Chỉ cần khai báo chân DE/RE:
```dts
/ {
	zephyr,user {
		rs485-de-gpios = <&gpioc 3 GPIO_ACTIVE_HIGH>;
	};
};
```
- Board lắp **R87** (tự đảo chiều): **bỏ** khối trên. Driver tự nhận biết và không điều khiển PC3.
- Nếu overlay đã có `zephyr,user` (ví dụ của driver `adc`), **gộp** thành một node:
  ```dts
  zephyr,user {
  	io-channels = <&adc1 15>;
  	rs485-de-gpios = <&gpioc 3 GPIO_ACTIVE_HIGH>;
  };
  ```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/protocols/uart
  ${DRIVER_DIR}/protocols/rs485
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/protocols/uart/driver_uart.c
  ${DRIVER_DIR}/protocols/rs485/driver_rs485.c
)
```

---

## 4. API (`driver_rs485.h`)

| Hàm | Mô tả |
| --- | --- |
| `int driver_rs485_init(uint32_t baud)` | Khởi tạo UART4 8N1 + chân DE. Baud thường dùng: 9600, 19200, 115200 |
| `int driver_rs485_send(const uint8_t *data, size_t len)` | Bật DE → gửi → **chờ byte cuối ra khỏi dây** → tắt DE |
| `int driver_rs485_print(const char *str)` | Gửi chuỗi |
| `int driver_rs485_receive(uint8_t *buf, size_t len, k_timeout_t timeout)` | Nhận tối đa `len` byte; trả về số byte |
| `int driver_rs485_receive_frame(uint8_t *buf, size_t len, uint32_t gap_ms, k_timeout_t timeout)` | Nhận đến khi **ngừng** `gap_ms` ms – cách tách khung kiểu Modbus |

Vì sao phải "chờ byte cuối ra khỏi dây"? RS485 chỉ có một đôi dây: đang gửi thì không nhận được.
Nếu tắt DE quá sớm, byte cuối bị cắt; quá muộn thì mất phần đầu câu trả lời của thiết bị kia.

---

## 5. Ví dụ `src/main.c` – echo

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_rs485.h"

int main(void)
{
	uint8_t buf[64];
	int ret = driver_rs485_init(9600);

	if (ret < 0) {
		printk("Loi RS485: %d\n", ret);
		return 0;
	}
	driver_rs485_print("RS485 san sang\r\n");

	while (1) {
		int n = driver_rs485_receive_frame(buf, sizeof(buf), 20, K_FOREVER);

		if (n > 0) {
			printk("Nhan %d byte\n", n);
			driver_rs485_print("ECHO: ");
			driver_rs485_send(buf, n);
		}
	}
	return 0;
}
```

## 6. Chạy và kết quả mong đợi
```bash
picocom -b 9600 /dev/ttyUSB0
```
Gõ `abc` → nhận lại `ECHO: abc`. LED1 trên board nháy khi có dữ liệu.

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Không nhận / không gửi được gì | Chưa cấp 24 V | Cấp 24 V vào DC1 |
| Ký tự sai, lẫn rác | Đảo A/B; sai baud | Đổi A↔B; cùng baud 2 bên |
| Nhận được nhưng gửi đi bị mất byte cuối | Board lắp R87 nhưng overlay vẫn khai PC3, hoặc ngược lại | Kiểm tra R87/R88, sửa overlay |
| Nhận lại chính dữ liệu mình gửi | Bộ chuyển USB-RS485 bật echo | Tắt echo trên bộ chuyển / bỏ qua |
| Nhiễu khi dây dài | Thiếu điện trở đầu cuối ở đầu kia | Thêm 120 Ω giữa A-B ở thiết bị cuối |

## 8. Tham khảo
- Datasheet TI ISO3088
- Modbus over serial line: https://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf

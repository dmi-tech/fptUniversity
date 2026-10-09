# Driver UART – Cổng nối tiếp

> Gửi/nhận dữ liệu với máy tính (qua bộ chuyển USB-UART), module GPS, Bluetooth HC-05, ESP32...
> trên USART6 (U16-13/14). Có bộ đệm nhận bằng ngắt nên không mất dữ liệu.

---

## 1. Phần cứng

Cần **bộ chuyển USB-UART 3.3 V** (CP2102, CH340, FT232 – đặt jumper 3.3 V).

```text
 Board                         USB-UART
 U16-14 (PC6, TX) ──────────►  RXD
 U16-13 (PC7, RX) ◄──────────  TXD
 U16-17 (GND)     ───────────  GND
                               (KHÔNG nối VCC)
```

**TX nối RX, RX nối TX** (đấu chéo).

| Cổng | TX | RX | U16 | Mặc định |
| --- | --- | --- | --- | --- |
| USART6 | PC6 | PC7 | 14, 13 | **Dùng cho driver này** |
| USART1 | PB14 | PB15 | 16, 15 | Dự phòng (trùng servo) |
| UART5 | PB13 | PB12 | 19, 20 | **Console MCUboot – không dùng** |
| UART4 | PA0 | PA1 | – | RS485 (driver `rs485`) |

## 2. Xung đột tài nguyên

| Chân | Dùng chung với | Ghi chú |
| --- | --- | --- |
| PC6, PC7 | Encoder (A, B) | Không dùng đồng thời |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_SERIAL=y
CONFIG_UART_INTERRUPT_DRIVEN=y
CONFIG_RING_BUFFER=y
```
- `CONFIG_UART_INTERRUPT_DRIVEN`: nhận dữ liệu bằng ngắt.
- `CONFIG_RING_BUFFER`: bộ đệm vòng lưu dữ liệu nhận được cho đến khi bạn đọc.

### Bước 2 – `app.overlay`
USART6 đã bật trong board DTS nhưng ở 921600 baud. Đổi về 115200:
```dts
&usart6 {
	current-speed = <115200>;
};
```
(Hàm `driver_uart_init` cũng đặt lại baud lúc chạy; dòng trên chỉ là giá trị mặc định.)

Dùng USART1 thay vì USART6: không cần overlay (board đã bật, 115200).

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/protocols/uart
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/protocols/uart/driver_uart.c
)
```

---

## 4. API (`driver_uart.h`)

```c
enum driver_uart_port {
	DRIVER_UART6 = 0,   /* U16-13/14 */
	DRIVER_UART1 = 1,   /* U16-15/16 */
	DRIVER_UART4 = 2,   /* RS485 – driver rs485 dùng, sinh viên không gọi trực tiếp */
};
typedef void (*driver_uart_rx_cb_t)(enum driver_uart_port port, uint8_t byte);
```

| Hàm | Mô tả |
| --- | --- |
| `int driver_uart_init(enum driver_uart_port port, uint32_t baud)` | Khởi tạo, 8N1. Bộ đệm nhận 256 byte |
| `int driver_uart_write(port, const uint8_t *data, size_t len)` | Gửi (chờ gửi xong) |
| `int driver_uart_print(port, const char *str)` | Gửi chuỗi |
| `int driver_uart_printf(port, const char *fmt, ...)` | Như `printf`, tối đa 128 ký tự |
| `int driver_uart_read(port, uint8_t *buf, size_t len, k_timeout_t timeout)` | Đọc tối đa `len` byte; trả về **số byte đọc được** (0 nếu hết thời gian) |
| `int driver_uart_read_line(port, char *buf, size_t len, k_timeout_t timeout)` | Đọc đến `\n` hoặc `\r`; bỏ ký tự xuống dòng; trả về độ dài |
| `int driver_uart_available(port)` | Số byte đang chờ trong bộ đệm |
| `int driver_uart_set_rx_callback(port, driver_uart_rx_cb_t cb)` | Gọi `cb` cho **mỗi byte** nhận được – chạy **trong ngắt**, chỉ được xử lý rất ngắn |

`timeout`: `K_MSEC(500)`, `K_SECONDS(2)`, `K_FOREVER` (chờ mãi), `K_NO_WAIT` (không chờ).

---

## 5. Ví dụ `src/main.c` – trò chuyện với máy tính

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include "driver_uart.h"

int main(void)
{
	char line[64];
	int ret = driver_uart_init(DRIVER_UART6, 115200);

	if (ret < 0) {
		printk("Loi UART: %d\n", ret);
		return 0;
	}
	driver_uart_print(DRIVER_UART6, "Xin chao! Go 'led on' hoac bat ky chu nao:\r\n");

	while (1) {
		int n = driver_uart_read_line(DRIVER_UART6, line, sizeof(line), K_FOREVER);

		if (n <= 0) {
			continue;
		}
		printk("Nhan %d ky tu: %s\n", n, line);       /* log RTT */
		driver_uart_printf(DRIVER_UART6, "Ban vua go: %s (%d ky tu)\r\n", line, n);

		if (strcmp(line, "led on") == 0) {
			driver_uart_print(DRIVER_UART6, "-> (bai tap: bat LED bang driver gpio)\r\n");
		}
	}
	return 0;
}
```

## 6. Chạy và kết quả mong đợi

Trên máy tính, mở cổng nối tiếp:
```bash
sudo apt install picocom          # cài 1 lần
picocom -b 115200 /dev/ttyUSB0    # thoát: Ctrl+A rồi Ctrl+X
```
(Trong máy ảo: chuyển thiết bị USB-UART vào máy ảo trước. Tên cổng có thể là `/dev/ttyACM0`.)

```text
Xin chao! Go 'led on' hoac bat ky chu nao:
hello                                  ← bạn gõ rồi Enter
Ban vua go: hello (5 ky tu)
```

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Không nhận được gì trên máy tính | TX/RX không đấu chéo; thiếu GND | Đảo 2 dây TX/RX; nối GND |
| Ký tự lạ `���` | Sai baud giữa 2 bên | Cùng 115200 cả 2 bên |
| `picocom: cannot open /dev/ttyUSB0: Permission denied` | User chưa thuộc nhóm `dialout` | `./scripts/setup_udev.sh`, đăng xuất rồi đăng nhập lại |
| Gõ Enter không thấy phản hồi | Terminal chỉ gửi `\r` | Đã xử lý trong `read_line`; kiểm tra lại baud |
| Mất ký tự khi gửi nhanh | Bộ đệm 256 byte bị đầy vì chương trình đọc chậm | Đọc thường xuyên hơn |

## 8. Tham khảo
- Zephyr UART API: https://docs.zephyrproject.org/latest/hardware/peripherals/uart.html
- Zephyr Ring buffer: https://docs.zephyrproject.org/latest/kernel/data_structures/ring_buffers.html

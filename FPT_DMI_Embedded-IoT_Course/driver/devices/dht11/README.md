# Driver DHT11 – Cảm biến nhiệt độ, độ ẩm giá rẻ

> Đọc nhiệt độ (0–50 °C, ±2 °C) và độ ẩm (20–90 %, ±5 %) từ module DHT11, cắm vào **chân U16
> bất kỳ** (mặc định PE0 – U16-8).

---

## 1. Phần cứng

Module DHT11 3 chân (loại có sẵn điện trở kéo lên):

| DHT11 | Nối đến | Ghi chú |
| --- | --- | --- |
| VCC (+) | 3.3 V ngoài **(khuyên dùng)** hoặc U16-1 (5 V) | Xem cảnh báo |
| DATA (out/S) | U16-8 (PE0) | |
| GND (−) | U16-17 | Nối chung GND nếu dùng nguồn 3.3 V ngoài |

⚠ Cấp 5 V thì điện trở kéo lên của module kéo DATA lên **5 V**. **[CẦN KIỂM TRA]** chân bạn chọn
có chịu 5 V không (mục 5 trong `driver/README.md`). DHT11 chạy tốt ở 3.3 V.

Cảm biến trần 4 chân (không module): thêm điện trở **10 kΩ** từ DATA lên VCC; chân 3 bỏ trống.

## 2. Xung đột tài nguyên

| Chân | Dùng chung với | Ghi chú |
| --- | --- | --- |
| PE0 (U16-8) | LED ngoài (driver `gpio` ví dụ `out1`) | Không dùng đồng thời |

Driver DHT của Zephyr **không dùng ngắt, timer hay AF** – chỉ bật/tắt chân và đo thời gian. Vì
vậy dùng được **mọi chân U16** (trừ PB12/PB13). Lúc đọc (~5 ms) driver tạm khóa ngắt để đo
chính xác.

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_GPIO=y
CONFIG_SENSOR=y
CONFIG_DHT=y
CONFIG_DHT_LOCK_IRQS=y
```
- `CONFIG_DHT`: driver DHT11/DHT22 của Zephyr.
- `CONFIG_DHT_LOCK_IRQS`: khóa ngắt khi đọc để thời gian đo chính xác (tránh lỗi checksum).

### Bước 2 – `app.overlay`
```dts
/ {
	dht11: dht11 {
		compatible = "aosong,dht";
		dio-gpios = <&gpioe 0 (GPIO_ACTIVE_LOW | GPIO_OPEN_DRAIN | GPIO_PULL_UP)>;
		/* DHT22/AM2302: thêm dòng   dht22;   */
	};
	aliases {
		dht0 = &dht11;
	};
};

&gpioe { status = "okay"; };   /* board chỉ bật sẵn gpioa..gpiod */
```
Giải thích cờ:
- `GPIO_OPEN_DRAIN`: MCU chỉ **kéo xuống 0** hoặc **thả nổi**, không bao giờ đẩy lên 1 – vì DHT11
  cũng kéo dây xuống khi trả lời; hai bên cùng đẩy sẽ chập.
- `GPIO_PULL_UP`: bật điện trở kéo lên bên trong (phụ thêm cho điện trở trên module).
- `GPIO_ACTIVE_LOW`: theo yêu cầu của driver Zephyr.

### Đổi sang chân khác

Chỉ sửa `dio-gpios` và tắt ngoại vi đang chiếm chân đó. **Không sửa code.**

| U16 | Chân | `dio-gpios` | Thêm vào overlay |
| --- | --- | --- | --- |
| 8 | PE0 | `<&gpioe 0 ...>` | `&gpioe { status = "okay"; };` |
| 3 | PA3 | `<&gpioa 3 ...>` | `&usart2 { status = "disabled"; };` |
| 4 | PA2 | `<&gpioa 2 ...>` | `&usart2 { status = "disabled"; };` |
| 5 | PC2 | `<&gpioc 2 ...>` | – |
| 6 / 7 / 11 | PC1 / PC0 / PA9 | `<&gpioc 1 ...>` ... | `&spi2 { status = "disabled"; };` |
| 12 | PA10 | `<&gpioa 10 ...>` | `/delete-node/ &user_btn;` |
| 13 / 14 | PC7 / PC6 | `<&gpioc 7 ...>` ... | `&usart6 { status = "disabled"; };` |
| 15 / 16 | PB15 / PB14 | `<&gpiob 15 ...>` ... | `&usart1 { status = "disabled"; };` |
| 9, 10 | PB9, PB6 | **Không nên** – mất I2C1 | |

Hai cảm biến: thêm node `dht11_b` trên chân khác và alias `dht1`.

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/dht11
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/dht11/driver_dht11.c
)
```

---

## 4. API (`driver_dht11.h`)

| Hàm | Mô tả |
| --- | --- |
| `int driver_dht11_init(uint8_t id)` | Khởi tạo cảm biến `dht<id>` (alias) |
| `int driver_dht11_read(uint8_t id, int *temp_c, int *humidity)` | Đọc (°C, %). **Cách nhau ≥ 2 giây**: gọi sớm hơn thì trả `-EAGAIN` và **không** đọc cảm biến |

Driver tự canh khoảng cách giữa hai lần đọc (tính từ lần đọc trước, `DRIVER_DHT11_MIN_INTERVAL_MS` = 2000 ms).
Nếu bạn gọi `driver_dht11_read` sớm hơn, hàm trả `-EAGAIN` ngay mà không chạm vào cảm biến; chờ thêm rồi đọc
lại. Mỗi cảm biến (`dht0`, `dht1`) có bộ đếm thời gian riêng.

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_dht11.h"

int main(void)
{
	int ret = driver_dht11_init(0);

	if (ret < 0) {
		printk("Loi DHT11: %d\n", ret);
		return 0;
	}

	while (1) {
		int t, h;

		ret = driver_dht11_read(0, &t, &h);
		if (ret == 0) {
			printk("DHT11: %d C, %d %%\n", t, h);
		} else {
			printk("DHT11 doc loi: %d\n", ret);
		}
		k_msleep(2000);
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
DHT11: 29 C, 65 %
```

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| `-EIO` liên tục | Sai chân DATA; thiếu nguồn; thiếu điện trở kéo lên (cảm biến trần) | Kiểm tra dây; thêm 10 kΩ |
| `-EAGAIN` | Đọc nhanh hơn 2 s kể từ lần đọc trước | Đợi đủ 2 s (dùng `k_msleep(2000)` hoặc hơn) |
| `-EIO` thỉnh thoảng | Lỗi checksum do nhiễu; dây dài > 1 m | Rút ngắn dây; thử đọc lại sau 2 s |
| `-ENODEV` | Quên `&gpioe { status = "okay"; }` khi dùng PE0 | Thêm dòng đó |
| Lỗi devicetree `dio-gpios` | Sai cú pháp `<&gpioe 0 (...)>` | So lại với mẫu |
| Đọc ra 0 °C, 0 % | Module là DHT22 nhưng khai DHT11 | Thêm `dht22;` vào node |

## 8. Tham khảo
- Datasheet Aosong DHT11
- Zephyr binding `aosong,dht`: `zephyr/dts/bindings/sensor/aosong,dht.yaml`

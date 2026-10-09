# Driver SPI – Bus 4 dây SCK/MOSI/MISO/CS

> Giao tiếp tốc độ cao với module SPI (RFID RC522, thẻ nhớ ngoài, màn hình, ADC ngoài...) qua
> SPI2 trên header U16. Bài lab đầu tiên chỉ cần **1 sợi dây** (loopback).

---

## 1. Phần cứng

| Tín hiệu | Chân MCU | U16 | Hướng |
| --- | --- | --- | --- |
| SCK | PA9 | 11 | MCU → module |
| MOSI | PC1 | 6 | MCU → module |
| MISO | PC2 | 5 | module → MCU |
| CS | PC0 | 7 | MCU → module (mức thấp = chọn) |
| GND | – | 17 | |

### Bài lab loopback (không cần module)
Nối **U16-6 (MOSI)** với **U16-5 (MISO)** bằng một sợi dây. Dữ liệu gửi đi sẽ quay về chính nó.

### Module thật (ví dụ RC522 RFID)
| RC522 | Nối đến | Ghi chú |
| --- | --- | --- |
| SDA (= CS) | U16-7 | |
| SCK | U16-11 | |
| MOSI | U16-6 | |
| MISO | U16-5 | |
| RST | GPIO bất kỳ còn trống, vd U16-8 (PE0) | điều khiển bằng driver `gpio` |
| 3.3V | **3.3 V ngoài** | RC522 **không chịu 5 V** |
| GND | U16-17 | |

## 2. Xung đột tài nguyên

| Chân | Dùng chung với | Ghi chú |
| --- | --- | --- |
| PC0, PC2 | HC-SR04 (TRIG, ECHO), ADC kênh 10/12 | Không dùng đồng thời |
| SPI2 trong board DTS | Node `st7920` (màn hình cũ) | Overlay bên dưới xóa node này |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_SPI=y
```

### Bước 2 – `app.overlay`
```dts
/* Board DTS khai báo sẵn màn hình ST7920 trên SPI2 → xóa để thay bằng thiết bị của mình */
/delete-node/ &st7920;

/* Board chỉ khai báo SCK + MOSI, chưa có MISO → tự khai báo PC2 = SPI2_MISO (AF5) */
&pinctrl {
	spi2_miso_pc2: spi2_miso_pc2 {
		pinmux = <STM32_PINMUX('C', 2, AF5)>;
		bias-pull-down;
	};
};

&spi2 {
	pinctrl-0 = <&spi2_sck_pa9 &spi2_miso_pc2 &spi2_mosi_pc1>;
	pinctrl-names = "default";
	cs-gpios = <&gpioc 0 GPIO_ACTIVE_LOW>;     /* CS = PC0, mức thấp = chọn */
	status = "okay";

	spidev: spidev@0 {
		compatible = "vnd,spi-device";
		reg = <0>;                          /* thiết bị thứ 0 = cs-gpios thứ 0 */
		spi-max-frequency = <1000000>;      /* 1 MHz */
	};
};
```
Giải thích:
- `cs-gpios`: CS là GPIO thường nên chọn chân nào cũng được. Board để CS mức cao cho ST7920; module
  SPI phổ biến dùng **mức thấp**.
- `spi-max-frequency`: tần số tối đa thiết bị chịu được. Driver không chạy nhanh hơn giá trị này.
- Clock SPI2 = 50 MHz, chia /2../256 → các tần số thực tế: 25 MHz, 12.5 MHz, ... 195 kHz.
  Driver chọn tần số **gần nhất nhưng không vượt** giá trị yêu cầu.

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/protocols/spi
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/protocols/spi/driver_spi.c
)
```

---

## 4. API (`driver_spi.h`)

| Hàm | Mô tả |
| --- | --- |
| `int driver_spi_init(uint32_t freq_hz, uint8_t mode)` | `mode` 0..3 (CPOL, CPHA). `freq_hz` ≤ `spi-max-frequency` |
| `int driver_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)` | Gửi và nhận đồng thời `len` byte. CS tự kéo thấp trong lúc truyền |
| `int driver_spi_write(const uint8_t *tx, size_t len)` | Chỉ gửi |
| `int driver_spi_read(uint8_t *rx, size_t len)` | Chỉ nhận (gửi 0xFF) |
| `int driver_spi_write_then_read(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)` | Gửi lệnh rồi đọc, **giữ CS thấp suốt** (kiểu "đọc thanh ghi") |

| Mode | CPOL | CPHA | Thiết bị ví dụ |
| --- | --- | --- | --- |
| 0 | 0 | 0 | RC522, hầu hết module |
| 1 | 0 | 1 | |
| 2 | 1 | 0 | |
| 3 | 1 | 1 | Một số ADC, thẻ SD (cũng chạy mode 0) |

---

## 5. Ví dụ `src/main.c` – loopback

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include "driver_spi.h"

int main(void)
{
	const uint8_t tx[] = "Hello SPI";
	uint8_t rx[sizeof(tx)] = {0};
	int ret = driver_spi_init(1000000, 0);

	if (ret < 0) {
		printk("Loi SPI: %d\n", ret);
		return 0;
	}

	while (1) {
		memset(rx, 0, sizeof(rx));
		ret = driver_spi_transfer(tx, rx, sizeof(tx));
		printk("Gui: %s | Nhan: %s | %s\n", tx, rx,
		       (ret == 0 && memcmp(tx, rx, sizeof(tx)) == 0) ? "OK" : "SAI");
		k_msleep(1000);
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
Gui: Hello SPI | Nhan: Hello SPI | OK      (có dây nối MOSI–MISO)
Gui: Hello SPI | Nhan:  | SAI              (rút dây: MISO kéo xuống 0)
```

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| `undefined node label 'spi2_miso_pc2'` | Thiếu khối `&pinctrl` | Thêm khối đó |
| `Duplicate label 'spidev'` hoặc lỗi node `st7920` | Quên `/delete-node/ &st7920;` | Thêm dòng đó ở đầu overlay |
| Nhận toàn 0xFF hoặc 0x00 | Sai MISO/MOSI (đảo), thiếu GND chung, module chưa cấp nguồn | Kiểm tra dây |
| Module không trả lời | Sai mode, CS sai mức, tần số quá cao | Xem datasheet module; thử 100 kHz |
| `-EINVAL` khi init | `freq_hz` lớn hơn `spi-max-frequency` | Tăng giá trị trong overlay |

## 8. Tham khảo
- Zephyr SPI API: https://docs.zephyrproject.org/latest/hardware/peripherals/spi.html
- Datasheet module SPI bạn dùng (mục "SPI timing")

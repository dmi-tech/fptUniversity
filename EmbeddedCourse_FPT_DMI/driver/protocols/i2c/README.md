# Driver I2C – Bus 2 dây SDA/SCL

> Giao tiếp với module I2C (LCD, cảm biến ánh sáng, EEPROM...) trên I2C1 (U16-9/10) và với
> SHT41, RTC có sẵn trên board qua I2C2. Có hàm **quét bus** để tìm địa chỉ thiết bị.

---

## 1. Phần cứng

| Bus | SDA | SCL | Ở đâu | Thiết bị có sẵn |
| --- | --- | --- | --- | --- |
| I2C1 | PB9 (U16-9) | PB6 (U16-10) | Header U16 | – (cắm module ngoài) |
| I2C2 | PB11 | PB10 | Trên board | SHT41 @0x44, RTC BM8563 @0x51 |

Đấu module ngoài vào I2C1:

| Module | Nối đến |
| --- | --- |
| VCC | U16-1 (5 V) – hoặc 3.3 V ngoài nếu module chạy 3.3 V |
| GND | U16-17 |
| SDA | U16-9 |
| SCL | U16-10 |

- I2C cần **điện trở kéo lên** trên SDA, SCL. Hầu hết module (PCF8574, GY-30...) đã có sẵn.
- Nhiều module dùng chung một bus được, miễn **khác địa chỉ**.
- ⚠ Module cấp 5 V có điện trở kéo lên 5 V: xem mục 5 "Mức điện áp" trong `driver/README.md`.

## 2. Xung đột tài nguyên

| Tài nguyên | Ghi chú |
| --- | --- |
| PB6, PB9 | Chỉ dùng cho I2C1. Không dùng làm GPIO cho driver khác |
| Địa chỉ trùng | Hai module cùng địa chỉ trên một bus → lỗi. Đổi jumper địa chỉ trên module |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_I2C=y
```

### Bước 2 – `app.overlay`
I2C1 và I2C2 đã bật sẵn trong board DTS. Board để I2C1 ở 400 kHz; nên giảm về 100 kHz cho module
rẻ tiền và dây dài:
```dts
&i2c1 {
	clock-frequency = <I2C_BITRATE_STANDARD>;   /* 100 kHz; FAST = 400 kHz */
};
```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/protocols/i2c
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/protocols/i2c/driver_i2c.c
)
```

---

## 4. API (`driver_i2c.h`)

```c
enum driver_i2c_bus { DRIVER_I2C1 = 0, DRIVER_I2C2 = 1 };
```

| Hàm | Mô tả |
| --- | --- |
| `int driver_i2c_init(enum driver_i2c_bus bus)` | Kiểm tra bus sẵn sàng |
| `int driver_i2c_scan(enum driver_i2c_bus bus)` | In địa chỉ mọi thiết bị trả lời; trả về **số thiết bị** tìm thấy |
| `int driver_i2c_write(bus, uint8_t addr, const uint8_t *data, size_t len)` | Ghi `len` byte |
| `int driver_i2c_read(bus, uint8_t addr, uint8_t *data, size_t len)` | Đọc `len` byte |
| `int driver_i2c_write_read(bus, addr, const uint8_t *wr, size_t wlen, uint8_t *rd, size_t rlen)` | Ghi rồi đọc liền (không STOP ở giữa) |
| `int driver_i2c_reg_write(bus, addr, uint8_t reg, uint8_t value)` | Ghi 1 thanh ghi |
| `int driver_i2c_reg_read(bus, addr, uint8_t reg, uint8_t *value)` | Đọc 1 thanh ghi |

`addr` là địa chỉ **7 bit** (0x08..0x77), **không** dịch trái. Lỗi `-EIO` = thiết bị không ACK.

---

## 5. Ví dụ `src/main.c` – quét 2 bus

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_i2c.h"

int main(void)
{
	driver_i2c_init(DRIVER_I2C1);
	driver_i2c_init(DRIVER_I2C2);

	printk("--- Quet I2C1 (U16-9/10) ---\n");
	printk("Tim thay %d thiet bi\n", driver_i2c_scan(DRIVER_I2C1));

	printk("--- Quet I2C2 (tren board) ---\n");
	printk("Tim thay %d thiet bi\n", driver_i2c_scan(DRIVER_I2C2));

	/* Đọc thanh ghi 0x02 (giây) của RTC BM8563 @0x51 */
	uint8_t sec;

	if (driver_i2c_reg_read(DRIVER_I2C2, 0x51, 0x02, &sec) == 0) {
		printk("RTC giay (BCD) = 0x%02x\n", sec & 0x7f);
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
--- Quet I2C1 (U16-9/10) ---
  0x27
Tim thay 1 thiet bi              (ví dụ đang cắm LCD PCF8574)
--- Quet I2C2 (tren board) ---
  0x44
  0x51
Tim thay 2 thiet bi
RTC giay (BCD) = 0x37
```

Địa chỉ thường gặp: PCF8574 0x20–0x27, PCF8574A 0x38–0x3F, BH1750 0x23/0x5C, SHT41 0x44,
BM8563 0x51, MPU6050 0x68, OLED SSD1306 0x3C, EEPROM 24Cxx 0x50–0x57.

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Quét không thấy gì trên I2C1 | Đảo SDA/SCL; chưa cấp nguồn module; thiếu GND chung | Kiểm tra dây |
| Quét thấy tất cả địa chỉ | SDA bị nối GND / chập | Kiểm tra dây |
| `-EIO` thỉnh thoảng | Dây quá dài, tốc độ 400 kHz | Dùng 100 kHz, dây < 30 cm |
| Treo bus (mọi lệnh đều lỗi) | Thiết bị giữ SDA ở mức thấp sau reset dở dang | Rút nguồn module rồi cắm lại |
| Địa chỉ in ra gấp đôi trong datasheet (vd 0x4E thay vì 0x27) | Datasheet ghi địa chỉ 8 bit (đã dịch trái) | Dùng địa chỉ 7 bit = 8 bit / 2 |

## 8. Tham khảo
- Zephyr I2C API: https://docs.zephyrproject.org/latest/hardware/peripherals/i2c.html
- NXP UM10204 – I2C-bus specification

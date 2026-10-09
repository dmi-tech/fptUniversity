# Driver LCD I2C – LCD ký tự HD44780 qua module PCF8574

> Hiển thị chữ lên LCD 16x2, 20x4... chỉ với 2 dây tín hiệu (I2C1: U16-9, U16-10). **Kích thước
> và địa chỉ chọn lúc khởi tạo**, gắn được nhiều LCD trên cùng bus.

**Cần thêm driver:** `protocols/i2c`.

---

## 1. Phần cứng

| Module LCD (mặt sau có PCF8574) | Nối đến |
| --- | --- |
| GND | U16-17 |
| VCC | U16-1 (5 V) |
| SDA | U16-9 (PB9) |
| SCL | U16-10 (PB6) |

- Sau khi cấp nguồn, **vặn biến trở xanh** trên module đến khi thấy hàng ô vuông/chữ. Đây là lỗi
  "không thấy chữ" phổ biến nhất.
- ⚠ Module 5 V kéo SDA/SCL lên 5 V – xem mục 5 trong `driver/README.md`.

### Kích thước hỗ trợ
| Hỗ trợ | Không hỗ trợ |
| --- | --- |
| 8x1, 8x2, 16x1, 16x2, 16x4, 20x2, **20x4**, 24x2, 40x1, 40x2 | **40x4** (có 2 chân E, module PCF8574 chỉ điều khiển 1); LCD đồ họa 128x64 (ST7920); OLED |

### Địa chỉ I2C
| Chip trên module | Dải địa chỉ | Hay gặp |
| --- | --- | --- |
| PCF8574 / PCF8574T | 0x20–0x27 | **0x27** |
| PCF8574A / PCF8574AT | 0x38–0x3F | **0x3F** |

Chọn bằng 3 jumper A0–A2 trên module (để hở = 1). Không chắc → chạy `driver_i2c_scan(DRIVER_I2C1)`.

## 2. Xung đột tài nguyên

| Tài nguyên | Ghi chú |
| --- | --- |
| I2C1 | Dùng chung được với BH1750 và module I2C khác, miễn **khác địa chỉ** |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_I2C=y
```

### Bước 2 – `app.overlay`
```dts
&i2c1 {
	clock-frequency = <I2C_BITRATE_STANDARD>;   /* 100 kHz – PCF8574 tối đa 100 kHz */
};
```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/protocols/i2c
  ${DRIVER_DIR}/devices/lcd_pcf8574
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/protocols/i2c/driver_i2c.c
  ${DRIVER_DIR}/devices/lcd_pcf8574/driver_lcd.c
)
```

---

## 4. Chọn kích thước LCD

Kích thước truyền vào lúc khởi tạo qua `struct driver_lcd_cfg`:

```c
struct driver_lcd_cfg {
	uint8_t addr;        /* địa chỉ I2C: 0x27, 0x3F, ... */
	uint8_t cols;        /* số cột: 8, 16, 20, 24, 40 */
	uint8_t rows;        /* số dòng: 1, 2, 4 */
	bool split_16x1;     /* chỉ cho một số LCD 16x1 nối nội như 8x2 (nửa phải không hiện) */
	bool font_5x10;      /* chỉ cho LCD 1 dòng có font 5x10 */
};

/* Macro tiện dụng */
#define DRIVER_LCD_CFG(_addr, _cols, _rows) { .addr = (_addr), .cols = (_cols), .rows = (_rows) }
#define DRIVER_LCD_CFG_16X2(_addr) DRIVER_LCD_CFG(_addr, 16, 2)
#define DRIVER_LCD_CFG_20X4(_addr) DRIVER_LCD_CFG(_addr, 20, 4)
```

Ví dụ:
```c
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_16X2(0x27);   /* LCD 1602 */
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_20X4(0x27);   /* LCD 2004 */
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG(0x3F, 16, 4); /* LCD 1604 */
```

Driver tự tính địa chỉ đầu mỗi dòng trong DDRAM: dòng 0 = 0x00, dòng 1 = 0x40, dòng 2 = `cols`,
dòng 3 = 0x40 + `cols` (20x4 → 0x00, 0x40, 0x14, 0x54).

---

## 5. API (`driver_lcd.h`)

Mỗi LCD là một biến `struct driver_lcd` (khai báo `static`).

| Hàm | Mô tả |
| --- | --- |
| `int driver_lcd_init(struct driver_lcd *lcd, const struct driver_lcd_cfg *cfg)` | `-EINVAL` kích thước không hỗ trợ; `-ENOTSUP` cột × dòng > 80; `-EIO` không thấy module |
| `int driver_lcd_clear(struct driver_lcd *lcd)` | Xóa màn hình (~2 ms) |
| `int driver_lcd_home(struct driver_lcd *lcd)` | Con trỏ về (0, 0) |
| `int driver_lcd_set_cursor(lcd, uint8_t col, uint8_t row)` | Cột, dòng tính từ **0** |
| `int driver_lcd_print(lcd, const char *str)` | In tại con trỏ; `\n` xuống dòng; tự cắt ở cuối dòng |
| `int driver_lcd_print_at(lcd, uint8_t col, uint8_t row, const char *str)` | In tại vị trí |
| `int driver_lcd_printf(lcd, const char *fmt, ...)` | Như `printf` |
| `int driver_lcd_clear_row(lcd, uint8_t row)` | Xóa một dòng |
| `int driver_lcd_backlight(lcd, bool on)` | Đèn nền |
| `int driver_lcd_cursor(lcd, bool show, bool blink)` | Hiện/nhấp nháy con trỏ |
| `int driver_lcd_create_char(lcd, uint8_t slot, const uint8_t bitmap[8])` | Tạo ký tự riêng, `slot` 0..7; in bằng `"\x01"`..`"\x07"` (slot 0 dùng `driver_lcd_putc(lcd, 0)`) |
| `int driver_lcd_putc(lcd, char c)` | In 1 ký tự |
| `void driver_lcd_get_size(lcd, uint8_t *cols, uint8_t *rows)` | Kích thước đã cấu hình |

LCD chỉ hiển thị ký tự ASCII; **không hiển thị tiếng Việt có dấu** (dùng không dấu hoặc tự tạo
tối đa 8 ký tự).

---

## 6. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_lcd.h"

static struct driver_lcd lcd;
static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_16X2(0x27);  /* đổi theo LCD của bạn */

static const uint8_t heart[8] = {
	0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00,
};

int main(void)
{
	uint8_t cols, rows;
	int ret = driver_lcd_init(&lcd, &lcd_cfg);

	if (ret < 0) {
		printk("Loi LCD: %d\n", ret);
		return 0;
	}
	driver_lcd_get_size(&lcd, &cols, &rows);
	printk("LCD %ux%u san sang\n", cols, rows);

	driver_lcd_create_char(&lcd, 1, heart);
	driver_lcd_print_at(&lcd, 0, 0, "Hello FPT \x01");
	if (rows >= 4) {
		driver_lcd_print_at(&lcd, 0, 2, "Dong 3 cua LCD 20x4");
	}

	for (int i = 0;; i++) {
		driver_lcd_clear_row(&lcd, 1);
		driver_lcd_set_cursor(&lcd, 0, 1);
		driver_lcd_printf(&lcd, "Dem: %d", i);
		k_msleep(1000);
	}
	return 0;
}
```

## 7. Kết quả mong đợi
```text
┌────────────────┐
│Hello FPT ♥     │
│Dem: 12         │
└────────────────┘
```

---

## 8. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Đèn sáng nhưng không thấy chữ | Độ tương phản | **Vặn biến trở** trên module |
| Một hàng ô vuông đen | LCD chưa được khởi tạo (sai địa chỉ / `init` lỗi) | Xem log; quét I2C |
| `-EIO` | Sai địa chỉ; đảo SDA/SCL | `driver_i2c_scan`, kiểm tra dây |
| Chữ lộn xộn, ký tự lạ | Module có sơ đồ chân PCF8574 khác (hiếm) | **[CẦN KIỂM TRA]** đổi hằng `LCD_PIN_*` trong `driver_lcd.c` (mặc định P0=RS, P1=RW, P2=E, P3=đèn, P4–P7=D4–D7) |
| Dòng 3, 4 hiện sai chỗ | Khai sai kích thước (vd 16x2 cho LCD 20x4) | Sửa `cfg` |
| LCD 16x1 chỉ hiện 8 ký tự | Loại 16x1 nối nội như 8x2 | `.split_16x1 = true` |

## 9. Tham khảo
- Datasheet Hitachi HD44780U (bảng lệnh, DDRAM, CGRAM)
- Datasheet NXP PCF8574

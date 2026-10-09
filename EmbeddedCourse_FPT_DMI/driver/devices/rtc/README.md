# Driver RTC – Đồng hồ thời gian thực BM8563 có sẵn trên board

> Đọc/đặt ngày giờ. Có pin CR1220 nên **giờ vẫn chạy khi rút nguồn board**.

---

## 1. Phần cứng

| Thông số | Giá trị |
| --- | --- |
| Chip | BM8563 (U17), tương thích PCF8563 |
| Bus | I2C2: SCL PB10, SDA PB11 |
| Địa chỉ | 0x51 |
| Pin | CR1220 3 V (BT1) – **lắp pin** để giữ giờ khi mất nguồn |
| Thạch anh | 32.768 kHz (X3) |

Board **không có** thạch anh LSE cho RTC bên trong STM32 (chân PC14/PC15 dùng làm ngõ vào cách
ly), nên dùng chip BM8563 này.

## 2. Xung đột tài nguyên
Không có. I2C2 dùng chung với SHT41 (khác địa chỉ).

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_I2C=y
CONFIG_RTC=y
CONFIG_RTC_PCF8563=y
```

### Bước 2 – `app.overlay`
BM8563 **chưa có** trong board DTS → thêm vào bus I2C2:
```dts
/ {
	aliases {
		rtc0 = &bm8563;
	};
};

&i2c2 {
	bm8563: rtc@51 {
		compatible = "nxp,pcf8563";   /* BM8563 dùng chung bộ thanh ghi với PCF8563 */
		reg = <0x51>;                 /* địa chỉ I2C */
	};
};
```
Giải thích: node con trong `&i2c2 { }` nghĩa là "thiết bị này nằm trên bus I2C2". `reg` của thiết
bị I2C là địa chỉ 7 bit; tên node `rtc@51` theo quy ước `<loại>@<địa chỉ hex>`.

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/rtc
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/rtc/driver_rtc.c
)
```

---

## 4. API (`driver_rtc.h`)

| Hàm | Mô tả |
| --- | --- |
| `int driver_rtc_init(void)` | Kiểm tra chip. Trả `-ENODATA` nếu giờ chưa được đặt (pin hết / lần đầu) |
| `int driver_rtc_set(int year, int month, int day, int hour, int min, int sec)` | Đặt giờ. `year` 2000..2099, `month` 1..12 |
| `int driver_rtc_get(struct rtc_time *t)` | Đọc giờ (`struct rtc_time` của Zephyr) |
| `int driver_rtc_to_string(char *buf, size_t len)` | Chuỗi `"2026-10-06 14:05:09"`, `len` ≥ 20 |

`struct rtc_time` dùng quy ước của C chuẩn: `tm_year` = năm − 1900, `tm_mon` = 0..11. Hàm
`driver_rtc_set` đã đổi giúp, bạn truyền năm/tháng bình thường.

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_rtc.h"

int main(void)
{
	char now[24];
	int ret = driver_rtc_init();

	if (ret == -ENODATA) {
		printk("RTC chua co gio -> dat gio mac dinh\n");
		driver_rtc_set(2026, 10, 6, 8, 0, 0);
	} else if (ret < 0) {
		printk("Loi RTC: %d\n", ret);
		return 0;
	}

	while (1) {
		driver_rtc_to_string(now, sizeof(now));
		printk("%s\n", now);
		k_msleep(1000);
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
RTC chua co gio -> dat gio mac dinh      (chỉ lần đầu)
2026-10-06 08:00:00
2026-10-06 08:00:01
```
Rút nguồn 1 phút, cắm lại → giờ vẫn đúng (nếu có pin).

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Lần nào khởi động cũng `-ENODATA` | Chưa lắp pin / pin hết | Lắp pin CR1220 mới |
| `-ENODEV` | Thiếu node trong overlay hoặc `CONFIG_RTC_PCF8563` | Xem mục 3 |
| `-EIO` | Sai địa chỉ | Chạy `driver_i2c_scan(DRIVER_I2C2)` (driver `i2c`) để xem địa chỉ thật |
| Giờ chạy nhanh/chậm vài giây mỗi ngày | Sai số thạch anh | Bình thường; đồng bộ lại qua mạng |

## 8. Tham khảo
- Datasheet NXP PCF8563 (BM8563 tương thích)
- Zephyr RTC API: https://docs.zephyrproject.org/latest/hardware/peripherals/rtc.html

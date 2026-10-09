# Driver SHT41 – Cảm biến nhiệt độ, độ ẩm có sẵn trên board

> Đọc nhiệt độ (±0.2 °C) và độ ẩm (±1.8 %RH). Cảm biến hàn sẵn trên board (J2), **không cần
> đấu dây**, không cần overlay.

---

## 1. Phần cứng

| Thông số | Giá trị |
| --- | --- |
| Vị trí | J2 trên board, có màng lọc bụi |
| Bus | I2C2: SCL PB10, SDA PB11 |
| Địa chỉ | 0x44 |
| Dải đo | -40..125 °C, 0..100 %RH |

Lưu ý: SHT41 nằm gần MCU và IC nguồn nên có thể đọc **cao hơn nhiệt độ phòng 1–3 °C** khi board
chạy lâu.

## 2. Xung đột tài nguyên
Không có. I2C2 dùng chung với RTC BM8563 (khác địa chỉ, không sao).

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_SHT4X=y
CONFIG_CBPRINTF_FP_SUPPORT=y
```
- `CONFIG_SHT4X`: driver SHT4x của Zephyr.
- `CONFIG_CBPRINTF_FP_SUPPORT`: cho phép `printk("%.2f")`. Thiếu dòng này `printk` in `%f`
  thay vì con số.

### Bước 2 – `app.overlay`
**Không cần** – node `sht41` đã có trong board DTS.

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/sht41
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/sht41/driver_sht41.c
)
```

---

## 4. API (`driver_sht41.h`)

| Hàm | Mô tả |
| --- | --- |
| `int driver_sht41_init(void)` | Kiểm tra cảm biến sẵn sàng |
| `int driver_sht41_read(float *temp_c, float *humidity)` | Đo và trả về °C, %RH (mất ~10 ms) |

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_sht41.h"

int main(void)
{
	int ret = driver_sht41_init();

	if (ret < 0) {
		printk("Loi SHT41: %d\n", ret);
		return 0;
	}

	while (1) {
		float t, h;

		ret = driver_sht41_read(&t, &h);
		if (ret == 0) {
			printk("Nhiet do: %.2f C   Do am: %.1f %%\n", (double)t, (double)h);
		} else {
			printk("Loi doc: %d\n", ret);
		}
		k_msleep(2000);
	}
	return 0;
}
```
`(double)` là bắt buộc khi in `float` bằng `printk` (quy tắc của C khi truyền số thực vào hàm
có số tham số thay đổi).

## 6. Kết quả mong đợi (RTT)
```text
Nhiet do: 28.47 C   Do am: 61.3 %
```
Thở vào cảm biến → độ ẩm tăng rõ rệt trong vài giây.

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| In ra `%.2f` thay vì số | Thiếu `CONFIG_CBPRINTF_FP_SUPPORT=y` | Thêm vào `prj.conf` |
| `-ENODEV` | Thiếu `CONFIG_SHT4X` / `CONFIG_I2C`; hoặc overlay khác đã tắt `sht41`, `i2c2` | Kiểm tra `prj.conf`, tìm `&sht41 { status = "disabled"; }` trong overlay |
| Nhiệt độ cao hơn thực tế | Nhiệt từ board | Bình thường; so với nhiệt kế để hiệu chỉnh |

## 8. Tham khảo
- Datasheet Sensirion SHT4x
- Zephyr Sensor API: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/index.html

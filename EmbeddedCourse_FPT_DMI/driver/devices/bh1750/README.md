
# Driver BH1750 – Cảm biến cường độ ánh sáng (lux)

> Đo độ sáng 1–65535 lux qua I2C1 (U16-9/10), module GY-30 / GY-302.

---

## 1. Phần cứng

| Module GY-30 / GY-302 | Nối đến                                                                |
| --------------------- | ------------------------------------------------------------------------- |
| VCC                   | U16-1 (5 V) – module có ổn áp 3.3 V                                   |
| GND                   | U16-17                                                                    |
| SCL                   | U16-10 (PB6)                                                              |
| SDA                   | U16-9 (PB9)                                                               |
| ADDR                  | Để trống / GND → địa chỉ**0x23**; nối VCC → **0x5C** |

⚠ **[CẦN KIỂM TRA]** điện trở kéo lên SDA/SCL trên module nối lên 3.3 V hay VCC (5 V) – xem mục 5
trong `driver/README.md`.

| Ánh sáng        | Lux (tham khảo) |
| ----------------- | ---------------- |
| Phòng tối       | 1–10            |
| Phòng làm việc | 300–500         |
| Ngoài trời râm | 1000–10000      |
| Nắng trực tiếp | > 30000          |

## 2. Xung đột tài nguyên

| Tài nguyên | Ghi chú                                                                     |
| ------------ | ---------------------------------------------------------------------------- |
| I2C1         | Dùng chung với LCD PCF8574 (0x27/0x3F) – khác địa chỉ nên không sao |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`

```
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_BH1750=y
CONFIG_CBPRINTF_FP_SUPPORT=y
```

### Bước 2 – `app.overlay`

```dts
/ {
	aliases {
		light0 = &bh1750;
	};
};

&i2c1 {
	clock-frequency = <I2C_BITRATE_STANDARD>;

	bh1750: bh1750@23 {
		compatible = "rohm,bh1750";
		reg = <0x23>;            /* 0x5C nếu ADDR nối VCC */
		/* resolution = <1>;      0 = thấp (4 lx, nhanh), 1 = cao (1 lx, mặc định), 2 = cao 2 (0.5 lx) */
	};
};
```

### Bước 3 – `CMakeLists.txt`

```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/bh1750
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/bh1750/driver_bh1750.c
)
```

(Không cần thêm driver `i2c` – driver này dùng thẳng API `sensor` của Zephyr.)

---

## 4. API (`driver_bh1750.h`)

| Hàm                                       | Mô tả                                                |
| ------------------------------------------ | ------------------------------------------------------ |
| `int driver_bh1750_init(void)`           | Kiểm tra cảm biến (alias`light0`)                 |
| `int driver_bh1750_read_lux(float *lux)` | Đo 1 lần (mất ~120–180 ms ở độ phân giải cao) |

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_bh1750.h"

int main(void)
{
	int ret = driver_bh1750_init();

	if (ret < 0) {
		printk("Loi BH1750: %d\n", ret);
		return 0;
	}

	while (1) {
		float lux;

		if (driver_bh1750_read_lux(&lux) == 0) {
			printk("Do sang: %.1f lux %s\n", (double)lux, lux < 50.0f ? "(TOI)" : "");
		}
		k_msleep(500);
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)

```text
Do sang: 312.5 lux
Do sang: 4.2 lux (TOI)          (che tay lên cảm biến)
```

---

## 7. Lỗi thường gặp

| Hiện tượng           | Nguyên nhân                                   | Cách sửa                                   |
| ----------------------- | ----------------------------------------------- | -------------------------------------------- |
| `-ENODEV`             | Sai địa chỉ`reg`; thiếu `CONFIG_BH1750` | Quét I2C1 (driver`i2c`) xem 0x23 hay 0x5C |
| `-EIO`                | Đảo SDA/SCL, dây lỏng                       | Kiểm tra dây                               |
| Luôn 0 lux             | Che kín cảm biến / mặt cảm biến bị keo   | Kiểm tra module                             |
| In`%.1f` thay vì số | Thiếu`CONFIG_CBPRINTF_FP_SUPPORT`            | Thêm vào`prj.conf`                       |

## 8. Tham khảo

- Datasheet ROHM BH1750FVI
- Zephyr binding `rohm,bh1750`: `zephyr/dts/bindings/sensor/rohm,bh1750.yaml`

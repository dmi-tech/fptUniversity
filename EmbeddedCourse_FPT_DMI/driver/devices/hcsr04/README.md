# Driver HC-SR04 – Cảm biến khoảng cách siêu âm

> Đo khoảng cách 2–400 cm. TRIG mặc định PC0 (U16-7), ECHO mặc định PC2 (U16-5).

---

## 1. Phần cứng

### 1.1 Nguyên lý
```text
 TRIG  ─┐ 10 µs ┌──────────────────────────────
        └───────┘
 ECHO  ─────────────┐                ┌─────────
                    └── t (µs) ──────┘
 khoảng cách (cm) = t × 0.0343 / 2   (âm thanh 343 m/s, đi rồi về)
```

### 1.2 Đấu dây – ⚠ ECHO ra **5 V**, phải chia áp

```text
 HC-SR04 VCC  ── U16-1 (5 V)
 HC-SR04 TRIG ── U16-7 (PC0)
 HC-SR04 ECHO ──[ 1 kΩ ]──┬── U16-5 (PC2)
                          │
                       [ 2 kΩ ]        (2 kΩ = 2 điện trở 1 kΩ nối tiếp)
                          │
 HC-SR04 GND  ────────────┴── U16-17 (GND)
```
Điện áp tại PC2: 5 × 2 / (1 + 2) = 3.33 V – an toàn cho MCU.

- TRIG nhận được mức 3.3 V từ MCU (ngưỡng của HC-SR04 ~2 V).
- Loại **HC-SR04P / RCWL-1601** chạy 3–5.5 V: cấp **3.3 V ngoài** thì ECHO cũng 3.3 V, **không
  cần chia áp**.

## 2. Xung đột tài nguyên

| Chân | Dùng chung với | Cách xử lý |
| --- | --- | --- |
| PC0, PC2 | SPI2 (CS, MISO), ADC kênh 10/12 | Tắt `spi2` (đã có trong overlay); không dùng chung với driver `spi` |
| EXTI2 (ngắt của PC2) | Mọi chân số 2 (PA2, PB2...) | PB2 là SDMMC (không dùng ngắt) nên không sao. Không dùng PA2 làm ngõ vào ngắt cùng lúc |

Đổi chân: TRIG dùng **chân bất kỳ**; ECHO dùng chân bất kỳ nhưng tránh trùng số EXTI với ngõ vào
ngắt khác (xem `driver/README.md` mục 3.3).

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_GPIO=y
CONFIG_SENSOR=y
CONFIG_HC_SR04=y
```

### Bước 2 – `app.overlay`
```dts
/ {
	hcsr04: hcsr04 {
		compatible = "hc-sr04";
		trigger-gpios = <&gpioc 0 GPIO_ACTIVE_HIGH>;   /* PC0 – U16-7 */
		echo-gpios = <&gpioc 2 GPIO_ACTIVE_HIGH>;      /* PC2 – U16-5 (qua chia áp) */
	};
	aliases {
		distance0 = &hcsr04;
	};
};

/* PC0 là CS của SPI2 trong board DTS → tắt SPI2 */
&spi2 { status = "disabled"; };
```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/hcsr04
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/hcsr04/driver_hcsr04.c
)
```

---

## 4. API (`driver_hcsr04.h`)

| Hàm | Mô tả |
| --- | --- |
| `int driver_hcsr04_init(void)` | Khởi tạo |
| `int driver_hcsr04_read_mm(uint32_t *mm)` | Đo 1 lần, kết quả mm. Mất tối đa ~60 ms |
| `int driver_hcsr04_read_avg_mm(uint8_t samples, uint32_t *mm)` | Đo `samples` lần (1..10), bỏ giá trị lớn nhất và nhỏ nhất, lấy trung bình |

- Hai lần đo cách nhau **≥ 60 ms** (driver tự chờ nếu gọi quá nhanh) để tiếng vọng cũ tắt hẳn.
- Trả `-ERANGE` nếu ngoài 20..4000 mm (không có vật / vật quá gần), `-EIO` nếu không có xung ECHO.

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_hcsr04.h"

int main(void)
{
	int ret = driver_hcsr04_init();

	if (ret < 0) {
		printk("Loi HC-SR04: %d\n", ret);
		return 0;
	}

	while (1) {
		uint32_t mm;

		ret = driver_hcsr04_read_avg_mm(5, &mm);
		if (ret == 0) {
			printk("Khoang cach: %u.%u cm\n", mm / 10, mm % 10);
		} else if (ret == -ERANGE) {
			printk("Ngoai tam do\n");
		} else {
			printk("Loi doc: %d\n", ret);
		}
		k_msleep(200);
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
Khoang cach: 23.4 cm
Khoang cach: 23.6 cm
Ngoai tam do            (hướng lên trần nhà xa > 4 m)
```

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Luôn `-EIO` | Sai chân TRIG/ECHO; chưa cấp 5 V; ECHO chia áp sai | Đo ECHO bằng đồng hồ: không vật cản ~0 V |
| Luôn `-ERANGE` | Vật quá gần (< 2 cm) hoặc mặt vật nghiêng/mềm (vải) hấp thụ sóng | Thử với tấm bìa cứng vuông góc |
| Giá trị nhảy lung tung | Nhiều cảm biến cùng phát; đo quá nhanh | Dùng `read_avg_mm`; đo cách nhau ≥ 60 ms |
| `-EBUSY` khi init | Chân ECHO trùng số EXTI với ngõ vào ngắt khác | Đổi chân ECHO |

## 8. Tham khảo
- Datasheet HC-SR04
- Zephyr binding `hc-sr04`: `zephyr/dts/bindings/sensor/hc-sr04.yaml`

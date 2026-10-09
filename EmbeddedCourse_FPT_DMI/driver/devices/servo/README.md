# Driver Servo – Động cơ servo RC (SG90, MG90S, MG996R...)

> Quay servo tới góc 0–180° trên PB14 (U16-16). Hỗ trợ 2 servo (thêm PB15 – U16-15).

**Cần thêm driver:** `peripherals/pwm`.

---

## 1. Phần cứng

Servo có 3 dây:

| Dây servo | Màu thường gặp | Nối đến |
| --- | --- | --- |
| Tín hiệu | Cam / vàng / trắng | U16-16 (PB14) |
| + (5 V) | Đỏ | **Nguồn 5 V riêng** (≥ 1 A) |
| − (GND) | Nâu / đen | U16-17 **và** cực âm nguồn 5 V |

```text
 Nguồn 5 V riêng (+) ── đỏ ──┐
                            SERVO ── cam ── U16-16 (PB14)
 Nguồn 5 V riêng (−) ── nâu ─┴───────────── U16-17 (GND)
```

⚠ SG90 hút đỉnh 0.5–0.7 A khi quay, MG996R > 1 A. Lấy từ U16-1 khi board chỉ cắm USB → sụt áp,
board reset. Tín hiệu 3.3 V từ MCU đủ cho hầu hết servo.

Servo nhận xung **mỗi 20 ms (50 Hz)**. **Độ rộng xung** quyết định góc:

```text
 0.5 ms  ┌┐                       → 0°
 1.5 ms  ┌───┐                    → 90°
 2.5 ms  ┌───────┐                → 180°
         |<------- 20 ms ------->|
```

## 2. Xung đột tài nguyên

| Tài nguyên | Dùng chung với | Cách xử lý |
| --- | --- | --- |
| PB14, PB15 (U16-16, 15) | USART1 (board bật sẵn) | Tắt `usart1` |
| TIM12 | – | Dùng riêng cho servo: 50 Hz khác tần số motor (TIM2) |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_PWM=y
```

### Bước 2 – `app.overlay`
```dts
/ {
	servos {
		compatible = "pwm-leds";
		servo_a: servo_0 {
			pwms = <&pwm12 1 PWM_MSEC(20) PWM_POLARITY_NORMAL>;   /* TIM12 kênh 1, 20 ms */
		};
	};
	aliases {
		servo0 = &servo_a;
	};
};

&usart1 { status = "disabled"; };   /* PB14/PB15 là USART1 trong board DTS */

&pinctrl {
	tim12_ch1_pb14: tim12_ch1_pb14 {
		pinmux = <STM32_PINMUX('B', 14, AF2)>;
	};
};

&timers12 {
	st,prescaler = <99>;    /* 250 MHz / 100 = 2.5 MHz → 20 ms = 50000 tick (< 65536), bước 0.4 µs */
	status = "okay";
	pwm12: pwm {
		pinctrl-0 = <&tim12_ch1_pb14>;
		pinctrl-names = "default";
		status = "okay";
	};
};
```

**Thêm servo thứ hai trên PB15 (U16-15):**
1. Trong khối `servos` thêm:
   `servo_b: servo_1 { pwms = <&pwm12 2 PWM_MSEC(20) PWM_POLARITY_NORMAL>; };`
2. Trong `aliases` thêm `servo1 = &servo_b;`
3. Trong `&pinctrl` thêm:
   `tim12_ch2_pb15: tim12_ch2_pb15 { pinmux = <STM32_PINMUX('B', 15, AF2)>; };`
4. Sửa `pinctrl-0 = <&tim12_ch1_pb14 &tim12_ch2_pb15>;`

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/pwm
  ${DRIVER_DIR}/devices/servo
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/pwm/driver_pwm.c
  ${DRIVER_DIR}/devices/servo/driver_servo.c
)
```

---

## 4. API (`driver_servo.h`)

```c
struct driver_servo_cfg {
	uint16_t min_pulse_us;   /* xung ứng với 0°            – mặc định 500  */
	uint16_t max_pulse_us;   /* xung ứng với max_angle     – mặc định 2500 */
	uint16_t max_angle;      /* 180 (SG90); 270 cho servo 270° */
};
#define DRIVER_SERVO_CFG_DEFAULT { .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle = 180 }
```

| Hàm | Mô tả |
| --- | --- |
| `int driver_servo_init(uint8_t id, const struct driver_servo_cfg *cfg)` | `id` = số trong alias `servo<id>`. `cfg = NULL` → mặc định |
| `int driver_servo_set_angle(uint8_t id, uint16_t deg)` | Quay tới góc. `-EINVAL` nếu > `max_angle` |
| `int driver_servo_set_pulse_us(uint8_t id, uint16_t us)` | Đặt độ rộng xung trực tiếp – dùng khi hiệu chỉnh |
| `int driver_servo_sweep(uint8_t id, uint16_t from, uint16_t to, uint32_t ms)` | Quay từ từ (chặn trong `ms`) |
| `int driver_servo_release(uint8_t id)` | Ngừng phát xung – servo thả lỏng, không giữ lực |

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_servo.h"

int main(void)
{
	/* SG90 thực tế thường 500..2400 µs – chỉnh nếu servo kêu rè ở 2 đầu */
	const struct driver_servo_cfg cfg = {
		.min_pulse_us = 500, .max_pulse_us = 2400, .max_angle = 180,
	};
	int ret = driver_servo_init(0, &cfg);

	if (ret < 0) {
		printk("Loi servo: %d\n", ret);
		return 0;
	}

	while (1) {
		for (int deg = 0; deg <= 180; deg += 45) {
			printk("Goc %d\n", deg);
			driver_servo_set_angle(0, deg);
			k_msleep(800);
		}
		printk("Quet cham 180 -> 0\n");
		driver_servo_sweep(0, 180, 0, 2000);
		k_msleep(500);
	}
	return 0;
}
```

## 6. Kết quả mong đợi
Servo dừng ở 0°, 45°, 90°, 135°, 180° rồi quay chậm về 0° trong 2 giây.

---

## 7. Hiệu chỉnh dải xung

1. Gọi `driver_servo_set_pulse_us(0, 500)` – nếu servo rè/rung liên tục, tăng dần (550, 600...)
   đến khi êm → đó là `min_pulse_us`.
2. Làm tương tự với 2500, giảm dần → `max_pulse_us`.
3. Đưa 2 giá trị vào `cfg`.

Đẩy xung ra ngoài dải thật khiến servo tì vào chặn cơ khí, nóng và có thể hỏng bánh răng.

## 8. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Board reset khi servo quay | Lấy nguồn servo từ board | Nguồn 5 V riêng, GND chung |
| Servo giật, rung | Nguồn yếu; thiếu GND chung | Nguồn ≥ 1 A; nối GND |
| Không quay, PB14 vẫn là UART | Quên tắt `usart1` | Thêm `&usart1 { status = "disabled"; };` |
| Góc lệch | Dải xung khác mặc định | Hiệu chỉnh (mục 7) |
| `-EINVAL` khi init | `st,prescaler` sai → 20 ms vượt 65535 tick | Giữ `st,prescaler = <99>` |

## 9. Tham khảo
- README driver `peripherals/pwm`
- Datasheet servo SG90 / MG996R

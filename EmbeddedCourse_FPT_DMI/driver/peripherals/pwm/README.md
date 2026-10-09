# Driver PWM – Điều chế độ rộng xung

> Tạo xung vuông có tần số và độ rộng (duty) tùy chỉnh trên PA2 (U16-4). Dùng để chỉnh độ sáng
> LED, tốc độ motor, góc servo.
> Driver `motor` và `servo` đều dùng driver này bên trong.

---

## 1. Phần cứng

```text
 U16-4 (PA2) ──[ 330 Ω ]──►|── U16-17 (GND)
                          LED
```

| Thông số | Giá trị |
| --- | --- |
| Timer / kênh | TIM2 kênh 3 (TIM2_CH3) |
| Mức điện áp | 0 / 3.3 V |
| Tần số mặc định | 1 kHz |

Khái niệm:

```text
          ┌──────┐            ┌──────┐
 3.3 V    │      │            │      │
          │      │            │      │
 0 V   ───┘      └────────────┘      └──────
          |<pulse>|
          |<------- period -------->|
   duty = pulse / period × 100 %       tần số = 1 / period
```

## 2. Xung đột tài nguyên

| Tài nguyên | Dùng chung với | Cách xử lý |
| --- | --- | --- |
| PA2 (U16-4) | USART2 TX (board bật sẵn), motor | Tắt `usart2`; không dùng chung với `motor` |
| TIM2 | Motor | **Mọi kênh của một timer có chung tần số**. Muốn 2 tần số khác nhau → dùng 2 timer khác nhau |

Chân U16 có PWM (chọn timer **khác** với driver đang dùng):

| U16 | Chân | Timer_Kênh (AF) |
| --- | --- | --- |
| 3 | PA3 | TIM2_CH4 (AF1), TIM15_CH2 (AF4) |
| 4 | PA2 | TIM2_CH3 (AF1), TIM15_CH1 (AF4) |
| 11 | PA9 | TIM1_CH2 (AF1) |
| 12 | PA10 | TIM1_CH3 (AF1) |
| 13, 14 | PC7, PC6 | TIM3_CH2/CH1 (AF2), TIM8_CH2/CH1 (AF3) |
| 15, 16 | PB15, PB14 | TIM12_CH2/CH1 (AF2) – servo |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_PWM=y
```

### Bước 2 – `app.overlay`
```dts
/ {
	pwm_outputs {
		compatible = "pwm-leds";
		pwm_out0: pwm_out_0 {
			pwms = <&pwm2 3 PWM_KHZ(1) PWM_POLARITY_NORMAL>;
		};
	};
	aliases {
		pwm0 = &pwm_out0;
	};
};

/* PA2 là USART2 TX trong board DTS → tắt */
&usart2 { status = "disabled"; };

/* Khai báo chức năng TIM2_CH3 (AF1) cho PA2 */
&pinctrl {
	tim2_ch3_pa2: tim2_ch3_pa2 {
		pinmux = <STM32_PINMUX('A', 2, AF1)>;
	};
};

&timers2 {
	st,prescaler = <0>;                 /* không chia: 250 MHz */
	status = "okay";
	pwm2: pwm {
		pinctrl-0 = <&tim2_ch3_pa2>;
		pinctrl-names = "default";
		status = "okay";
	};
};
```

Đọc dòng `pwms`:
```text
<&pwm2   3   PWM_KHZ(1)   PWM_POLARITY_NORMAL>
   │     │       │              └─ NORMAL: duty 30 % = 30 % thời gian ở mức cao
   │     │       └─ chu kỳ mặc định (đổi được lúc chạy)
   │     └─ số KÊNH – phải khớp với chân (PA2 = TIM2_CH3 → 3)
   └─ timer (TIM2)
```

**Prescaler và tần số:** clock timer 250 MHz. Bộ đếm 16 bit (TIM1, 3, 8, 12, 15) chỉ đếm tới
65535 nên tần số thấp nhất = 250 MHz / (prescaler + 1) / 65536.

| Timer | `st,prescaler` | Clock đếm | Tần số thấp nhất |
| --- | --- | --- | --- |
| TIM2, TIM5 (32 bit) | 0 | 250 MHz | ~0.06 Hz |
| 16 bit | 0 | 250 MHz | ~3.8 kHz |
| 16 bit | 9 | 25 MHz | ~381 Hz |
| 16 bit | 99 | 2.5 MHz | ~38 Hz (dùng cho servo 50 Hz) |

Thêm kênh thứ 2 trên PA3 (cùng TIM2 → **cùng tần số**): thêm `&tim2_ch4_pa3` vào `pinctrl-0`,
khai báo nhãn đó trong `&pinctrl` (`STM32_PINMUX('A', 3, AF1)`), thêm node
`pwm_out1 { pwms = <&pwm2 4 PWM_KHZ(1) PWM_POLARITY_NORMAL>; }` và alias `pwm1`.

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/pwm
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/pwm/driver_pwm.c
)
```

---

## 4. API (`driver_pwm.h`)

| Hàm | Mô tả |
| --- | --- |
| `int driver_pwm_init(uint8_t id)` | Khởi tạo kênh `pwm<id>` (alias), ban đầu tắt (duty 0) |
| `int driver_pwm_set_freq_duty(uint8_t id, uint32_t freq_hz, uint8_t duty_percent)` | Đặt tần số (Hz) và duty (0..100 %) |
| `int driver_pwm_set_pulse(uint8_t id, uint32_t period_ns, uint32_t pulse_ns)` | Đặt chính xác theo nano-giây |
| `int driver_pwm_stop(uint8_t id)` | Tắt xung (chân về 0) |

Dành cho driver khác (motor, servo) – sinh viên thường không cần:
`int driver_pwm_spec_set(const struct pwm_dt_spec *spec, uint32_t period_ns, uint32_t pulse_ns)`.

Trả về `-EINVAL` nếu tần số ngoài khả năng của timer (xem bảng prescaler) hoặc duty > 100.

---

## 5. Ví dụ `src/main.c` – LED "thở"

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_pwm.h"

#define LED_PWM 0   /* alias pwm0 */

int main(void)
{
	int ret = driver_pwm_init(LED_PWM);

	if (ret < 0) {
		printk("Loi PWM: %d\n", ret);
		return 0;
	}

	while (1) {
		for (int d = 0; d <= 100; d += 5) {
			driver_pwm_set_freq_duty(LED_PWM, 1000, d);
			k_msleep(30);
		}
		for (int d = 100; d >= 0; d -= 5) {
			driver_pwm_set_freq_duty(LED_PWM, 1000, d);
			k_msleep(30);
		}
		printk("1 chu ky tho\n");
	}
	return 0;
}
```

## 6. Kết quả mong đợi
LED sáng dần rồi tối dần (~1.2 giây/chu kỳ). Đo bằng máy hiện sóng trên U16-4: xung 1 kHz, duty
thay đổi.

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Build OK nhưng không có xung | Số kênh trong `pwms` không khớp chân (vd ghi 1 cho PA2) | PA2 = kênh 3 |
| `undefined node label 'tim2_ch3_pa2'` | Thiếu khối `&pinctrl` | Thêm khối đó |
| `-EINVAL` khi đặt tần số thấp | Timer 16 bit không đếm đủ | Tăng `st,prescaler` hoặc dùng TIM2/TIM5 |
| Đổi tần số kênh này làm kênh kia đổi theo | Cùng timer | Dùng timer khác |
| Không có xung, PA2 vẫn là UART | Quên tắt `usart2` | Thêm `&usart2 { status = "disabled"; };` |

## 8. Tham khảo
- Zephyr PWM API: https://docs.zephyrproject.org/latest/hardware/peripherals/pwm.html
- Datasheet STM32H573: bảng "Alternate function" (AF0..AF15)

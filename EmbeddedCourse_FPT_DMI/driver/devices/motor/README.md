# Driver Motor – Motor DC: bật/tắt và điều chỉnh tốc độ

> Điều khiển motor DC một chiều quay qua module công suất trên PA2 (U16-4). Hai chế độ, **chọn
> bằng overlay**, không sửa code:
> - **PWM**: bật/tắt + tốc độ 0–100 % + khởi động mềm.
> - **GPIO**: chỉ bật/tắt.

**Cần thêm driver:** `peripherals/pwm` (chỉ ở chế độ PWM).

---

## 1. Phần cứng

Chân MCU chỉ cho dòng vài mA, motor cần hàng trăm mA → **bắt buộc** qua module công suất:
module MOSFET (IRF520, AO3400, D4184), transistor TIP120, hoặc một kênh L298N / L9110.

```text
 Nguồn motor (+) ─────────────┬──────────────┐
  (5–12 V, tách riêng)        │              │
                            Motor DC     diode bảo vệ (module MOSFET thường có sẵn)
                              │              │
                          [Module MOSFET] ───┘
 U16-4 (PA2)  ──────────────► SIG
 U16-17 (GND) ──────────────► GND ─── Nguồn motor (−)    ← GND phải nối chung
```

| Module | Nối đến |
| --- | --- |
| SIG / IN | U16-4 (PA2) |
| GND | U16-17 **và** cực âm nguồn motor |
| VIN / V+ | Nguồn motor (+) |

⚠ **Không** lấy nguồn motor từ U16-1 (5 V) khi board chỉ cắm USB – motor khởi động hút dòng lớn
làm sụt áp, MCU bị reset.

## 2. Xung đột tài nguyên

| Tài nguyên | Dùng chung với | Cách xử lý |
| --- | --- | --- |
| PA2 (U16-4) | USART2 TX (board bật sẵn), PWM chung `pwm0` | Tắt `usart2`; không dùng `pwm0` trên PA2 cùng lúc |
| TIM2 | PWM chung | Nếu đã dán overlay của driver `pwm` (cùng TIM2) thì chỉ thêm khối `motor_pwm` và alias |

---

## 3. Cấu hình – chọn **một** trong hai chế độ

### Chế độ A – PWM (bật/tắt + tốc độ)

**Bước 1 – `prj.conf`**
```
CONFIG_PWM=y
```

**Bước 2 – `app.overlay`**
```dts
/ {
	motor_pwm {
		compatible = "pwm-leds";
		motor: motor_0 {
			pwms = <&pwm2 3 PWM_KHZ(1) PWM_POLARITY_NORMAL>;   /* TIM2 kênh 3, 1 kHz */
		};
	};
	aliases {
		motor0 = &motor;
	};
};

&usart2 { status = "disabled"; };

&pinctrl {
	tim2_ch3_pa2: tim2_ch3_pa2 {
		pinmux = <STM32_PINMUX('A', 2, AF1)>;
	};
};

&timers2 {
	st,prescaler = <0>;
	status = "okay";
	pwm2: pwm {
		pinctrl-0 = <&tim2_ch3_pa2>;
		pinctrl-names = "default";
		status = "okay";
	};
};
```
Tần số PWM: 1 kHz chạy được với hầu hết module. Tăng lên 10–20 kHz (`PWM_KHZ(20)`) thì motor bớt
rít nhưng MOSFET kích bằng 3.3 V có thể nóng. **[CẦN KIỂM TRA]** theo module của bạn.

**Bước 3 – `CMakeLists.txt`**
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/pwm
  ${DRIVER_DIR}/devices/motor
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/pwm/driver_pwm.c
  ${DRIVER_DIR}/devices/motor/driver_motor.c
)
```

### Chế độ B – GPIO (chỉ bật/tắt)

**Bước 1 – `prj.conf`**
```
CONFIG_GPIO=y
```

**Bước 2 – `app.overlay`**
```dts
/ {
	motor_gpio {
		compatible = "gpio-leds";
		motor: motor_0 {
			gpios = <&gpioa 2 GPIO_ACTIVE_HIGH>;
			label = "Motor";
		};
	};
	aliases {
		motor0 = &motor;
	};
};

&usart2 { status = "disabled"; };
```

**Bước 3 – `CMakeLists.txt`**
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/motor
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/motor/driver_motor.c
)
```

Driver tự biết chế độ: node `motor0` có thuộc tính `pwms` → chế độ A; có `gpios` → chế độ B.

---

## 4. API (`driver_motor.h`)

| Hàm | Mô tả | Chế độ B (GPIO) |
| --- | --- | --- |
| `int driver_motor_init(void)` | Khởi tạo, motor **dừng** | ✓ |
| `int driver_motor_on(void)` | Chạy ở tốc độ đã đặt (mặc định 100 %) | ✓ (luôn 100 %) |
| `int driver_motor_off(void)` | Dừng | ✓ |
| `bool driver_motor_is_on(void)` | Đang chạy? | ✓ |
| `int driver_motor_set_speed(uint8_t percent)` | 0..100 %; 0 = dừng | trả `-ENOTSUP` |
| `uint8_t driver_motor_get_speed(void)` | Tốc độ hiện tại | 0 hoặc 100 |
| `int driver_motor_set_min_duty(uint8_t percent)` | Duty nhỏ nhất motor bắt đầu quay (thường 20–30 %). Tốc độ 1 % được đổi thành mức này | trả `-ENOTSUP` |
| `int driver_motor_ramp_to(uint8_t percent, uint32_t ms)` | Tăng/giảm tốc từ từ trong `ms` (chặn) | trả `-ENOTSUP` |

Vùng chết: motor nhỏ thường **không quay** khi duty < 20–30 % (không đủ lực thắng ma sát).
`set_min_duty` giúp thang 1..100 % của bạn luôn làm motor quay.

---

## 5. Ví dụ `src/main.c` (chế độ A)

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_motor.h"

int main(void)
{
	int ret = driver_motor_init();

	if (ret < 0) {
		printk("Loi motor: %d\n", ret);
		return 0;
	}
	driver_motor_set_min_duty(25);

	while (1) {
		printk("Khoi dong mem len 100%%\n");
		driver_motor_ramp_to(100, 2000);
		k_msleep(2000);

		printk("Toc do 40%%\n");
		driver_motor_set_speed(40);
		k_msleep(2000);

		printk("Dung mem\n");
		driver_motor_ramp_to(0, 1000);
		k_msleep(2000);
	}
	return 0;
}
```

Chế độ B: chỉ dùng `driver_motor_on()` / `driver_motor_off()`.

## 6. Kết quả mong đợi
Motor tăng tốc dần trong 2 s, chạy nhanh 2 s, chậm lại 2 s, dừng dần, nghỉ 2 s, lặp lại.

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Board reset khi motor chạy | Lấy nguồn motor từ board | Dùng nguồn riêng, chỉ nối chung GND |
| Motor không quay ở tốc độ thấp | Vùng chết | `driver_motor_set_min_duty(30)` |
| Motor luôn chạy hết tốc | Module là relay / module kích mức thấp | Dùng module MOSFET; hoặc đổi `PWM_POLARITY_INVERTED` |
| `set_speed` trả `-ENOTSUP` | Đang ở chế độ GPIO | Dùng overlay chế độ A |
| Motor không chạy, PA2 vẫn là UART | Quên tắt `usart2` | Thêm `&usart2 { status = "disabled"; };` |

## 8. Tham khảo
- README driver `peripherals/pwm`
- Datasheet module MOSFET/L298N bạn dùng

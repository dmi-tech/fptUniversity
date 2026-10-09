# Driver GPIO – Ngõ ra / ngõ vào số

> **Bài đầu tiên nên học.** Nháy LED LIFE có sẵn trên board (không cần đấu dây), sau đó đọc nút
> nhấn ngoài có chống dội.

---

## 1. Phần cứng

### 1.1 Ngõ ra – LED LIFE (có sẵn, không cần đấu dây)

| Tên | Chân MCU | Mức sáng | Ghi chú |
| --- | --- | --- | --- |
| LED5 "LED LIFE" | PA8 | **Thấp** (active low) | Anode nối +3V3, cathode nối PA8 |

### 1.2 Ngõ vào – nút nhấn ngoài

```text
 U16-12 (PA10) ────┐
                   │  nút nhấn (thường hở)
 U16-17 (GND)  ────┘
```

| Dây | Nối từ | Nối đến |
| --- | --- | --- |
| Chân 1 nút | U16-12 (PA10) | |
| Chân 2 nút | U16-17 (GND) | |

Không cần điện trở: MCU bật **điện trở kéo lên bên trong**. Nhả nút → PA10 = 1; nhấn → PA10 = 0.

### 1.3 Ngõ ra thứ hai – LED ngoài (tùy chọn)

```text
 U16-8 (PE0) ──[ 330 Ω ]──►|── U16-17 (GND)
                         LED (chân dài về phía điện trở)
```

---

## 2. Xung đột tài nguyên

| Chân | Dùng chung với | Ghi chú |
| --- | --- | --- |
| PA10 (U16-12) | Nút encoder (`devices/encoder`) | Không dùng 2 driver cùng lúc trên PA10 |
| PE0 (U16-8) | DHT11 (`devices/dht11`) | Đổi LED sang chân khác nếu dùng DHT11 |
| EXTI10 | Mọi chân số 10 (PB10, PC10...) | PB10 là I2C2 (không dùng ngắt) nên không sao |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_GPIO=y
```
`CONFIG_GPIO` bật driver GPIO của Zephyr (thường đã bật sẵn, thêm cho chắc).

### Bước 2 – `app.overlay`
```dts
/ {
	/* LED ngoài trên PE0 (bỏ khối này nếu chỉ dùng LED LIFE) */
	ec_outputs {
		compatible = "gpio-leds";
		ext_led: ext_led {
			gpios = <&gpioe 0 GPIO_ACTIVE_HIGH>;
			label = "External LED";
		};
	};

	aliases {
		out0 = &user_led;   /* LED LIFE PA8 – đã có trong board DTS */
		out1 = &ext_led;    /* LED ngoài PE0 */
		in0  = &user_btn;   /* Nút PA10 – đã có trong board DTS (pull-up, active low) */
	};
};

&gpioe { status = "okay"; };   /* board chỉ bật gpioa..gpiod */
```

Giải thích:
- Driver tìm phần cứng qua **alias** `out0`..`out3` (ngõ ra) và `in0`..`in3` (ngõ vào). Số trong
  tên alias chính là `id` bạn truyền cho hàm.
- `user_led`, `user_btn` là nhãn đã có trong board DTS, chỉ cần trỏ alias tới.
- `GPIO_ACTIVE_HIGH`: ghi "bật" → chân lên mức 1. LED LIFE khai báo `GPIO_ACTIVE_LOW` trong board
  nên "bật" → chân xuống 0 → LED sáng. Code của bạn **không cần quan tâm** mức điện áp.
- Chỉ dùng LED LIFE: chỉ cần `aliases { out0 = &user_led; };`.

Thêm một nút ngoài khác, ví dụ trên PC0 (U16-7):
```dts
/ {
	ec_inputs {
		compatible = "gpio-keys";
		ext_btn: ext_btn {
			gpios = <&gpioc 0 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
			label = "External button";
		};
	};
	aliases { in1 = &ext_btn; };
};
&spi2 { status = "disabled"; };   /* PC0 là CS của SPI2 trong board DTS */
```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/gpio
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/gpio/driver_gpio.c
)
```

---

## 4. API (`driver_gpio.h`)

| Hàm | Mô tả | Trả về |
| --- | --- | --- |
| `int driver_gpio_out_init(uint8_t id)` | Khởi tạo ngõ ra `out<id>`, ban đầu **tắt** | 0 / `-ENODEV` nếu alias không có |
| `int driver_gpio_set(uint8_t id, bool on)` | Bật / tắt ngõ ra | 0 / lỗi âm |
| `int driver_gpio_toggle(uint8_t id)` | Đảo trạng thái | 0 / lỗi âm |
| `int driver_gpio_in_init(uint8_t id, driver_gpio_in_cb_t cb)` | Khởi tạo ngõ vào `in<id>`. `cb = NULL` → chỉ đọc bằng `driver_gpio_get` | 0 / lỗi âm |
| `int driver_gpio_get(uint8_t id)` | Đọc ngõ vào | 1 = đang tích cực (đang nhấn), 0 = không, < 0 = lỗi |

Kiểu callback:
```c
typedef void (*driver_gpio_in_cb_t)(uint8_t id, bool active);
```
- Gọi khi ngõ vào **đổi trạng thái** và đã ổn định **50 ms** (chống dội).
- Chạy trong **system workqueue** (không phải trong ngắt) nên được phép `printk`, gọi driver khác.
  Không nên chặn lâu (> vài ms).

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_gpio.h"

#define LED_LIFE  0   /* alias out0 */
#define BUTTON    0   /* alias in0  */

static void on_button(uint8_t id, bool pressed)
{
	printk("Nut %u: %s\n", id, pressed ? "NHAN" : "NHA");
	if (pressed) {
		driver_gpio_toggle(1);   /* đảo LED ngoài (out1) mỗi lần nhấn */
	}
}

int main(void)
{
	int ret;

	ret = driver_gpio_out_init(LED_LIFE);
	if (ret < 0) {
		printk("Loi khoi tao LED: %d\n", ret);
		return 0;
	}
	driver_gpio_out_init(1);                 /* LED ngoài – bỏ nếu không có */

	ret = driver_gpio_in_init(BUTTON, on_button);
	if (ret < 0) {
		printk("Loi khoi tao nut: %d\n", ret);
	}

	while (1) {
		driver_gpio_toggle(LED_LIFE);        /* nháy LED LIFE 1 Hz */
		k_msleep(500);
	}
	return 0;
}
```

## 6. Chạy và kết quả mong đợi

```bash
./scripts/build.sh && ./scripts/flash.sh && ./scripts/rtt.sh
```
- LED LIFE nháy 1 lần/giây.
- Nhấn nút: RTT in `Nut 0: NHAN`, nhả in `Nut 0: NHA`; LED ngoài đổi trạng thái.

---

## 7. Đổi chân

Sửa số port/chân trong `gpios = <&gpioX N ...>`. Nếu chân thuộc ngoại vi board đang bật, tắt ngoại
vi đó (bảng mục 3.3 trong `driver/README.md`). Không phải sửa code.

## 8. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| `driver_gpio_out_init` trả `-19` | Chưa khai báo alias `out<id>` | Thêm vào khối `aliases` |
| LED ngoài không sáng | Cắm ngược LED / thiếu `&gpioe { status = "okay"; }` | Đảo LED; thêm dòng bật gpioe |
| Nhấn 1 lần in nhiều lần | Nút dội mạnh hơn 50 ms | Thay nút; hoặc tăng `DRIVER_GPIO_DEBOUNCE_MS` trong `driver_gpio.c` |
| Nút không phản hồi | Nối nút vào 5 V thay vì GND | Nút phải nối xuống GND (U16-17) |
| `-EBUSY` khi init ngõ vào | Chân đang dùng ngắt cho driver khác, hoặc trùng số EXTI | Xem ràng buộc EXTI trong `driver/README.md` |

## 9. Tham khảo
- Zephyr GPIO API: https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html
- Board: `boards/st/stm32h573ri_custom/stm32h573ri_custom.dts` (node `user_led`, `user_btn`)

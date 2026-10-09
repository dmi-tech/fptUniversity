# Driver Encoder – Núm xoay KY-040

> Đọc chiều và số nấc xoay, cùng nút nhấn của núm xoay KY-040. Mặc định A (CLK) = PC6 (U16-14),
> B (DT) = PC7 (U16-13), nút (SW) = PA10 (U16-12).

---

## 1. Phần cứng

| KY-040 | Nối đến |
| --- | --- |
| CLK (A) | U16-14 (PC6) |
| DT (B) | U16-13 (PC7) |
| SW | U16-12 (PA10) |
| + | U16-1 (5 V) hoặc 3.3 V ngoài |
| GND | U16-17 |

⚠ KY-040 có điện trở kéo lên CLK/DT về chân `+`. Cấp 5 V → các chân lên 5 V: xem mục 5 trong
`driver/README.md`. Cấp 3.3 V ngoài là an toàn nhất.

Nguyên lý:
```text
 Xoay phải (thuận):  A ─┐_┌─┐_┌─      B ──┐_┌─┐_┌      A xuống trước B
 Xoay trái (ngược):  A ──┐_┌─┐_┌      B ─┐_┌─┐_┌─      B xuống trước A
```
KY-040 có 20 nấc/vòng, mỗi nấc = 4 lần đổi trạng thái A/B.

## 2. Xung đột tài nguyên

| Chân / tài nguyên | Dùng chung với | Cách xử lý |
| --- | --- | --- |
| PC6, PC7 | USART6 (driver `uart`) | Tắt `usart6` (đã có trong overlay) |
| PA10 | Nút nhấn driver `gpio` (`in0`) | Không dùng đồng thời |
| EXTI6, EXTI7, EXTI10 | Chân số 6, 7, 10 khác dùng ngắt | Tránh dùng ngắt trên PA6, PB7... cùng lúc |
| `CONFIG_INPUT` | Các node `gpio-keys` trong board (nút PA10, ngõ vào CN2 PC13–15) cũng được kích hoạt | Overlay bên dưới gán mã phím cho tất cả – **bắt buộc**, thiếu là lỗi build |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_GPIO=y
CONFIG_INPUT=y
```
`CONFIG_INPUT` bật hệ thống input của Zephyr; driver `gpio-qdec` (encoder) và `gpio-keys` (nút)
tự được bật theo devicetree.

### Bước 2 – `app.overlay`
```dts
#include <zephyr/dt-bindings/input/input-event-codes.h>   /* đặt ở ĐẦU file */

/ {
	encoder: encoder {
		compatible = "gpio-qdec";
		gpios = <&gpioc 6 (GPIO_ACTIVE_HIGH | GPIO_PULL_UP)>,    /* A – PC6 */
			<&gpioc 7 (GPIO_ACTIVE_HIGH | GPIO_PULL_UP)>;    /* B – PC7 */
		steps-per-period = <4>;          /* 4 lần đổi trạng thái = 1 nấc */
		zephyr,axis = <INPUT_REL_WHEEL>; /* loại sự kiện gửi đi */
		sample-time-us = <2000>;         /* lấy mẫu mỗi 2 ms khi đang xoay */
		idle-timeout-ms = <200>;         /* ngừng xoay 200 ms → về chế độ chờ ngắt */
	};
	aliases {
		encoder0 = &encoder;
	};
};

&usart6 { status = "disabled"; };      /* PC6/PC7 là USART6 trong board DTS */

/* Nút SW của KY-040 = node user_btn (PA10) có sẵn – gán mã phím */
&user_btn { zephyr,code = <INPUT_KEY_ENTER>; };

/* Bắt buộc khi CONFIG_INPUT=y: mọi node gpio-keys của board phải có mã phím */
&input_1 { zephyr,code = <INPUT_KEY_1>; };
&input_2 { zephyr,code = <INPUT_KEY_2>; };
&input_3 { zephyr,code = <INPUT_KEY_3>; };
```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/encoder
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/encoder/driver_encoder.c
)
```

---

## 4. API (`driver_encoder.h`)

```c
typedef void (*driver_encoder_cb_t)(int32_t position, int8_t step);   /* step = +1 / -1 */
typedef void (*driver_encoder_btn_cb_t)(bool pressed);
```

| Hàm | Mô tả |
| --- | --- |
| `int driver_encoder_init(void)` | Khởi tạo, vị trí = 0 |
| `int32_t driver_encoder_get_position(void)` | Vị trí hiện tại (tổng số nấc, có dấu) |
| `void driver_encoder_set_position(int32_t pos)` | Đặt lại vị trí |
| `void driver_encoder_set_limits(int32_t min, int32_t max)` | Kẹp vị trí trong [min, max] (vd âm lượng 0..100) |
| `void driver_encoder_set_callback(driver_encoder_cb_t cb)` | Gọi mỗi nấc xoay |
| `void driver_encoder_set_button_callback(driver_encoder_btn_cb_t cb)` | Gọi khi nhấn/nhả nút |
| `bool driver_encoder_button_is_pressed(void)` | Trạng thái nút |

Callback chạy trong **thread input** của Zephyr (không phải ngắt) → được phép `printk`, gọi driver
khác; không chặn lâu.

---

## 5. Ví dụ `src/main.c` – chỉnh giá trị 0..100

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_encoder.h"

static void on_turn(int32_t pos, int8_t step)
{
	printk("%s  gia tri = %d\n", step > 0 ? "-> phai" : "<- trai", pos);
}

static void on_button(bool pressed)
{
	if (pressed) {
		printk("Nhan nut: reset ve 50\n");
		driver_encoder_set_position(50);
	}
}

int main(void)
{
	int ret = driver_encoder_init();

	if (ret < 0) {
		printk("Loi encoder: %d\n", ret);
		return 0;
	}
	driver_encoder_set_limits(0, 100);
	driver_encoder_set_position(50);
	driver_encoder_set_callback(on_turn);
	driver_encoder_set_button_callback(on_button);
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
-> phai  gia tri = 51
-> phai  gia tri = 52
<- trai  gia tri = 51
Nhan nut: reset ve 50
```

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Build lỗi `zephyr-code must be specified` | Thiếu `zephyr,code` cho node `gpio-keys` của board | Thêm 4 dòng `&user_btn`, `&input_1..3` |
| `'INPUT_REL_WHEEL' undeclared` / lỗi DT | Thiếu `#include <zephyr/dt-bindings/input/input-event-codes.h>` | Thêm ở đầu `app.overlay` |
| Xoay ngược chiều mong muốn | Đảo A/B | Đổi thứ tự 2 dòng trong `gpios` |
| 1 nấc nhảy 2 hoặc 4 | `steps-per-period` không khớp encoder | Thử 2 hoặc 1 |
| Bỏ sót khi xoay nhanh | Lấy mẫu chậm | `sample-time-us = <1000>` |
| Nút không hoạt động | Module KY-040 không có trở kéo lên SW | Đã có pull-up nội trong `user_btn`; kiểm tra dây |

## 8. Tham khảo
- Zephyr Input subsystem: https://docs.zephyrproject.org/latest/services/input/index.html
- Zephyr binding `gpio-qdec`: `zephyr/dts/bindings/input/gpio-qdec.yaml`

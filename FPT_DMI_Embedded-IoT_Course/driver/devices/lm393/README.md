# Driver LM393 – Module cảm biến dùng comparator LM393

> Một driver cho cả họ module có chip LM393: cảm biến **ánh sáng** (quang trở), **âm thanh**
> (micro), **độ ẩm đất**, **mưa**, **hồng ngoại tránh vật cản**, **lửa**... DO mặc định PE0
> (U16-8), AO mặc định PC1 (U16-6).

---

## 1. Phần cứng

### 1.1 Module LM393 hoạt động thế nào

```text
 cảm biến ──► điện áp đo ──┬──────────────────────────────► AO (tương tự, 0..VCC)
                           │      ┌────────┐
                           └────► │ LM393  │──► DO (số: 0 hoặc 1)
 biến trở xanh ──► ngưỡng ──────► │ so sánh│    LED "DO" trên module sáng khi DO = 0
                                  └────────┘
```

- **DO**: LM393 so sánh điện áp cảm biến với ngưỡng chỉnh bằng **biến trở trên module**. Vượt
  ngưỡng thì DO đổi mức. Trên hầu hết module, **DO = 0 khi phát hiện** (LED DO sáng), nên overlay
  khai báo `GPIO_ACTIVE_LOW`. Driver báo `detected = true` khi DO tích cực.
- **AO**: điện áp cảm biến trước khi so sánh, đọc bằng ADC để biết **mức** (sáng bao nhiêu, đất
  ẩm bao nhiêu). Module 3 chân (VCC, GND, DO) **không có AO**, chỉ dùng phần DO của driver.

| Module | DO = 0 (phát hiện) khi | AO tăng khi |
| --- | --- | --- |
| Ánh sáng (quang trở) | Tối hơn ngưỡng (một số module ngược lại) | Tối hơn |
| Âm thanh | Có tiếng to (xung rất ngắn) | Tiếng to hơn (dao động, nên đọc trung bình) |
| Độ ẩm đất / mưa | Ẩm, ướt hơn ngưỡng | Khô hơn |
| Hồng ngoại tránh vật cản | Có vật trước mặt | (thường không có AO) |
| Lửa | Có ngọn lửa | Xa lửa hơn |

[CẦN KIỂM TRA] chiều DO/AO trên module thật: đo bằng đồng hồ hoặc nhìn LED DO, rồi chọn
`GPIO_ACTIVE_LOW` hay `GPIO_ACTIVE_HIGH` trong overlay.

### 1.2 Đấu dây

**Cách A (khuyến nghị): cấp 3.3 V từ nguồn ngoài.** Khi đó DO và AO đều không quá 3.3 V và nối
thẳng vào MCU. U16 **không có chân 3.3 V** (dùng module nguồn AMS1117, nối chung GND).

```text
 Module VCC ── 3.3 V (nguồn ngoài)
 Module GND ── U16-17 (GND)  (chung GND với nguồn ngoài)
 Module DO  ── U16-8 (PE0)
 Module AO  ── U16-6 (PC1)
```

**Cách B: cấp 5 V từ U16-1.** DO và AO lên tới **5 V**:

```text
 Module VCC ── U16-1 (5 V)
 Module GND ── U16-17 (GND)
 Module DO  ── U16-8 (PE0)          [CẦN KIỂM TRA] PE0 chịu 5 V (FT) trong datasheet STM32H573
 Module AO  ──[ 10 kΩ ]──┬── U16-6 (PC1)
                         │
                      [ 20 kΩ ]       AO tối đa 5 × 20 / 30 = 3.33 V
                         │
 U16-17 (GND) ───────────┘
```

⚠ **Không bao giờ nối thẳng AO 5 V vào chân ADC.** Với chia áp, điện áp AO thật = số đọc × 1.5.

### 1.3 Chỉnh ngưỡng

Vặn biến trở trên module đến khi LED DO **vừa tắt** ở điều kiện bình thường. Khi có tác động
(che sáng, vỗ tay, tưới nước, đưa tay lại gần), LED DO sáng và driver gọi callback.

---

## 2. Xung đột tài nguyên

| Chân / ngoại vi | Dùng chung với | Cách xử lý |
| --- | --- | --- |
| PE0 (U16-8) | DHT11, LED ngoài của `gpio` | Đổi một trong hai sang chân khác. Phải **bật** `gpioe` |
| PC1 (U16-6) | SPI2 MOSI (board bật sẵn) | Tắt `spi2` (đã có trong overlay) |
| EXTI0 (ngắt của PE0) | Mọi chân số 0 (PA0, PC0...) | PA0 là RS485 (UART, không dùng ngắt) nên không sao. Không dùng PC0 làm ngõ vào ngắt cùng lúc |
| `zephyr,user` `io-channels` | Driver `adc` (đọc **kênh đầu tiên**) | Dùng chung một node, xem mục 3 "Dùng cùng driver `adc`" |

Chân U16 có ADC: PA3 (kênh 15), PA2 (14), PC2 (12), PC1 (11), PC0 (10). **Số kênh phải đúng với
chân**.

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_GPIO=y
CONFIG_ADC=y
```
- `CONFIG_GPIO`: đọc DO.
- `CONFIG_ADC`: đọc AO. Bỏ dòng này nếu chỉ dùng DO.

### Bước 2 – `app.overlay`
```dts
/ {
	zephyr,user {
		/* DO module 0: PE0 – U16-8. DO = 0 khi phát hiện → ACTIVE_LOW */
		lm393-gpios = <&gpioe 0 GPIO_ACTIVE_LOW>;
		/* AO module 0: kênh 11 = PC1 – U16-6 */
		io-channels = <&adc1 11>;
		io-channel-names = "lm393_0";
	};
};

&gpioe { status = "okay"; };   /* board chỉ bật gpioa..gpiod */

/* PC1 là SPI2 MOSI trong board DTS → tắt SPI2 */
&spi2 { status = "disabled"; };

/* Board chưa khai báo chức năng analog của PC1 → tự khai báo */
&pinctrl {
	adc1_inp11_pc1: adc1_inp11_pc1 {
		pinmux = <STM32_PINMUX('C', 1, ANALOG)>;
	};
};

&adc1 {
	clocks = <&rcc STM32_CLOCK(AHB2, 10)>,
		 <&rcc STM32_SRC_HCLK ADCDAC_SEL(0)>;
	clock-names = "adcx", "adc_ker";
	pinctrl-0 = <&adc1_inp11_pc1>;
	pinctrl-names = "default";
	st,adc-clock-source = "ASYNC";
	st,adc-prescaler = <6>;
	#address-cells = <1>;
	#size-cells = <0>;
	status = "okay";

	channel@b {                           /* b (hex) = 11 */
		reg = <11>;
		zephyr,gain = "ADC_GAIN_1";
		zephyr,reference = "ADC_REF_INTERNAL";
		zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
		zephyr,resolution = <12>;
	};
};
```

Giải thích:
- `lm393-gpios`: mỗi phần tử là DO của một module. Phần tử thứ `id` (đếm từ 0) là module `id`
  bạn truyền cho hàm.
- `io-channel-names = "lm393_0"`: AO của module 0. Driver tìm AO **theo tên**, không theo thứ tự.
- **Chỉ dùng DO** (module 3 chân): chỉ giữ `lm393-gpios` trong `zephyr,user`, dòng `&gpioe`, bỏ
  `io-channels`, `io-channel-names`, `&spi2`, `&pinctrl`, `&adc1` và `CONFIG_ADC`.

**Hai module**, ví dụ thêm module 1 với DO trên PA9 (U16-11), AO trên PC0 (U16-7, kênh 10):
```dts
/ {
	zephyr,user {
		lm393-gpios = <&gpioe 0 GPIO_ACTIVE_LOW>, <&gpioa 9 GPIO_ACTIVE_LOW>;
		io-channels = <&adc1 11>, <&adc1 10>;
		io-channel-names = "lm393_0", "lm393_1";
	};
};
```
Trong `&pinctrl` thêm nhãn `adc1_inp10_pc0` (`'C', 0`), trong `&adc1` đổi thành
`pinctrl-0 = <&adc1_inp11_pc1 &adc1_inp10_pc0>;` và thêm khối `channel@a { reg = <10>; ... }`.
Không chọn DO trên PC0 khi DO kia ở PE0: cùng số chân 0, chung ngắt EXTI0.

**Dùng cùng driver `adc`** (biến trở PA3): driver `adc` đọc kênh **đầu tiên**, nên đặt kênh của
nó lên trước và gộp vào **một** node `zephyr,user`, **một** khối `&adc1` (hai nhãn trong
`pinctrl-0`, hai khối `channel@`):
```dts
		io-channels = <&adc1 15>, <&adc1 11>;
		io-channel-names = "adc", "lm393_0";
```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/lm393
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/lm393/driver_lm393.c
)
```

---

## 4. API (`driver_lm393.h`)

| Hàm | Mô tả | Trả về |
| --- | --- | --- |
| `int driver_lm393_init(uint8_t id, driver_lm393_cb_t cb)` | Khởi tạo DO của module `id`. `cb = NULL` → chỉ đọc bằng `driver_lm393_get` | 0 / lỗi âm |
| `int driver_lm393_get(uint8_t id)` | Đọc DO ngay lúc gọi | 1 = phát hiện, 0 = không, < 0 = lỗi |
| `int driver_lm393_set_hold(uint8_t id, uint32_t ms)` | Thời gian giữ (1..60000 ms, mặc định 100) | 0 / `-EINVAL` |
| `int driver_lm393_read_raw(uint8_t id, int16_t *raw)` | AO thô 0..4095 | 0 / lỗi âm |
| `int driver_lm393_read_mv(uint8_t id, int32_t *mv)` | Điện áp chân AO, mV | 0 / lỗi âm |
| `int driver_lm393_read_avg_mv(uint8_t id, uint8_t samples, int32_t *mv)` | Trung bình `samples` lần (1..64), mV | 0 / lỗi âm |
| `int driver_lm393_read_percent(uint8_t id, uint8_t *percent)` | AO theo % thang đo, 0..100 | 0 / lỗi âm |

Kiểu callback:
```c
typedef void (*driver_lm393_cb_t)(uint8_t id, bool detected);
```
- `detected = true` được báo **ngay** khi DO tích cực, kể cả xung chỉ vài µs (tiếng vỗ tay).
- `detected = false` chỉ được báo khi DO **không tích cực liên tục trong thời gian giữ**
  (`set_hold`, mặc định 100 ms). Gần ngưỡng, LM393 không có trễ nên DO "rung" rất nhiều lần;
  thời gian giữ biến chuỗi rung đó thành **một** lần phát hiện.
- Chạy trong **system workqueue** (không phải trong ngắt) nên được phép `printk`, gọi driver khác.

Công thức: `mV = raw × 3300 / 4095`, `% = raw × 100 / 4095`. AO trả `-ENODEV` nếu không khai báo
kênh tên `lm393_<id>`.

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_lm393.h"

#define SENSOR 0   /* phần tử 0 của lm393-gpios, kênh tên "lm393_0" */

static void on_detect(uint8_t id, bool detected)
{
	printk("Module %u: %s\n", id, detected ? "PHAT HIEN" : "binh thuong");
}

int main(void)
{
	int ret;

	ret = driver_lm393_init(SENSOR, on_detect);
	if (ret < 0) {
		printk("Loi khoi tao LM393: %d\n", ret);
		return 0;
	}
	driver_lm393_set_hold(SENSOR, 200);      /* giữ trạng thái phát hiện ít nhất 200 ms */

	while (1) {
		int32_t mv;
		uint8_t pct;

		ret = driver_lm393_read_avg_mv(SENSOR, 8, &mv);
		if (ret == 0 && driver_lm393_read_percent(SENSOR, &pct) == 0) {
			printk("AO: %d mV (%u%%), DO: %d\n", mv, pct, driver_lm393_get(SENSOR));
		} else {
			printk("Loi doc AO: %d\n", ret);
		}
		k_msleep(1000);
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
AO: 1240 mV (38%), DO: 0
Module 0: PHAT HIEN          (che quang trở / vỗ tay / đưa tay lại gần)
AO: 2875 mV (87%), DO: 1
Module 0: binh thuong        (sau khi hết tác động ít nhất 200 ms)
```
Module chỉ có DO: dòng `AO` in `Loi doc AO: -19`, phần phát hiện vẫn chạy bình thường.

---

## 7. Đổi chân

- **DO**: chân bất kỳ, sửa `<&gpioX N ...>` trong `lm393-gpios`. Tránh trùng số EXTI với ngõ vào
  ngắt khác (`driver/README.md` mục 3.3). Chân thuộc ngoại vi board đang bật thì tắt ngoại vi đó.
- **AO**: chỉ các chân có ADC (mục 2). Đổi cả 4 chỗ: số kênh trong `io-channels`, nhãn `&pinctrl`
  (`'C', 1` → port, chân), `pinctrl-0` và `channel@<hex>` / `reg`.

Không phải sửa code.

## 8. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| `driver_lm393_init` trả `-19` | Thiếu `lm393-gpios` trong `zephyr,user`, hoặc thiếu `&gpioe { status = "okay"; }` | Thêm vào overlay |
| Đọc AO trả `-19` | Thiếu `io-channel-names = "lm393_0"`, thiếu `CONFIG_ADC=y`, hoặc `&adc1` chưa `okay` | Kiểm tra overlay và `prj.conf` |
| Callback không bao giờ được gọi, LED DO trên module có đổi | Sai chân DO | Đo PE0 bằng đồng hồ khi LED DO sáng/tắt |
| LED DO không bao giờ đổi | Ngưỡng chưa chỉnh | Vặn biến trở trên module (mục 1.3) |
| Báo ngược (bình thường lại in PHAT HIEN) | Module này DO = 1 khi phát hiện | Đổi `GPIO_ACTIVE_LOW` → `GPIO_ACTIVE_HIGH` |
| Phát hiện/bình thường xen kẽ liên tục | Điều kiện đo nằm sát ngưỡng | Tăng `driver_lm393_set_hold`, hoặc chỉnh lại ngưỡng |
| AO luôn ~3300 mV / 100% | AO 5 V nối thẳng (nguy hiểm!) hoặc module ở trạng thái cực đại | Thêm chia áp (mục 1.2) |
| AO nhảy nhiều (module âm thanh) | Micro cho tín hiệu dao động | Dùng `read_avg_mv`; với âm thanh nên dùng DO |
| `-EBUSY` khi init | Chân DO trùng số EXTI với ngõ vào ngắt khác | Đổi chân DO |

## 9. Tham khảo
- Datasheet LM393 (TI): https://www.ti.com/product/LM393
- Zephyr ADC API: https://docs.zephyrproject.org/latest/hardware/peripherals/adc.html
- `zephyr,user`: https://docs.zephyrproject.org/latest/build/dts/zephyr-user-node.html

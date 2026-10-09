# Driver ADC – Đọc điện áp analog

> Đọc điện áp 0–3.3 V từ biến trở trên PA3 (U16-3), cùng nhiệt độ chip và điện áp nguồn VDDA đo
> bằng kênh nội. Phần kênh nội học được **không cần linh kiện ngoài**.

---

## 1. Phần cứng

### 1.1 Biến trở 10 kΩ (tùy chọn)

U16 **không có chân 3.3 V**, nên cần một nguồn 3.3 V ngoài (module nguồn AMS1117, hoặc chân 3V3
của board khác) **nối chung GND**:

```text
 3.3 V (nguồn ngoài) ──┐
                       ├─ biến trở 10 kΩ ─── chân giữa ──► U16-3 (PA3)
 U16-17 (GND)       ───┘
```

Nếu chỉ có 5 V (U16-1), dùng thêm điện trở để điện áp chân giữa không quá 3.3 V:

```text
 U16-1 (5 V) ──[ 5.1 kΩ ]──┐
                           ├─ biến trở 10 kΩ ─── chân giữa ──► U16-3 (PA3)
 U16-17 (GND) ─────────────┘           (tối đa 5 × 10 / 15.1 ≈ 3.31 V)
```

⚠ **Không bao giờ đưa quá 3.3 V vào chân ADC.**

### 1.2 Kênh nội (không cần đấu dây)

| Kênh | Đo gì | Kênh ADC1 |
| --- | --- | --- |
| Nhiệt độ chip | Nhiệt độ lõi MCU (không phải nhiệt độ phòng, thường cao hơn 5–15 °C) | 16 |
| VREFINT | Điện áp tham chiếu nội, từ đó tính ra VDDA thật | 17 |

## 2. Xung đột tài nguyên

| Chân / ngoại vi | Dùng chung với | Cách xử lý |
| --- | --- | --- |
| PA3 (U16-3) | USART2 RX (board bật sẵn) | Tắt `usart2` (đã có trong overlay bên dưới) |
| ADC1 | – | Một driver ADC dùng chung được cho nhiều kênh |

Chân U16 có ADC: PA3 (kênh 15), PA2 (14), PC2 (12), PC1 (11), PC0 (10). **Số kênh phải đúng với
chân**, ví dụ PA3 luôn là kênh 15.

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_ADC=y
CONFIG_SENSOR=y
```
- `CONFIG_ADC`: driver ADC của Zephyr.
- `CONFIG_SENSOR`: nhiệt độ chip và VREFINT được Zephyr cung cấp dưới dạng "sensor".

### Bước 2 – `app.overlay`
```dts
/ {
	zephyr,user {
		io-channels = <&adc1 15>;        /* kênh 15 = PA3 */
	};
};

/* PA3 là USART2 RX trong board DTS → tắt USART2 */
&usart2 { status = "disabled"; };

/* Board chưa khai báo chức năng analog của PA3 → tự khai báo */
&pinctrl {
	adc1_inp15_pa3: adc1_inp15_pa3 {
		pinmux = <STM32_PINMUX('A', 3, ANALOG)>;
	};
};

&adc1 {
	clocks = <&rcc STM32_CLOCK(AHB2, 10)>,
		 <&rcc STM32_SRC_HCLK ADCDAC_SEL(0)>;
	clock-names = "adcx", "adc_ker";
	pinctrl-0 = <&adc1_inp15_pa3>;
	pinctrl-names = "default";
	st,adc-clock-source = "ASYNC";
	st,adc-prescaler = <6>;
	#address-cells = <1>;
	#size-cells = <0>;
	status = "okay";

	channel@f {                           /* f (hex) = 15 */
		reg = <15>;
		zephyr,gain = "ADC_GAIN_1";
		zephyr,reference = "ADC_REF_INTERNAL";
		zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
		zephyr,resolution = <12>;
	};
};

&die_temp { status = "okay"; };   /* nhiệt độ chip – kênh 16 */
&vref     { status = "okay"; };   /* VREFINT – kênh 17 */
```

Giải thích từng khối:
- `zephyr,user { io-channels }` – nói cho driver biết **kênh nào** cần đọc. Driver đọc kênh đầu
  tiên trong danh sách.
- `&pinctrl` – chuyển PA3 sang chế độ **ANALOG**.
- `&adc1` – bật ADC1, chọn clock và khai báo kênh 15: hệ số khuếch đại 1, tham chiếu 3.3 V,
  độ phân giải 12 bit (0..4095).
- Chỉ dùng kênh nội (không có biến trở): giữ `&adc1 { ... }` nhưng xóa `pinctrl-0`, `pinctrl-names`,
  `channel@f`, khối `zephyr,user`, khối `&pinctrl` và dòng `&usart2`.

Dùng chân khác, ví dụ PC0 (kênh 10): đổi `15` → `10`, `channel@f` → `channel@a`,
`'A', 3` → `'C', 0`, đổi tên nhãn thành `adc1_inp10_pc0`, và thay `&usart2` bằng
`&spi2 { status = "disabled"; };`.

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/adc
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/adc/driver_adc.c
)
```

---

## 4. API (`driver_adc.h`)

| Hàm | Mô tả | Đơn vị |
| --- | --- | --- |
| `int driver_adc_init(void)` | Khởi tạo kênh ngoài (nếu có) và kênh nội | – |
| `int driver_adc_read_raw(int16_t *raw)` | Giá trị thô 0..4095 | – |
| `int driver_adc_read_mv(int32_t *mv)` | Điện áp chân | mV |
| `int driver_adc_read_avg_mv(uint8_t samples, int32_t *mv)` | Trung bình `samples` lần đọc (1..64) | mV |
| `int driver_adc_read_die_temp(int32_t *milli_c)` | Nhiệt độ chip | m°C (25000 = 25.000 °C) |
| `int driver_adc_read_vdda(int32_t *mv)` | Điện áp nguồn analog thật | mV |

Công thức: `mV = raw × 3300 / 4095`.

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_adc.h"

int main(void)
{
	int ret = driver_adc_init();

	if (ret < 0) {
		printk("Loi ADC: %d\n", ret);
		return 0;
	}

	while (1) {
		int16_t raw;
		int32_t mv, temp_mc, vdda;

		driver_adc_read_raw(&raw);
		driver_adc_read_avg_mv(16, &mv);
		driver_adc_read_die_temp(&temp_mc);
		driver_adc_read_vdda(&vdda);

		printk("PA3: raw=%4d  %4d mV | chip: %d.%01d C | VDDA: %d mV\n",
		       raw, mv, temp_mc / 1000, (temp_mc % 1000) / 100, vdda);
		k_msleep(500);
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
PA3: raw=2047  1650 mV | chip: 38.2 C | VDDA: 3301 mV
PA3: raw=4095  3300 mV | chip: 38.3 C | VDDA: 3299 mV    (vặn biến trở hết cỡ)
```

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| `undefined node label 'adc1_inp15_pa3'` | Thiếu khối `&pinctrl` | Thêm khối đó |
| Giá trị nhảy lung tung khi không nối gì | Chân để hở thu nhiễu – bình thường | Nối biến trở; dùng `read_avg_mv` |
| Luôn đọc ~0 hoặc ~4095 | Biến trở đấu sai (chân giữa phải vào PA3) | Kiểm tra lại dây |
| Giá trị đúng nhưng lệch vài chục mV | VDDA thực khác 3300 mV | So với `driver_adc_read_vdda()` để hiệu chỉnh |
| `-ENODEV` khi đọc nhiệt độ | Thiếu `&die_temp`/`&vref` hoặc `CONFIG_SENSOR` | Xem mục 3 |

## 8. Tham khảo
- Zephyr ADC API: https://docs.zephyrproject.org/latest/hardware/peripherals/adc.html
- Datasheet STM32H573: bảng "Pin definitions" (cột ADC) và thông số "Temperature sensor"

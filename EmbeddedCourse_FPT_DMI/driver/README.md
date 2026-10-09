# Thư viện driver – Hướng dẫn chung

Đọc file này **trước** khi dùng bất kỳ driver nào. README của từng driver chỉ ghi phần riêng của
driver đó. Các khái niệm chung (3 file cấu hình, cách gộp overlay, cách build/nạp/xem log, lỗi
thường gặp) nằm ở đây.

---

## 1. Driver nằm ở đâu, chia thành mấy tầng?

```text
driver/
├── peripherals/   Ngoại vi BÊN TRONG chip STM32: adc, pwm, gpio, watchdog, timer
├── protocols/     Bus giao tiếp: i2c, spi, uart, rs485, can
├── devices/       Thiết bị (cảm biến, màn hình, motor...): dùng peripherals/ và protocols/
└── services/      Dịch vụ mạng: mqtt (dùng devices/ethernet)
```

Quy tắc phụ thuộc chỉ đi **một chiều**:

```text
services  ──►  devices  ──►  peripherals, protocols
```

Ví dụ: `devices/lcd_pcf8574` dùng `protocols/i2c`, nên khi thêm LCD vào project bạn phải thêm
**cả hai** driver. README của mỗi driver luôn ghi rõ phải thêm những driver nào.

### Danh sách driver

| Driver | Thư mục | Cần thêm driver | Phần cứng | Mức độ |
| --- | --- | --- | --- | --- |
| GPIO vào/ra | `peripherals/gpio` | – | LED LIFE trên board, nút ngoài | ★ Bắt đầu ở đây |
| Timer | `peripherals/timer` | – | Không cần | ★ |
| Watchdog | `peripherals/watchdog` | – | Không cần | ★ |
| ADC | `peripherals/adc` | – | Biến trở (hoặc chỉ dùng cảm biến nhiệt trong chip) | ★ |
| PWM | `peripherals/pwm` | – | LED ngoài / máy hiện sóng | ★★ |
| SHT41 | `devices/sht41` | – | Có sẵn trên board | ★ |
| RTC | `devices/rtc` | – | Có sẵn trên board (pin CR1220) | ★★ |
| DHT11 | `devices/dht11` | – | Module DHT11 | ★ |
| HC-SR04 | `devices/hcsr04` | – | Module HC-SR04 + 2 điện trở | ★★ |
| BH1750 | `devices/bh1750` | – | Module GY-30/GY-302 | ★★ |
| Encoder | `devices/encoder` | – | Module KY-040 | ★★ |
| LCD I2C | `devices/lcd_pcf8574` | `i2c` | LCD 16x2/20x4 + module PCF8574 | ★★ |
| Motor DC | `devices/motor` | `pwm` (chế độ PWM) | Motor + module MOSFET | ★★ |
| Servo | `devices/servo` | `pwm` | Servo SG90 + nguồn 5 V riêng | ★★ |
| I2C | `protocols/i2c` | – | Module I2C bất kỳ | ★★ |
| UART | `protocols/uart` | – | USB-UART 3.3 V | ★★ |
| SPI | `protocols/spi` | – | 1 sợi dây (loopback) | ★★★ |
| RS485 | `protocols/rs485` | `uart` | USB-RS485 + nguồn 24 V | ★★★ |
| CAN | `protocols/can` | – | Không cần (loopback) / board thứ 2 + 24 V | ★★★ |
| Ethernet | `devices/ethernet` | – | Cáp mạng | ★★ |
| MQTT | `services/mqtt` | `ethernet` | Cáp mạng + máy tính chạy broker | ★★★ |

---

## 2. Ba file bạn sẽ sửa

Mỗi khi thêm một driver, bạn sửa **tối đa 3 file** ở thư mục gốc project. Đây là 3 "trụ cột" của
việc build trong Zephyr:

| File | Trụ cột | Trả lời câu hỏi | Ví dụ |
| --- | --- | --- | --- |
| `prj.conf` | **Kconfig** | Bật **phần mềm** nào của Zephyr? | `CONFIG_I2C=y` – biên dịch driver I2C của Zephyr |
| `app.overlay` | **Devicetree** | **Phần cứng** nối ở đâu, chân nào? | `&i2c1 { ... };` – I2C1 dùng chân PB6/PB9 |
| `CMakeLists.txt` | **CMake** | Biên dịch **file .c** nào của mình? | `driver/protocols/i2c/driver_i2c.c` |

Thiếu một trong ba thì sẽ gặp lỗi khác nhau (xem mục 6).

### 2.1 `prj.conf`
- Mỗi dòng có dạng `CONFIG_<TÊN>=y` (bật) hoặc `=n` (tắt), hoặc `=<số>` / `="chuỗi"`.
- Dán các dòng trong README của driver vào **cuối file**.
- Một dòng xuất hiện **2 lần là không sao** (ví dụ cả LCD và BH1750 đều cần `CONFIG_I2C=y`).
  Tuy vậy nên xóa bớt cho gọn.
- Dòng bắt đầu bằng `#` là chú thích.

### 2.2 `app.overlay`
"Overlay" là phần **bổ sung/ghi đè** lên mô tả phần cứng gốc của board
(`boards/st/stm32h573ri_custom/stm32h573ri_custom.dts`). **Không bao giờ sửa file board**; mọi
thay đổi đặt trong `app.overlay`.

Các mẫu cú pháp bạn sẽ gặp:

```dts
/* 1. Sửa một node đã có, tham chiếu bằng nhãn (label) có dấu & */
&i2c1 {
	clock-frequency = <I2C_BITRATE_STANDARD>;
};

/* 2. Tắt một ngoại vi để lấy lại chân của nó */
&usart2 { status = "disabled"; };

/* 3. Thêm node mới ở gốc cây (/) và đặt bí danh (alias) cho driver tìm */
/ {
	dht11: dht11 {
		compatible = "aosong,dht";
		dio-gpios = <&gpioe 0 (GPIO_ACTIVE_LOW | GPIO_OPEN_DRAIN | GPIO_PULL_UP)>;
	};
	aliases {
		dht0 = &dht11;
	};
};

/* 4. Khai báo chức năng thay thế (AF) của chân – xem mục 2.4 */
&pinctrl {
	tim2_ch3_pa2: tim2_ch3_pa2 { pinmux = <STM32_PINMUX('A', 2, AF1)>; };
};
```

Đọc một dòng `gpios`:

```text
<&gpioe   0   (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>
   │      │    └─ cờ: mức tích cực, điện trở kéo, open-drain...
   │      └────── số chân trong port (PE0 → 0)
   └───────────── port (PE0 → gpioe)
```

### 2.3 `CMakeLists.txt`
Thêm **2 loại dòng** cho mỗi driver, vào 2 khối có sẵn chữ `TODO (Sinh viên)`:

```cmake
target_include_directories(app PRIVATE
  src
  ${DRIVER_DIR}/protocols/i2c          # thư mục chứa driver_i2c.h
  ${DRIVER_DIR}/devices/lcd_pcf8574    # thư mục chứa driver_lcd.h
)

target_sources(app PRIVATE
  src/main.c
  ${DRIVER_DIR}/protocols/i2c/driver_i2c.c
  ${DRIVER_DIR}/devices/lcd_pcf8574/driver_lcd.c
)
```

- `target_include_directories` → để `#include "driver_lcd.h"` tìm được file.
- `target_sources` → để file `.c` được biên dịch và liên kết.
- Thêm driver phụ thuộc **trước**, driver dùng nó **sau** (dễ đọc, không bắt buộc về kỹ thuật).

### 2.4 Vì sao overlay có khối `&pinctrl`?

Mỗi chân STM32 có nhiều chức năng thay thế (AF – Alternate Function). Ví dụ PA2 có thể là
USART2_TX (AF7), TIM2_CH3 (AF1) hoặc ADC (ANALOG).

File pinctrl của board này (`stm32h573ri_custom-pinctrl.dtsi`) **chỉ khai báo các chân board đang
dùng** (UART, I2C, SPI, CAN, SDMMC). Các chức năng khác như ADC, PWM, SPI2 MISO **chưa có nhãn**,
nên README của driver sẽ yêu cầu bạn tự thêm, ví dụ:

```dts
&pinctrl {
	adc1_inp15_pa3: adc1_inp15_pa3 { pinmux = <STM32_PINMUX('A', 3, ANALOG)>; };
};
```

Số AF lấy từ bảng "Alternate function" trong datasheet STM32H573, hoặc từ file
`modules/hal/stm32/dts/st/h5/stm32h573rivx-pinctrl.dtsi`. Bảng đầy đủ cho các chân U16 nằm trong
`docs/pinout.md`.

Nếu quên khối này, build báo: `devicetree error: ... undefined node label 'adc1_inp15_pa3'`.

---

## 3. Dùng nhiều driver cùng lúc – cách gộp

### 3.1 Gộp `app.overlay`
Mỗi README cho một đoạn overlay riêng. Khi dùng nhiều driver, **dán tất cả vào cùng một file
`app.overlay`**, với các lưu ý:

| Trường hợp | Cách gộp |
| --- | --- |
| Hai driver cùng có khối `/ { aliases { ... }; };` | Được phép để riêng nhiều khối `/ { }`. Devicetree tự gộp |
| Hai driver cùng có `&pinctrl { ... }` | Được phép để riêng. **Nhưng** cùng một nhãn (vd `tim2_ch3_pa2`) chỉ được khai báo **một lần** |
| Hai driver cùng sửa `&timers2 { ... }` | Chỉ giữ **một** khối, gộp nội dung |
| Hai driver cùng có `zephyr,user { ... }` | Gộp các thuộc tính vào **một** node `zephyr,user` |
| Hai driver dùng **cùng một chân** | **Không được**. Xem mục 3.3 |
| Có dòng `#include <...>` | Đặt ở **đầu** file `app.overlay` |

### 3.2 Gộp `prj.conf`, `CMakeLists.txt`
Chỉ việc nối thêm. Dòng trùng trong `prj.conf` không gây lỗi. Trong `CMakeLists.txt` không thêm
cùng một file `.c` hai lần (sẽ lỗi `multiple definition`).

### 3.3 Xung đột chân

Header U16 chỉ có 14 chân tín hiệu nên nhiều driver mặc định dùng chung chân. Bảng dưới đây là
chân **mặc định** trong README; mọi driver dùng GPIO đều đổi chân được (xem mục "Đổi chân" trong
README của driver).

| U16 | Chân | Mặc định dùng cho | Ngoại vi board bật sẵn phải tắt |
| --- | --- | --- | --- |
| 3 | PA3 | ADC (biến trở) | `usart2` |
| 4 | PA2 | PWM chung / motor | `usart2` |
| 5 | PC2 | SPI2 MISO / HC-SR04 ECHO | – (SPI2 MISO chưa khai báo trong board) |
| 6 | PC1 | SPI2 MOSI | – |
| 7 | PC0 | SPI2 CS / HC-SR04 TRIG | `spi2` (nếu dùng cho HC-SR04) |
| 8 | PE0 | DHT11 / LED ngoài (gpio) | – (phải **bật** `gpioe`) |
| 9, 10 | PB9, PB6 | I2C1: LCD, BH1750 | – (dùng chung bus được, khác địa chỉ) |
| 11 | PA9 | SPI2 SCK | – |
| 12 | PA10 | Nút nhấn (gpio) / nút encoder | – |
| 13, 14 | PC7, PC6 | UART (USART6) / encoder A, B | `usart6` (nếu dùng cho encoder) |
| 15, 16 | PB15, PB14 | Servo 2, servo 1 | `usart1` |
| 19, 20 | PB13, PB12 | **Console MCUboot – không dùng** | – |

**Ràng buộc ngắt (EXTI):** STM32 chỉ có **một** đường ngắt cho mỗi số chân, dùng chung cho mọi
port (PA10, PB10, PC10 cùng dùng EXTI10). Các driver dùng ngắt: `gpio` (ngõ vào), `hcsr04` (ECHO),
`encoder` (A, B, nút), `ethernet` (PC4). Hai chân **cùng số** không thể cùng bật ngắt, ví dụ không
dùng PA6 làm ECHO khi encoder đang ở PC6.

---

## 4. Build, nạp, xem log

```bash
cd ~/zephyr-boards/fw_showcase-workspace/fptUniversity/EmbeddedCourse_FPT_DMI
./scripts/build.sh          # build MCUboot + app, ký ảnh
./scripts/flash.sh          # nạp qua ST-LINK (mặc định) hoặc: ./scripts/flash.sh jlink
./scripts/rtt.sh            # xem printk qua SWD (Ctrl+C để thoát)
```

- Log của **ứng dụng** (`printk`) đi qua **RTT** trên dây SWD (J3), không cần cáp UART.
- Log của **MCUboot** đi qua UART5 (U16-19 TX, U16-20 RX, 115200 8N1).
- Sau khi sửa `app.overlay` hoặc `prj.conf`, `build.sh` luôn build lại từ đầu, không cần xóa
  thư mục `build/`.

---

## 5. Mức điện áp – đọc kỹ trước khi đấu dây

- MCU chạy **3.3 V**. U16 chỉ có chân **5 V** (U16-1, U16-2) và GND (U16-17, U16-18), **không
  có 3.3 V**.
- Module cấp 5 V thường kéo chân tín hiệu lên 5 V. **[CẦN KIỂM TRA]** khả năng chịu 5 V (FT) của
  từng chân trong datasheet STM32H573. Khi chưa chắc: cấp 3.3 V cho module từ nguồn ngoài (nối
  chung GND), hoặc dùng mạch chuyển mức / chia áp.
- Chân dùng cho **ADC tuyệt đối không quá 3.3 V**.
- **Không** lấy nguồn motor, servo từ U16 khi board chỉ cắm USB.
- **Không** nối GND (logic) với GND-ISO (CN1–CN5) nếu không thật sự cần.

---

## 6. Lỗi thường gặp (mọi driver)

| Thông báo / hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| `fatal error: driver_xxx.h: No such file or directory` | Thiếu dòng `target_include_directories` | Thêm thư mục driver vào `CMakeLists.txt` |
| `undefined reference to 'driver_xxx_init'` | Thiếu dòng `target_sources` | Thêm file `.c` vào `CMakeLists.txt` |
| `undefined reference to 'i2c_...'` / `'pwm_...'` / `'__device_dts_ord_..'` | Thiếu `CONFIG_...=y` hoặc node đang `disabled` | Kiểm tra `prj.conf` và `status = "okay"` trong overlay |
| `devicetree error: ... undefined node label 'xxx'` | Dùng nhãn chưa khai báo | Thêm khối `&pinctrl` (mục 2.4) hoặc kiểm tra chính tả |
| `devicetree error: ... parse error` | Sai cú pháp overlay | Thiếu `;` sau `}` hoặc sau thuộc tính; thiếu `<` `>` |
| `'DT_N_ALIAS_xxx' undeclared` | Chưa khai báo alias driver cần | Thêm khối `aliases { ... }` trong overlay |
| `warning: ... was assigned the value 'y' but got the value 'n'` | Kconfig phụ thuộc chưa thỏa (thường do node DT chưa `okay`) | Bật node trong overlay trước |
| Build OK, chạy in `-19` (`-ENODEV`) | Thiết bị chưa sẵn sàng: sai địa chỉ I2C, chưa cấp nguồn, sai chân | Kiểm tra dây, nguồn, địa chỉ |
| Chạy in `-5` (`-EIO`) | Thiết bị không trả lời trên bus | Kiểm tra SDA/SCL, điện trở kéo lên, địa chỉ |
| Chạy in `-22` (`-EINVAL`) | Tham số sai (ngoài phạm vi) | Đọc bảng API trong README |
| `rtt.sh` không thấy log | Chưa nạp, sai mạch nạp, hoặc thiếu `CONFIG_RTT_CONSOLE=y` | Kiểm tra `prj.conf` gốc của project |

Bảng mã lỗi đầy đủ: `zephyr/lib/libc/minimal/include/errno.h` (giá trị âm của các hằng `E...`).

---

## 7. Lộ trình học gợi ý

1. `gpio` → nháy LED LIFE, đọc nút nhấn.
2. `timer` → làm việc định kỳ không dùng `k_msleep`.
3. `watchdog` → tự khởi động lại khi treo.
4. `adc`, `pwm` → analog và xung.
5. `sht41`, `rtc`, `dht11`, `hcsr04`, `bh1750`, `encoder` → cảm biến.
6. `i2c` + `lcd_pcf8574`, `motor`, `servo` → ghép module.
7. `uart`, `spi`, `rs485`, `can` → truyền thông.
8. `ethernet`, `mqtt` → mạng và IoT.

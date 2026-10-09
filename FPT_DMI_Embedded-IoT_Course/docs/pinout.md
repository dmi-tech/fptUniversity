
# Bảng chân – header U16, cổng CN1–CN5 và ngoại vi trên board

Tài liệu tra cứu chân của board **Base Board STMH5** (STM32H573RI) cho môn học. Đọc cùng
[`driver/README.md`](../driver/README.md) (mục 3.3 "Xung đột chân") trước khi đổi chân trong
`app.overlay`.

Quy ước: **[Mặc định]** = chức năng board DTS đang bật sẵn. **[Gợi ý]** = cấu hình đề xuất
trong README của driver. **[CẦN KIỂM TRA]** = điểm chưa xác nhận trên phần cứng.

---

## 1. Header U16 (2×10, 2.54 mm, vùng GND logic)

```text
        +------+------+
   +5V  |  1   |  2   |  +5V
   PA3  |  3   |  4   |  PA2
   PC2  |  5   |  6   |  PC1
   PC0  |  7   |  8   |  PE0
   PB9  |  9   |  10  |  PB6
   PA9  |  11  |  12  |  PA10
   PC7  |  13  |  14  |  PC6
   PB15 |  15  |  16  |  PB14
   GND  |  17  |  18  |  GND
   PB13 |  19  |  20  |  PB12
        +------+------+
```

U16 **không có chân 3.3 V**. Có 14 chân tín hiệu, 2 chân 5 V, 2 chân GND và 2 chân console MCUboot.

### 1.1 Chức năng của từng chân

Chỉ liệt kê chức năng dùng được trên board này. Mọi chân đều dùng được làm GPIO.

| U16    | Chân | [Mặc định] trong board DTS                     | UART                | SPI       | I2C      | Timer / PWM                   | ADC                |
| ------ | ----- | ------------------------------------------------- | ------------------- | --------- | -------- | ----------------------------- | ------------------ |
| 1, 2   | +5V   | Nguồn 5 V                                        |                     |           |          |                               |                    |
| 3      | PA3   | USART2 RX                                         | USART2_RX           |           |          | TIM2_CH4, TIM5_CH4, TIM15_CH2 | **kênh 15** |
| 4      | PA2   | USART2 TX                                         | USART2_TX           |           |          | TIM2_CH3, TIM5_CH3, TIM15_CH1 | kênh 14           |
| 5      | PC2   | (chưa cấu hình)                                |                     | SPI2_MISO |          | TIM4_CH4, TIM17_CH1           | kênh 12           |
| 6      | PC1   | SPI2 MOSI                                         |                     | SPI2_MOSI |          |                               | kênh 11           |
| 7      | PC0   | SPI2 CS (GPIO, active high)                       |                     |           |          | TIM16_BKIN                    | kênh 10           |
| 8      | PE0   | (chưa cấu hình,**`gpioe` chưa bật**) |                     |           |          | LPTIM2_CH2                    |                    |
| 9      | PB9   | I2C1 SDA                                          | UART4_TX            |           | I2C1_SDA | TIM4_CH4, TIM17_CH1           |                    |
| 10     | PB6   | I2C1 SCL                                          | USART1_TX, UART5_TX |           | I2C1_SCL | TIM4_CH1                      |                    |
| 11     | PA9   | SPI2 SCK                                          | USART1_TX           | SPI2_SCK  |          | TIM1_CH2                      |                    |
| 12     | PA10  | Nút nhấn`user_btn` (pull-up)                  | USART1_RX           |           |          | TIM1_CH3                      |                    |
| 13     | PC7   | USART6 RX                                         | USART6_RX           |           |          | TIM3_CH2, TIM8_CH2            |                    |
| 14     | PC6   | USART6 TX                                         | USART6_TX           |           |          | TIM3_CH1, TIM8_CH1            |                    |
| 15     | PB15  | USART1 RX                                         | USART1_RX           | SPI2_MOSI |          | TIM12_CH2                     |                    |
| 16     | PB14  | USART1 TX                                         | USART1_TX           | SPI2_MISO |          | TIM12_CH1                     |                    |
| 17, 18 | GND   | Đất logic                                       |                     |           |          |                               |                    |
| 19     | PB13  | UART5 TX –**console MCUboot**              | UART5_TX            |           |          |                               |                    |
| 20     | PB12  | UART5 RX –**console MCUboot**              | UART5_RX            |           | I2C2_SDA |                               |                    |

### 1.2 Chân của từng driver

| U16    | Chân      | Chức năng trong các driver             | Driver                               | Ngoại vi board phải tắt          |
| ------ | ---------- | ----------------------------------------- | ------------------------------------ | ----------------------------------- |
| 3      | PA3        | ADC1_INP15 (biến trở)                   | `adc`                              | `usart2`                          |
| 4      | PA2        | PWM TIM2_CH3 / motor                      | `pwm`, `motor`                   | `usart2`                          |
| 5      | PC2        | SPI2_MISO / HC-SR04 ECHO                  | `spi` / `hcsr04`                 | –                                  |
| 6      | PC1        | SPI2_MOSI                                 | `spi`                              | –                                  |
| 7      | PC0        | SPI2 CS / HC-SR04 TRIG                    | `spi` / `hcsr04`                 | `spi2` (nếu dùng cho HC-SR04)   |
| 8      | PE0        | DHT11 DATA / LED ngoài                   | `dht11`, `gpio`                  | – (phải**bật** `gpioe`)  |
| 9, 10  | PB9, PB6   | I2C1: LCD, BH1750, module I2C             | `i2c`, `lcd_pcf8574`, `bh1750` | –                                  |
| 11     | PA9        | SPI2_SCK                                  | `spi`                              | –                                  |
| 12     | PA10       | Nút nhấn (gpio) / nút encoder          | `gpio`, `encoder`                | –                                  |
| 13, 14 | PC7, PC6   | USART6 RX/TX / encoder B, A               | `uart` / `encoder`               | `usart6` (nếu dùng cho encoder) |
| 15, 16 | PB15, PB14 | Servo 2, servo 1 (TIM12)                  | `servo`                            | `usart1`                          |
| 19, 20 | PB13, PB12 | **Console MCUboot – không dùng** | –                                   | –                                  |

### 1.3 Xung đột trên U16

| Tài nguyên       | Dùng bởi                          | Không dùng đồng thời với                                                                        |
| ------------------ | ----------------------------------- | ----------------------------------------------------------------------------------------------------- |
| PA2, PA3           | motor / PWM (PA2), ADC (PA3)        | `usart2` (board bật sẵn → phải `status = "disabled"`)                                         |
| PC0, PC1, PC2, PA9 | `spi`                             | `hcsr04` (PC0, PC2), ADC kênh 10/11/12                                                             |
| PC6, PC7           | `uart` (USART6)                   | `encoder`                                                                                           |
| PB14, PB15         | `servo`                           | `usart1` (phải tắt)                                                                               |
| PA10               | `gpio` (nút), `encoder` (nút) | Không dùng hai driver cùng lúc trên PA10                                                         |
| PE0                | `dht11`, `gpio` (LED ngoài)    | Đổi một trong hai sang chân khác                                                                 |
| TIM2 / TIM12       | motor (TIM2), servo (TIM12)         | Mọi kênh của**một** timer chung tần số: đừng đặt servo (50 Hz) cùng timer với motor |
| PB6, PB9           | I2C1                                | Dùng làm UART/GPIO cho việc khác                                                                  |

### 1.4 Cảnh báo điện

- Module cấp 5 V thường kéo chân tín hiệu lên 5 V. **[CẦN KIỂM TRA]** khả năng chịu 5 V (FT) của
  từng chân trong datasheet STM32H573. Khi chưa chắc, cấp 3.3 V cho module từ nguồn ngoài (chung
  GND) hoặc dùng chia áp / mạch chuyển mức (HC-SR04 ECHO dùng chia áp 1 kΩ / 2 kΩ).
- Chân dùng cho **ADC tuyệt đối không quá 3.3 V**. Vì U16 không có 3.3 V, chia áp từ 5 V
  (ví dụ 5.1 kΩ nối tiếp biến trở 10 kΩ, tối đa ≈ 3.31 V) hoặc dùng nguồn 3.3 V ngoài.
- Không lấy nguồn motor, servo từ U16 khi board chỉ cắm USB.
- Không nối GND logic (U16) với GND-ISO (CN1–CN5) nếu không thật sự cần.

---

## 2. Cổng CN1–CN5 (terminal 3.81 mm, vùng GND-ISO)

Đi qua mạch cách ly, chức năng cố định theo phần cứng, không chọn AF được. Phía cách ly chỉ có
nguồn khi cấp **24 V** (jack DC1 hoặc CN4); chỉ cắm USB thì CN1–CN5 không hoạt động.

### CN1 – RS485 (ISO3088)

| Chân | Tín hiệu | Phía MCU                                                    | Ghi chú                                          |
| ----- | ---------- | ------------------------------------------------------------ | ------------------------------------------------- |
| 1     | RS485A     | UART4_TX PA0 → D                                            | Trở đầu cuối 120 Ω (R21) có sẵn            |
| 2     | RS485B     | UART4_RX PA1 ← R                                            |                                                   |
| 3     | +24V       | –                                                           | Không nối vào bộ USB-RS485                    |
| 4     | GND-ISO    | –                                                           |                                                   |
| –    | DE/RE      | **PC3** khi lắp R88; mạch 555 tự đảo khi lắp R87 | **[CẦN KIỂM TRA]** board lắp R87 hay R88 |

`uart4` 9600 8N1 đã bật trong board DTS. Driver: `rs485` (dùng `uart`).

### CN2 – Ngõ vào số cách ly 

| Chân | Tín hiệu    | Phía MCU         |
| ----- | ------------- | ----------------- |
| 1, 2  | +24V, GND-ISO | –                |
| 3, 4  | IN1, COM1     | PC13 (active low) |
| 5, 6  | IN2, COM2     | PC14 (active low) |
| 7, 8  | IN3, COM3     | PC15 (active low) |

Node `input_1..3` (alias `input1..3`) có sẵn. Đọc bằng `GPIO_DT_SPEC_GET(DT_ALIAS(input1), gpios)`.

### CN3 – Ngõ ra relay 

| Chân | Tín hiệu | Phía MCU         |
| ----- | ---------- | ----------------- |
| 1, 2  | COM1, OUT1 | PB5 (active high) |
| 3, 4  | COM2, OUT2 | PB4               |
| 5, 6  | COM3, OUT3 | PA15              |

Node `relay_1..3` (alias `relay1..3`). Dùng `gpio_pin_set_dt()`. Nhãn net trên schematic bị
ngược; board DTS đi theo dây thực tế.

### CN4 – Nguồn 24 V

| Chân | Tín hiệu | Ghi chú                                                                |
| ----- | ---------- | ----------------------------------------------------------------------- |
| 1     | +24V       | Song song jack DC1 nhưng**không qua diode chống ngược cực** |
| 2     | GND-ISO    |                                                                         |

### CN5 – CAN FD (ISO7731 → TCAN1057)

| Chân | Tín hiệu   | Phía MCU                                                   |
| ----- | ------------ | ----------------------------------------------------------- |
| 1     | CANH         | FDCAN1_TX PB7                                               |
| 2     | CANL         | FDCAN1_RX PB8                                               |
| 3     | GND-ISO      |                                                             |
| 4     | +24V         |                                                             |
| –    | S (standby)  | PB0, do Zephyr điều khiển –**không đụng tới** |
| –    | LED6 TX / RX | PD11 / PD12, active low                                     |

`fdcan1` 500 kbit/s đã cấu hình. Board có sẵn trở đầu cuối 120 Ω (R77 + R78). Chế độ loopback
của driver `can` không cần 24 V.

---

## 3. Ngoại vi trên board

| Khối                   | Chân / bus                     | [Mặc định]                                        | Driver                                      |
| ----------------------- | ------------------------------- | ---------------------------------------------------- | ------------------------------------------- |
| SHT41                   | I2C2 (PB10/PB11) @0x44          | node`sht41` bật                                   | `sht41`                                   |
| RTC BM8563 + pin CR1220 | I2C2 @0x51                      | **chưa có node** (README `rtc` thêm vào) | `rtc`                                     |
| W5500 Ethernet          | SPI1 PA4–PA7, INT PC4, RST PC5 | bật                                                 | `ethernet`, `mqtt`                      |
| LED5 "LED LIFE"         | PA8 (active low)                | `user_led`                                         | `gpio`                                    |
| LED6 TX/RX của CAN     | PD11 / PD12                     | –                                                   | `can`                                     |
| Còi, microSD, USB-C    | PB1 / SDMMC1 / PA11–PA12       | có phần cứng                                      | **không có driver trong môn học** |
| DAC                     | PA4 / PA5                       | tắt,**W5500 chiếm**                          | không dùng                                |

---

## 4. Ràng buộc chân

### 4.1 Ngoại vi chỉ chạy trên đúng chân

| Ngoại vi         | Chân dùng được                                                                        | Điều phải khớp                                                                                                                                                                                                  |
| ----------------- | ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SPI2              | SCK PA9, MISO PC2, MOSI PC1                                                                | AF cố định. CS là GPIO bất kỳ (`cs-gpios`)                                                                                                                                                                  |
| I2C1, I2C2        | PB6/PB9, PB10/PB11                                                                         | SCL/SDA không đổi chéo                                                                                                                                                                                          |
| FDCAN1, UART4     | PB7/PB8, PA0/PA1                                                                           | Board đi dây cố định                                                                                                                                                                                           |
| **ADC**     | PA3 (kênh 15), PA2 (14), PC2 (12), PC1 (11), PC0 (10)                                     | **Số kênh phải đúng với chân**: `channel@f` / `reg = <15>` chỉ dành cho PA3. Điện áp ≤ 3.3 V                                                                                                 |
| **PWM**     | Chân có TIMx_CHy (bảng 1.1)                                                             | (1) Số kênh: PA2 = TIM2_CH3 →`pwms = <&pwm2 3 ...>`; sai kênh vẫn build nhưng không có xung. (2) Kênh bù CHxN cần cờ `STM32_PWM_COMPLEMENTARY`. (3) Các kênh cùng timer **chung tần số** |
| UART (USART1/2/6) | PB14/PB15, PA2/PA3, PC6/PC7                                                                | TX/RX cố định theo AF                                                                                                                                                                                            |
| Encoder           | Hai pha A, B nên cùng một timer nếu dùng timer phần cứng (PC6/PC7: TIM3 hoặc TIM8) | Driver`encoder` dùng GPIO + ngắt nên không bị ràng buộc này                                                                                                                                               |

### 4.2 Ngoại vi dùng GPIO bất kỳ

DHT11, nút nhấn, LED, motor chế độ GPIO, DE của RS485, CS của SPI, TRIG của HC-SR04. Chỉ cần chân còn trống; nếu ngoại vi của chân đang bật trong board DTS thì tắt nó
trong overlay (cột "Ngoại vi board phải tắt" ở mục 1.2); port chưa bật (vd `gpioe`) phải
`status = "okay"`.

**Ràng buộc ngắt (EXTI):** mỗi *số chân* chỉ có **một** đường ngắt chung cho mọi port (PA10,
PB10, PC10 cùng dùng EXTI10). Hai chân cùng số không thể cùng bật ngắt. Driver dùng ngắt:
`gpio` (ngõ vào), `hcsr04` (ECHO), `encoder` (A, B, nút), `ethernet` (INT PC4).

### 4.3 Chân không được cấu hình lại

| Chân                                 | Lý do                                                                   |
| ------------------------------------- | ------------------------------------------------------------------------ |
| PA13 (SWDIO), PA14 (SWCLK), PB3 (SWO) | Mất khả năng nạp và đọc log RTT                                   |
| PB12, PB13 (U16-19/20)                | Console của MCUboot                                                     |
| PC14, PC15                            | Ngõ vào IN2/IN3 (CN2); MCU không có LSE → dùng RTC BM8563 on-board |
| PH0, PH1                              | Thạch anh 25 MHz                                                        |
| PA4–PA7, PC4, PC5                    | W5500 (dây đã nối vào chip, kể cả khi không dùng Ethernet)      |
| BOOT0, NRST                           | Chân hệ thống                                                         |

---

## 5. Nhãn pinctrl: có sẵn và phải tự khai báo

Board dùng file pinctrl riêng (`stm32h573ri_custom-pinctrl.dtsi`), **không** include pinctrl của
hal_stm32. Chỉ có các nhãn sau; chức năng khác phải tự khai báo trong khối `&pinctrl` của
`app.overlay`.

**Có sẵn:** `usart1_tx_pb14 usart1_rx_pb15 usart2_tx_pa2 usart2_rx_pa3 uart5_tx_pb13 uart5_rx_pb12 fdcan1_tx_pb7 fdcan1_rx_pb8 i2c1_scl_pb6 i2c1_sda_pb9 i2c2_scl_pb10 i2c2_sda_pb11 uart4_tx_pa0 uart4_rx_pa1 spi1_sck_pa5 spi1_miso_pa6 spi1_mosi_pa7 spi2_sck_pa9 spi2_mosi_pc1 sdmmc1_d0_pc8 sdmmc1_d1_pc9 sdmmc1_d2_pc10 sdmmc1_d3_pc11 sdmmc1_ck_pc12 sdmmc1_cmd_pb2 usart6_tx_pc6 usart6_rx_pc7`

**Tự khai báo (đã dùng trong README các driver):**

| Nhãn              | `pinmux`                                         | Dùng cho                     |
| ------------------ | -------------------------------------------------- | ----------------------------- |
| `adc1_inp15_pa3` | `STM32_PINMUX('A', 3, ANALOG)`                   | `adc`                       |
| `tim2_ch3_pa2`   | `STM32_PINMUX('A', 2, AF1)`                      | `pwm`, `motor`            |
| `tim2_ch4_pa3`   | `STM32_PINMUX('A', 3, AF1)`                      | kênh PWM thứ hai trên TIM2 |
| `tim12_ch1_pb14` | `STM32_PINMUX('B', 14, AF2)`                     | `servo` 0                   |
| `tim12_ch2_pb15` | `STM32_PINMUX('B', 15, AF2)`                     | `servo` 1                   |
| `spi2_miso_pc2`  | `STM32_PINMUX('C', 2, AF5)` + `bias-pull-down` | `spi`                       |

Chân PWM khác (số AF theo datasheet, bảng "Alternate function"): PA9 TIM1_CH2 AF1, PA10 TIM1_CH3
AF1, PC6/PC7 TIM3_CH1/CH2 AF2 hoặc TIM8_CH1/CH2 AF3, PA2/PA3 TIM15_CH1/CH2 AF4. Tra thêm trong
`modules/hal/stm32/dts/st/h5/stm32h573rivx-pinctrl.dtsi`.

---

## 6. Tổ hợp không xung đột cho bài lab tổng hợp

| Bài                 | Driver                                                               | Chân U16 dùng                    |
| -------------------- | -------------------------------------------------------------------- | ---------------------------------- |
| Trạm môi trường  | `sht41`, `rtc`, `lcd_pcf8574`, `bh1750`                      | 1, 9, 10, 17                       |
| Điều khiển motor  | `adc` (biến trở → tốc độ), `motor`, `lcd_pcf8574`        | 1, 3, 4, 9, 10, 17                 |
| IoT                  | `sht41`, `dht11`, `ethernet`, `mqtt`                         | 1, 8, 17                           |
| Truyền thông       | `uart`, `rs485`, `can` (loopback), `spi` (loopback)          | 5, 6, 7, 11, 13, 14, 17            |
| Cơ cấu chấp hành | `adc`, `motor` (PWM), `servo`, `lcd_pcf8574`                 | 1, 3, 4, 9, 10, 16, 17             |
| Đo khoảng cách    | `hcsr04`, `encoder` (đặt ngưỡng), `lcd_pcf8574`, `servo` | 1, 5, 7, 9, 10, 12, 13, 14, 16, 17 |

Cấu hình đầy đủ cho tất cả driver không chung chân (`dht11`, `hcsr04`, `encoder`, `motor`,
`servo`, `bh1750`, `rtc`, `ethernet`) đã được build thử trong `tests/build_all/configs/devices.overlay`.

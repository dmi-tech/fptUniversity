# Lộ trình bài lab (Day 1 – Day 8)

Mỗi buổi có một hoặc nhiều bài lab. Mỗi lab dùng các driver trong [`driver/`](../driver); README của
driver ghi đủ phần cứng, cấu hình, API và bài tập. File này cho biết **mỗi lab làm gì, dùng driver
nào, nghiệm thu thế nào**. Ví dụ tham khảo nằm ở [`docs/examples/`](examples) và đều được build
thử bằng `tests/lab_examples/run.sh`.

Quy trình chung của mọi lab:

1. Đọc README của các driver liên quan (mục 1–4).
2. Dán cấu hình vào 3 file: `prj.conf`, `app.overlay`, `CMakeLists.txt` (xem
   [`driver/README.md`](../driver/README.md) mục 2).
3. Viết `src/main.c` (hoặc chép ví dụ), rồi `./scripts/build.sh`, `./scripts/flash.sh`,
   `./scripts/rtt.sh`.
4. Đối chiếu với tiêu chí nghiệm thu của lab.

Chân dùng chung giữa các lab: xem [`pinout.md`](pinout.md). **Tháo dây của lab trước** nếu hai lab
chung chân.

## Bảng tổng hợp

| Buổi | Lab | Tên bài | Loại | Driver chính | Ví dụ |
| --- | --- | --- | --- | --- | --- |
| Day 1 | 1 | First Zephyr Application | Bắt buộc | `gpio` + Zephyr Shell | [`lab1_shell.c`](examples/lab1_shell.c) |
| Day 2 | 2A | Button → Relay | Bắt buộc | `gpio` | [`lab2a_button_relay.c`](examples/lab2a_button_relay.c) |
| | 2B | Sensor → Buzzer | Bắt buộc | `adc`, `gpio` | [`lab2b_sensor_buzzer.c`](examples/lab2b_sensor_buzzer.c) |
| | 2C | Điều khiển AUTO/MANUAL | Mở rộng, về nhà | `gpio` (+ 2A, 2B) | tự viết |
| Day 3 | 3A | CAN → Relay (Heartbeat & Timeout) | Bắt buộc | `can`, `gpio` | [`lab3a_can_relay.c`](examples/lab3a_can_relay.c) |
| | 3B | RS485 Echo / Modbus RTU | Chọn 1 trong 2 | `rs485` hoặc Modbus của Zephyr | xem mục Lab 3B |
| | 3C | Servo SG90 PWM | Chọn 1 trong 2 | `servo`, `pwm` | README `servo` |
| Day 4 | 4 | Local HMI | Bắt buộc | `lcd_pcf8574`, `sht41`, `gpio` | xem mục Lab 4 |
| Day 5 | 5 | MQTT IoT Device over Ethernet | Bắt buộc | `ethernet`, `mqtt` | README `mqtt` |
| Day 6 | 6A / 6B | MQTT over Wi-Fi / 4G | – | **Tạm thời chưa có** | – |
| Day 7 | 7 | MCUboot Lifecycle & Rollback | Chỉ nhóm OTA | sysbuild + MCUboot | [`lab7_mcuboot.c`](examples/lab7_mcuboot.c) |
| Day 8 | – | Showcase & Demo | – | – | Không có bài mới |

Bài nền để học từng driver riêng lẻ (không bắt buộc) nằm ở [phụ lục](#phụ-lục--bài-nền-theo-driver).

---

## Day 1 – Lab 1: First Zephyr Application

**Công nghệ:** west, CMake/Ninja, GPIO LED, Shell, log, 1 thread.

**Việc cần làm:** clone manifest, `west update` (`./scripts/setup.sh`), build, nạp, nháy LED LIFE,
điều khiển LED bằng lệnh shell, in boot log có phiên bản và build ID.

**Cấu hình** (ví dụ [`lab1_shell.c`](examples/lab1_shell.c)):

`prj.conf` – thêm vào cuối:
```
CONFIG_GPIO=y
CONFIG_SHELL=y
CONFIG_SHELL_BACKEND_RTT=y
CONFIG_LOG=y
CONFIG_LOG_PRINTK=y
```
`app.overlay`:
```dts
/ { aliases { out0 = &user_led; }; };
```
`CMakeLists.txt`: thêm `${DRIVER_DIR}/peripherals/gpio` vào include và `driver_gpio.c` vào sources.

- Shell chạy **trên kênh RTT**, không dùng UART (UART5 dành cho MCUboot). Log `printk` cũng đi qua
  shell nhờ `CONFIG_LOG_PRINTK`.
- Muốn **gõ lệnh**, kết nối hai chiều tới cổng RTT: với ST-LINK, mở `openocd -f scripts/rtt_stlink.cfg`
  rồi `nc localhost 19021` (`scripts/rtt.sh` dùng đúng cổng này; **chưa thử gõ lệnh trên board thật**); với J-Link dùng
  `JLinkRTTClient`. Thử `led on`, `led off`, `led toggle`, `help`.
- File [`VERSION`](../VERSION) ở thư mục gốc quyết định `APP_VERSION_STRING`. Sửa file này để đổi phiên
  bản hiện trong boot log (và cũng là phiên bản MCUboot dùng ở Lab 7).

**Nghiệm thu:**
- Clean build, nạp lặp lại ổn định.
- Boot log hiện đúng phiên bản và build ID (`APP_VERSION_STRING`, `__DATE__ __TIME__`).
- LED và shell phản hồi đúng lệnh.
- Ghi commit hash (`git rev-parse --short HEAD`) vào `PROJECT_LOG.md`.

---

## Day 2 – Lab 2: GPIO, ADC và logic điều khiển

### Lab 2A – Button → Relay

**Trọng tâm:** ngắt GPIO, chống dội phím.

Driver [`gpio`](../driver/peripherals/gpio/README.md) **đã có sẵn chống dội** (50 ms,
`DRIVER_GPIO_DEBOUNCE_MS`): ngắt chỉ khởi động bộ hẹn giờ, callback chạy trong workqueue khi tín hiệu
ổn định. Sinh viên không viết lại phần chống dội, mà dùng đúng API và giải thích vì sao phải làm vậy.

- Ví dụ: [`lab2a_button_relay.c`](examples/lab2a_button_relay.c). Nút `in0` = `&user_btn`; relay là
  ngõ ra `out1` (đặt ở `PE0` trong ví dụ, đổi chân theo mạch của bạn trong overlay).
- Relay đóng/ngắt bằng `driver_gpio_toggle()` **chỉ khi nhấn** (`active == true`), không khi nhả.
- `driver_gpio_out_init()` luôn khởi tạo ngõ ra ở mức tắt, nên **relay ngắt sau reset** (trạng thái an toàn).

**Nghiệm thu:** 20 lần nhấn liên tiếp, relay đổi trạng thái đúng 20 lần (không dội); relay ngắt sau
mỗi lần reset MCU.

### Lab 2B – Sensor → Buzzer

**Trọng tâm:** lấy mẫu ADC, trung bình động, ngưỡng có trễ (hysteresis).

Còi báo động chỉ là **ngõ ra bật/tắt** bằng `driver_gpio_set()`, không cần driver riêng. Ví dụ
[`lab2b_sensor_buzzer.c`](examples/lab2b_sensor_buzzer.c) dùng nhiệt độ chip (`driver_adc_read_die_temp`);
thay `read_sensor()` bằng biến trở (`driver_adc_read_mv`) hoặc cảm biến của bạn.

```
CONFIG_ADC=y
CONFIG_SENSOR=y
CONFIG_CBPRINTF_FP_SUPPORT=y
```
Overlay ADC: dán từ README [`adc`](../driver/peripherals/adc/README.md) (kênh nội) và thêm ngõ ra còi:
```dts
/ {
	ec_outputs {
		compatible = "gpio-leds";
		ext_buzzer: ext_buzzer { gpios = <&gpioe 1 GPIO_ACTIVE_HIGH>; };
	};
	aliases { out2 = &ext_buzzer; };
};
&gpioe { status = "okay"; };
```
- Ngưỡng bật cao hơn ngưỡng tắt (ví dụ 35 °C bật, 33 °C tắt) để còi không chập chờn.

**Nghiệm thu:** còi không kêu chập chờn quanh ngưỡng (ghi log `avg` và `alarm`); buzzer tắt sau reset.

### Lab 2C – AUTO/MANUAL (mở rộng, về nhà)

Máy trạng thái hai chế độ: `MANUAL` (nút điều khiển relay như 2A), `AUTO` (ngưỡng như 2B điều khiển
relay). Một nút khác (`in1`) chuyển chế độ. Không có ví dụ; mô hình hóa bằng `enum` và `switch`.

---

## Day 3 – Lab 3: Truyền thông công nghiệp và điều khiển

### Lab 3A – CAN → Relay với Heartbeat + Timeout (bắt buộc)

Board là **slave**, master (board của GV/TA) gửi heartbeat và lệnh. Driver [`can`](../driver/protocols/can/README.md)
có sẵn bộ theo dõi heartbeat: `driver_can_heartbeat_watch(id, timeout_ms, cb)`.

| ID | Hướng | Nội dung |
| --- | --- | --- |
| `0x100` | master → slave | Heartbeat (chu kỳ ví dụ 250 ms) |
| `0x200` | master → slave | Lệnh relay, byte 0 = 0/1 |
| `0x201` | slave → master | Trạng thái: byte 0 = relay, byte 1 = heartbeat còn sống |

Ví dụ: [`lab3a_can_relay.c`](examples/lab3a_can_relay.c) (bitrate 500 kbit/s, timeout 1 s).
- Mất heartbeat quá `HB_TIMEOUT_MS` → callback `cb(false)` chạy ngay, relay **ngắt** (an toàn).
- Lệnh `0x200` chỉ có tác dụng khi heartbeat còn sống.
- Relay ngắt sau reset. Cấu hình CAN và đấu dây (CN5, nguồn 24 V cho cách ly): xem README `can`.
- Chưa có master: dùng `driver_can_init(500000, true)` (loopback) và tự gửi `0x100` bằng thread phụ
  để thử logic timeout.

**Nghiệm thu:** giao tiếp CAN thông suốt, khung lệnh/trạng thái đúng định dạng, rút dây CAN hoặc dừng
heartbeat thì relay ngắt trong khoảng timeout.

### Lab 3B – RS485 Echo / Modbus RTU (chọn 1 trong 2 với 3C)

Hai hướng, chọn theo mục tiêu:

1. **RS485 echo:** dùng driver [`rs485`](../driver/protocols/rs485/README.md) (half-duplex, chân DE,
   `driver_rs485_receive_frame` tách khung theo khoảng lặng).
2. **Modbus RTU client:** dùng **Modbus có sẵn của Zephyr** (`CONFIG_MODBUS`), **không dùng**
   driver `rs485` vì cả hai cùng sở hữu UART4. Cấu hình (đã build thử trong `tests/build_all/configs/labs.*`):

`prj.conf`:
```
CONFIG_SERIAL=y
CONFIG_GPIO=y
CONFIG_MODBUS=y
CONFIG_MODBUS_ROLE_CLIENT=y
```
`app.overlay` (UART4 và DE/RE trên PC3 đã có trong board DTS; R87 lắp thì bỏ `de-gpios`):
```dts
&uart4 {
	modbus0 {
		compatible = "zephyr,modbus-serial";
		de-gpios = <&gpioc 3 GPIO_ACTIVE_HIGH>;
	};
};
```
Mã nguồn dùng API `zephyr/modbus/modbus.h`: `modbus_iface_get_by_name("MODBUS0")`,
`modbus_init_client()` với `baud`, `parity`, `rx_timeout`, rồi `modbus_read_holding_regs()`.
Tham khảo mẫu `samples/modbus/rtu_client` của Zephyr. Không cần `CMakeLists.txt` thêm driver nào.

**Nghiệm thu:** (echo) dữ liệu về đúng, khung tách đúng; (Modbus) đọc đúng giá trị thanh ghi từ slave,
xử lý lỗi timeout và CRC.

### Lab 3C – Servo SG90 PWM (chọn 1 trong 2 với 3B)

Dùng [`servo`](../driver/devices/servo/README.md) và `pwm`: xung 50 Hz, `driver_servo_set_angle`,
`driver_servo_sweep(id, from, to, ms)` để tăng/giảm tốc mượt (ramp). Cấp nguồn 5 V riêng cho servo.

**Nghiệm thu:** servo quay đúng góc đặt, quét 0°–180° mượt, hiệu chỉnh `min/max_pulse_us` cho đúng
servo đang dùng.

---

## Day 4 – Lab 4: Local HMI

**Trọng tâm:** hiển thị tách khỏi luồng dữ liệu, không chặn luồng đọc cảm biến.

- Màn hình: LCD1602 qua I²C PCF8574 (mặc định), driver [`lcd_pcf8574`](../driver/devices/lcd_pcf8574/README.md).
  (LVGL chưa có trong project này.)
- Dữ liệu: nhiệt độ/độ ẩm từ [`sht41`](../driver/devices/sht41/README.md); trạng thái bơm = relay (`gpio`);
  nút chuyển AUTO/MANUAL = `driver_gpio_in_init`.
- Kiến trúc gợi ý: **hai thread** — thread cảm biến ghi vào một cấu trúc dùng chung (`k_mutex` hoặc
  `k_msgq`), thread giao diện đọc cấu trúc đó và vẽ LCD mỗi 500 ms bằng `driver_lcd_printf`. Thread
  giao diện không gọi cảm biến, thread cảm biến không gọi LCD.
- Cảnh báo: dòng 2 hiện `ALARM` khi vượt ngưỡng; `driver_lcd_backlight` nháy để gây chú ý.

**Nghiệm thu:** màn hình cập nhật mượt, không làm chậm chu kỳ đọc cảm biến (đo bằng `k_uptime_get()`),
nút đổi AUTO/MANUAL ngay; nộp bản mẫu Application v1.

---

## Day 5 – Lab 5: MQTT IoT Device over Ethernet

**Driver:** [`ethernet`](../driver/devices/ethernet/README.md) + [`mqtt`](../driver/services/mqtt/README.md).
Broker thử nghiệm: `./scripts/mqtt_broker.sh` (Mosquitto) chạy trên máy tính cùng mạng LAN.

Topic chuẩn trong lớp (`<team>` là tên nhóm):

| Topic | Hướng | Nội dung |
| --- | --- | --- |
| `ws26/<team>/telemetry` | board → broker | JSON cảm biến, chu kỳ ví dụ 5 s: `{"t":27.4,"h":61.2}` |
| `ws26/<team>/cmd` | broker → board | Lệnh điều khiển relay/servo |
| `ws26/<team>/ack` | board → broker | Xác nhận đã thực hiện lệnh |
| `ws26/<team>/status` | board → broker | `online` khi kết nối |

- `driver_mqtt_publishf(topic, qos, fmt, ...)` để gửi JSON; `driver_mqtt_subscribe(topic, qos, cb)` để
  nhận lệnh.
- **Reconnect tự động:** `driver_mqtt_process()` tự kết nối lại khi mất mạng và **đăng ký lại** các
  topic đã subscribe. Vòng lặp chính chỉ cần gọi nó liên tục và publish theo chu kỳ khi
  `driver_mqtt_is_connected()`.
- Driver **chưa hỗ trợ Last Will**; publish `online` lên `status` mỗi lần kết nối lại (phát hiện bằng
  `driver_mqtt_is_connected()` chuyển từ false sang true).
- `cmd` chạy callback trong luồng gọi `driver_mqtt_process()`: tránh thao tác chặn lâu trong callback.

**Nghiệm thu:**
- Telemetry đều (ví dụ mỗi 5 s).
- Lệnh từ dashboard điều khiển đúng actuator, có `ack`.
- Rút dây mạng rồi cắm lại: hệ thống tự reconnect và tiếp tục truyền nhận.

---

## Day 6 – Lab 6A (Wi-Fi) / 6B (4G)

**Phạm vi hiện tại:** project **chỉ có Ethernet**. Chưa có driver Wi-Fi hay modem 4G (PPP) và chưa có
module để kiểm tra, nên Lab 6 **tạm thời chưa triển khai** trên board này.

Khi nhóm làm Lab 6, logic ứng dụng MQTT của Day 5 giữ nguyên; chỉ thay tầng mạng. Yêu cầu nghiệm thu
của đề (telemetry + command qua Wi-Fi/4G, re-subscribe sau khi mất mạng; với 4G: đo chu kỳ keep-alive
và ước tính dung lượng/ngày, bắt buộc TLS) vẫn áp dụng. Driver `mqtt` có `KEEPALIVE_S = 30` s làm điểm
xuất phát cho phép tính dung lượng.

---

## Day 7 – Lab 7: MCUboot Lifecycle & Rollback (chỉ nhóm làm OTA)

**Trọng tâm:** vòng đời cập nhật an toàn trên `slot0` / `slot1` với MCUboot (swap using scratch,
RSA-2048). Các nhóm còn lại dùng thời gian này để tích hợp và kiểm thử theo Checklist Showcase.

Phân vùng flash (xem board DTS): `slot0` ở `0x08020000` (704 KiB), `slot1` ở `0x080F0000`.

Ứng dụng ([`lab7_mcuboot.c`](examples/lab7_mcuboot.c)) cần thêm vào `prj.conf`:
```
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_STREAM_FLASH=y
CONFIG_IMG_MANAGER=y
```
Khi khởi động, ứng dụng in phiên bản, chạy `self_test()`; **đạt** thì `boot_write_img_confirmed()`;
**không đạt** thì reboot mà không xác nhận để MCUboot quay về bản cũ.

**Quy trình:**

1. **V1:** đặt `VERSION` = 1.0.0, `./scripts/flash.sh` (nạp MCUboot + V1). Boot log hiện `1.0.0`;
   V1 tự confirm.
2. **V2:** đổi `VERSION` = 2.0.0 và nhớ `self_test()` trả về đạt. Build, rồi tạo ảnh nâng cấp và nạp
   vào `slot1`:
   ```sh
   ./scripts/build.sh build-v2
   ./scripts/lab7_upgrade_image.sh build-v2 2.0.0
   STM32_Programmer_CLI -c port=SWD -d build-v2/upgrade-2.0.0.bin 0x080F0000 -rst
   ```
   Reset: MCUboot đổi chỗ ảnh (log MCUboot ở UART5), V2 chạy ở chế độ **TEST**, self-test đạt →
   confirm. Reset lần nữa: vẫn chạy V2.
3. **V3 lỗi:** đặt `SELFTEST_FORCE_FAIL` = 1, `VERSION` = 3.0.0, làm lại như bước 2 với `3.0.0`.
   V3 chạy, self-test **không đạt**, reboot không confirm → MCUboot **rollback về V2** ở lần khởi
   động kế tiếp. Boot log lại hiện `2.0.0`.

- `scripts/lab7_upgrade_image.sh` ký lại `zephyr.bin` với phiên bản cho trước, đệm tới kích thước
  slot và thêm magic trailer của MCUboot (đã kiểm tra: ảnh dài đúng 0xB0000 byte, magic ở cuối).
  Nó **không nạp** board.
- `CONFIG_MCUBOOT_DOWNGRADE_PREVENTION=y`: phiên bản phải **tăng** (2.0.0 → 3.0.0), nên V3 phải có
  số phiên bản lớn hơn V2.
- Khóa ký: mặc định là khóa phát triển công khai (chỉ dùng cho lab). Đặt `STM32H573_MCUBOOT_KEY_FILE`
  để dùng khóa riêng.
- **Chưa chạy thử trên board thật** các bước 2–3 (nạp, swap, rollback); phần build, ký và ảnh
  nâng cấp đã kiểm tra bằng build.

**Nghiệm thu:** boot log thể hiện rõ phiên bản; V2 được confirm; V3 lỗi và hệ thống tự quay về V2.

---

## Day 8 – Showcase & Demo

Không có bài lab mới: dựng booth demo trực tiếp và vấn đáp.

---

## Kiểm tra của giảng viên

```sh
./tests/build_all/run.sh          # link toàn bộ driver trong nhiều cấu hình, gồm cấu hình "labs" (Shell, Modbus)
./tests/readme_examples/run.sh    # ví dụ main.c của từng README
./tests/lab_examples/run.sh       # ví dụ trong docs/examples (lab 1, 2A, 2B, 3A, 7)
```
Các lệnh trên chỉ build, không nạp board.

---

## Phụ lục – Bài nền theo driver

Dành cho sinh viên cần luyện từng driver trước khi vào lab. Mỗi bài dùng **một driver**.

| Bài | Driver | Cần thêm | Phần cứng | Học được |
| --- | --- | --- | --- | --- |
| N1 | [`gpio`](../driver/peripherals/gpio/README.md) | – | LED LIFE, 1 nút | Xuất/nhập số, active low, chống dội |
| N2 | [`timer`](../driver/peripherals/timer/README.md) | – | – | `k_timer`, workqueue, đo thời gian |
| N3 | [`watchdog`](../driver/peripherals/watchdog/README.md) | – | – | IWDG, nguyên nhân reset |
| N4 | [`adc`](../driver/peripherals/adc/README.md) | – | Biến trở (hoặc kênh nội) | ADC 12 bit, mV, nhiệt độ chip |
| N5 | [`pwm`](../driver/peripherals/pwm/README.md) | – | LED ngoài | Chu kỳ, duty |
| N6 | [`sht41`](../driver/devices/sht41/README.md) | – | Có sẵn | Cảm biến I2C |
| N7 | [`rtc`](../driver/devices/rtc/README.md) | – | Có sẵn (pin CR1220) | RTC, giữ giờ khi mất điện |
| N8 | [`dht11`](../driver/devices/dht11/README.md) | – | DHT11 | GPIO open-drain, đổi chân bằng overlay |
| N9 | [`bh1750`](../driver/devices/bh1750/README.md) | – | GY-30 | I2C1, địa chỉ 0x23/0x5C |
| N10 | [`hcsr04`](../driver/devices/hcsr04/README.md) | – | HC-SR04 + 2 điện trở | Ngắt, đo xung, chia áp |
| N11 | [`encoder`](../driver/devices/encoder/README.md) | – | KY-040 | Input subsystem |
| N12 | [`lcd_pcf8574`](../driver/devices/lcd_pcf8574/README.md) | `i2c` | LCD + PCF8574 | HD44780, kích thước LCD |
| N13 | [`motor`](../driver/devices/motor/README.md) | `pwm` | Motor DC + MOSFET | Vùng chết, tăng/giảm tốc mềm |
| N14 | [`servo`](../driver/devices/servo/README.md) | `pwm` | SG90 + nguồn 5 V | Xung 50 Hz, hiệu chỉnh góc |
| N15 | [`i2c`](../driver/protocols/i2c/README.md) | – | Module I2C | Quét bus |
| N16 | [`uart`](../driver/protocols/uart/README.md) | – | USB-UART 3.3 V | Baud, ngắt nhận, ring buffer |
| N17 | [`spi`](../driver/protocols/spi/README.md) | – | 1 dây loopback | Chế độ SPI, CS |
| N18 | [`rs485`](../driver/protocols/rs485/README.md) | `uart` | USB-RS485 + 24 V | Half-duplex, DE |
| N19 | [`can`](../driver/protocols/can/README.md) | – | Loopback | ID, mask lọc |
| N20 | [`ethernet`](../driver/devices/ethernet/README.md) | – | Cáp mạng | DHCP, link, IP tĩnh |
| N21 | [`mqtt`](../driver/services/mqtt/README.md) | `ethernet` | Cáp mạng + broker | Publish/subscribe, QoS |

Tiêu chí chung: code driver **không sửa** (chỉ cấu hình qua 3 file), log rõ ràng bằng `printk`, xử
lý mã lỗi trả về (không bỏ qua giá trị âm), và có `README` ngắn mô tả cách đấu dây.

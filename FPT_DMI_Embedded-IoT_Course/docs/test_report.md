# Báo cáo kiểm tra driver trên phần cứng thật

Kế hoạch: `tests/fw_showcase/prompt.txt`. Ngày bắt đầu: 2026-10-06.

## Môi trường

| Mục | Giá trị |
| --- | --- |
| Board | Base Board STM32H573RI (E-Desk), `stm32h573ri_custom` |
| Chip | Device ID 0x484 (STM32H56x/573), Rev X, product state Open, TrustZone tắt |
| Mạch nạp | ST-LINK/V2.1, firmware V2J47M34, SN 066CFF495082854967091221, SWD vào J3 |
| Công cụ nạp | STM32_Programmer_CLI v2.23.0 (symlink `~/.local/bin`) |
| Công cụ đọc RTT | pyocd 0.45.1 (`stm32h573ritx`). OpenOCD 0.12 trên máy **không có** `target/stm32h5x.cfg` nên `scripts/rtt.sh` (đường ST-LINK) không chạy được |

## Kết quả

| Ca | Driver | Ngày | Kết quả | Ghi chú |
| --- | --- | --- | --- | --- |
| T0.1 | – | 2026-10-06 | PASS | Đã sao lưu 2 MB flash vào `~/board-backups/stm32h573ri-flash-2MB-20261006-before-hello.bin`. MCUboot, slot0 và vùng cấu hình có dữ liệu; slot1 trống. |
| T0.2 | – | 2026-10-06 | PASS | `scripts/flash.sh` nạp MCUboot + app (23 KB ở 0x08020000), reset mềm. RTT in `*** Booting Zephyr OS build 60dde3fd9b62 ***` và `Hello, World !!!`, Target `stm32h573ri_custom/stm32h573xx`. |
| T0.3 | – | 2026-10-06 | MỘT PHẦN | Xem bên dưới. |
| A1 | gpio | 2026-10-06 | PASS (loopback) | `tests/hw/apps/gpio_loop`: PA10 và PA8 đặt chế độ input+output để phần mềm tự "nhấn nút" và đọc lại LED. 20 lần nhấn sạch = 20 NHAN + 20 NHA; 20 lần nhấn có dội (chatter ~3 ms) vẫn đúng 20/20; đọc lại ngõ ra đúng; id sai trả `-22`. **Chưa thử nút bấm thật và mắt nhìn LED.** |
| A2 | timer | 2026-10-06 | PASS | `k_busy_wait(1234)` đo đúng 1234 µs; uptime +1000 ms mỗi lần; timer một lần chạy đúng một lần. Chưa thử TIM5 phần cứng (ví dụ README không dùng). |
| A3 | watchdog | 2026-10-06 | PASS | Treo giả lập → board reset sau timeout; lần sau báo `bi WATCHDOG reset`; feed đều 10 lần không reset. |
| A4 | adc | 2026-10-06 | **FAIL** | Kênh nội đọc sai: nhiệt độ chip 90–102 °C (mong đợi 25–60 °C), VDDA ≈ 2,7 V (ST-LINK đo VDD 3,22 V). VREFINT_CAL đọc từ 0x08FFF810 = 0x05E5 (hợp lý); driver Zephyr dùng thời gian lấy mẫu tối đa, `vrefint-cal-mv` = 3300. Nghi: VREF+/VDDA trên board khác 3,3 V hoặc thiết lập ADC. Cần đo VOM chân VDDA/VREF+. PA3 chưa nối biến trở nên chưa kiểm tra kênh ngoài (ca B1). |
| A5 | rtc | 2026-10-06 | PASS một phần | Giây tăng đều (9 mẫu), giờ vẫn đúng sau reset mềm (08:00:17 sau ~16 s). **Chưa thử rút nguồn ≥ 60 s** (cần thao tác tay, pin CR1220). |
| A6 | sht41 | 2026-10-06 | **FAIL** | `Loi SHT41: -19` (ENODEV). |
| A7 | i2c | 2026-10-06 | PASS một phần | I2C2 chỉ thấy `0x51` (BM8563); đọc giây RTC hợp lệ. **Không thấy `0x44`** (SHT41) nên A6 là lỗi phần cứng/địa chỉ, không phải driver: kiểm tra sensor J2 có lắp không, hoặc địa chỉ 0x45/0x46 (đã quét toàn dải). I2C1 không có thiết bị (chưa cắm module). |
| A8 | can (loopback) | 2026-10-06 | PASS | `tests/hw/apps/can_hb`: lọc exact 0x123 và dải 0x200–0x20F (mask 0x7F0) nhận đúng, khung ngoài mask bị bỏ; ID 0x800 trả `-22`; trạng thái 0. Ví dụ README cũng nhận lại đúng 5 byte. |
| A9 | can heartbeat | 2026-10-06 | PASS | Mất heartbeat báo `cb(false)` sau đúng 1000 ms; có lại thì `cb(true)`; gọi watch lần hai trả `-EALREADY` (-120 trên libc này). |
| B3 | dht11 | 2026-10-06 | PASS (một phần) | DATA = PE0 (U16-8). Chạy 2 phút: 61 lần đọc đúng, 1 lỗi (`-5`) → **98,4%** (tiêu chí ≥ 95%); 28 °C, 56–61% (đo 69–73% khi người dùng thở vào). Log: `tests/hw/logs/dht11-2min.log`. Lần đọc **đầu tiên ngay sau khi khởi động luôn lỗi `-5`** (cảm biến cần ~1 s sau khi cấp nguồn): đề xuất ví dụ README chờ `k_msleep(1000)` trước lần đọc đầu. Trước đó cũng đọc lỗi `-5` liên tục khi dây/nguồn DHT11 chưa đúng. **Chưa thử:** so sánh với SHT41 (SHT41 không có trên board), đổi sang chân khác bằng overlay. |
| B4 | bh1750 | 2026-10-06 | BLOCKED | Chưa có module: ví dụ README **không in gì** khi đọc lỗi (chỉ in khi `read_lux == 0`), nên sinh viên không biết lỗi. Đề xuất sửa ví dụ in mã lỗi. |
| B5 | hcsr04 | 2026-10-06 | BLOCKED | Chưa có module: `Loi doc: -5` (timeout) đều đặn, không treo. |
| B6 | encoder | 2026-10-06 | BLOCKED | Chưa có module: không có sự kiện (đúng, chưa xoay), driver khởi tạo không lỗi. |
| B7 | lcd_pcf8574 | 2026-10-06 | PASS | LCD 16x2 @0x27, cấp 5 V. RTT `LCD 16x2 san sang`; người dùng xác nhận bằng mắt: dòng 1 `Hello FPT` + ký tự tự vẽ, dòng 2 đếm `Dem: N` mỗi giây. **Chưa thử:** backlight, con trỏ, 20x4. Lần đầu bị lỗi do cắm đảo SDA/SCL (phát hiện bằng bit-bang I2C trên PB6/PB9: `0x27` trả lời khi đổi vai). Lúc nạp, SWD lỗi chập chờn (`Unable to get core ID`, ST-LINK re-enumerate USB 010→013); MCUboot bị ghi dở một lần, nạp lại thì boot bình thường. |
| C3 | rs485 | 2026-10-06 | PASS (người dùng xác nhận) | Board lắp 24 V (DC1), bộ USB-RS485 của người dùng: nhận `BEACON n` từ board; gõ `abc` nhận lại `ECHO: abc`. Qua SWD (24 V tắt) xác nhận UART4 9600 (BRR 26042), PA0/PA1 AF8, DE=PC3 đổi mức đúng nhịp phát (24 cạnh/6 s, ~2,3% thời gian). **Chưa đo:** 1000 khung không lỗi (bộ chuyển không xuất hiện trên máy phát triển), R87/R88. **Lưu ý:** khi cấp 24 V thì SWD không kết nối được (`Unable to get core ID`); nạp/đọc RTT phải tắt 24 V. Lúc đầu không thấy dữ liệu do tắt/bật 24 V sai thời điểm và bộ chuyển cắm vào máy khác; `1a86:55d2` (ttyACM0/1) không phải bộ RS485. Ca tùy chỉnh `tests/hw/apps/rs485_beacon`. |
| C5/C6 | can (bus thật), can heartbeat thật | 2026-10-06 | **FAIL / BLOCKED (nghi lỗi phần cứng CAN trên board)** | Đối tác 500 kbit/s không nhận `0x321`. Firmware chẩn đoán `tests/hw/apps/can_diag` (log qua RS485 vì SWD không dùng được khi có 24 V): ở mọi bitrate 125k/250k/500k/1M `txok=0 rx=0`, `tec=rec=0`, `cb_err=-114` (ENETUNREACH = bus-off), `PSR act=1`. Phép thử độc lập với bộ điều khiển CAN (PB7 làm GPIO kéo TXD thấp 10 s, PB0 ép thấp = normal mode, đọc PB8): **RXD luôn = 1** với 24 V bật (RS485 cùng rail +5V-ISO chạy bình thường) → transceiver/cách ly (U7 ISO7731, U15 TCAN1057) không đưa TXD sang RXD. Pinmux đúng (PB7/PB8 AF9), STB (PB0) = 0, CAN loopback nội bộ (A8/A9) PASS. Cần đo VOM các điểm U7/U15/CANH/CANL để xác định (chưa có số đo). Lưu ý: lần phép thử đầu bị sai vì `can_stop()` đưa transceiver vào standby; đã sửa. |
| C1 | uart | 2026-10-06 | PASS (một phần) | USART6 115200 8N1, CH343P (người dùng) ↔ U16-13 (PC7 RX) / U16-14 (PC6 TX) / GND. Board→PC: lời chào nhận được. PC→board: **chỉ chạy khi terminal gửi CR/LF cuối dòng** (lúc đầu terminal không gửi Enter nên `read_line` không trả về: 15 byte `hhelloled onabc` nằm trong ring buffer, không phải lỗi driver). Gửi hex có `0D`: 7 dòng (`helo`×3, `led on`×4) đều được nhận đúng, mỗi dòng đúng 1 phản hồi `Ban vua go: <dòng> (<N> ky tu)`; RTT khớp (`B: read_line returned N`, `D: reply ret=0`), không có lỗi PE/FE/NE/ORE. Ca tùy chỉnh `tests/hw/apps/uart_dbg`. **Chưa thử:** 20 dòng liên tiếp, dòng > 64 ký tự, gửi nhanh/khối lớn (tràn bộ đệm 256 byte), `hello` đúng chính tả. Log người dùng: `tests/fw_showcase/log.txt`. |
| N20 | ethernet | 2026-10-06 | PASS | W5500 `eth init ret=0`; link UP sau ~2 s khi cắm cáp; DHCP cấp `192.168.100.118`; ping từ PC 3/3, ~0,7 ms. Khi chưa cắm cáp: `link=DOWN`, `wait_ip` hết giờ, không treo. |
| N21 | mqtt | 2026-10-06 | PASS (một phần) | Broker cloud `mqtt.iot-dmi.cloud:1883` có đăng nhập (user + JWT 207 ký tự) → **thêm `driver_mqtt_set_auth()`** vào driver (trước đó user/password luôn NULL). Ca `tests/hw/apps/mqtt_test` (thông tin broker lấy từ biến môi trường lúc build, không lưu trong source). Board publish `cnt=N` mỗi 2 s, QoS 0/1 xen kẽ: 58/58 `ret=0`, nhận lại đủ qua subscribe (QoS 1). PC → board: 21/21 lệnh `cmd:` nhận đúng, echo về PC sau ~0,1 s; tin 300 byte bị cắt còn 255 (đúng `DRIVER_MQTT_MAX_PAYLOAD`), kết nối không lỗi. Broker cục bộ (mosquitto) chưa có trên máy nên `-111` (ECONNREFUSED) đúng như mong đợi. **Chưa thử:** tự kết nối lại khi rút cáp, chạy dài (keep-alive). Driver chỉ nhận IP, chưa có DNS (dùng IP `72.60.234.159`). |
| B1, B2, B8 | adc ngoài, pwm, motor | – | CHƯA CHẠY | Cần dụng cụ đo/nguồn ngoài; motor cần nguồn riêng. |
| B9 | servo | 2026-10-06 | PASS | SG90, tín hiệu PB14 (U16-16), nguồn 5 V riêng, GND chung. Đo TIM12: chu kỳ 20,000 ms (50 Hz); xung 500/975/1450/1925/2400 µs cho 0/45/90/135/180°; quét liên tục 500..2400 µs, bước nhảy lớn nhất 43 µs/20 ms, đổi chiều đúng. Người dùng xác nhận servo chạy đúng. Lần đầu servo không quay do đấu dây/GND ngoài (PWM trên PB14 đã đúng từ đầu). Ca tùy chỉnh `servo_90` (giữ 90°), `servo_cont` (quét liên tục). |


**Ghi chú SWD:** từ khi cắm LCD/DHT11/servo, kết nối SWD chập chờn nhiều lần (`Unable to get core ID`), ST-LINK re-enumerate USB 3 lần (15:00, 15:41, 16:17); hồi lại sau khi cắm lại dây SWD/khởi động lại phần cứng. Không liên quan trực tiếp đến nguồn servo (ngắt nguồn servo vẫn lỗi). `tests/hw/run_case.sh` tự thử nạp lại 4 lần.

### T0.3 – các điểm cần kiểm tra
- **Kết nối SWD:** `mode=HOTPLUG` và `pyocd` ban đầu báo "Unable to get core ID"; chế độ mặc định (`port=SWD`), NORMAL, UR đều kết nối được. Nạp bằng `--reset-mode=hw` (trong `board.cmake`) vẫn thành công dù J3 không có NRST (reset mềm được thực hiện).
- **ST-LINK clone:** STM32CubeProgrammer nhận và nạp được STM32H5. OpenOCD `stlink-dap` kết nối được probe (điện áp 3,217 V) nhưng thiếu file cấu hình H5 trên máy này, nên **chưa kiểm tra được** đọc RTT bằng OpenOCD.
- **Đọc RTT:** `pyocd rtt` thấy 3 kênh lên/3 kênh xuống nhưng cần terminal (lỗi ioctl khi chạy không có tty); đọc trực tiếp vùng đệm RTT (`_SEGGER_RTT` @ 0x20000410) bằng pyocd API cho kết quả đúng.
- **R87/R88 (RS485):** CHƯA kiểm tra (cần ca C3).
- **Log MCUboot trên UART5:** CHƯA kiểm tra.

## Việc cần làm tiếp
1. Xử lý A4 (đo VDDA/VREF+ bằng VOM) và A6/A7 (sensor SHT41 không thấy trên I2C2).
2. A5: thử rút nguồn ≥ 60 s; A1: thử nút thật.
3. Đợt B (cần module ngoài), C, D, E theo `prompt.txt` mục 3.

## Công cụ đã thêm
- `scripts/rtt.sh` dùng `scripts/rtt_pyocd.py` khi OpenOCD thiếu `stm32h5x.cfg` (`--reset`, `--seconds`, `--all`, `--send`).
- `tests/hw/run_case.sh <driver|case> [giây]`: build sysbuild + MCUboot, nạp, đọc RTT, so với `tests/hw/expect/<ca>.re`. Cần `HW_FLASH_OK=1`.
- Ca tùy chỉnh trong `tests/hw/apps/` (`gpio_loop`, `can_hb`).

# fw_showcase

[English](README.md)

Firmware Zephyr RTOS cho board **STM32H573RI** tùy biến. Thiết bị:
- theo dõi nhiệt độ/độ ẩm bằng DHT11 và hiển thị trên LCD 20x4;
- có trang web cấu hình tích hợp;
- gửi dữ liệu qua MQTT (có thể dùng TLS);
- khi có cảnh báo thì gửi tin RS485, điều khiển relay và motor.

Image được **MCUboot** khởi động và xác thực (chữ ký RSA-2048, chống downgrade).

- Phiên bản firmware: **3.0.0**
- Zephyr: **v4.4.2** (pin trong [`west.yml`](west.yml))
- Board: `stm32h573ri_custom` (định nghĩa nằm trong [`boards/`](boards))

## Tính năng

### Cảm biến và hiển thị
- Đọc **DHT11** mỗi 2 giây.
- **LCD ký tự 20x4** (HD44780 qua module I2C PCF8574/PCF8574A). Địa chỉ I2C
  được tự dò: thử 0x27/0x3F trước, sau đó 0x20-0x27 và 0x38-0x3F. Nếu LCD bị
  rút ra, firmware sẽ dò lại mỗi 10 giây.

  ```text
  IP:192.168.1.50
  T: 28.0°C , H: 45.0%
  MQTT:OK   Motor:OFF
  !! TEMP HIGH !!          <- chỉ hiện khi đang cảnh báo
  ```

### Cảnh báo
Giá trị được kiểm tra ở **mỗi lần đọc**. Có hysteresis để cảnh báo không bật/tắt
liên tục quanh ngưỡng.

| Điều kiện       | Bắt đầu khi           | Kết thúc khi                 |
| --------------- | --------------------- | ---------------------------- |
| Nhiệt độ cao    | > 40 °C               | <= 39 °C                     |
| Nhiệt độ thấp   | < 0 °C                | >= 1 °C                      |
| Độ ẩm cao       | > 90 %                | <= 88 %                      |
| Độ ẩm thấp      | < 20 %                | >= 22 %                      |
| Lỗi cảm biến    | 3 lần đọc lỗi liên tiếp | lần đọc tốt, trong ngưỡng đầu tiên |

Khi cảnh báo **bắt đầu**:
- Gửi tin RS485 `ALARM` ngay, và lặp lại mỗi 10 giây trong khi còn cảnh báo.
- **Relay 1** nháy (500 ms bật / 500 ms tắt).
- **Motor** bật.
- LCD hiện cảnh báo.
- Web/MQTT được cập nhật ngay.

Khi cảnh báo **kết thúc**:
- Gửi tin RS485 `CLEAR`.
- Tắt relay.
- Tắt motor nếu motor do cảnh báo bật.

Các ngưỡng nằm trong [`src/app/alarm.h`](src/app/alarm.h).

### Motor và nút nhấn
- **Nút PA10** bật/tắt motor bất cứ lúc nào (dùng ngắt, chống dội 50 ms).
- Motor do **cảnh báo** bật sẽ tự tắt khi cảnh báo kết thúc.
- Motor do **nút nhấn** bật thì không bao giờ tự tắt.
- Nếu dùng nút tắt motor trong lúc đang cảnh báo, motor sẽ tắt đến lần cảnh báo
  kế tiếp.

### RS485 (chỉ truyền)
Dùng UART4 ở 9600 8N1, chân điều khiển hướng là PC3. Mỗi tin là một dòng ASCII
kết thúc bằng `\r\n`:

```text
ALARM #12 T=42C H=30% REASON=T_HIGH
ALARM #13 T=-- H=-- REASON=SENSOR
CLEAR #14 T=28C H=45%
```

`#N` là số thứ tự tin nhắn. `REASON` là một trong `T_HIGH`, `T_LOW`, `H_HIGH`,
`H_LOW`, `SENSOR`.

### Ethernet, web và MQTT
- **Ethernet W5500** lấy địa chỉ bằng DHCP. Nếu sau 30 giây không có lease, thiết
  bị chuyển sang IP tĩnh đã lưu (mặc định `192.168.1.100/24`, gateway
  `192.168.1.1`).
- **Web server** chạy ở cổng 80:
  - `/login`: có mật khẩu, chỉ một phiên đăng nhập tại một thời điểm, nhập sai
    thì thời gian chờ tăng dần.
  - `/config`: thông tin thiết bị và trạng thái cảnh báo/motor/RS485 (tự cập
    nhật mỗi 5 giây), cài đặt MQTT, đổi mật khẩu web.
  - `/status`: trả về JSON.
  - Khi lưu, cấu hình được ghi vào flash rồi thiết bị khởi động lại.
- **MQTT** (3.1.1, QoS 0):
  - Publish mỗi 60 giây, và publish ngay khi cảnh báo bắt đầu/kết thúc.
  - Cổng **8883** dùng TLS: chứng chỉ broker được kiểm tra theo ISRG Root X1
    (Let's Encrypt), có SNI. Cổng khác dùng TCP thường.
  - Tự kết nối lại khi mất kết nối.

Payload MQTT và JSON `/status`:

```json
{"id":"mini_GW","fw":"3.0.0","temp":28,"humi":45,"sensor_ok":1,
 "alarm":0,"reason":"NONE","motor":0,"cnt":7,"ip":"192.168.1.50"}
```

### Lưu cấu hình và khởi động
- Cấu hình được lưu thành **2 bản trong flash** (`config-a` / `config-b`), mỗi bản
  có CRC32. Nếu bản A hỏng, firmware dùng bản B.
- **MCUboot**:
  - swap dùng scratch;
  - image ký RSA-2048;
  - xác thực slot 0 mỗi lần khởi động;
  - chống downgrade;
  - image đang chạy được confirm khi khởi động.
- **Chuyển image bằng NRST:** trong 3 giây đầu sau reset, LED nháy nhanh. Nếu nhấn
  **NRST** trong khoảng thời gian này, thiết bị sẽ chuyển sang image ở slot phụ
  (MCUboot test upgrade).
- **LED LIFE** (PA8) nháy đôi (heartbeat) khi firmware đang chạy.
- **Console:** log xuất qua **SEGGER RTT** kênh 0 qua mạch nạp SWD, không cần
  adapter UART.

## Phần cứng và sơ đồ chân

### Đấu nối ngoài (header U16 trên Base Board)

| Chân U16 | Chân MCU | Chức năng                                | Ghi chú |
| -------- | -------- | ---------------------------------------- | ------- |
| 1, 2     | +5V      | Cấp nguồn DHT11 / LCD / driver motor     | tùy nhu cầu |
| 17, 18   | GND      | Mass chung                               | |
| 5        | PC2      | DHT11 DATA                               | open drain; thêm điện trở kéo lên 4.7k-10k về 3V3 nếu module chưa có |
| 10       | PB6      | I2C1 SCL → module PCF8574 của LCD        | 100 kHz |
| 9        | PB9      | I2C1 SDA → module PCF8574 của LCD        | 100 kHz |
| 4        | PA2      | Ngõ vào driver motor, mức cao = chạy     | **không nối motor trực tiếp**: dùng transistor/MOSFET hoặc module driver có diode chống ngược |
| 12       | PA10     | Nút nhấn, tích cực mức thấp (pull-up nội) | bật/tắt motor |

### Ngoại vi trên board

| Chức năng          | Chân MCU                                                         |
| ------------------ | ---------------------------------------------------------------- |
| Ethernet W5500     | SPI1: SCK PA5, MISO PA6, MOSI PA7, CS PA4; INT PC4, RESET PC5     |
| RS485              | UART4: TX PA0, RX PA1; DE/RE (hướng) PC3                         |
| Relay 1            | PB5 (mức cao = đóng)                                             |
| LED LIFE           | PA8 (tích cực mức thấp)                                          |
| SWD / console RTT  | PA13 SWDIO, PA14 SWCLK                                           |

Các ngoại vi trên board mà firmware này tắt:
- SHT41 / I2C2
- CAN (FDCAN1)
- thẻ SD (SDMMC1)
- USART1, USART2, USART6
- SPI2

Xem [`boards/stm32h573ri_custom.overlay`](boards/stm32h573ri_custom.overlay).

### Phân vùng flash (2 MB)

| Phân vùng     | Offset     | Kích thước |
| ------------- | ---------- | ---------- |
| mcuboot       | 0x000000   | 128 KB     |
| image-0       | 0x020000   | 704 KB     |
| image-scratch | 0x0D0000   | 128 KB     |
| image-1       | 0x0F0000   | 704 KB     |
| config-a      | 0x1A0000   | 32 KB      |
| config-b      | 0x1A8000   | 32 KB      |
| swapmark      | 0x1B0000   | 8 KB       |

## Build

### 1. Chuẩn bị (cài một lần)
- Linux (đã thử trên Ubuntu) có `git`, `cmake` >= 3.20, `ninja`, `python3` và
  `python3-venv`:
  ```sh
  sudo apt install git cmake ninja-build python3 python3-venv
  ```
- Để nạp firmware: [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
  (`STM32_Programmer_CLI` nằm trong `PATH`) và mạch nạp ST-LINK.

Không cần tự cài Zephyr SDK, vì `setup.sh` sẽ cài giúp.

### 2. Clone và cài đặt

Clone vào một **thư mục trống**, vì Zephyr và các module sẽ được tải về cạnh
bản clone.

```sh
mkdir fw_showcase-workspace && cd fw_showcase-workspace
git clone https://github.com/dmi-tech/fptUniversity.git
./fptUniversity/fw_showcase/scripts/setup.sh
```

`setup.sh` làm các bước sau. Chạy lại nhiều lần vẫn an toàn, ví dụ sau khi
`west.yml` thay đổi.
1. Kiểm tra các công cụ trên máy.
2. Tạo `.venv` và cài west.
3. Chạy `west init` và `west update` để tải Zephyr v4.4.2 cùng các module.
4. Cài các thư viện Python cho Zephyr và MCUboot.
5. Cài Zephyr SDK với toolchain `arm-zephyr-eabi`, theo phiên bản ghi trong
   `zephyr/SDK_VERSION`. Nếu phiên bản đó đã được cài thì bỏ qua.
6. Cảnh báo nếu thiếu `STM32_Programmer_CLI` hoặc khóa ký.

Cấu trúc sau khi cài:

```text
fw_showcase-workspace/
├── .venv/  .west/
├── fptUniversity/    <- repo git (github.com/dmi-tech/fptUniversity)
│   └── fw_showcase/  <- project này
├── zephyr/           <- Zephyr v4.4.2
├── bootloader/mcuboot/
└── modules/          <- hal_stm32, mbedtls, segger, ...
```

### 3. Khóa ký
MCUboot chỉ khởi động image được ký bằng khóa của nó, và repo không chứa khóa
nào. Các script lấy khóa từ biến `STM32H573_MCUBOOT_KEY_FILE`.

- **Khóa chung của project** (xin người quản lý project): thiết bị đang chạy
  ngoài thực tế chỉ nhận image ký bằng khóa này.
  ```sh
  export STM32H573_MCUBOOT_KEY_FILE=~/keys/fw_showcase-rsa2048.pem
  ```
- **Khóa mới**, chỉ dùng cho board của riêng bạn. Không đưa khóa vào repo và
  nhớ sao lưu:
  ```sh
  .venv/bin/python bootloader/mcuboot/scripts/imgtool.py keygen -t rsa-2048 -k ~/keys/fw_showcase-rsa2048.pem
  export STM32H573_MCUBOOT_KEY_FILE=~/keys/fw_showcase-rsa2048.pem
  ```

Thêm dòng `export` vào `~/.bashrc` để terminal mới nào cũng có sẵn biến này.

### 4. Build

```sh
./fptUniversity/fw_showcase/scripts/build.sh            # kết quả: fptUniversity/fw_showcase/build
./fptUniversity/fw_showcase/scripts/build.sh /tmp/out   # hoặc thư mục build tùy chọn
```

Các script tự dùng `.venv` của workspace, nên không cần `source`. `build.sh`
build MCUboot và ứng dụng bằng sysbuild, rồi ký ứng dụng bằng khóa của bạn. Các
file kết quả:

| File                                   | Nội dung                  |
| -------------------------------------- | ------------------------- |
| `build/mcuboot/zephyr/zephyr.hex`      | Bootloader MCUboot        |
| `build/fw_showcase/zephyr/zephyr.signed.hex` / `.bin` | Ứng dụng đã ký |

## Nạp firmware (Flash)

Nối ST-LINK vào cổng SWD rồi chạy:

```sh
./fptUniversity/fw_showcase/scripts/flash.sh            # build + nạp MCUboot và ứng dụng
```

Hoặc nạp bản đã build bằng west. Cần kích hoạt venv trước:

```sh
source .venv/bin/activate
west flash -d fptUniversity/fw_showcase/build            # ST-LINK (STM32CubeProgrammer)
west flash -d fptUniversity/fw_showcase/build -r jlink   # J-Link
```

Để chỉ cập nhật ứng dụng, có hai cách:
- nạp `zephyr.signed.hex` vào image-0; hoặc
- đặt image đã ký vào image-1 rồi dùng thao tác chuyển image bằng NRST.

## Console (RTT)

Log nằm ở SEGGER RTT kênh 0. Có thể đọc bằng:
- **J-Link:** `JLinkRTTViewer`, hoặc `JLinkRTTLogger -Device STM32H573RI -If SWD -Speed 4000 -RTTChannel 0 log.txt`
- **ST-LINK + OpenOCD:** chạy `rtt setup 0x20000000 0x40000 "SEGGER RTT"`, rồi
  `rtt start` và `rtt server start 19021 0`, sau đó kết nối tới cổng TCP 19021.

## Sử dụng lần đầu

1. Cắm dây mạng và cấp nguồn. LCD sẽ hiện địa chỉ IP.
2. Mở `http://<ip-thiết-bị>/` và đăng nhập bằng mật khẩu mặc định **`123456`**.
3. Điền phần **MQTT Settings**: broker host, port (`8883` nếu dùng TLS), client
   ID, topic, user và password. Trên thiết bị mới các trường này để trống, và
   MQTT sẽ ở trạng thái `not configured` cho đến khi được điền.
4. **Đổi mật khẩu web**, rồi nhấn **Save & Apply**. Thiết bị sẽ khởi động lại với
   cấu hình mới.

## Cấu trúc project

```text
fptUniversity/fw_showcase/
├── west.yml                 manifest west (Zephyr v4.4.2 + module cần thiết)
├── CMakeLists.txt
├── prj.conf                 Kconfig của ứng dụng
├── sysbuild.conf            chọn MCUboot / kiểu chữ ký
├── sysbuild/mcuboot.conf    Kconfig của MCUboot
├── boards/
│   ├── st/stm32h573ri_custom/       định nghĩa board (DTS, pinctrl, Kconfig)
│   └── stm32h573ri_custom.overlay   thay đổi devicetree riêng cho showcase
├── scripts/
│   ├── setup.sh             cài workspace một lần (west, Python, SDK)
│   ├── build.sh             build (không nạp)
│   └── flash.sh             build + nạp qua ST-LINK
└── src/
    ├── main.c               trình tự khởi động và vòng lặp chính
    ├── app/                 logic ứng dụng
    │   ├── alarm.c/.h         ngưỡng, nháy relay, motor, nút nhấn
    │   ├── alarm_notify.c/.h  tin RS485 ALARM/CLEAR
    │   ├── app_config.c/.h    cấu hình, lưu flash A/B
    │   ├── app_state.c/.h     trạng thái dùng chung giữa main loop, web, MQTT
    │   └── version.h
    ├── drivers/             driver ngoại vi
    │   ├── dht11.c/.h         DHT11 (driver aosong,dht của Zephyr)
    │   ├── lcd_pcf8574.c/.h   HD44780 20x4 qua PCF8574
    │   └── rs485.c/.h         UART4 + chân hướng
    ├── net/                 mạng
    │   ├── network.c/.h       bật Ethernet, DHCP / IP tĩnh
    │   ├── http.c/.h          hàm hỗ trợ HTTP
    │   ├── web_server.c       route, đăng nhập, trang cấu hình
    │   ├── web_pages.h        HTML / CSS
    │   ├── mqtt_app.c/.h      thread MQTT client
    │   └── mqtt_ca.h          chứng chỉ gốc TLS (ISRG Root X1)
    ├── system/
    │   ├── boot_swap.c/.h     confirm image, chuyển image bằng NRST
    │   └── status_led.c/.h    heartbeat LED LIFE
    ├── ui/
    │   └── lcd_view.c/.h      bố cục màn hình LCD
    └── config/
        └── mbedtls_user_config.h   kích thước buffer TLS
```

Web server và MQTT client mỗi cái chạy trong một thread riêng
(`K_THREAD_DEFINE`). Vòng lặp chính đọc cảm biến, xử lý cảnh báo, cập nhật LCD
và chuyển dữ liệu mới cho web/MQTT qua `app_state`.

## Giấy phép

[Apache-2.0](LICENSE)

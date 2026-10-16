# Hướng dẫn SmartEnvironment

Firmware SmartEnvironment 1.0.0 cho Base Board STM32H573RI (Zephyr + MCUboot).

## 1. Showcase làm gì

Giám sát môi trường cho vườn / nhà kính mini và tưới phun sương:

- Đo nhiệt độ, độ ẩm bằng DHT11 mỗi 2 giây, đánh giá môi trường (nóng, lạnh,
  khô, ẩm, quá nóng, lỗi cảm biến) kèm mức cảnh báo và lời khuyên.
- Relay 1 nháy chậm khi cảnh báo, nháy nhanh khi nguy hiểm.
- Bơm mini tự tưới theo chu kỳ khi không khí nóng và khô hoặc quá khô.
- Servo hướng vòi phun: tự quét khi bơm chạy, hoặc chỉnh tay.
- Hiển thị trên LCD 20x4, điều khiển qua trang web, gửi dữ liệu lên MQTT.

## 2. Chuẩn bị

| Thiết bị | Ghi chú |
| -------- | ------- |
| Base Board STM32H573RI + nguồn 24 V DC | relay chỉ chạy khi có 24 V |
| ST-LINK (hoặc J-Link) + cáp cho cổng J3 | nạp firmware |
| LCD 20x4 có module I2C (PCF8574) | |
| Module DHT11 | loại có sẵn điện trở kéo lên |
| Bơm mini + nguồn riêng, ống, vòi phun | |
| Servo SG90 / MG90S + nguồn 5 V riêng | MG90S bền hơn khi gần nước |
| 4 nút nhấn thường mở | |
| Cáp Ethernet, router có DHCP | máy tính cùng mạng LAN với board |
| MQTT broker (tùy chọn) | ví dụ Mosquitto trong mạng LAN |

## 3. Đấu nối

Tắt nguồn board trước khi đấu dây, giữ nước tránh xa board.

| Thiết bị | Chân thiết bị | Nối vào board |
| -------- | ------------- | ------------- |
| DHT11 | VCC / DATA / GND | U16-1 (5 V) / U16-5 / U16-17 (GND) |
| LCD 20x4 | VCC / GND / SDA / SCL | U16-2 / U16-18 / U16-9 / U16-10 |
| Servo | tín hiệu / VCC / GND | U16-16 / 5 V nguồn riêng / GND nguồn riêng **nối chung** U16-17 |
| Bơm (qua Relay 3) | | nguồn (+) → CN3-5, CN3-6 → bơm (+), bơm (−) → nguồn (−) |
| Đèn / còi cảnh báo (tùy chọn) | | Relay 1: CN3-1, CN3-2 |
| BTN1 bơm | | U16-12 và GND |
| BTN2 đổi chế độ servo | | U16-11 và GND |
| BTN3 / BTN4 servo trái / phải | | U16-3 / U16-4 và GND |
| Ethernet, nguồn | | RJ45, jack DC 24 V |

Lưu ý:
- Relay là tiếp điểm khô: phải đưa nguồn của bơm vào COM3. Không nối bơm hay
  servo trực tiếp vào chân GPIO.
- Mắc diode song song hai đầu bơm để chống nhiễu.
- Servo dùng nguồn 5 V riêng nhưng GND phải nối chung với board.

## 4. Cài đặt, build và nạp

Máy ảo của lớp đã cài sẵn môi trường. Cài trên máy mới (Ubuntu):

```sh
mkdir showcase-workspace && cd showcase-workspace
git clone --filter=blob:none --sparse https://github.com/dmi-tech/fptUniversity.git
git -C fptUniversity sparse-checkout set showcase/SmartEnvironment
./fptUniversity/showcase/SmartEnvironment/scripts/setup.sh
```

Lệnh clone trên chỉ tải về showcase này, không tải cả repo. Giữ nguyên tên
thư mục `fptUniversity` và phải clone bằng git (không tải file zip), vì
script cần thông tin của git. Cập nhật code mới: `git -C fptUniversity pull`.

**Khóa ký:** MCUboot chỉ chạy firmware được ký. Dùng khóa chung của lớp, hoặc
tự tạo khóa cho board của riêng mình:

```sh
.venv/bin/python bootloader/mcuboot/scripts/imgtool.py keygen -t rsa-2048 -k ~/keys/showcase-rsa2048.pem
export STM32H573_MCUBOOT_KEY_FILE=~/keys/showcase-rsa2048.pem
```

**Build và nạp** (cắm ST-LINK vào J3, board đã cấp nguồn):

```sh
./fptUniversity/showcase/SmartEnvironment/scripts/flash.sh    # build + nạp
./fptUniversity/showcase/SmartEnvironment/scripts/build.sh    # chỉ build
```

Sau khi nạp: LED LIFE nháy nhanh 3 giây rồi nháy đúp đều; LCD hiện màn hình
khởi động 5 giây rồi tới màn hình trạng thái.

**Xem log:** log ứng dụng qua SEGGER RTT (J-Link RTT Viewer), log MCUboot qua
UART5 (U16-19 / U16-20, 115200).

## 5. Sử dụng lần đầu

1. Cắm Ethernet, cấp nguồn. LCD hiện địa chỉ IP (không có DHCP thì board
   dùng 192.168.1.100).
2. Mở `http://<IP>/` trên trình duyệt, đăng nhập bằng mật khẩu **123456**.
3. Điền thông tin MQTT broker, đổi mật khẩu, bấm **Save & Apply** (board khởi
   động lại).

## 6. Cách sử dụng

**Nút nhấn**

| Nút | Chức năng |
| --- | --------- |
| BTN1 | bật / tắt bơm |
| BTN2 (giữ 3 giây) | đổi chế độ servo AUTO / MANUAL |
| BTN3 / BTN4 (giữ) | quay servo trái / phải (chế độ MANUAL) |

**Bơm:** tự tưới khi môi trường cần nước, mỗi chu kỳ chạy 20 giây rồi nghỉ
60 giây. Nếu tắt bơm bằng tay trong lúc đang tự tưới, chế độ tự động tạm
dừng cho tới khi bấm **Re-enable auto** trên web hoặc môi trường hết cần
nước. Bật bơm bằng tay thì bơm tự tắt sau 10 phút.

**LCD:** dòng 1 IP, dòng 2 nhiệt độ / độ ẩm, dòng 3 bơm và chế độ servo,
dòng 4 trạng thái MQTT. Khi môi trường không tốt, LCD xen kẽ màn hình cảnh
báo ghi lý do.

**Trang web:** xem nhiệt độ, độ ẩm, đánh giá và lời khuyên; bật / tắt bơm,
chọn chế độ và góc servo; cấu hình MQTT và mật khẩu.

**MQTT:** trạng thái đầy đủ gửi mỗi 30 giây lên
`users/admin@example.com/<NNN>/status` (NNN = số cuối của IP); khi một thiết
bị thay đổi, gửi ngay lên `.../pump`, `.../servo`, `.../env`, `.../sensor`,
`.../relay`. Thử bằng: `mosquitto_sub -h <broker> -t 'users/#' -v`.

## 7. Sự cố thường gặp

| Hiện tượng | Cách xử lý |
| ---------- | ---------- |
| LCD sáng nhưng không có chữ | vặn biến trở độ tương phản trên module I2C |
| LCD báo lỗi cảm biến DHT11 | kiểm tra dây DATA (U16-5), nguồn, điện trở kéo lên |
| Bơm không chạy | kiểm tra nguồn 24 V của board và nguồn bơm đã vào COM3 |
| Servo không quay / giật | kiểm tra nguồn 5 V riêng và GND nối chung |
| Không mở được trang web | máy tính phải cùng mạng với board; thử IP 192.168.1.100 |
| LCD báo `MQTT: N/A` | kiểm tra thông tin broker trên web và cáp mạng |
| Quên mật khẩu web | xóa flash: `STM32_Programmer_CLI -c port=SWD -e all`, rồi nạp lại |
| Nạp xong board không chạy | firmware ký sai khóa: chạy lại `flash.sh` để nạp cả MCUboot |

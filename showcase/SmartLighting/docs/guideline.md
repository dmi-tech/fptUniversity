# Hướng dẫn SmartLighting

Firmware SmartLighting 1.0.0 cho Base Board STM32H573RI (Zephyr + MCUboot).

## 1. Showcase làm gì

Đèn tự động bật khi trời tối, tắt khi trời sáng:

- Đo ánh sáng bằng BH1750 mỗi giây. Đèn bật khi trời tối đủ lâu và tắt khi
  trời sáng đủ lâu, nên không chập chờn lúc chạng vạng hay khi có bóng râm,
  đèn xe.
- Bật / tắt tay bằng nút hoặc trang web; hẹn giờ tắt 0-12 giờ.
- Ngưỡng sáng / tối và thời gian xác nhận chỉnh được trên web, lưu lại khi
  mất điện.
- Hiển thị trên LCD 20x4, điều khiển qua trang web, gửi dữ liệu lên MQTT.

## 2. Chuẩn bị

| Thiết bị | Ghi chú |
| -------- | ------- |
| Base Board STM32H573RI + nguồn 24 V DC | relay chỉ chạy khi có 24 V |
| ST-LINK (hoặc J-Link) + cáp cho cổng J3 | nạp firmware |
| LCD 20x4 có module I2C (PCF8574) | |
| Module BH1750 (GY-30 / GY-302) | |
| Đèn 12/24 V + nguồn của đèn | |
| 4 nút nhấn thường mở | |
| Cáp Ethernet, router có DHCP | máy tính cùng mạng LAN với board |
| MQTT broker (tùy chọn) | ví dụ Mosquitto trong mạng LAN |

## 3. Đấu nối

Tắt nguồn board trước khi đấu dây.

| Thiết bị | Chân thiết bị | Nối vào board |
| -------- | ------------- | ------------- |
| BH1750 | VCC / GND / SDA / SCL | U16-1 (5 V) / U16-17 (GND) / U16-9 / U16-10 |
| LCD 20x4 | VCC / GND / SDA / SCL | U16-2 / U16-18 / U16-9 / U16-10 (chung với BH1750) |
| Đèn (qua Relay 3) | | nguồn (+) → CN3-5, CN3-6 → đèn (+), đèn (−) → nguồn (−) |
| BTN1 bật / tắt đèn | | U16-12 và GND |
| BTN2 AUTO / MANUAL | | U16-11 và GND |
| BTN3 / BTN4 hẹn giờ +1 h / −1 h | | U16-3 / U16-4 và GND |
| Ethernet, nguồn | | RJ45, jack DC 24 V |

Lưu ý:
- Đặt BH1750 nơi đèn không chiếu thẳng vào (ví dụ hướng ra cửa sổ).
- Relay 3 chỉ dùng cho đèn điện áp thấp (≤ 24 V DC). Đèn 220 V cần relay /
  contactor ngoài đúng định mức.

## 4. Cài đặt, build và nạp

Máy ảo của lớp đã cài sẵn môi trường. Cài trên máy mới (Ubuntu):

```sh
mkdir showcase-workspace && cd showcase-workspace
git clone --filter=blob:none --sparse https://github.com/dmi-tech/fptUniversity.git
git -C fptUniversity sparse-checkout set showcase/SmartLighting
./fptUniversity/showcase/SmartLighting/scripts/setup.sh
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
./fptUniversity/showcase/SmartLighting/scripts/flash.sh    # build + nạp
./fptUniversity/showcase/SmartLighting/scripts/build.sh    # chỉ build
```

Sau khi nạp: LED LIFE nháy nhanh 3 giây rồi nháy đúp đều; LCD hiện màn hình
khởi động 5 giây rồi tới màn hình trạng thái.

**Trên Windows** (VS Code + extension IDE for Zephyr):

1. Cài [VS Code](https://code.visualstudio.com/), extension **IDE for Zephyr**
   (`mylonics.zephyr-ide`), Git và STM32CubeProgrammer. Trong trang Overview của
   extension chọn **Host Tools → Install Host Tools**, rồi mở lại VS Code.
2. Mở PowerShell, dùng thư mục ngắn (ví dụ `C:\zws`):

   ```powershell
   git config --global core.longpaths true
   mkdir C:\zws; cd C:\zws
   git clone --filter=blob:none --sparse https://github.com/dmi-tech/fptUniversity.git
   git -C fptUniversity sparse-checkout set showcase/SmartLighting
   powershell -ExecutionPolicy Bypass -File fptUniversity\showcase\SmartLighting\scripts\setup.ps1 -KeyFile C:\keys\showcase-rsa2048.pem
   ```

   Thiếu công cụ nào, script in lệnh `winget` để cài. Chưa có khóa thì bỏ
   `-KeyFile`: firmware sẽ được ký bằng `C:\keys\showcase-rsa2048.pem`. Tạo
   khóa đó (trong `C:\zws`): `mkdir C:\keys; .venv\Scripts\python bootloader\mcuboot\scripts\imgtool.py keygen -t rsa-2048 -k C:\keys\showcase-rsa2048.pem`.
3. VS Code: **File → Open Folder** `C:\zws`, rồi `Ctrl+Shift+P` →
   **Zephyr IDE: Setup Workspace from Current Directory** → **Use .west folder**.
4. Chọn project `SmartLighting`, build `build`, bấm **Build** hoặc **Flash**.

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
| BTN1 | bật / tắt đèn (chuyển sang MANUAL) |
| BTN2 | đổi AUTO / MANUAL |
| BTN3 / BTN4 | hẹn giờ tắt +1 giờ / −1 giờ (0 = không hẹn) |

**AUTO (mặc định):** đèn bật khi ánh sáng dưới 10 lx liên tục 30 giây, tắt
khi trên 30 lx liên tục 30 giây. Lần đọc đầu tiên sau khi bật nguồn quyết
định ngay. Firmware tự trừ phần ánh sáng của chính đèn chiếu lên cảm biến.

**MANUAL:** đèn giữ nguyên trạng thái đã bật / tắt bằng tay.

**Hẹn giờ:** tính từ lúc đèn bật, áp dụng cả hai chế độ. Hết giờ đèn tắt; ở
AUTO đèn chờ tới đêm hôm sau.

**LCD:** dòng 1 IP, dòng 2 độ sáng và Dark / Bright, dòng 3 đèn và chế độ,
dòng 4 trạng thái MQTT và thời gian hẹn giờ còn lại. Cảm biến lỗi thì LCD
xen kẽ màn hình cảnh báo.

**Trang web:** xem độ sáng; bật / tắt đèn, chọn chế độ, hẹn giờ; chỉnh ngưỡng
sáng / tối và thời gian xác nhận; cấu hình MQTT và mật khẩu.

**MQTT:** trạng thái đầy đủ gửi mỗi 30 giây lên
`users/admin@example.com/<NNN>/status` (NNN = số cuối của IP); khi có thay
đổi, gửi ngay lên `.../lamp`, `.../light`, `.../mode`, `.../sensor`. Thử bằng:
`mosquitto_sub -h <broker> -t 'users/#' -v`.

## 7. Sự cố thường gặp

| Hiện tượng | Cách xử lý |
| ---------- | ---------- |
| LCD sáng nhưng không có chữ | vặn biến trở độ tương phản trên module I2C |
| LCD báo lỗi cảm biến BH1750 | kiểm tra dây SDA / SCL (chung với LCD) và nguồn |
| Đèn không bật | kiểm tra nguồn 24 V của board và nguồn đèn đã vào COM3 |
| Đèn bật / tắt liên tục | cảm biến đang bị đèn chiếu thẳng: đổi vị trí, hoặc tăng ngưỡng tắt trên web |
| Không mở được trang web | máy tính phải cùng mạng với board; thử IP 192.168.1.100 |
| LCD báo `MQTT: N/A` | kiểm tra thông tin broker trên web và cáp mạng |
| Quên mật khẩu web | xóa flash: `STM32_Programmer_CLI -c port=SWD -e all`, rồi nạp lại |
| Nạp xong board không chạy | firmware ký sai khóa: chạy lại `flash.sh` để nạp cả MCUboot |

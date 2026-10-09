# fptUniversity

Các project firmware nhúng của DMI Tech. Mỗi project nằm trong một thư mục con
riêng, có README, hướng dẫn build và giấy phép riêng.

## Danh sách project

| Project | Mô tả | Nền tảng |
| ------- | ----- | -------- |
| [fw_showcase](fw_showcase) | Theo dõi nhiệt độ/độ ẩm bằng DHT11, hiển thị LCD 20x4, cấu hình qua web, gửi dữ liệu MQTT (hỗ trợ TLS), cảnh báo qua RS485/relay (quá nhiệt thì bật thêm motor), điều khiển motor qua Relay 3 từ web/RS485/nút nhấn. Khởi động an toàn bằng MCUboot. | STM32H573RI, Zephyr v4.4.2 |
| [EmbeddedCourse_FPT_DMI](EmbeddedCourse_FPT_DMI) | Project thực hành môn Lập trình nhúng: ghép 21 driver có sẵn (GPIO, I2C, SPI, UART, RS485, CAN, Ethernet, MQTT...) vào project, log qua SEGGER RTT. Khởi động an toàn bằng MCUboot. | Base Board STMH5 (STM32H573RI), Zephyr v4.4.2 |

## Bắt đầu

Mỗi project có hướng dẫn cài đặt riêng. Với `fw_showcase`, cần có sẵn `git`,
`cmake`, `ninja`, `python3`, `python3-venv` (và STM32CubeProgrammer nếu muốn
nạp firmware):

```sh
mkdir fw_showcase-workspace && cd fw_showcase-workspace
git clone https://github.com/dmi-tech/fptUniversity.git
./fptUniversity/fw_showcase/scripts/setup.sh              # cài một lần: Zephyr, module, SDK

export STM32H573_MCUBOOT_KEY_FILE=~/keys/fw_showcase-rsa2048.pem   # khóa ký MCUboot
./fptUniversity/fw_showcase/scripts/build.sh              # build
./fptUniversity/fw_showcase/scripts/flash.sh              # build + nạp qua ST-LINK
```

Chi tiết xem [fw_showcase/README.vi.md](fw_showcase/README.vi.md)
([English](fw_showcase/README.md)).

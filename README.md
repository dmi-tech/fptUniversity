# fptUniversity

Các project firmware nhúng của DMI Tech. Mỗi project nằm trong một thư mục con
riêng, có README, hướng dẫn build và giấy phép riêng.

## Danh sách project

| Project | Mô tả | Nền tảng |
| ------- | ----- | -------- |
| [showcase](showcase) | Các firmware trình diễn độc lập (mỗi showcase có board, driver, web, MQTT hỗ trợ TLS, LCD 20x4 và MCUboot riêng): **SmartEnvironment** (DHT11, phân tích môi trường kèm cảnh báo và lời khuyên, bơm mini tưới tự động/tay qua Relay 3, servo hướng vòi phun AUTO/MANUAL, 4 nút nhấn, điều khiển qua web, MQTT theo từng thiết bị) và **SmartLighting** (đèn tự bật khi trời tối, tắt khi trời sáng theo cảm biến BH1750, chế độ AUTO/MANUAL, hẹn giờ tắt, chỉnh ngưỡng trên web). Khởi động an toàn bằng MCUboot. | STM32H573RI, Zephyr v4.4.2 |
| [FPT_DMI_Embedded-IoT_Course](FPT_DMI_Embedded-IoT_Course) | Project thực hành môn Lập trình nhúng: ghép 22 driver có sẵn (GPIO, I2C, SPI, UART, RS485, CAN, LM393, Ethernet, MQTT...) vào project, log qua SEGGER RTT. Khởi động an toàn bằng MCUboot. | Base Board STMH5 (STM32H573RI), Zephyr v4.4.2 |

## Bắt đầu

Mỗi project có hướng dẫn cài đặt riêng. Mỗi thư mục trong `showcase/`
(`SmartEnvironment`, `SmartLighting`) là một project độc lập, có sẵn
board, driver, script và tài liệu. Cần có `git`, `cmake`, `ninja`, `python3`,
`python3-venv` (và STM32CubeProgrammer nếu muốn nạp firmware):

```sh
mkdir showcase-workspace && cd showcase-workspace
git clone https://github.com/dmi-tech/fptUniversity.git
./fptUniversity/showcase/SmartEnvironment/scripts/setup.sh      # cài một lần: Zephyr, module, SDK

export STM32H573_MCUBOOT_KEY_FILE=~/keys/showcase-rsa2048.pem   # khóa ký MCUboot
./fptUniversity/showcase/SmartEnvironment/scripts/build.sh      # build
./fptUniversity/showcase/SmartEnvironment/scripts/flash.sh      # build + nạp qua ST-LINK
```

Chi tiết xem [showcase/README.md](showcase/README.md) và README của từng
showcase.

# FPT_DMI_Embedded-IoT_Course

Project thực hành môn **Lập trình nhúng** trên board **Base Board STMH5** (STM32H573RI,
Cortex-M33, Zephyr RTOS v4.4.2). Sinh viên học bằng cách **ghép driver có sẵn vào project**:
mỗi driver có README riêng, chỉ cần sửa 3 file cấu hình (`prj.conf`, `app.overlay`,
`CMakeLists.txt`) và viết `src/main.c`.

Mọi nội dung hướng dẫn bằng tiếng Việt. Log đọc qua **SEGGER RTT** trên dây SWD, không cần cáp UART.

---

## 1. Cần chuẩn bị

**Máy tính:** Ubuntu/Debian (khuyến nghị dùng máy ảo dành riêng cho môn học) hoặc Windows 10/11
với VS Code.

| Công cụ | Phiên bản | Ghi chú |
| --- | --- | --- |
| Zephyr RTOS | v4.4.2 | Tải tự động theo `west.yml` |
| Zephyr SDK | 1.0.1, toolchain `arm-zephyr-eabi` | |
| Git | bản mới | |
| CMake | ≥ 3.20.5 | |
| Ninja | bản mới | |
| Python | ≥ 3.12 | Cần `venv` (Ubuntu/Debian: gói `python3-venv`) |
| Devicetree compiler (DTC) | ≥ 1.4.6 | |
| STM32CubeProgrammer | ≥ 2.23.0 | Nạp bằng ST-LINK, kèm driver ST-LINK trên Windows |
| VS Code | bản mới | Chỉ trên Windows |
| Extension IDE for Zephyr (`mylonics.zephyr-ide`) | ≥ 4.2.0 | Chỉ trên Windows |
| Zephyr IDE Extension Pack (`mylonics.zephyr-ide-extension-pack`) | bản mới | Chỉ trên Windows: Cortex-Debug, C/C++, Serial Monitor |
| 7-Zip | bản mới | Chỉ trên Windows: giải nén Zephyr SDK |

Ubuntu/Debian: `sudo apt install git cmake ninja-build python3 python3-venv device-tree-compiler`.

**Dung lượng ổ đĩa:** để trống ít nhất **10 GB** (Windows: trên ổ `C:`). Toàn bộ chiếm khoảng 7 GB:

| Phần | Dung lượng |
| --- | --- |
| Workspace (Zephyr, module, `.venv`) | ~4,5 GB khi tải cả lịch sử git (extension VS Code); ~2,6 GB với `setup.sh` / `setup.ps1` |
| Zephyr SDK 1.0.1, chỉ toolchain `arm-zephyr-eabi` | ~0,8 GB (cài đủ mọi toolchain: ~10 GB) |
| VS Code, extension, STM32CubeProgrammer, Git, Python, CMake... | ~2 GB |
| Mỗi thư mục build | ~50 MB |

**Phần cứng:**

| Thứ | Dùng cho |
| --- | --- |
| Board Base Board STMH5 | Bắt buộc |
| Mạch nạp ST-LINK (hoặc J-Link) nối J3 (SWD) | Nạp chương trình và đọc log |
| Nguồn 24 V (jack DC1) | Chỉ khi dùng RS485 hoặc CAN nối bus thật |
| Module và linh kiện | Xem bảng "Phần cứng" trong README từng driver |

Nếu chạy trong máy ảo: gắn thiết bị USB của mạch nạp vào máy ảo (không gắn đồng thời cho máy thật).

---

## 2. Cài đặt (một lần)

Thư mục làm việc (*workspace*) chứa bản clone này; Zephyr và các module được tải vào cạnh nó:

```text
course-workspace/
├── fptUniversity/FPT_DMI_Embedded-IoT_Course/   <- project này
├── .venv/  .west/                          <- script tạo
└── zephyr/  modules/  bootloader/          <- script tải về
```

```bash
mkdir course-workspace && cd course-workspace
git clone https://github.com/dmi-tech/fptUniversity.git

./fptUniversity/FPT_DMI_Embedded-IoT_Course/scripts/setup.sh       # Zephyr, module, SDK (mất vài phút)
./fptUniversity/FPT_DMI_Embedded-IoT_Course/scripts/setup_udev.sh  # Linux: quyền USB cho ST-LINK/J-Link
```

Sau `setup_udev.sh`: rút và cắm lại mạch nạp, đăng xuất rồi đăng nhập lại (nhóm `plugdev`,
`dialout` mới có hiệu lực).

> `setup.sh` chạy lại được nhiều lần. Nếu `.venv` bị hỏng (ví dụ sau khi nâng cấp Python của hệ
> thống) nó tự đổi tên venv cũ và tạo venv mới.

### Trên Windows: VS Code + IDE for Zephyr

Cài các công cụ ở mục 1 (gồm các dòng "Chỉ trên Windows"). Các bước cài đặt và cấu hình trong
VS Code nằm trong slide hướng dẫn riêng *FPT_DMI_WindowsSetupGuideline*.

---

## 3. Build, nạp, xem log

```bash
cd course-workspace/fptUniversity/FPT_DMI_Embedded-IoT_Course

./scripts/build.sh      # build (MCUboot + ứng dụng đã ký), không nạp
./scripts/flash.sh      # build + nạp qua ST-LINK   (RUNNER=jlink ./scripts/flash.sh cho J-Link)
./scripts/rtt.sh        # xem printk qua SWD, Ctrl+C để thoát
```

`src/main.c` ban đầu là chương trình "Hello, World !!!" (in qua RTT), build được ngay. Log của
**MCUboot** đi qua UART5 (U16-19 TX, U16-20 RX, 115200 8N1), log của **ứng dụng** đi qua RTT.

> **Khóa ký MCUboot.** Nếu không đặt `STM32H573_MCUBOOT_KEY_FILE`, `build.sh` ký bằng **khóa dev
> công khai** của MCUboot và in cảnh báo. Khóa này chỉ dùng cho lab: ai cũng ký được ảnh mà
> bootloader chấp nhận. Không dùng cho sản phẩm. Có khóa riêng thì:
> `export STM32H573_MCUBOOT_KEY_FILE=~/keys/my-rsa2048.pem` (khóa không được đưa vào repo; `*.pem`
> đã nằm trong `.gitignore`).

---

## 4. Cách dùng driver

Bạn chỉ sửa **4 file** ở thư mục gốc project:

| File | Việc |
| --- | --- |
| `src/main.c` | Chương trình của bạn |
| `CMakeLists.txt` | Thêm file `.c` và thư mục header của driver |
| `prj.conf` | Bật các `CONFIG_...` mà driver cần |
| `app.overlay` | Khai báo phần cứng (chân, alias) cho driver |

Các bước: đọc README của driver → dán cấu hình → chép ví dụ `main.c` → `build.sh` →
`flash.sh` → `rtt.sh`. **Không sửa** thư mục `driver/` và
`boards/` trong lúc làm việc. Hướng dẫn chung (cú pháp overlay, gộp nhiều driver, xung đột chân,
lỗi thường gặp): [`driver/README.md`](driver/README.md).

---

## 5. Danh sách driver

| Nhóm | Driver |
| --- | --- |
| Ngoại vi trong chip | [`gpio`](driver/peripherals/gpio/README.md), [`timer`](driver/peripherals/timer/README.md), [`watchdog`](driver/peripherals/watchdog/README.md), [`adc`](driver/peripherals/adc/README.md), [`pwm`](driver/peripherals/pwm/README.md) |
| Giao thức | [`i2c`](driver/protocols/i2c/README.md), [`spi`](driver/protocols/spi/README.md), [`uart`](driver/protocols/uart/README.md), [`rs485`](driver/protocols/rs485/README.md), [`can`](driver/protocols/can/README.md) |
| Thiết bị | [`sht41`](driver/devices/sht41/README.md), [`rtc`](driver/devices/rtc/README.md), [`dht11`](driver/devices/dht11/README.md), [`bh1750`](driver/devices/bh1750/README.md), [`hcsr04`](driver/devices/hcsr04/README.md), [`encoder`](driver/devices/encoder/README.md), [`lm393`](driver/devices/lm393/README.md), [`lcd_pcf8574`](driver/devices/lcd_pcf8574/README.md), [`motor`](driver/devices/motor/README.md), [`servo`](driver/devices/servo/README.md), [`ethernet`](driver/devices/ethernet/README.md) |
| Dịch vụ | [`mqtt`](driver/services/mqtt/README.md) (broker mosquitto: `scripts/mqtt_broker.sh`) |

Phụ thuộc đi một chiều: `services → devices → (peripherals, protocols)`. README của mỗi driver
luôn ghi rõ phải thêm những driver nào.

Bảng chân và ràng buộc: [`docs/pinout.md`](docs/pinout.md).

---

## 6. Cấu trúc thư mục

```text
FPT_DMI_Embedded-IoT_Course/
├── src/main.c  CMakeLists.txt  prj.conf  app.overlay   # 4 file của sinh viên
├── driver/            # Driver + README từng driver (peripherals, protocols, devices, services)
├── boards/            # Mô tả board stm32h573ri_custom (không sửa)
├── sysbuild/  sysbuild.conf   # MCUboot
├── scripts/           # setup (+ setup.ps1 cho Windows), build, flash, rtt, setup_udev, mqtt_broker
├── docs/              # pinout.md (bảng chân), test_report.md
├── tests/             # Kiểm tra build cho giảng viên (mục 7)
├── west.yml           # Manifest west (Zephyr v4.4.2 + module cần dùng)
└── LICENSE            # Apache-2.0
```

---

## 7. Dành cho giảng viên: kiểm tra build

Các bộ kiểm tra **chỉ build, không nạp**; chạy sau khi sửa driver hoặc README:

```bash
./tests/build_all/run.sh          # link toàn bộ driver trong nhiều cấu hình
./tests/readme_examples/run.sh    # build ví dụ main.c của từng README (ghép với overlay/Kconfig của README đó)
```

`build_all` gồm cấu hình đầy đủ và cấu hình "không có alias" (driver phải vẫn build và trả
`-ENODEV` lúc chạy). `readme_examples` trích khối code của README, nên README sai cú pháp hay
thiếu dòng cấu hình sẽ bị phát hiện.

Điểm cần kiểm tra trên phần cứng thật (chưa xác nhận bằng build): xem các mục **[CẦN KIỂM TRA]**
trong `docs/pinout.md` và README các driver, ví dụ board lắp R87 hay R88 (RS485), địa chỉ PCF8574,
khả năng chịu 5 V của chân U16.

---

## 8. Gặp lỗi?

Bảng lỗi chung: [`driver/README.md` mục 6](driver/README.md). Lỗi riêng của từng driver ở mục
"Lỗi thường gặp" trong README của driver đó. Thông báo `-19` là `-ENODEV` (thiết bị chưa sẵn
sàng), `-5` là `-EIO` (thiết bị không trả lời), `-22` là `-EINVAL` (tham số sai).

### Lỗi khi cài đặt trên Windows (VS Code + IDE for Zephyr)

| Triệu chứng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Install 7-Zip / Host Tools: *Variable workspaceFolder can not be resolved* | VS Code chưa mở thư mục nào | File → Open Folder `C:\zws`, rồi cài lại |
| Host Tools: *winget not found* | Thiếu App Installer | Cài App Installer từ Microsoft Store |
| Extension biến mất sau khi mở thư mục | Thư mục chưa được tin tưởng (Restricted Mode) | Bấm **Restricted Mode** trên thanh trạng thái → **Trust** |
| `west`, `cmake`, `ninja` not found | Công cụ mới cài chưa có trong PATH | Đóng và mở lại VS Code |
| Lỡ cài mọi toolchain của SDK (~10 GB) | Chọn hết toolchain khi cài SDK | Trang Zephyr SDK: bỏ chọn, chỉ giữ `arm-zephyr-eabi` → **Apply Changes**; hoặc xóa các thư mục `gnu\<toolchain>` trong `%USERPROFILE%\.zephyr_ide\toolchains\zephyr-sdk-1.0.1`. Không bấm icon thùng rác (xóa cả SDK) |
| Workspace Setup chỉ gợi ý *Create new west.yml* | Không có thư mục `.west` (ví dụ tạo nhầm file `config.west`) | Explorer → New File, gõ `.west/config` |
| Add Build dừng ngay sau khi chọn thư mục board | West update chưa xong, extension chưa biết phiên bản Zephyr | Chờ West Update hiện *Updated*, rồi Add Build lại |
| West update: *Fatal error in launcher … python.exe* | `.venv` bị chuyển theo workspace, còn trỏ tới đường dẫn cũ | Xóa `.venv`, chạy lại Workspace Setup → From Workspace Directory → Use .west folder |
| Build: file `.obj` *No such file or directory*, hoặc *Filename too long* | Đường dẫn dài quá 260 ký tự | Đặt workspace ở `C:\zws`, đặt tên build là `build` |
| Build: *Build directory … is for application …* | Thư mục build tạo ở đường dẫn khác | Xóa thư mục build, bấm **Build Pristine** |
| Flash: *Unable to get core ID* / không thấy ST-LINK | SWD chập chờn, ST-LINK đang bị chương trình khác giữ, hoặc thiếu driver | Tắt 24 V, cắm lại SWD, đóng STM32CubeProgrammer GUI; cài lại STM32CubeProgrammer |
| `rtt_pyocd.py`: *Target type stm32h573ritx not recognized* | pyocd chưa có pack STM32H5 | `.venv\Scripts\pyocd pack install stm32h573ritx` |
| Cảnh báo *Overlapping memory regions … (STM32H543UGYx)* | Dữ liệu của chip khác trong cùng pack | Vô hại, bỏ qua |
| `rtt_pyocd.py` kết nối được nhưng không in gì | Banner đã in lúc khởi động, trước khi kết nối | Chạy với `--reset` (khởi động lại, đọc từ đầu) hoặc `--all` |
| *No available debug probes are connected* | Chương trình khác đang giữ ST-LINK | Đóng STM32CubeProgrammer GUI hoặc phiên debug, cắm lại ST-LINK |

Chuyển workspace sang thư mục khác: xóa `.venv` và thư mục build, chạy lại Workspace Setup (Use .west
folder), rồi **Build Pristine**. Zephyr và các module được giữ nguyên, không phải tải lại.

## Giấy phép

Apache-2.0, xem [`LICENSE`](LICENSE).

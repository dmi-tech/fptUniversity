# Driver MQTT – Gửi/nhận dữ liệu IoT

> Gửi dữ liệu cảm biến lên broker MQTT và nhận lệnh điều khiển từ máy tính/điện thoại. Không mã
> hóa (TLS) – chỉ dùng trong mạng lab.

**Cần thêm driver:** `devices/ethernet`.

---

## 1. Phần cứng và mạng

```text
 [Board] ──RJ45── [Router lab] ── [Máy tính chạy broker mosquitto]
     publish  fpt/SE123456/temp  ──►  broker  ──►  mosquitto_sub (máy tính)
     subscribe fpt/SE123456/cmd  ◄──  broker  ◄──  mosquitto_pub (máy tính)
```

Board và máy tính phải **cùng mạng** (máy ảo: card mạng chế độ **Bridged**).

### Chạy broker trên máy tính
```bash
./scripts/mqtt_broker.sh        # cài + chạy mosquitto cổng 1883, in IP máy tính
```
Ghi lại IP máy tính mà script in ra (ví dụ `192.168.1.20`) để điền vào code.

## 2. Xung đột tài nguyên
Như driver `ethernet`.

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
Toàn bộ khối `prj.conf` của driver `ethernet`, **cộng thêm**:
```
CONFIG_MQTT_LIB=y
CONFIG_NET_MAX_CONN=8
CONFIG_NET_MAX_CONTEXTS=12
CONFIG_ZVFS_POLL_MAX=6
CONFIG_POSIX_API=y
```

### Bước 2 – `app.overlay`
Như driver `ethernet` (đổi MAC).

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/ethernet
  ${DRIVER_DIR}/services/mqtt
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/ethernet/driver_ethernet.c
  ${DRIVER_DIR}/services/mqtt/driver_mqtt.c
)
```

---

## 4. Khái niệm nhanh

| Khái niệm | Ý nghĩa |
| --- | --- |
| Broker | "Bưu điện" trung gian, mọi client kết nối tới đây |
| Topic | "Địa chỉ" của tin nhắn, dạng thư mục: `fpt/SE123456/temp` |
| Publish | Gửi tin lên một topic |
| Subscribe | Đăng ký nhận tin của topic. `+` = 1 cấp bất kỳ, `#` = mọi cấp còn lại |
| QoS 0 | Gửi 1 lần, không xác nhận (nhanh) |
| QoS 1 | Đảm bảo tới ít nhất 1 lần |
| Client ID | Tên duy nhất của mỗi client – **trùng tên thì broker ngắt client cũ** |

Quy ước topic của môn học: `fpt/<mã SV>/<tên>`, ví dụ `fpt/SE123456/temp`.

---

## 5. API (`driver_mqtt.h`)

```c
typedef void (*driver_mqtt_msg_cb_t)(const char *topic, const uint8_t *payload, size_t len);
```

| Hàm | Mô tả |
| --- | --- |
| `int driver_mqtt_init(const char *broker_ip, uint16_t port, const char *client_id)` | Lưu cấu hình (chưa kết nối) |
| `int driver_mqtt_set_auth(const char *user, const char *pass)` | Đăng nhập bằng user/password (broker không cho ẩn danh). Gọi sau `init`, trước `connect`. User ≤ 63, password ≤ 255 ký tự. Gửi dạng rõ (không TLS) |
| `int driver_mqtt_connect(k_timeout_t timeout)` | Kết nối broker (cần có IP trước) |
| `bool driver_mqtt_is_connected(void)` | Đang kết nối? |
| `int driver_mqtt_publish(const char *topic, const char *payload, uint8_t qos)` | Gửi chuỗi |
| `int driver_mqtt_publishf(const char *topic, uint8_t qos, const char *fmt, ...)` | Gửi kiểu `printf` |
| `int driver_mqtt_subscribe(const char *topic, uint8_t qos, driver_mqtt_msg_cb_t cb)` | Đăng ký nhận. Tối đa 4 topic |
| `int driver_mqtt_process(k_timeout_t timeout)` | **Phải gọi thường xuyên** trong vòng lặp: nhận tin, gửi keep-alive, **tự kết nối lại** khi mất |

Callback được gọi từ bên trong `driver_mqtt_process` (cùng thread với `main`) → an toàn để điều
khiển driver khác. **Không** gọi `driver_mqtt_publish` bên trong callback quá nhiều lần.

---

## 6. Ví dụ `src/main.c` – gửi nhiệt độ, nhận lệnh bật/tắt LED

Dùng thêm driver `sht41` và `gpio` (thêm các dòng cấu hình của 2 driver đó; với `gpio`
chỉ cần dòng `aliases { out0 = &user_led; };` trong `app.overlay`).

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include "driver_ethernet.h"
#include "driver_mqtt.h"
#include "driver_sht41.h"
#include "driver_gpio.h"

#define BROKER_IP  "192.168.1.20"     /* IP máy tính chạy mosquitto */
#define MSSV       "SE123456"         /* mã sinh viên của bạn */

static void on_cmd(const char *topic, const uint8_t *payload, size_t len)
{
	printk("Nhan [%s]: %.*s\n", topic, (int)len, payload);
	if (len == 2 && memcmp(payload, "on", 2) == 0) {
		driver_gpio_set(0, true);    /* LED LIFE sáng */
	} else if (len == 3 && memcmp(payload, "off", 3) == 0) {
		driver_gpio_set(0, false);
	}
}

int main(void)
{
	int64_t last_pub = 0;

	driver_sht41_init();
	driver_gpio_out_init(0);
	driver_ethernet_init();
	if (driver_ethernet_wait_ip(K_SECONDS(30)) < 0) {
		printk("Khong co IP\n");
		return 0;
	}

	driver_mqtt_init(BROKER_IP, 1883, "board-" MSSV);
	while (driver_mqtt_connect(K_SECONDS(5)) < 0) {
		printk("Chua ket noi duoc broker, thu lai...\n");
		k_msleep(2000);
	}
	driver_mqtt_subscribe("fpt/" MSSV "/cmd", 1, on_cmd);
	printk("Da ket noi MQTT\n");

	while (1) {
		driver_mqtt_process(K_MSEC(100));

		if (k_uptime_get() - last_pub >= 10000) {
			float t, h;

			last_pub = k_uptime_get();
			if (driver_sht41_read(&t, &h) == 0) {
				driver_mqtt_publishf("fpt/" MSSV "/temp", 0, "%.2f", (double)t);
				driver_mqtt_publishf("fpt/" MSSV "/hum", 0, "%.1f", (double)h);
			}
		}
	}
	return 0;
}
```
(`prj.conf` cần thêm `CONFIG_CBPRINTF_FP_SUPPORT=y` để in số thực.)

## 7. Kiểm tra trên máy tính

Cửa sổ 1 – xem dữ liệu board gửi:
```bash
mosquitto_sub -h localhost -t 'fpt/SE123456/#' -v
```
```text
fpt/SE123456/temp 28.47
fpt/SE123456/hum 61.3
```

Cửa sổ 2 – gửi lệnh cho board:
```bash
mosquitto_pub -h localhost -t 'fpt/SE123456/cmd' -m on
```
→ LED LIFE trên board sáng, RTT in `Nhan [fpt/SE123456/cmd]: on`. Gửi `-m off` để tắt.

Điện thoại: cài app "MQTT Dash" / "IoT MQTT Panel", nhập IP máy tính, cổng 1883.

---

## 8. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| `connect` trả `-ECONNREFUSED` | Broker chưa chạy / chặn kết nối ngoài | Chạy `scripts/mqtt_broker.sh` (cấu hình `listener 1883` + `allow_anonymous true`) |
| `connect` hết giờ | Sai IP broker; máy ảo NAT; tường lửa | Ping IP máy tính từ mạng; Bridged; `sudo ufw allow 1883` |
| Kết nối rồi bị ngắt liên tục | Trùng client ID với bạn khác | Dùng mã SV trong client ID |
| Không nhận được lệnh | Không gọi `driver_mqtt_process` đủ thường xuyên; sai topic | Gọi trong vòng lặp; kiểm tra chính tả topic |
| Mất kết nối sau vài phút | `process` bị chặn lâu (vd `k_msleep(60000)`) → hết keep-alive | Không chặn vòng lặp lâu |

## 9. Tham khảo
- Zephyr MQTT: https://docs.zephyrproject.org/latest/connectivity/networking/api/mqtt.html
- MQTT 3.1.1: https://mqtt.org/mqtt-specification/
- Mosquitto: https://mosquitto.org/

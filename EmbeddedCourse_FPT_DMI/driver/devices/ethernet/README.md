# Driver Ethernet – Kết nối mạng qua W5500 (cổng RJ45)

> Đưa board lên mạng LAN: lấy IP tự động (DHCP) hoặc IP tĩnh. Là nền cho MQTT, HTTP, NTP...

---

## 1. Phần cứng

| Thông số | Giá trị |
| --- | --- |
| Chip | WIZnet W5500 (U9), 10/100 Mbit/s |
| Giao tiếp MCU | SPI1: SCK PA5, MISO PA6, MOSI PA7, CS PA4; INT PC4; RESET PC5 |
| Cổng | RJ45 J1, có LED Link (xanh) / Act (vàng) |

Đấu nối: cắm cáp mạng từ RJ45 của board vào **router/switch** của phòng lab (có DHCP).

Nối thẳng vào máy tính (không qua router): không có DHCP → dùng **IP tĩnh** (mục 5) và đặt IP
tĩnh cho máy tính cùng dải.

Máy ảo: đặt card mạng máy ảo ở chế độ **Bridged** để máy ảo và board cùng mạng.

## 2. Xung đột tài nguyên

| Tài nguyên | Ghi chú |
| --- | --- |
| PA4–PA7, PC4, PC5 | Dành riêng cho W5500 (kể cả khi không dùng mạng) |
| EXTI4 (PC4) | Không dùng ngắt trên PA4, PB4... cùng lúc |
| Địa chỉ MAC | **Mỗi board một MAC khác nhau** trong cùng phòng lab (xem overlay) |

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_SPI=y
CONFIG_ETH_W5500=y
CONFIG_NETWORKING=y
CONFIG_NET_L2_ETHERNET=y
CONFIG_NET_IPV4=y
CONFIG_NET_TCP=y
CONFIG_NET_UDP=y
CONFIG_NET_SOCKETS=y
CONFIG_NET_DHCPV4=y
CONFIG_NET_MGMT=y
CONFIG_NET_MGMT_EVENT=y
CONFIG_NET_TX_STACK_SIZE=2048
CONFIG_NET_RX_STACK_SIZE=2048
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_HEAP_MEM_POOL_SIZE=16384
```
| Dòng | Ý nghĩa |
| --- | --- |
| `ETH_W5500` | Driver chip W5500 |
| `NETWORKING`, `NET_L2_ETHERNET` | Bật ngăn xếp mạng, lớp Ethernet |
| `NET_IPV4`, `NET_TCP`, `NET_UDP` | Giao thức IPv4, TCP, UDP (DHCP dùng UDP) |
| `NET_SOCKETS` | API socket (giống Linux) |
| `NET_DHCPV4` | Lấy IP tự động |
| `NET_MGMT`, `NET_MGMT_EVENT` | Nhận sự kiện "đã có IP" |
| `*_STACK_SIZE`, `HEAP_MEM_POOL_SIZE` | Bộ nhớ cho thread mạng |

### Bước 2 – `app.overlay`
SPI1 và W5500 đã có trong board DTS. **Đổi MAC** để không trùng board khác:
```dts
&w5500 {
	local-mac-address = [00 08 DC 01 02 07];   /* đổi byte cuối = số thứ tự board của bạn (hex) */
};
```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/ethernet
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/ethernet/driver_ethernet.c
)
```

---

## 4. API (`driver_ethernet.h`)

| Hàm | Mô tả |
| --- | --- |
| `int driver_ethernet_init(void)` | Khởi tạo, bắt đầu DHCP |
| `int driver_ethernet_wait_ip(k_timeout_t timeout)` | Chờ có IP. `-EAGAIN` nếu hết giờ |
| `bool driver_ethernet_has_ip(void)` | Đã có IP chưa |
| `bool driver_ethernet_link_up(void)` | Cáp đã cắm và có link chưa |
| `int driver_ethernet_get_ip(char *buf, size_t len)` | IP dạng chuỗi `"192.168.1.57"` (`len` ≥ 16) |
| `int driver_ethernet_set_static(const char *ip, const char *mask, const char *gw)` | Dùng IP tĩnh thay vì DHCP – gọi **thay cho** chờ DHCP |

---

## 5. Ví dụ `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_ethernet.h"

#define USE_STATIC_IP 0     /* 1 = nối thẳng máy tính, không có router */

int main(void)
{
	char ip[16];
	int ret = driver_ethernet_init();

	if (ret < 0) {
		printk("Loi Ethernet: %d\n", ret);
		return 0;
	}

#if USE_STATIC_IP
	driver_ethernet_set_static("192.168.10.50", "255.255.255.0", "192.168.10.1");
#else
	printk("Dang cho DHCP...\n");
	ret = driver_ethernet_wait_ip(K_SECONDS(30));
	if (ret < 0) {
		printk("Khong lay duoc IP (%d). Cap mang da cam chua?\n", ret);
		return 0;
	}
#endif
	driver_ethernet_get_ip(ip, sizeof(ip));
	printk("IP cua board: %s\n", ip);

	while (1) {
		printk("Link: %s\n", driver_ethernet_link_up() ? "UP" : "DOWN");
		k_msleep(5000);
	}
	return 0;
}
```

## 6. Kiểm tra
RTT in `IP cua board: 192.168.1.57`. Trên máy tính:
```bash
ping 192.168.1.57
```
Có phản hồi → board đã lên mạng.

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| `wait_ip` hết giờ, LED RJ45 tắt | Chưa cắm cáp / cáp hỏng | Kiểm tra cáp, LED Link phải sáng |
| LED sáng nhưng không có IP | Mạng không có DHCP (nối thẳng máy tính) | Dùng IP tĩnh |
| Có IP nhưng máy ảo không ping được | Máy ảo ở chế độ NAT | Chuyển sang Bridged |
| Hai board tranh IP, mất kết nối chập chờn | Trùng MAC | Đổi `local-mac-address` |
| Hard fault / treo khi khởi động mạng | Stack/heap nhỏ | Giữ đủ các dòng bộ nhớ trong `prj.conf` |

## 8. Tham khảo
- Zephyr Networking: https://docs.zephyrproject.org/latest/connectivity/networking/index.html
- Datasheet WIZnet W5500

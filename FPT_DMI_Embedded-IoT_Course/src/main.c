#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/socket.h>
#include "driver_ethernet.h"
#include "driver_mqtt.h"

/*
 * ===================== CẤU HÌNH MQTT BROKER =====================
 * BROKER_HOST: địa chỉ broker, điền tên miền (URL) hoặc IP đều được.
 *              Chỉ điền phần tên, bỏ "mqtt://", "tcp://" và ":port" ở đầu/cuối.
 *              Ví dụ: URL "mqtt://broker.abc.com:1883" -> "broker.abc.com"
 * BROKER_PORT: cổng TCP của broker, thường là 1883.
 *              Driver không hỗ trợ TLS nên không dùng được cổng 8883.
 * CLIENT_ID  : tên của board trên broker, tối đa 31 ký tự.
 *              Mỗi board phải dùng một tên khác nhau (trùng tên thì broker ngắt board cũ).
 * MQTT_USER  : tên đăng nhập. Để "" nếu broker cho phép ẩn danh (allow_anonymous true).
 * MQTT_PASS  : mật khẩu. Để "" nếu không cần. Mật khẩu được gửi dạng rõ (không mã hóa).
 * TOPIC_PUB  : topic board gửi dữ liệu lên.
 * TOPIC_SUB  : topic board đăng ký nhận tin.
 *              Ký tự '+' khớp 1 cấp, '#' khớp mọi cấp còn lại.
 * =================================================================
 */
#define BROKER_HOST  "broker.abc.com"
#define BROKER_PORT  1883
#define CLIENT_ID    "board-SE123456"
#define MQTT_USER    ""
#define MQTT_PASS    ""
#define TOPIC_PUB    "fpt/SE123456/data"
#define TOPIC_SUB    "fpt/SE123456/cmd"

#define PUB_PERIOD_MS 5000

/* Tra DNS: đổi tên miền BROKER_HOST ra địa chỉ IP (dạng chuỗi "x.x.x.x") */
static int resolve_host(const char *host, char *ip, size_t len)
{
	struct zsock_addrinfo hints = {
		.ai_family = NET_AF_INET,
		.ai_socktype = NET_SOCK_STREAM,
	};
	struct zsock_addrinfo *res = NULL;
	struct net_sockaddr_in *addr;

	if (zsock_getaddrinfo(host, NULL, &hints, &res) != 0 || res == NULL) {
		return -1;
	}
	addr = (struct net_sockaddr_in *)res->ai_addr;
	net_addr_ntop(NET_AF_INET, &addr->sin_addr, ip, len);
	zsock_freeaddrinfo(res);
	return 0;
}

/* Được gọi mỗi khi có tin nhắn tới topic đã subscribe */
static void on_message(const char *topic, const uint8_t *payload, size_t len)
{
	printk("Nhan [%s]: %.*s\n", topic, (int)len, payload);
}

int main(void)
{
	char ip[DRIVER_ETHERNET_IP_STR_LEN];
	char broker_ip[DRIVER_ETHERNET_IP_STR_LEN];
	uint32_t count = 0;
	int64_t last_pub = 0;

	printk("Example: MQTT publish / subscribe\n");

	/* 1. Lên mạng, lấy IP bằng DHCP */
	if (driver_ethernet_init() < 0) {
		printk("Loi khoi tao Ethernet\n");
		return 0;
	}
	if (driver_ethernet_wait_ip(K_SECONDS(30)) < 0) {
		printk("Khong lay duoc IP, kiem tra cap mang\n");
		return 0;
	}
	driver_ethernet_get_ip(ip, sizeof(ip));
	printk("IP cua board: %s\n", ip);

	/* 2. Đổi tên miền broker ra IP (DNS do router cấp qua DHCP) */
	while (resolve_host(BROKER_HOST, broker_ip, sizeof(broker_ip)) < 0) {
		printk("Khong tra duoc DNS cho %s, thu lai...\n", BROKER_HOST);
		k_msleep(3000);
	}
	printk("Broker %s -> %s:%d\n", BROKER_HOST, broker_ip, BROKER_PORT);

	/* 3. Cấu hình broker, user/password */
	if (driver_mqtt_init(broker_ip, BROKER_PORT, CLIENT_ID) < 0) {
		printk("Sai BROKER_PORT / CLIENT_ID\n");
		return 0;
	}
	if (MQTT_USER[0] != '\0') {
		driver_mqtt_set_auth(MQTT_USER, MQTT_PASS);
	}

	/* 4. Đăng ký topic: driver tự subscribe lại sau mỗi lần kết nối lại */
	driver_mqtt_subscribe(TOPIC_SUB, 1, on_message);

	/* 5. Kết nối broker */
	while (driver_mqtt_connect(K_SECONDS(5)) < 0) {
		printk("Chua ket noi duoc broker %s:%d, thu lai...\n", BROKER_HOST, BROKER_PORT);
		k_msleep(2000);
	}
	printk("Da ket noi MQTT\n");

	while (1) {
		/* Phải gọi thường xuyên: nhận tin, giữ kết nối, tự kết nối lại khi mất */
		driver_mqtt_process(K_MSEC(100));

		/* Publish mỗi PUB_PERIOD_MS */
		if (k_uptime_get() - last_pub >= PUB_PERIOD_MS) {
			last_pub = k_uptime_get();
			count++;
			if (driver_mqtt_publishf(TOPIC_PUB, 0, "count=%u", count) == 0) {
				printk("Gui [%s]: count=%u\n", TOPIC_PUB, count);
			}
		}
	}
	return 0;
}

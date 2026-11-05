#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/socket.h>
#include "driver_ethernet.h"
#include "driver_mqtt.h"

/*
 * ===================== MQTT BROKER SETTINGS =====================
 * BROKER_HOST: broker address, either a domain name (URL) or an IP.
 *              Host name only: drop "mqtt://", "tcp://" and ":port".
 *              Example: URL "mqtt://broker.abc.com:1883" -> "broker.abc.com"
 * BROKER_PORT: TCP port of the broker, usually 1883.
 *              The driver has no TLS, so port 8883 does not work.
 * CLIENT_ID  : name of this board on the broker, at most 31 characters.
 *              Every board needs its own name (the broker drops a client
 *              when another one connects with the same name).
 * MQTT_USER  : user name. Leave "" if the broker allows anonymous clients.
 * MQTT_PASS  : password. Leave "" if not needed. Sent in clear text (no TLS).
 * TOPIC_PUB  : topic the board publishes to.
 * TOPIC_SUB  : topic the board subscribes to.
 *              '+' matches one level, '#' matches all remaining levels.
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

/* DNS lookup: turn the host name into an IP address string "x.x.x.x" */
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

/* Called for every message on a subscribed topic */
static void on_message(const char *topic, const uint8_t *payload, size_t len)
{
	printk("Received [%s]: %.*s\n", topic, (int)len, payload);
}

int main(void)
{
	char ip[DRIVER_ETHERNET_IP_STR_LEN];
	char broker_ip[DRIVER_ETHERNET_IP_STR_LEN];
	uint32_t count = 0;
	int64_t last_pub = 0;

	printk("Example: MQTT publish / subscribe\n");

	/* 1. Join the network, get an IP address by DHCP */
	if (driver_ethernet_init() < 0) {
		printk("Ethernet init failed\n");
		return 0;
	}
	if (driver_ethernet_wait_ip(K_SECONDS(30)) < 0) {
		printk("No IP address, check the network cable\n");
		return 0;
	}
	driver_ethernet_get_ip(ip, sizeof(ip));
	printk("Board IP: %s\n", ip);

	/* 2. Resolve the broker name (DNS server given by the router over DHCP) */
	while (resolve_host(BROKER_HOST, broker_ip, sizeof(broker_ip)) < 0) {
		printk("DNS lookup for %s failed, retrying...\n", BROKER_HOST);
		k_msleep(3000);
	}
	printk("Broker %s -> %s:%d\n", BROKER_HOST, broker_ip, BROKER_PORT);

	/* 3. Broker address, user name and password */
	if (driver_mqtt_init(broker_ip, BROKER_PORT, CLIENT_ID) < 0) {
		printk("Invalid BROKER_PORT / CLIENT_ID\n");
		return 0;
	}
	if (MQTT_USER[0] != '\0') {
		driver_mqtt_set_auth(MQTT_USER, MQTT_PASS);
	}

	/* 4. Subscribe: the driver subscribes again after every reconnection */
	driver_mqtt_subscribe(TOPIC_SUB, 1, on_message);

	/* 5. Connect to the broker */
	while (driver_mqtt_connect(K_SECONDS(5)) < 0) {
		printk("Cannot connect to broker %s:%d, retrying...\n", BROKER_HOST, BROKER_PORT);
		k_msleep(2000);
	}
	printk("MQTT connected\n");

	while (1) {
		/* Call often: receives messages, keeps the connection alive, reconnects */
		driver_mqtt_process(K_MSEC(100));

		/* Publish every PUB_PERIOD_MS */
		if (k_uptime_get() - last_pub >= PUB_PERIOD_MS) {
			last_pub = k_uptime_get();
			count++;
			if (driver_mqtt_publishf(TOPIC_PUB, 0, "count=%u", count) == 0) {
				printk("Sent [%s]: count=%u\n", TOPIC_PUB, count);
			}
		}
	}
	return 0;
}

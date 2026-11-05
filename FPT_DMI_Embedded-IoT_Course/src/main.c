#include <strings.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/socket.h>
#include "driver_ethernet.h"
#include "driver_mqtt.h"
#include "driver_gpio.h"
#include "driver_dht11.h"

/*
 * ===================== MQTT BROKER SETTINGS =====================
 * BROKER_HOST: broker address, either a domain name (URL) or an IP.
 *              Host name only: drop "mqtt://", "tcp://" and ":port".
 *              Example: URL "mqtt://broker.abc.com:1883" -> "broker.abc.com"
 * BROKER_PORT: TCP port of the broker, usually 1883.
 *              The driver has no TLS, so port 8883 does not work.
 * DEVICE_NAME: name of this board, used as the MQTT client id and in the
 *              topics. At most 31 characters. Every board needs its own name
 *              (the broker drops a client when another one connects with
 *              the same name).
 * MQTT_USER  : user name. Leave "" if the broker allows anonymous clients.
 * MQTT_PASS  : password. Leave "" if not needed. Sent in clear text (no TLS).
 * TOPIC_PREFIX: the server only accepts topics that start with this prefix.
 * TOPIC_PUB  : the board publishes its status here (TOPIC_PREFIX/DEVICE_NAME/status).
 * TOPIC_SUB  : the board receives commands here   (TOPIC_PREFIX/DEVICE_NAME/cmd).
 *              '+' matches one level, '#' matches all remaining levels.
 *              A topic may be at most 63 characters (checked at build time).
 * =================================================================
 */
#define BROKER_HOST  "broker.abc.com"
#define BROKER_PORT  1883
#define DEVICE_NAME  "fptSE123456"
#define MQTT_USER    ""
#define MQTT_PASS    ""
#define TOPIC_PREFIX "users/admin@example.com"

#define CLIENT_ID    DEVICE_NAME
#define TOPIC_PUB    TOPIC_PREFIX "/" DEVICE_NAME "/status"
#define TOPIC_SUB    TOPIC_PREFIX "/" DEVICE_NAME "/cmd"

/* Stop the build if a name is too long for the MQTT driver */
BUILD_ASSERT(sizeof(CLIENT_ID) - 1 <= DRIVER_MQTT_MAX_CLIENT_ID, "DEVICE_NAME is too long");
BUILD_ASSERT(sizeof(TOPIC_PUB) - 1 <= DRIVER_MQTT_MAX_TOPIC, "TOPIC_PUB is too long");
BUILD_ASSERT(sizeof(TOPIC_SUB) - 1 <= DRIVER_MQTT_MAX_TOPIC, "TOPIC_SUB is too long");

#define STATUS_PERIOD_MS 5000   /* status every 5 s (DHT11 needs at least 2 s) */

#define RELAY_1 0                /* alias out0 */
#define DHT     0                /* alias dht0 */

static bool relay_on;
static bool status_now;          /* set by a command: publish the status at once */

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

/*
 * Read the DHT11 and publish the status as JSON, for example:
 *   {"temp":28,"humi":61,"relay":"ON"}
 * If the sensor cannot be read, temp and humi are null.
 */
static void publish_status(void)
{
	const char *relay = relay_on ? "ON" : "OFF";
	int temp, humi;
	int ret;

	if (driver_dht11_read(DHT, &temp, &humi) == 0) {
		ret = driver_mqtt_publishf(TOPIC_PUB, 0,
					   "{\"temp\":%d,\"humi\":%d,\"relay\":\"%s\"}",
					   temp, humi, relay);
		printk("Status: temp %d C, humi %d %%, relay %s", temp, humi, relay);
	} else {
		ret = driver_mqtt_publishf(TOPIC_PUB, 0,
					   "{\"temp\":null,\"humi\":null,\"relay\":\"%s\"}",
					   relay);
		printk("Status: DHT11 read error, relay %s", relay);
	}
	if (ret == 0) {
		printk(" -> sent\n");
	} else {
		printk(" -> not sent (%d)\n", ret);
	}
}

/* Called for every message on TOPIC_SUB: "ON" or "OFF" switches Relay 1 */
static void on_message(const char *topic, const uint8_t *payload, size_t len)
{
	printk("Received [%s]: %.*s\n", topic, (int)len, payload);

	if (len == 2 && strncasecmp((const char *)payload, "ON", 2) == 0) {
		relay_on = true;
	} else if (len == 3 && strncasecmp((const char *)payload, "OFF", 3) == 0) {
		relay_on = false;
	} else {
		printk("Unknown command, use ON or OFF\n");
		return;
	}
	driver_gpio_set(RELAY_1, relay_on);
	printk("Relay 1 %s\n", relay_on ? "ON" : "OFF");
	status_now = true;
}

int main(void)
{
	char ip[DRIVER_ETHERNET_IP_STR_LEN];
	char broker_ip[DRIVER_ETHERNET_IP_STR_LEN];
	int64_t last_status = 0;

	printk("Example: MQTT status (DHT11 + Relay 1)\n");

	/* 1. Relay 1 (starts OFF) and DHT11 */
	if (driver_gpio_out_init(RELAY_1) < 0) {
		printk("Relay init failed\n");
		return 0;
	}
	if (driver_dht11_init(DHT) < 0) {
		printk("DHT11 init failed\n");
		return 0;
	}

	/* 2. Join the network, get an IP address by DHCP */
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

	/* 3. Resolve the broker name (DNS server given by the router over DHCP) */
	while (resolve_host(BROKER_HOST, broker_ip, sizeof(broker_ip)) < 0) {
		printk("DNS lookup for %s failed, retrying...\n", BROKER_HOST);
		k_msleep(3000);
	}
	printk("Broker %s -> %s:%d\n", BROKER_HOST, broker_ip, BROKER_PORT);

	/* 4. Broker address, user name and password */
	if (driver_mqtt_init(broker_ip, BROKER_PORT, CLIENT_ID) < 0) {
		printk("Invalid BROKER_PORT / DEVICE_NAME\n");
		return 0;
	}
	if (MQTT_USER[0] != '\0') {
		driver_mqtt_set_auth(MQTT_USER, MQTT_PASS);
	}

	/* 5. Subscribe: the driver subscribes again after every reconnection */
	driver_mqtt_subscribe(TOPIC_SUB, 1, on_message);

	/* 6. Connect to the broker */
	while (driver_mqtt_connect(K_SECONDS(5)) < 0) {
		printk("Cannot connect to broker %s:%d, retrying...\n", BROKER_HOST, BROKER_PORT);
		k_msleep(2000);
	}
	printk("MQTT connected\n");

	while (1) {
		/* Call often: receives messages, keeps the connection alive, reconnects */
		driver_mqtt_process(K_MSEC(100));

		/* Publish the status every STATUS_PERIOD_MS, and right after a command.
		 * The DHT11 can only be read every 2 s.
		 */
		if (k_uptime_get() - last_status >= STATUS_PERIOD_MS ||
		    (status_now && k_uptime_get() - last_status >= DRIVER_DHT11_MIN_INTERVAL_MS)) {
			last_status = k_uptime_get();
			status_now = false;
			publish_status();
		}
	}
	return 0;
}

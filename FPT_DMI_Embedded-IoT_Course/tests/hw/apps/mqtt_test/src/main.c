/*
 * MQTT hardware test (N20 ethernet + N21 mqtt), log on RTT.
 * One topic (brokers with an ACL often allow only one feed): the board publishes "cnt=N" every
 * 2 s (QoS 0 and 1 alternately) and subscribes to the same topic. Any received message that
 * starts with "cmd:" is answered with "echo:<rest>"; the board's own messages are only counted.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>
#include "driver_ethernet.h"
#include "driver_mqtt.h"

/* STM32H5 RCC_RSR: reset flags, cleared at boot so the next reset shows only its own cause */
#define RCC_RSR (*(volatile uint32_t *)0x44020CF4)
#define RSR_RMVF BIT(23)

static void print_reset_cause(void)
{
	uint32_t v = RCC_RSR;

	printk("RESET CAUSE RSR=0x%08x:%s%s%s%s%s%s\n", v, v & BIT(31) ? " LPWR" : "",
	       v & BIT(30) ? " WWDG" : "", v & BIT(29) ? " IWDG" : "", v & BIT(28) ? " SFT" : "",
	       v & BIT(27) ? " BOR" : "", v & BIT(26) ? " PIN" : "");
	RCC_RSR = RSR_RMVF;
}

#ifndef MQTT_BROKER_IP
#define MQTT_BROKER_IP "192.168.100.200"
#endif
#ifndef MQTT_TOPIC
#define MQTT_TOPIC "fpt/HWTEST/data"
#endif

static int rx_own, rx_cmd, rx_other;

static void on_msg(const char *topic, const uint8_t *payload, size_t len)
{
	char msg[80];

	if (len >= 4 && memcmp(payload, "cnt=", 4) == 0) {
		rx_own++;
	} else if (len >= 4 && memcmp(payload, "cmd:", 4) == 0) {
		rx_cmd++;
		printk("RX CMD #%d len=%u: %.*s\n", rx_cmd, (unsigned)len, (int)len, payload);
		snprintf(msg, sizeof(msg), "echo:%.*s", (int)MIN(len - 4, 60), payload + 4);
		printk("echo ret=%d\n", driver_mqtt_publish(MQTT_TOPIC, msg, 1));
	} else if (len >= 5 && memcmp(payload, "echo:", 5) == 0) {
		/* our own answer coming back */
	} else {
		rx_other++;
		printk("RX other len=%u: %.*s\n", (unsigned)len, (int)MIN(len, 80), payload);
	}
}

int main(void)
{
	char ip[DRIVER_ETHERNET_IP_STR_LEN];
	int64_t last_pub = 0;
	bool was_conn;
	int n = 0, ret;

	print_reset_cause();
	printk("MQTT TEST start, broker %s topic %s\n", MQTT_BROKER_IP, MQTT_TOPIC);
	ret = driver_ethernet_init();
	printk("eth init ret=%d\n", ret);
	for (int i = 0; i < 30 && !driver_ethernet_has_ip(); i++) {
		printk("wait IP: link=%s %d s\n", driver_ethernet_link_up() ? "UP" : "DOWN", i);
		driver_ethernet_wait_ip(K_SECONDS(1));
	}
	if (!driver_ethernet_has_ip()) {
		printk("NO IP (link=%s)\n", driver_ethernet_link_up() ? "UP" : "DOWN");
		return 0;
	}
	driver_ethernet_get_ip(ip, sizeof(ip));
	printk("IP %s link=%s\n", ip, driver_ethernet_link_up() ? "UP" : "DOWN");

	printk("init ret=%d\n", driver_mqtt_init(MQTT_BROKER_IP, 1883, "board-HWTEST"));
#ifdef MQTT_USER
	printk("auth ret=%d (user len %u, pass len %u)\n", driver_mqtt_set_auth(MQTT_USER, MQTT_PASS),
	       (unsigned)strlen(MQTT_USER), (unsigned)strlen(MQTT_PASS));
#endif
	while ((ret = driver_mqtt_connect(K_SECONDS(5))) < 0) {
		printk("connect ret=%d, retry\n", ret);
		k_msleep(2000);
	}
	printk("CONNECTED\n");
	printk("subscribe ret=%d\n", driver_mqtt_subscribe(MQTT_TOPIC, 1, on_msg));
	was_conn = true;

	while (1) {
		driver_mqtt_process(K_MSEC(100));
		bool c = driver_mqtt_is_connected();

		if (c != was_conn) {
			printk("%s at %lld ms\n", c ? "RECONNECTED" : "DISCONNECTED", k_uptime_get());
			was_conn = c;
		}
		if (k_uptime_get() - last_pub >= 2000) {
			static bool link_was = true;
			bool l = driver_ethernet_link_up();

			if (l != link_was) {
				printk("LINK %s at %lld ms\n", l ? "UP" : "DOWN", k_uptime_get());
				link_was = l;
			}
			last_pub = k_uptime_get();
			n++;
			ret = driver_mqtt_publishf(MQTT_TOPIC, n & 1, "cnt=%d", n);
			if (ret != 0 || n % 10 == 0)
			printk("PUB cnt=%d qos=%d ret=%d conn=%d link=%d rx_own=%d rx_cmd=%d\n", n, n & 1,
			       ret, c, driver_ethernet_link_up(), rx_own, rx_cmd);
		}
	}
	return 0;
}

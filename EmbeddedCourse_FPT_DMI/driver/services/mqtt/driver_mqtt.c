/**
 * @file driver_mqtt.c
 * @brief MQTT 3.1.1 client (no TLS) on top of the Zephyr MQTT library.
 *
 * Only driver_mqtt_process() and driver_mqtt_connect() touch the MQTT client; publish and
 * subscribe may be called from the same thread. Use one thread (main) for all of them.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/util.h>

#include "driver_ethernet.h"
#include "driver_mqtt.h"

#define RX_BUF_SIZE        512
#define TX_BUF_SIZE        512
#define KEEPALIVE_S        30
#define RECONNECT_GAP_MS   3000
#define RECONNECT_WAIT     K_SECONDS(3)
#define PUBLISHF_MAX       128
#define MAX_POLL_MS        (KEEPALIVE_S * 1000 / 2)

struct sub {
	char topic[DRIVER_MQTT_MAX_TOPIC + 1];
	uint8_t qos;
	driver_mqtt_msg_cb_t cb;
};

static struct mqtt_client client;
static struct net_sockaddr_storage broker;
static uint8_t rx_buf[RX_BUF_SIZE];
static uint8_t tx_buf[TX_BUF_SIZE];
static uint8_t payload_buf[DRIVER_MQTT_MAX_PAYLOAD + 1];

static char client_id[DRIVER_MQTT_MAX_CLIENT_ID + 1];
static char user_buf[DRIVER_MQTT_MAX_USER + 1];
static char pass_buf[DRIVER_MQTT_MAX_PASS + 1];
static struct mqtt_utf8 user_name;
static struct mqtt_utf8 password;
static bool configured;
static bool connected;
static bool connack_seen;
static int connack_code;
static uint16_t next_message_id = 1;
static int64_t last_attempt_ms;

static struct sub subs[DRIVER_MQTT_MAX_SUBS];
static int sub_count;

/* ---- Helpers --------------------------------------------------------------------------- */

static uint16_t new_message_id(void)
{
	uint16_t id = next_message_id++;

	if (next_message_id == 0) {
		next_message_id = 1; /* 0 is not a valid message id */
	}
	return id;
}

/* MQTT topic filter match with '+' (one level) and '#' (the rest) */
static bool topic_match(const char *filter, const char *topic)
{
	while (*filter != '\0') {
		if (*filter == '#') {
			return true;
		}
		if (*filter == '+') {
			while (*topic != '\0' && *topic != '/') {
				topic++;
			}
			filter++;
			continue;
		}
		if (*filter != *topic) {
			return false;
		}
		filter++;
		topic++;
	}
	return *topic == '\0';
}

static int timeout_to_ms(k_timeout_t t)
{
	if (K_TIMEOUT_EQ(t, K_FOREVER)) {
		return MAX_POLL_MS;
	}
	return (int)MIN(k_ticks_to_ms_ceil64(t.ticks), (uint64_t)MAX_POLL_MS);
}

/* ---- Incoming messages ----------------------------------------------------------------- */

static void handle_publish(struct mqtt_client *c, const struct mqtt_publish_param *p)
{
	char topic[DRIVER_MQTT_MAX_TOPIC + 1];
	size_t topic_len = MIN((size_t)p->message.topic.topic.size, (size_t)DRIVER_MQTT_MAX_TOPIC);
	size_t total = p->message.payload.len;
	size_t kept = 0;

	memcpy(topic, p->message.topic.topic.utf8, topic_len);
	topic[topic_len] = '\0';

	/* Read the whole payload from the socket; keep the first part, drop the excess */
	while (kept < total) {
		int n;

		if (kept < DRIVER_MQTT_MAX_PAYLOAD) {
			n = mqtt_read_publish_payload(c, payload_buf + kept,
						      MIN(total - kept, DRIVER_MQTT_MAX_PAYLOAD - kept));
			if (n > 0) {
				kept += n;
				continue;
			}
		} else {
			uint8_t sink[32];

			n = mqtt_read_publish_payload(c, sink, MIN(total - kept, sizeof(sink)));
			if (n > 0) {
				kept += n; /* counts as consumed, but is not stored */
				continue;
			}
		}
		if (n == -EAGAIN) {
			continue;
		}
		return; /* connection problem: mqtt_input() reports it */
	}

	if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
		const struct mqtt_puback_param ack = {.message_id = p->message_id};

		mqtt_publish_qos1_ack(c, &ack);
	}

	for (int i = 0; i < sub_count; i++) {
		if (subs[i].cb != NULL && topic_match(subs[i].topic, topic)) {
			subs[i].cb(topic, payload_buf, MIN(kept, (size_t)DRIVER_MQTT_MAX_PAYLOAD));
		}
	}
}

static void mqtt_event(struct mqtt_client *c, const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		connack_seen = true;
		connack_code = evt->result == 0 ? evt->param.connack.return_code : evt->result;
		break;
	case MQTT_EVT_DISCONNECT:
		connected = false;
		break;
	case MQTT_EVT_PUBLISH:
		handle_publish(c, &evt->param.publish);
		break;
	default: /* PUBACK, SUBACK, PINGRESP: nothing to do */
		break;
	}
}

/* ---- Connection ------------------------------------------------------------------------ */

static int send_subscribe(const struct sub *s)
{
	struct mqtt_topic topic = {
		.topic = {.utf8 = (uint8_t *)s->topic, .size = strlen(s->topic)},
		.qos = s->qos,
	};
	const struct mqtt_subscription_list list = {
		.list = &topic,
		.list_count = 1,
		.message_id = new_message_id(),
	};

	return mqtt_subscribe(&client, &list);
}

static void drop_connection(void)
{
	if (connected || client.transport.tcp.sock >= 0) {
		mqtt_abort(&client);
	}
	connected = false;
}

int driver_mqtt_init(const char *broker_ip, uint16_t port, const char *id)
{
	struct net_sockaddr_in *addr = (struct net_sockaddr_in *)&broker;
	size_t id_len;

	if (broker_ip == NULL || port == 0 || id == NULL) {
		return -EINVAL;
	}
	id_len = strlen(id);
	if (id_len == 0 || id_len > DRIVER_MQTT_MAX_CLIENT_ID) {
		return -EINVAL;
	}

	memset(&broker, 0, sizeof(broker));
	addr->sin_family = NET_AF_INET;
	addr->sin_port = net_htons(port);
	if (net_addr_pton(NET_AF_INET, broker_ip, &addr->sin_addr) < 0) {
		return -EINVAL;
	}

	memcpy(client_id, id, id_len + 1);
	configured = true;
	return 0;
}

int driver_mqtt_set_auth(const char *user, const char *pass)
{
	size_t user_len = user != NULL ? strlen(user) : 0;
	size_t pass_len = pass != NULL ? strlen(pass) : 0;

	if (user_len > DRIVER_MQTT_MAX_USER || pass_len > DRIVER_MQTT_MAX_PASS) {
		return -EINVAL;
	}
	memcpy(user_buf, user != NULL ? user : "", user_len + 1);
	memcpy(pass_buf, pass != NULL ? pass : "", pass_len + 1);
	user_name.utf8 = (const uint8_t *)user_buf;
	user_name.size = user_len;
	password.utf8 = (const uint8_t *)pass_buf;
	password.size = pass_len;
	return 0;
}

int driver_mqtt_connect(k_timeout_t timeout)
{
	k_timepoint_t end = sys_timepoint_calc(timeout);
	struct zsock_pollfd fds;
	int ret;

	if (!configured) {
		return -EACCES;
	}
	if (!driver_ethernet_has_ip()) {
		return -ENETDOWN;
	}

	drop_connection();
	mqtt_client_init(&client);
	client.broker = &broker;
	client.evt_cb = mqtt_event;
	client.client_id.utf8 = (uint8_t *)client_id;
	client.client_id.size = strlen(client_id);
	/* MQTT 3.1.1: a password is only allowed together with a user name */
	client.user_name = user_name.size > 0 ? &user_name : NULL;
	client.password = user_name.size > 0 && password.size > 0 ? &password : NULL;
	client.protocol_version = MQTT_VERSION_3_1_1;
	client.keepalive = KEEPALIVE_S;
	client.clean_session = 1;
	client.rx_buf = rx_buf;
	client.rx_buf_size = sizeof(rx_buf);
	client.tx_buf = tx_buf;
	client.tx_buf_size = sizeof(tx_buf);
	client.transport.type = MQTT_TRANSPORT_NON_SECURE;

	connack_seen = false;
	last_attempt_ms = k_uptime_get();
	ret = mqtt_connect(&client);
	if (ret < 0) {
		drop_connection();
		return ret;
	}

	/* The TCP connection is up: wait for the broker's CONNACK */
	fds.fd = client.transport.tcp.sock;
	fds.events = ZSOCK_POLLIN;
	while (!connack_seen) {
		k_timeout_t left = sys_timepoint_timeout(end);
		int wait_ms = K_TIMEOUT_EQ(left, K_FOREVER) ? MAX_POLL_MS
							     : (int)k_ticks_to_ms_ceil64(left.ticks);

		if (wait_ms <= 0 && sys_timepoint_expired(end)) {
			drop_connection();
			return -ETIMEDOUT;
		}
		ret = zsock_poll(&fds, 1, wait_ms);
		if (ret > 0) {
			ret = mqtt_input(&client);
			if (ret < 0) {
				drop_connection();
				return ret;
			}
		}
	}
	if (connack_code != MQTT_CONNECTION_ACCEPTED) {
		drop_connection();
		return -ECONNREFUSED;
	}

	connected = true;
	for (int i = 0; i < sub_count; i++) {
		send_subscribe(&subs[i]);
	}
	return 0;
}

bool driver_mqtt_is_connected(void)
{
	return connected;
}

/* ---- Publish, subscribe ---------------------------------------------------------------- */

int driver_mqtt_publish(const char *topic, const char *payload, uint8_t qos)
{
	struct mqtt_publish_param param;
	size_t topic_len;

	if (topic == NULL || payload == NULL || qos > 2) {
		return -EINVAL;
	}
	topic_len = strlen(topic);
	if (topic_len == 0 || topic_len > DRIVER_MQTT_MAX_TOPIC) {
		return -EINVAL;
	}
	if (qos == 2) {
		return -ENOTSUP;
	}
	if (!connected) {
		return -ENOTCONN;
	}

	memset(&param, 0, sizeof(param));
	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = (const uint8_t *)topic;
	param.message.topic.topic.size = topic_len;
	param.message.payload.data = (uint8_t *)payload;
	param.message.payload.len = strlen(payload);
	param.message_id = new_message_id();
	return mqtt_publish(&client, &param);
}

int driver_mqtt_publishf(const char *topic, uint8_t qos, const char *fmt, ...)
{
	char buf[PUBLISHF_MAX];
	va_list ap;
	int n;

	if (fmt == NULL) {
		return -EINVAL;
	}
	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0) {
		return -EINVAL;
	}
	return driver_mqtt_publish(topic, buf, qos);
}

int driver_mqtt_subscribe(const char *topic, uint8_t qos, driver_mqtt_msg_cb_t cb)
{
	size_t topic_len;
	int ret = 0;

	if (topic == NULL || cb == NULL || qos > 1) {
		return -EINVAL;
	}
	topic_len = strlen(topic);
	if (topic_len == 0 || topic_len > DRIVER_MQTT_MAX_TOPIC) {
		return -EINVAL;
	}
	if (sub_count >= DRIVER_MQTT_MAX_SUBS) {
		return -ENOSPC;
	}

	memcpy(subs[sub_count].topic, topic, topic_len + 1);
	subs[sub_count].qos = qos;
	subs[sub_count].cb = cb;
	if (connected) {
		ret = send_subscribe(&subs[sub_count]);
	}
	sub_count++; /* kept for the next (re)connection even if sending failed */
	return ret;
}

/* ---- Main loop ------------------------------------------------------------------------- */

int driver_mqtt_process(k_timeout_t timeout)
{
	struct zsock_pollfd fds;
	int ms = timeout_to_ms(timeout);
	int ret;

	if (!configured) {
		return -EACCES;
	}

	if (!connected) {
		int64_t since = k_uptime_get() - last_attempt_ms;

		if (last_attempt_ms == 0 || since >= RECONNECT_GAP_MS) {
			if (driver_ethernet_has_ip()) {
				driver_mqtt_connect(RECONNECT_WAIT);
			} else {
				last_attempt_ms = k_uptime_get();
			}
		}
		if (!connected) {
			k_msleep(ms);
			return -ENOTCONN;
		}
	}

	fds.fd = client.transport.tcp.sock;
	fds.events = ZSOCK_POLLIN;
	ret = zsock_poll(&fds, 1, ms);
	if (ret > 0) {
		if (fds.revents & (ZSOCK_POLLERR | ZSOCK_POLLHUP | ZSOCK_POLLNVAL)) {
			drop_connection();
			return -ENOTCONN;
		}
		ret = mqtt_input(&client);
		if (ret < 0) {
			drop_connection();
			return -ENOTCONN;
		}
	}

	/* Keep-alive: -EAGAIN just means that no ping is due yet */
	ret = mqtt_live(&client);
	if (ret < 0 && ret != -EAGAIN) {
		drop_connection();
		return -ENOTCONN;
	}
	return connected ? 0 : -ENOTCONN;
}

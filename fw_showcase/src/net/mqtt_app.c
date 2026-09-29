/* mqtt_app.c — MQTT client
 * Resolves the broker hostname (DNS), connects with username/password,
 * publishes the DHT11 reading + alarm state as JSON to the configured topic, and auto-reconnects.
 */
#include "mqtt_app.h"
#include "app_config.h"
#include "app_state.h"           /* g_device_ip, g_web_temp_mc/humi_mp/cnt */
#include "alarm.h"
#include "mqtt_ca.h"
#include "version.h"
#include <zephyr/sys/atomic.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/tls_credentials.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define RX_SZ            512
#define TX_SZ            512
/* Port 8883 = MQTT over TLS (verified against mqtt_ca_pem); any other port is plain TCP */
#define MQTT_TLS_PORT    8883
#define MQTT_CA_SEC_TAG  1

static const sec_tag_t s_sec_tags[] = { MQTT_CA_SEC_TAG };
static bool s_ca_loaded;

static struct mqtt_client   client;
static struct sockaddr_storage broker;
static uint8_t  rx_buf[RX_SZ];
static uint8_t  tx_buf[TX_SZ];
static struct mqtt_utf8 s_user, s_pass;

static bool  s_connected;
static char  s_status[48] = "starting";
static uint16_t s_mid;
/* Set by main every 60 s and on every alarm change: publish at once */
static atomic_t s_pub_now;

void mqtt_app_publish_now(void) { atomic_set(&s_pub_now, 1); }

bool        mqtt_app_is_connected(void) { return s_connected; }
const char *mqtt_app_status(void)       { return s_status; }

static void mqtt_evt(struct mqtt_client *c, const struct mqtt_evt *evt)
{
    ARG_UNUSED(c);
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result == 0) {
            s_connected = true;
            snprintf(s_status, sizeof(s_status), "connected");
            printk("[MQTT] CONNACK OK\n");
        } else {
            snprintf(s_status, sizeof(s_status), "connack err %d", evt->result);
            printk("[MQTT] CONNACK failed %d\n", evt->result);
        }
        break;
    case MQTT_EVT_DISCONNECT:
        s_connected = false;
        snprintf(s_status, sizeof(s_status), "disconnected");
        printk("[MQTT] DISCONNECT\n");
        break;
    case MQTT_EVT_PUBACK:
        printk("[MQTT] PUBACK %u\n", evt->param.puback.message_id);
        break;
    case MQTT_EVT_PINGRESP:
        break;
    default:
        break;
    }
}

static int resolve_broker(void)
{
    struct zsock_addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    struct zsock_addrinfo *res = NULL;
    int rc = zsock_getaddrinfo(g_cfg.mqtt_host, NULL, &hints, &res);
    if (rc != 0 || res == NULL) {
        printk("[MQTT] DNS resolve '%s' failed (%d)\n", g_cfg.mqtt_host, rc);
        snprintf(s_status, sizeof(s_status), "dns fail");
        return -1;
    }
    struct sockaddr_in *b = (struct sockaddr_in *)&broker;
    b->sin_family = AF_INET;
    b->sin_port   = htons(g_cfg.mqtt_port);
    b->sin_addr   = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    char ips[16];
    net_addr_ntop(AF_INET, &b->sin_addr, ips, sizeof(ips));
    printk("[MQTT] %s -> %s:%u\n", g_cfg.mqtt_host, ips, g_cfg.mqtt_port);
    zsock_freeaddrinfo(res);
    return 0;
}

static bool use_tls(void) { return g_cfg.mqtt_port == MQTT_TLS_PORT; }

static int load_ca(void)
{
    if (s_ca_loaded) return 0;
    int rc = tls_credential_add(MQTT_CA_SEC_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
                                mqtt_ca_pem, sizeof(mqtt_ca_pem));
    if (rc == 0 || rc == -EEXIST) { s_ca_loaded = true; return 0; }
    printk("[MQTT] CA load err %d\n", rc);
    return rc;
}

static int mqtt_sock(void)
{
    return use_tls() ? client.transport.tls.sock : client.transport.tcp.sock;
}

static void client_setup(void)
{
    mqtt_client_init(&client);
    client.broker          = &broker;
    client.evt_cb          = mqtt_evt;
    client.client_id.utf8  = (uint8_t *)g_cfg.mqtt_client_id;
    client.client_id.size  = strlen(g_cfg.mqtt_client_id);

    s_user.utf8 = (uint8_t *)g_cfg.mqtt_user;
    s_user.size = strlen(g_cfg.mqtt_user);
    s_pass.utf8 = (uint8_t *)g_cfg.mqtt_pass;
    s_pass.size = strlen(g_cfg.mqtt_pass);
    client.user_name = g_cfg.mqtt_user[0] ? &s_user : NULL;
    client.password  = g_cfg.mqtt_pass[0] ? &s_pass : NULL;

    client.protocol_version = MQTT_VERSION_3_1_1;
    client.rx_buf           = rx_buf;
    client.rx_buf_size      = RX_SZ;
    client.tx_buf           = tx_buf;
    client.tx_buf_size      = TX_SZ;
    if (use_tls()) {
        struct mqtt_sec_config *tls = &client.transport.tls.config;

        client.transport.type = MQTT_TRANSPORT_SECURE;
        tls->peer_verify      = TLS_PEER_VERIFY_REQUIRED;
        tls->cipher_list      = NULL;
        tls->sec_tag_list     = s_sec_tags;
        tls->sec_tag_count    = ARRAY_SIZE(s_sec_tags);
        tls->hostname         = g_cfg.mqtt_host;   /* SNI + name check */
    } else {
        client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    }
    client.keepalive        = 60;
    client.clean_session    = 1;
}

static int do_publish(void)
{
    char payload[224];
    int t = g_web_temp_mc / 1000, h = g_web_humi_mp / 1000;

    int n = snprintf(payload, sizeof(payload),
        "{\"id\":\"%s\",\"fw\":\"" FW_VERSION "\",\"temp\":%d,\"humi\":%d,"
        "\"sensor_ok\":%d,\"alarm\":%d,\"reason\":\"%s\",\"motor\":%d,"
        "\"cnt\":%u,\"ip\":\"%s\"}",
        g_cfg.mqtt_client_id, t, h, g_web_sensor_ok ? 1 : 0,
        alarm_active() ? 1 : 0, alarm_reason_str(alarm_reason()),
        motor_is_on() ? 1 : 0, g_web_cnt, g_device_ip);

    struct mqtt_publish_param p;
    memset(&p, 0, sizeof(p));
    p.message.topic.qos        = MQTT_QOS_0_AT_MOST_ONCE;
    p.message.topic.topic.utf8 = (uint8_t *)g_cfg.mqtt_topic;
    p.message.topic.topic.size = strlen(g_cfg.mqtt_topic);
    p.message.payload.data     = (uint8_t *)payload;
    p.message.payload.len      = n;
    p.message_id               = ++s_mid ? s_mid : ++s_mid;
    p.dup_flag                 = 0;
    p.retain_flag              = 0;
    return mqtt_publish(&client, &p);
}

static void mqtt_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    /* Wait until the network has an IP (max ~40 s) */
    for (int i = 0; i < 80; i++) {
        if (app_state_has_ip()) break;
        k_msleep(500);
    }

    while (1) {
        s_connected = false;

        /* No broker configured yet (set it on the /config page) */
        if (g_cfg.mqtt_host[0] == '\0') {
            snprintf(s_status, sizeof(s_status), "not configured");
            k_msleep(5000);
            continue;
        }

        if (resolve_broker() != 0) { k_msleep(5000); continue; }

        if (use_tls() && load_ca() != 0) { k_msleep(5000); continue; }

        client_setup();
        printk("[MQTT] connecting %s:%u%s as '%s'...\n",
               g_cfg.mqtt_host, g_cfg.mqtt_port, use_tls() ? " (TLS)" : "",
               g_cfg.mqtt_client_id);
        snprintf(s_status, sizeof(s_status), "connecting");

        int rc = mqtt_connect(&client);
        if (rc != 0) {
            printk("[MQTT] mqtt_connect err %d\n", rc);
            snprintf(s_status, sizeof(s_status), "connect err %d", rc);
            k_msleep(5000);
            continue;
        }

        struct zsock_pollfd fds;
        fds.fd     = mqtt_sock();
        fds.events = ZSOCK_POLLIN;

        /* Wait for CONNACK (max 5 s) */
        int64_t t0 = k_uptime_get();
        while (!s_connected && (k_uptime_get() - t0) < 5000) {
            zsock_poll(&fds, 1, 500);
            if (mqtt_input(&client) != 0) break;
            mqtt_live(&client);
        }
        if (!s_connected) {
            printk("[MQTT] no CONNACK — retrying\n");
            mqtt_abort(&client);
            k_msleep(5000);
            continue;
        }

        /* Connected — publish loop */
        atomic_set(&s_pub_now, 1);   /* publish the current state right after CONNACK */
        while (s_connected) {
            int pr = zsock_poll(&fds, 1, 500);
            if (pr < 0) break;
            if (pr > 0 && (fds.revents & ZSOCK_POLLIN)) {
                if (mqtt_input(&client) != 0) break;
            }
            int lr = mqtt_live(&client);
            if (lr != 0 && lr != -EAGAIN) break;

            if (g_web_cnt > 0 && atomic_set(&s_pub_now, 0)) {
                int wr = do_publish();
                if (wr != 0) {
                    printk("[MQTT] publish err %d\n", wr);
                    snprintf(s_status, sizeof(s_status), "pub err %d", wr);
                    break;
                }
                printk("[MQTT] published -> %s\n", g_cfg.mqtt_topic);
                snprintf(s_status, sizeof(s_status), "connected (pub ok)");
            }
        }

        printk("[MQTT] connection lost — reconnecting in 3s\n");
        mqtt_abort(&client);
        k_msleep(3000);
    }
}

K_THREAD_DEFINE(mqtt_tid, 10240, mqtt_thread, NULL, NULL, NULL, 7, 0, 4000);

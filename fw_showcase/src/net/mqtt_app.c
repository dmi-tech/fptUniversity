/* mqtt_app.c — MQTT client for mini_GW
 * Resolves the broker hostname (DNS), connects with username/password,
 * publishes the DHT11 reading + alarm/motor state as JSON to
 * "users/admin@example.com/<NNN>/status" (NNN = last byte of the device IP),
 * serves the web page's subscribe / publish tools and auto-reconnects.
 * Only this thread calls the MQTT library; the web thread queues requests.
 */
#include "mqtt_app.h"
#include "app_config.h"
#include "app_state.h"           /* g_device_ip, g_web_temp_mc/humi_mp/cnt */
#include "alarm.h"
#include "motor.h"
#include "mqtt_ca.h"
#include "version.h"
#include <zephyr/sys/atomic.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/tls_credentials.h>
#include <stdlib.h>
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
/* Set by main every 30 s and on every alarm/motor change: publish at once */
static atomic_t s_pub_now;

/* Web tools, guarded by s_lock (the web thread reads/writes them too) */
static K_MUTEX_DEFINE(s_lock);
static char s_sub_want[MQTT_TOPIC_MAX];     /* subscription asked by the web */
static bool s_sub_dirty;                    /* s_sub_want not applied yet */
static char s_sub_state[24];
static struct mqtt_sub_msg s_msgs[MQTT_SUB_KEEP];
static int  s_msg_head, s_msg_count;        /* ring, s_msg_head = oldest */
static char s_pub_topic[MQTT_TOPIC_MAX];
static char s_pub_msg[MQTT_MSG_MAX];
static bool s_pub_pending;

/* MQTT thread only */
static char     s_sub_active[MQTT_TOPIC_MAX];   /* subscribed on the broker */
static uint16_t s_sub_mid;

void mqtt_app_publish_now(void) { atomic_set(&s_pub_now, 1); }

/* NNN: last byte of the device IP */
static int id_num(void)
{
    const char *dot = strrchr(g_device_ip, '.');
    return dot ? atoi(dot + 1) : 0;
}

void mqtt_app_status_topic(char *out, size_t len)
{
    snprintf(out, len, "users/admin@example.com/%03d/status", id_num());
}

static uint16_t next_mid(void)
{
    if (++s_mid == 0) ++s_mid;     /* 0 is not a valid message id */
    return s_mid;
}

static void set_sub_state(const char *st)
{
    k_mutex_lock(&s_lock, K_FOREVER);
    strncpy(s_sub_state, st, sizeof(s_sub_state) - 1);
    k_mutex_unlock(&s_lock);
}

/* ── Web side ─────────────────────────────────────────────────── */
int mqtt_app_subscribe(const char *topic)
{
    if (strlen(topic) >= MQTT_TOPIC_MAX) return -EINVAL;
    k_mutex_lock(&s_lock, K_FOREVER);
    strcpy(s_sub_want, topic);
    s_sub_dirty = true;
    s_msg_head = s_msg_count = 0;
    strcpy(s_sub_state, !topic[0] ? "" : s_connected ? "subscribing" : "waiting for broker");
    k_mutex_unlock(&s_lock);
    return 0;
}

int mqtt_app_publish(const char *topic, const char *msg, char *sent, size_t sent_len)
{
    int rc = 0;

    /* Sent exactly as typed, no prefix */
    if (!topic[0] || strpbrk(topic, "+#") || strlen(topic) >= MQTT_TOPIC_MAX) return -EINVAL;
    if (strlen(msg) >= MQTT_MSG_MAX) return -EMSGSIZE;
    if (!s_connected) return -ENOTCONN;

    k_mutex_lock(&s_lock, K_FOREVER);
    if (s_pub_pending) {
        rc = -EBUSY;
    } else {
        strcpy(s_pub_topic, topic);
        strcpy(s_pub_msg, msg);
        s_pub_pending = true;
    }
    k_mutex_unlock(&s_lock);
    if (rc == 0 && sent) snprintf(sent, sent_len, "%s", topic);
    return rc;
}

void mqtt_app_sub_view(struct mqtt_sub_view *v)
{
    k_mutex_lock(&s_lock, K_FOREVER);
    strcpy(v->topic, s_sub_want);
    strcpy(v->state, s_sub_state);
    v->count = s_msg_count;
    for (int i = 0; i < s_msg_count; i++)
        v->msgs[i] = s_msgs[(s_msg_head + i) % MQTT_SUB_KEEP];
    k_mutex_unlock(&s_lock);
}

/* ── Incoming PUBLISH (runs inside mqtt_input on this thread) ─── */
static void store_msg(const struct mqtt_utf8 *topic, const uint8_t *data,
                      size_t len, bool truncated)
{
    k_mutex_lock(&s_lock, K_FOREVER);
    int slot = (s_msg_head + s_msg_count) % MQTT_SUB_KEEP;
    if (s_msg_count == MQTT_SUB_KEEP) s_msg_head = (s_msg_head + 1) % MQTT_SUB_KEEP;
    else                              s_msg_count++;

    struct mqtt_sub_msg *m = &s_msgs[slot];
    size_t tl = MIN(topic->size, sizeof(m->topic) - 1);
    memcpy(m->topic, topic->utf8, tl);
    m->topic[tl] = '\0';
    for (size_t i = 0; i < len; i++) m->data[i] = data[i] ? (char)data[i] : '.';
    m->data[len]  = '\0';
    m->truncated  = truncated;
    m->uptime_s   = (uint32_t)(k_uptime_get() / 1000);
    k_mutex_unlock(&s_lock);
}

static void on_publish(struct mqtt_client *c, const struct mqtt_publish_param *p)
{
    static uint8_t buf[MQTT_RX_MAX - 1];
    uint8_t junk[32];
    size_t len  = p->message.payload.len;
    size_t keep = MIN(len, sizeof(buf));

    /* The whole payload must be read, or the stream loses sync */
    if (mqtt_readall_publish_payload(c, buf, keep) != 0) return;
    for (size_t left = len - keep; left > 0; ) {
        size_t n = MIN(left, sizeof(junk));
        if (mqtt_readall_publish_payload(c, junk, n) != 0) return;
        left -= n;
    }
    store_msg(&p->message.topic.topic, buf, keep, keep < len);
    printk("[MQTT] RX %.*s (%u B)\n", p->message.topic.topic.size,
           p->message.topic.topic.utf8, (unsigned)len);

    if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
        struct mqtt_puback_param ack = { .message_id = p->message_id };
        mqtt_publish_qos1_ack(c, &ack);
    }
}

bool        mqtt_app_is_connected(void) { return s_connected; }
const char *mqtt_app_status(void)       { return s_status; }

static void mqtt_evt(struct mqtt_client *c, const struct mqtt_evt *evt)
{
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
    case MQTT_EVT_PUBLISH:
        if (evt->result == 0) on_publish(c, &evt->param.publish);
        break;
    case MQTT_EVT_SUBACK:
        if (evt->param.suback.message_id == s_sub_mid) {
            const struct mqtt_binstr *rc = &evt->param.suback.return_codes;
            bool ok = rc->len > 0 && rc->data[0] != MQTT_SUBACK_FAILURE;
            set_sub_state(ok ? "subscribed" : "refused by broker");
            printk("[MQTT] SUBACK %s\n", ok ? "OK" : "FAILURE");
        }
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

static int publish_raw(const char *topic, const char *data, size_t len)
{
    struct mqtt_publish_param p;
    memset(&p, 0, sizeof(p));
    p.message.topic.qos        = MQTT_QOS_0_AT_MOST_ONCE;
    p.message.topic.topic.utf8 = (uint8_t *)topic;
    p.message.topic.topic.size = strlen(topic);
    p.message.payload.data     = (uint8_t *)data;
    p.message.payload.len      = len;
    p.message_id               = next_mid();
    p.dup_flag                 = 0;
    p.retain_flag              = 0;
    return mqtt_publish(&client, &p);
}

static int publish_status(void)
{
    char payload[256], topic[MQTT_TOPIC_MAX], id[16];
    int t = g_web_temp_mc / 1000, h = g_web_humi_mp / 1000;

    snprintf(id, sizeof(id), "Board %03d", id_num());
    int n = snprintf(payload, sizeof(payload),
        "{\"id\":\"%s\",\"fw\":\"" FW_VERSION "\",\"temp\":%d,\"humi\":%d,"
        "\"sensor_ok\":%d,\"alarm\":%d,\"reason\":\"%s\",\"motor\":%d,"
        "\"motor_by\":\"%s\",\"ip\":\"%s\"}",
        id, t, h, g_web_sensor_ok ? 1 : 0,
        alarm_active() ? 1 : 0, alarm_reason_str(alarm_reason()),
        motor_is_on() ? 1 : 0, motor_src_str(motor_last_src()),
        g_device_ip);
    mqtt_app_status_topic(topic, sizeof(topic));

    int rc = publish_raw(topic, payload, n);
    if (rc == 0) printk("[MQTT] published -> %s\n", topic);
    return rc;
}

/* Apply a new subscription from the web page (or re-apply after CONNACK) */
static int apply_subscription(void)
{
    char want[MQTT_TOPIC_MAX];

    k_mutex_lock(&s_lock, K_FOREVER);
    bool dirty = s_sub_dirty;
    s_sub_dirty = false;
    strcpy(want, s_sub_want);
    k_mutex_unlock(&s_lock);
    if (!dirty) return 0;

    if (s_sub_active[0] && strcmp(s_sub_active, want) != 0) {
        struct mqtt_topic t = {
            .topic = { .utf8 = (uint8_t *)s_sub_active, .size = strlen(s_sub_active) },
        };
        struct mqtt_subscription_list l = { .list = &t, .list_count = 1,
                                            .message_id = next_mid() };
        int rc = mqtt_unsubscribe(&client, &l);
        printk("[MQTT] unsubscribe %s (%d)\n", s_sub_active, rc);
        s_sub_active[0] = '\0';
        if (rc != 0) return rc;
    }
    if (!want[0]) return 0;
    if (strcmp(s_sub_active, want) == 0) {     /* same topic asked again */
        set_sub_state("subscribed");
        return 0;
    }

    struct mqtt_topic t = {
        .topic = { .utf8 = (uint8_t *)want, .size = strlen(want) },
        .qos   = MQTT_QOS_0_AT_MOST_ONCE,
    };
    s_sub_mid = next_mid();
    struct mqtt_subscription_list l = { .list = &t, .list_count = 1,
                                        .message_id = s_sub_mid };
    int rc = mqtt_subscribe(&client, &l);
    printk("[MQTT] subscribe %s (%d)\n", want, rc);
    if (rc != 0) {
        set_sub_state("subscribe error");
        return rc;
    }
    strcpy(s_sub_active, want);
    set_sub_state("subscribing");
    return 0;
}

/* Send the publish queued by the web page, if any */
static int apply_user_publish(void)
{
    char topic[MQTT_TOPIC_MAX], msg[MQTT_MSG_MAX];

    k_mutex_lock(&s_lock, K_FOREVER);
    bool pending = s_pub_pending;
    s_pub_pending = false;
    strcpy(topic, s_pub_topic);
    strcpy(msg, s_pub_msg);
    k_mutex_unlock(&s_lock);
    if (!pending) return 0;

    int rc = publish_raw(topic, msg, strlen(msg));
    printk("[MQTT] web publish -> %s (%d)\n", topic, rc);
    return rc;
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

        /* Clean session: the broker forgot the subscription, set it again */
        s_sub_active[0] = '\0';
        k_mutex_lock(&s_lock, K_FOREVER);
        s_sub_dirty = (s_sub_want[0] != '\0');
        k_mutex_unlock(&s_lock);

        /* Connected — service loop (200 ms so web actions feel immediate) */
        atomic_set(&s_pub_now, 1);   /* publish the current state right after CONNACK */
        while (s_connected) {
            /* Cable out: drop the session now instead of waiting for the
             * keepalive, so the status (web, LCD) is right and the
             * reconnect starts as soon as the link is back */
            if (!net_if_is_up(net_if_get_default())) {
                printk("[MQTT] link down\n");
                snprintf(s_status, sizeof(s_status), "link down");
                break;
            }
            int pr = zsock_poll(&fds, 1, 200);
            if (pr < 0) break;
            if (pr > 0 && (fds.revents & ZSOCK_POLLIN)) {
                if (mqtt_input(&client) != 0) break;
            }
            int lr = mqtt_live(&client);
            if (lr != 0 && lr != -EAGAIN) break;

            if (apply_subscription() != 0) break;
            if (apply_user_publish() != 0) break;

            if (g_web_cnt > 0 && atomic_set(&s_pub_now, 0)) {
                int wr = publish_status();
                if (wr != 0) {
                    printk("[MQTT] publish err %d\n", wr);
                    snprintf(s_status, sizeof(s_status), "pub err %d", wr);
                    break;
                }
                snprintf(s_status, sizeof(s_status), "connected (pub ok)");
            }
        }

        printk("[MQTT] connection lost — reconnecting in 3s\n");
        s_connected = false;
        k_mutex_lock(&s_lock, K_FOREVER);
        if (s_sub_want[0]) strcpy(s_sub_state, "waiting for broker");
        k_mutex_unlock(&s_lock);
        mqtt_abort(&client);
        k_msleep(3000);
    }
}

K_THREAD_DEFINE(mqtt_tid, 10240, mqtt_thread, NULL, NULL, NULL, 7, 0, 4000);

#ifndef MQTT_APP_H
#define MQTT_APP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* MQTT runs in its own thread (auto-started). These expose its state. */
bool        mqtt_app_is_connected(void);
const char *mqtt_app_status(void);

/* Ask the MQTT thread to publish the status JSON now (alarm/motor change,
 * 30 s period) */
void        mqtt_app_publish_now(void);

/* Status topic "users/admin@example.com/<NNN>/status", NNN = last byte of the
 * device IP, 3 digits */
void        mqtt_app_status_topic(char *out, size_t len);

/* ── Web page tools: one subscription + one-shot publish ─────── */
#define MQTT_TOPIC_MAX   64    /* incl. the NUL */
#define MQTT_MSG_MAX     129   /* published from the web page, incl. the NUL */
#define MQTT_RX_MAX      385   /* received message kept for the web page, incl. the NUL */
#define MQTT_SUB_KEEP    3     /* last messages kept for the subscription */

struct mqtt_sub_msg {
    uint32_t uptime_s;              /* when it arrived */
    char     topic[MQTT_TOPIC_MAX]; /* actual topic (differs with wildcards) */
    char     data[MQTT_RX_MAX];     /* payload, cut to MQTT_RX_MAX - 1 */
    bool     truncated;
};

struct mqtt_sub_view {
    char topic[MQTT_TOPIC_MAX];     /* "" = no subscription */
    char state[24];                 /* subscribing / subscribed / ... */
    int  count;                     /* valid entries in msgs[], oldest first */
    struct mqtt_sub_msg msgs[MQTT_SUB_KEEP];
};

/* Replace the subscription ("" = unsubscribe). Applied by the MQTT thread,
 * re-applied after a reconnect, not kept across a reboot. */
int  mqtt_app_subscribe(const char *topic);
/* Queue one QoS 0 publish to topic, sent exactly as typed (no prefix).
 * The topic is copied to sent.
 * -ENOTCONN when offline, -EBUSY when one is already waiting. */
int  mqtt_app_publish(const char *topic, const char *msg, char *sent, size_t sent_len);
/* Snapshot of the subscription and its last messages */
void mqtt_app_sub_view(struct mqtt_sub_view *v);

#endif /* MQTT_APP_H */

#ifndef MQTT_APP_H
#define MQTT_APP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* MQTT runs in its own thread (auto-started). These expose its state. */
bool        mqtt_app_is_connected(void);
const char *mqtt_app_status(void);

/* Ask the MQTT thread to publish the status JSON now (30 s period) */
void        mqtt_app_publish_now(void);

/* Ask the MQTT thread to publish the event of device dev (0..31) now, on
 * "users/admin@example.com/<NNN>/<name>"; name and payload come from
 * mqtt_app_build_event(). Several requests for one device before it is sent
 * give one message with the latest state. */
void        mqtt_app_notify(unsigned int dev);

/* Topic "users/admin@example.com/<NNN>/<name>", NNN = last byte of the device
 * IP, 3 digits */
void        mqtt_app_topic(const char *name, char *out, size_t len);
/* Status topic: mqtt_app_topic("status") */
void        mqtt_app_status_topic(char *out, size_t len);
/* NNN of the topics (last byte of the device IP) */
int         mqtt_app_id_num(void);

/* ── Implemented by each showcase ─────────────────────────────── */
/* Status JSON of all the devices into buf. Returns its length, or 0 when
 * there is nothing to report yet (the request stays pending). */
int mqtt_app_build_status(char *buf, size_t len);
/* Event of device dev: topic name (e.g. "pump") and JSON payload. Returns the
 * payload length, or 0 to send nothing. */
int mqtt_app_build_event(unsigned int dev, char *name, size_t name_len,
                         char *buf, size_t len);

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

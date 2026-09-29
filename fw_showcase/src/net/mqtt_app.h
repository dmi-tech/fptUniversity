#ifndef MQTT_APP_H
#define MQTT_APP_H
#include <stdbool.h>

/* MQTT runs in its own thread (auto-started). These expose its state. */
bool        mqtt_app_is_connected(void);
const char *mqtt_app_status(void);

/* Ask the MQTT thread to publish immediately (alarm start/clear) */
void        mqtt_app_publish_now(void);

#endif /* MQTT_APP_H */

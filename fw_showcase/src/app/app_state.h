#ifndef APP_STATE_H
#define APP_STATE_H
#include <stdbool.h>
#include <stdint.h>

/* Runtime state shared between the main loop, the web server and MQTT */

/* Current device IP ("0.0.0.0" until an address is assigned) and MAC */
extern char g_device_ip[16];
extern char g_device_mac[18];

/* Last reported reading, served on /status and published over MQTT */
extern volatile int32_t  g_web_temp_mc;   /* milli-Celsius (DHT11: whole degrees) */
extern volatile int32_t  g_web_humi_mp;   /* milli-percent */
extern volatile uint32_t g_web_cnt;       /* number of reports so far */
extern volatile bool     g_web_sensor_ok;

/* true once the network has an IP address */
bool app_state_has_ip(void);

/* Store a new reading for the web page and ask MQTT to publish it */
void app_state_report(int temp_c, int humi_pct, bool sensor_ok);

#endif /* APP_STATE_H */

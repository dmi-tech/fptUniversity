#ifndef APP_STATE_H
#define APP_STATE_H
#include <stdbool.h>
#include <stdint.h>

/* Runtime state shared between the main loop, the web server and MQTT */

/* Current device IP ("0.0.0.0" until an address is assigned) and MAC */
extern char g_device_ip[16];
extern char g_device_mac[18];

/* Latest reading, updated after every DHT11 read; shown on /config, /status
 * and in the MQTT status message */
extern volatile int32_t  g_web_temp_mc;   /* milli-Celsius (DHT11: whole degrees), last good read */
extern volatile int32_t  g_web_humi_mp;   /* milli-percent, last good read */
extern volatile uint32_t g_web_cnt;       /* number of reads so far */
extern volatile bool     g_web_sensor_ok; /* the latest read succeeded */
extern volatile bool     g_web_have_data; /* at least one good read */

/* true once the network has an IP address */
bool app_state_has_ip(void);

/* Store the reading of one DHT11 read for the web page and MQTT */
void app_state_update(int temp_c, int humi_pct, bool sensor_ok, bool have_data);

#endif /* APP_STATE_H */

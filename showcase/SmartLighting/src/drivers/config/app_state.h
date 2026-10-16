#ifndef APP_STATE_H
#define APP_STATE_H
#include <stdbool.h>

/* Network state shared between the main loop, the web server and MQTT. The
 * readings of each showcase live in its own modules. */

/* Current device IP ("0.0.0.0" until an address is assigned) and MAC */
extern char g_device_ip[16];
extern char g_device_mac[18];

/* true once the network has an IP address */
bool app_state_has_ip(void);

#endif /* APP_STATE_H */

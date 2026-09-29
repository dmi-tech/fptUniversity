#ifndef STATUS_LED_H
#define STATUS_LED_H
#include <stdbool.h>

/* LED LIFE on PA8 (devicetree alias led0) */
bool status_led_init(void);
void status_led_set(bool on);
void status_led_toggle(void);

/* Non-blocking "double blink" heartbeat; call it often from the main loop */
void status_led_heartbeat_start(void);
void status_led_heartbeat(void);

#endif /* STATUS_LED_H */

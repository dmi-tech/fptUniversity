#ifndef MOTOR_H
#define MOTOR_H
#include <stdbool.h>

/* Motor output (Relay 3 PA15, high = run) and the PA10 toggle button.
 *
 * Four sources can switch the motor: the web page, an RS485 ON/OFF line, the
 * button and the alarm. Web / RS485 / button are manual and are never undone
 * automatically. The ALARM starts it only on over-temperature (T_HIGH) and
 * switches it off when that ends; a manual OFF meanwhile holds until the
 * next over-temperature. */
enum motor_src {
    MOTOR_SRC_NONE = 0,
    MOTOR_SRC_ALARM,
    MOTOR_SRC_BUTTON,
    MOTOR_SRC_WEB,
    MOTOR_SRC_RS485,
};

/* Configure the motor relay (PA15) and start polling the PA10 button */
bool motor_init(void);

/* Manual switch from the web page, RS485 or the button */
void motor_set(bool on, enum motor_src src);

/* Alarm hooks: start the motor if it is off / stop it if the alarm started it */
void motor_alarm_start(void);
void motor_alarm_clear(void);

bool           motor_is_on(void);
/* Source of the last change (shown as "by ..." on the web page) */
enum motor_src motor_last_src(void);
const char    *motor_src_str(enum motor_src src);   /* "web", "RS485", ... */

#endif /* MOTOR_H */

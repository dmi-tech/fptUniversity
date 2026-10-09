#ifndef ALARM_NOTIFY_H
#define ALARM_NOTIFY_H
#include <stdbool.h>

/* RS485 alarm messages, ASCII lines ending "\r\n" with a running number:
 *   ALARM #12 T=42C H=30% REASON=T_HIGH
 *   ALARM #13 T=-- H=-- REASON=SENSOR      (sensor_ok == false)
 *   CLEAR #14 T=28C H=45%
 * REASON is taken from alarm_reason(). */
void alarm_notify_rs485(bool sensor_ok, int temp_c, int humi_pct, bool clear);

#endif /* ALARM_NOTIFY_H */

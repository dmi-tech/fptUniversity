#ifndef ALARM_H
#define ALARM_H
#include <stdbool.h>
#include <stdint.h>

/* Abnormal-value thresholds (°C / %RH) and the hysteresis used to clear */
#define ALARM_T_HIGH_C     40
#define ALARM_T_LOW_C       0
#define ALARM_H_HIGH_PCT   90
#define ALARM_H_LOW_PCT    20
#define ALARM_T_HYST_C      1
#define ALARM_H_HYST_PCT    2
#define ALARM_SENSOR_FAILS  3   /* consecutive DHT11 failures => sensor alarm */

enum alarm_reason {
    ALARM_REASON_NONE = 0,
    ALARM_REASON_T_HIGH,
    ALARM_REASON_T_LOW,
    ALARM_REASON_H_HIGH,
    ALARM_REASON_H_LOW,
    ALARM_REASON_SENSOR,
};

enum alarm_event {
    ALARM_EVT_NONE = 0,
    ALARM_EVT_START,   /* normal -> abnormal */
    ALARM_EVT_CLEAR,   /* abnormal -> normal */
};

/* Configure relay 1 (PB5); call motor_init() first */
bool alarm_init(void);

/* Feed one DHT11 sample (ok=false when the read failed). Starts/stops the
 * 1 s relay blink and the alarm-driven motor (motor.h) on a state change. */
enum alarm_event alarm_update(bool ok, int temp_c, int humi_pct);

bool              alarm_active(void);
enum alarm_reason alarm_reason(void);
const char       *alarm_reason_str(enum alarm_reason r);   /* T_HIGH, ... */
const char       *alarm_reason_text(enum alarm_reason r);  /* LCD text */

#endif /* ALARM_H */

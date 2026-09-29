/* alarm_notify.c — RS485 ALARM / CLEAR messages */
#include "alarm_notify.h"
#include "alarm.h"
#include "rs485.h"
#include <stdint.h>
#include <stdio.h>

static uint32_t s_seq;

void alarm_notify_rs485(bool sensor_ok, int temp_c, int humi_pct, bool clear)
{
    char m[96];

    s_seq++;
    if (clear) {
        snprintf(m, sizeof(m), "CLEAR #%u T=%dC H=%d%%\r\n", s_seq, temp_c, humi_pct);
    } else if (sensor_ok) {
        snprintf(m, sizeof(m), "ALARM #%u T=%dC H=%d%% REASON=%s\r\n",
                 s_seq, temp_c, humi_pct, alarm_reason_str(alarm_reason()));
    } else {
        snprintf(m, sizeof(m), "ALARM #%u T=-- H=-- REASON=%s\r\n",
                 s_seq, alarm_reason_str(alarm_reason()));
    }
    rs485_send(m);
}

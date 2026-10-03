/* alarm.c — abnormal-value detection and the actions it drives.
 *
 *   relay 1 (PB5)  blinks with a 1 s period (500 ms on / 500 ms off) while
 *                  the alarm is active
 *   motor  (PA2)   started on an alarm, stopped again when it clears unless
 *                  switched by hand meanwhile (see motor.h)
 */
#include "alarm.h"
#include "motor.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define BLINK_HALF_PERIOD_MS 500

static const struct gpio_dt_spec s_relay  = GPIO_DT_SPEC_GET(DT_ALIAS(relay1), gpios);

static bool              s_active;
static enum alarm_reason s_reason;
static int               s_fails;

static struct k_timer s_blink_timer;

/* ── Relay 1 blink ───────────────────────────────────────────── */
static void blink_expiry(struct k_timer *t)
{
    ARG_UNUSED(t);
    gpio_pin_toggle_dt(&s_relay);
}

static void blink_start(void)
{
    gpio_pin_set_dt(&s_relay, 1);
    k_timer_start(&s_blink_timer, K_MSEC(BLINK_HALF_PERIOD_MS), K_MSEC(BLINK_HALF_PERIOD_MS));
}

static void blink_stop(void)
{
    k_timer_stop(&s_blink_timer);
    gpio_pin_set_dt(&s_relay, 0);
}

bool alarm_init(void)
{
    if (!gpio_is_ready_dt(&s_relay)) {
        printk("[ALARM] relay GPIO not ready\n");
        return false;
    }
    gpio_pin_configure_dt(&s_relay, GPIO_OUTPUT_INACTIVE);
    k_timer_init(&s_blink_timer, blink_expiry, NULL);

    printk("[ALARM] RL1 PB5 OK\n");
    return true;
}

/* ── Detection ───────────────────────────────────────────────── */
static enum alarm_reason evaluate(int t, int h, bool active)
{
    /* When already active a value must move back by the hysteresis before it
     * counts as normal again, so it cannot chatter around a threshold. */
    int th = active ? ALARM_T_HYST_C   : 0;
    int hh = active ? ALARM_H_HYST_PCT : 0;

    if (t > ALARM_T_HIGH_C - th) return ALARM_REASON_T_HIGH;
    if (t < ALARM_T_LOW_C  + th) return ALARM_REASON_T_LOW;
    if (h > ALARM_H_HIGH_PCT - hh) return ALARM_REASON_H_HIGH;
    if (h < ALARM_H_LOW_PCT  + hh) return ALARM_REASON_H_LOW;
    return ALARM_REASON_NONE;
}

enum alarm_event alarm_update(bool ok, int temp_c, int humi_pct)
{
    enum alarm_reason r;

    if (ok) {
        s_fails = 0;
        r = evaluate(temp_c, humi_pct, s_active);
    } else {
        if (s_fails < ALARM_SENSOR_FAILS) s_fails++;
        /* Keep the current state until the failure is persistent */
        r = (s_fails >= ALARM_SENSOR_FAILS) ? ALARM_REASON_SENSOR : s_reason;
    }

    bool now_active = (r != ALARM_REASON_NONE);
    s_reason = r;

    if (now_active && !s_active) {
        s_active = true;
        blink_start();
        motor_alarm_start();
        return ALARM_EVT_START;
    }
    if (!now_active && s_active) {
        s_active = false;
        blink_stop();
        motor_alarm_clear();
        return ALARM_EVT_CLEAR;
    }
    return ALARM_EVT_NONE;
}

bool              alarm_active(void) { return s_active; }
enum alarm_reason alarm_reason(void) { return s_reason; }

const char *alarm_reason_str(enum alarm_reason r)
{
    switch (r) {
    case ALARM_REASON_T_HIGH: return "T_HIGH";
    case ALARM_REASON_T_LOW:  return "T_LOW";
    case ALARM_REASON_H_HIGH: return "H_HIGH";
    case ALARM_REASON_H_LOW:  return "H_LOW";
    case ALARM_REASON_SENSOR: return "SENSOR";
    default:                  return "NONE";
    }
}

const char *alarm_reason_text(enum alarm_reason r)
{
    switch (r) {
    case ALARM_REASON_T_HIGH: return "!! TEMP HIGH !!";
    case ALARM_REASON_T_LOW:  return "!! TEMP LOW !!";
    case ALARM_REASON_H_HIGH: return "!! HUMI HIGH !!";
    case ALARM_REASON_H_LOW:  return "!! HUMI LOW !!";
    case ALARM_REASON_SENSOR: return "!! SENSOR ERR !!";
    default:                  return "Status: OK";
    }
}

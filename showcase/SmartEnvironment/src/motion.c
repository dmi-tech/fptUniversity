/* motion.c — spray servo and BTN2 hold (motion thread, 20 ms tick).
 *
 * AUTO sweeps SERVO_AUTO_MIN_DEG..SERVO_AUTO_MAX_DEG while the pump runs and
 * parks at SERVO_PARK_DEG otherwise; MANUAL turns while BTN3 / BTN4 are held
 * or goes to the angle set on the web page. BTN2 held MODE_HOLD_MS switches
 * the mode (through the main thread). The angle is kept in milli-degrees so
 * slow speeds move smoothly. */
#include <zephyr/sys/printk.h>

#include "driver_servo.h"
#include "mqtt_app.h"
#include "app.h"

/* Move *mdeg towards target by at most step; true once there */
static bool approach(int32_t *mdeg, int32_t target, int32_t step)
{
    if (*mdeg < target)      *mdeg = MIN(*mdeg + step, target);
    else if (*mdeg > target) *mdeg = MAX(*mdeg - step, target);
    return *mdeg == target;
}

/* Main thread, g_lock held */
void servo_set_mode(enum servo_mode m)
{
    if (atomic_set(&g_servo_mode, m) != m) {
        printk("[SERVO] mode %s\n", m == SERVO_AUTO ? "auto" : "manual");
        g_events |= BIT(DEV_SERVO);
    }
}

int servo_angle(void)
{
    return (int)((atomic_get(&g_servo_mdeg) + 500) / 1000);
}

static void motion_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    int32_t mdeg = MDEG(SERVO_PARK_DEG);
    int8_t  dir = 1, last_hold = 0;
    uint32_t pulse_us = 0;
    bool    mode_done = false;
    int64_t next = k_uptime_get();

    while (1) {
        int64_t now = k_uptime_get();

        /* BTN2: switch once after MODE_HOLD_MS, then wait for the release */
        uint32_t pressed_at = (uint32_t)atomic_get(&g_mode_press_ms);
        if (pressed_at == 0) {
            mode_done = false;
        } else if (!mode_done && (uint32_t)now - pressed_at >= MODE_HOLD_MS) {
            mode_done = true;
            app_post(CMD_SERVO_TOGGLE, 0);
        }

        /* Servo */
        if (atomic_get(&g_servo_mode) == SERVO_AUTO) {
            int32_t step = SERVO_AUTO_SPEED_DPS * MOTION_TICK_MS;   /* milli-degrees */
            bool sweep = atomic_get(&g_pump_on);
            atomic_set(&g_sweeping, sweep);
            if (sweep) {
                int32_t target = dir > 0 ? MDEG(SERVO_AUTO_MAX_DEG) : MDEG(SERVO_AUTO_MIN_DEG);
                if (approach(&mdeg, target, step)) dir = -dir;
            } else {
                approach(&mdeg, MDEG(SERVO_PARK_DEG), step);
            }
            last_hold = 0;
            atomic_set(&g_servo_req, -1);
        } else {
            atomic_set(&g_sweeping, 0);
            atomic_val_t req = atomic_set(&g_servo_req, -1);
            if (req >= 0) mdeg = MDEG(req);
            bool l = atomic_get(&g_hold_left), r = atomic_get(&g_hold_right);
            int8_t hold = (l == r) ? 0 : (l ? -1 : 1);
            if (hold) {
                approach(&mdeg, hold > 0 ? MDEG(SERVO_MAX_DEG) : MDEG(SERVO_MIN_DEG),
                         SERVO_MANUAL_SPEED_DPS * MOTION_TICK_MS);
            } else if (last_hold) {
                mqtt_app_notify(DEV_SERVO);    /* report where the user stopped */
            }
            last_hold = hold;
        }
        atomic_set(&g_servo_mdeg, mdeg);
        uint32_t us = SERVO_PULSE_MIN_US + (uint32_t)((int64_t)mdeg *
                      (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US) / MDEG(SERVO_MAX_DEG));
        if (us != pulse_us) {
            pulse_us = us;
            driver_servo_set_pulse_us(SERVO_ID, (uint16_t)us);
        }
        /* Fixed 20 ms grid (absolute wake-up): the time spent above does
         * not add up, so the sweep speed stays exact */
        next += MOTION_TICK_MS;
        if (next < now) next = now + MOTION_TICK_MS;      /* overrun: no catch-up burst */
        k_sleep(K_TIMEOUT_ABS_MS(next));
    }
}

/* Started by main after the boot screen */
K_THREAD_DEFINE(motion_tid, MOTION_STACK, motion_thread, NULL, NULL, NULL, MOTION_PRIO, 0,
                SYS_FOREVER_MS);

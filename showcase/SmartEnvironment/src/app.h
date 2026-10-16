#ifndef APP_H
#define APP_H

/* Types, shared state and functions of the SmartEnvironment files:
 *
 *   main.c         start-up, main loop (DHT11, ticks, commands), buttons
 *   env.c          situation table, environment analysis, relay 1
 *   pump.c         pump state machine
 *   motion.c       motion thread: servo, BTN2 3 s hold
 *   lcd_view.c     lcd thread: boot, status and warning screens
 *   status_json.c  JSON of the devices (MQTT status / events, web page)
 *   web_app.c      web page hooks and /api/... routes (page text: web_page.h)
 *
 * The environment and pump state (g_st) only change in the main thread,
 * under g_lock; the other threads read it under g_lock or use the atomics. */
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <stdbool.h>
#include <stdint.h>
#include "config.h"

/* MQTT event topics, one per device (names in status_json.c) */
enum dev { DEV_PUMP, DEV_SERVO, DEV_ENV, DEV_SENSOR, DEV_RELAY, DEV_COUNT };

/* ── Environment situations (env.c) ──────────────────────────── */
enum level { LVL_OK, LVL_WARN, LVL_CRIT };

enum code {
    ENV_WAIT, ENV_OK, ENV_HOT_DRY, ENV_HOT, ENV_HOT_EXTREME, ENV_HOT_HUMID,
    ENV_DRY, ENV_HUMID, ENV_COLD_DRY, ENV_COLD, ENV_COLD_HUMID, ENV_SENSOR,
    ENV_CODE_COUNT,
};

struct env_info {
    const char *str;
    enum level  level;
    bool        waters;
    const char *title;     /* Vietnamese, web / MQTT */
    const char *advice;    /* Vietnamese, web / MQTT */
    const char *lcd;       /* English, LCD warning screen, <= 20 characters */
};

extern const struct env_info env_info[ENV_CODE_COUNT];
extern const char *const level_str[];

/* ── Pump and servo ──────────────────────────────────────────── */
enum pump_src  { SRC_NONE, SRC_AUTO, SRC_BUTTON, SRC_WEB, SRC_TIMEOUT };
extern const char *const src_str[];
enum servo_mode { SERVO_AUTO, SERVO_MANUAL };

/* What the LCD, the web page and MQTT show (written by the main thread) */
struct state {
    /* environment */
    bool      have_data, sensor_ok;
    int       temp_c, humi_pct, heat_index_c;
    bool      hi_valid;
    uint32_t  reads;
    enum code code;
    bool      water_request;
    /* pump */
    bool      pump_on;
    enum pump_src pump_by;
    bool      inhibited;          /* auto mode INHIBITED, else ARMED */
    bool      session, resting;   /* automatic watering, pump off between cycles */
    int64_t   phase_end;          /* end of the cycle phase / manual run */
    uint8_t   cycles;             /* automatic cycles in the current hour */
};

extern struct k_mutex g_lock;
extern struct state   g_st;
extern uint32_t       g_events;          /* BIT(dev) to publish (main thread) */

/* Atomics shared with the motion thread and the button callbacks */
extern atomic_t g_servo_mode;            /* enum servo_mode */
extern atomic_t g_servo_mdeg;            /* servo angle, milli-degrees */
extern atomic_t g_servo_req;             /* web angle request, -1 = none */
extern atomic_t g_sweeping;
extern atomic_t g_pump_on;               /* the servo sweeps while it is set */
extern atomic_t g_hold_left, g_hold_right;   /* BTN3 / BTN4 held */
extern atomic_t g_mode_press_ms;         /* BTN2 pressed since (uptime ms), 0 = released */
extern atomic_t g_alert_half_ms;         /* relay 1 blink half period, 0 = off */

/* ── Commands (buttons, web, motion thread) → main thread ────── */
enum cmd_type {
    CMD_PUMP_TOGGLE,      /* BTN1 */
    CMD_PUMP_SET,         /* web: val = 0/1 */
    CMD_PUMP_REARM,       /* web */
    CMD_SERVO_TOGGLE,     /* BTN2 held 3 s */
    CMD_SERVO_SET_MODE,   /* web: val = mode */
    CMD_SERVO_ANGLE,      /* web: val = degrees */
};

/* main.c */
bool app_post(uint8_t type, int16_t val);     /* false when the queue is full */
void app_snapshot(struct state *st);           /* copy of g_st under g_lock */

/* env.c (main thread, g_lock held) */
void env_update(bool ok, int temp_c, int humi_pct, int64_t now);
enum level env_level(enum code c);

/* pump.c (main thread, g_lock held) */
void pump_start(int64_t now);
void pump_tick(int64_t now);
void pump_env_update(bool was_req, int64_t now);
void pump_manual(bool on, enum pump_src src, int64_t now);
void pump_rearm(int64_t now);

/* motion.c */
extern const k_tid_t motion_tid;
void servo_set_mode(enum servo_mode m);        /* main thread */
int  servo_angle(void);                        /* degrees */

/* lcd_view.c */
extern const k_tid_t lcd_tid;
void lcd_view_start(void);                     /* probe the LCD, boot screen */
bool lcd_view_ok(void);

#endif /* APP_H */

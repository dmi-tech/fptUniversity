#ifndef APP_H
#define APP_H

/* Types, shared state and functions of the SmartLighting files:
 *
 *   main.c         start-up, main loop (BH1750 read, commands), buttons
 *   lamp.c         dark / bright, AUTO / MANUAL, own light, off timer,
 *                  sensor fault action, quiet hours
 *   settings.c     settings, mode and manual lamp in flash (g_cfg.app_data)
 *   clock.c        clock thread: local time over SNTP
 *   lcd_view.c     lcd thread: boot, status and warning screens
 *   status_json.c  JSON of the devices (MQTT status / events, web page)
 *   web_app.c      web page hooks and /api/... routes (page text: web_page.h)
 *
 * The lamp state (g_st) and the settings (g_set) only change in the main
 * thread, under g_lock; the other threads read them under g_lock. */
#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include "config.h"

/* MQTT event topics, one per device (names in status_json.c) */
enum dev { DEV_LAMP, DEV_LIGHT, DEV_MODE, DEV_SENSOR, DEV_COUNT };

enum src   { SRC_NONE, SRC_AUTO, SRC_BUTTON, SRC_WEB, SRC_TIMER, SRC_RESTORE };
enum mode  { MODE_AUTO, MODE_MANUAL };
enum fault { FAULT_KEEP, FAULT_ON, FAULT_OFF };    /* AUTO lamp on a sensor fault */
extern const char *const src_str[];
extern const char *const fault_str[];

struct settings {
    uint16_t on_lux;
    uint16_t off_lux;
    uint16_t confirm_s;
    uint16_t timer_s;        /* off timer, 0 = none */
    uint8_t  fault;          /* enum fault */
    uint8_t  quiet_on;       /* quiet hours: AUTO keeps the lamp off */
    uint16_t quiet_from;     /* minute of the day, local time */
    uint16_t quiet_to;
    /* restored after a power cut (saved SAVE_DELAY_MS after a change) */
    uint8_t  mode;           /* enum mode */
    uint8_t  lamp;           /* lamp in MANUAL */
};

/* What the LCD, the web page and MQTT show (written by the main thread) */
struct state {
    /* sensor */
    bool      have_data, ok, fault;
    int32_t   lux;
    uint32_t  reads;
    /* lamp */
    bool      on;
    enum src  by;
    enum mode mode;
    bool      night_known, night, hold, quiet;
    int32_t   ambient, lamp_lux;
    int64_t   on_since, cand_since;
};

extern struct k_mutex   g_lock;
extern struct state     g_st;
extern struct settings  g_set;
extern uint32_t         g_events;        /* BIT(dev) to publish (main thread) */

/* ── Commands (buttons, web) → main thread ───────────────────── */
enum cmd_type {
    CMD_LAMP_TOGGLE,      /* BTN1 */
    CMD_LAMP_SET,         /* web: val = 0/1 */
    CMD_MODE_TOGGLE,      /* BTN2 */
    CMD_MODE_SET,         /* web: val = mode */
    CMD_TIMER_STEP,       /* BTN3 / BTN4: val = +1 / -1 steps */
    CMD_TIMER_SET,        /* web: val = seconds */
    CMD_SETTINGS,         /* web: set */
};

struct cmd {
    uint8_t type;
    int32_t val;
    struct settings set;
};

/* main.c */
bool app_post(const struct cmd *c);                       /* false when the queue is full */
void app_snapshot(struct state *st, struct settings *set); /* copies under g_lock */

/* lamp.c (main thread, g_lock held) */
void lamp_update(int rc, float lux, int64_t now);         /* one BH1750 reading */
void lamp_manual(bool on, enum src src, int64_t now);     /* leaves AUTO */
void lamp_set_mode(enum mode m, int64_t now);
void lamp_restore(int64_t now);                           /* start-up: saved mode / lamp */

/* settings.c */
void settings_load(void);                                  /* at start-up */
const char *settings_check(const struct settings *s);     /* NULL or the reason */
void settings_save_at(int64_t at);                         /* main thread, g_lock held */
void settings_poll(int64_t now);                           /* main thread: write when due */

/* clock.c */
bool clock_minute(int *min);                               /* local minute of the day; false = not synced */

/* lcd_view.c */
extern const k_tid_t lcd_tid;
void lcd_view_start(void);                                 /* probe the LCD, boot screen */
bool lcd_view_ok(void);

#endif /* APP_H */

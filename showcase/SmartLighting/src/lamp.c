/* lamp.c — dusk-to-dawn lamp on Relay 1 (out0).
 *
 * AUTO   night starts when the ambient light stays below on_lux for
 *        confirm_s, ends when it stays above off_lux for confirm_s; the lamp
 *        is on during the night. The first reading decides at once. The
 *        lamp's own light on the sensor is measured SELF_MEASURE_MS after each
 *        switch-on and removed from the reading while the lamp is on.
 * MANUAL BTN1 / web switch the lamp; used in AUTO they switch to MANUAL.
 *        Back to AUTO by BTN2 / web, or by itself at the next confirmed
 *        day / night change; the lamp then follows the sensor at once.
 * Saved  the mode and the MANUAL lamp are restored after a power cut.
 * Timer  0..12 h in seconds, counted from the switch-on, both modes; in AUTO
 *        the lamp then stays off until the next day.
 * Quiet  quiet hours (local time, clock.c): AUTO keeps the lamp off.
 * Fault  SENSOR_FAILS failed reads in a row: AUTO keeps the lamp, or switches
 *        it on / off (g_set.fault); BTN1 / web still switch it by hand.
 * Relay  switched at most once per RELAY_MIN_MS; a faster request is ignored.
 * All functions run in the main thread with g_lock held. */
#include <zephyr/sys/printk.h>

#include "driver_gpio.h"
#include "app.h"

static int     s_fails;
static int32_t s_lux_before;      /* reading just before the last switch-on */
static int64_t s_measure_at;      /* measure the lamp's own light then, 0 = no */
static int64_t s_switched_at;     /* last relay switch, 0 = none yet */

static void lamp_output(bool on, enum src src, int64_t now)
{
    if (on == g_st.on && src == g_st.by) return;
    if (on != g_st.on) {
        if (s_switched_at && now - s_switched_at < RELAY_MIN_MS) {
            printk("[LAMP] %s (%s) ignored: relay switched < %d ms ago\n",
                   on ? "ON" : "OFF", src_str[src], RELAY_MIN_MS);
            return;
        }
        s_switched_at = now;
        printk("[LAMP] %s (%s)\n", on ? "ON" : "OFF", src_str[src]);
        if (on) {
            g_st.on_since = now;
            s_lux_before  = g_st.lux;
            /* no reading before the switch-on (start-up): keep the old value */
            s_measure_at  = g_st.have_data ? now + SELF_MEASURE_MS : 0;
        } else {
            s_measure_at = 0;
        }
        driver_gpio_set(OUT_LAMP, on);
    }
    g_events |= BIT(DEV_LAMP);
    g_st.on = on;
    g_st.by = src;
    if (g_st.mode == MODE_MANUAL && g_set.lamp != on) {
        g_set.lamp = on;
        settings_save_at(now + SAVE_DELAY_MS);
    }
}

static void set_night(bool night, int64_t now)
{
    bool change = g_st.night_known;   /* not the first reading */

    g_st.night = night;
    g_st.night_known = true;
    g_st.cand_since = 0;
    g_events |= BIT(DEV_LIGHT);
    printk("[LAMP] %s (ambient %d lx)\n", night ? "night" : "day", g_st.ambient);
    if (!night) g_st.hold = false;   /* a bright period ends the timer hold */
    if (change && g_st.mode == MODE_MANUAL) {
        printk("[LAMP] %s: back to auto\n", night ? "dusk" : "dawn");
        lamp_set_mode(MODE_AUTO, now);
    }
}

/* Quiet hours now? from > to spans midnight (23:00..05:00) */
static bool quiet_now(void)
{
    int m;

    if (!g_set.quiet_on || !clock_minute(&m)) return false;
    if (g_set.quiet_from <= g_set.quiet_to) return m >= g_set.quiet_from && m < g_set.quiet_to;
    return m >= g_set.quiet_from || m < g_set.quiet_to;
}

/* AUTO: the lamp follows the night (or the fault action), unless held off
 * by the timer or the quiet hours */
static void apply_auto(int64_t now)
{
    bool want;

    if (g_st.mode != MODE_AUTO) return;
    if (g_st.fault) {
        if (g_set.fault == FAULT_KEEP) return;
        want = (g_set.fault == FAULT_ON);
    } else if (g_st.night_known) {
        want = g_st.night;
    } else {
        return;
    }
    want = want && !g_st.hold && !g_st.quiet;
    if (want != g_st.on) lamp_output(want, SRC_AUTO, now);
}

/* One BH1750 reading: sensor fault, own light, dark/bright, timer, AUTO */
void lamp_update(int rc, float lux_f, int64_t now)
{
    bool was_fault = g_st.fault;

    g_st.reads++;
    g_st.ok = (rc == 0);
    if (rc == 0) {
        s_fails = 0;
        g_st.have_data = true;
        g_st.lux = (int32_t)(lux_f + 0.5f);
    } else if (s_fails < SENSOR_FAILS) {
        s_fails++;
    }
    g_st.fault = (s_fails >= SENSOR_FAILS);
    if (g_st.fault != was_fault) {
        g_events |= BIT(DEV_SENSOR);
        printk("[LIGHT] sensor %s\n", g_st.fault ? "FAULT" : "back");
        if (!g_st.fault && !g_st.night) g_st.hold = false;   /* hold set during the fault */
    }

    if (rc == 0) {
        /* The lamp's own light, once it is fully on */
        if (g_st.on && s_measure_at && now >= s_measure_at) {
            g_st.lamp_lux = MAX(g_st.lux - s_lux_before, 0);
            s_measure_at  = 0;
            printk("[LAMP] own light %d lx\n", g_st.lamp_lux);
        }
        g_st.ambient = g_st.on ? MAX(g_st.lux - g_st.lamp_lux, 0) : g_st.lux;

        /* Dark / bright: first reading at once, then confirmed changes, not
         * while the lamp's own light is being measured */
        if (!g_st.night_known) {
            set_night(g_st.ambient < g_set.on_lux, now);
        } else if (!(g_st.on && s_measure_at)) {
            bool dark = g_st.night ? (g_st.ambient <= g_set.off_lux)
                                   : (g_st.ambient < g_set.on_lux);
            if (dark == g_st.night) {
                g_st.cand_since = 0;
            } else if (g_st.cand_since == 0) {
                g_st.cand_since = now;
            } else if (now - g_st.cand_since >= (int64_t)g_set.confirm_s * 1000) {
                set_night(dark, now);
            }
        }
    }

    /* Quiet hours */
    bool quiet = quiet_now();
    if (quiet != g_st.quiet) {
        g_st.quiet = quiet;
        g_events |= BIT(DEV_MODE);
        printk("[LAMP] quiet hours %s\n", quiet ? "start" : "end");
    }

    /* Off timer, both modes */
    if (g_st.on && g_set.timer_s && now - g_st.on_since >= (int64_t)g_set.timer_s * 1000) {
        printk("[LAMP] timer %u s elapsed\n", g_set.timer_s);
        if (g_st.mode == MODE_AUTO && (g_st.night || g_st.fault)) g_st.hold = true;
        lamp_output(false, SRC_TIMER, now);
    }

    apply_auto(now);
}

void lamp_set_mode(enum mode m, int64_t now)
{
    if (m != g_st.mode) {
        g_st.mode = m;
        g_events |= BIT(DEV_MODE);
        printk("[LAMP] mode %s\n", m == MODE_AUTO ? "auto" : "manual");
        g_set.mode = m;
        g_set.lamp = g_st.on;
        settings_save_at(now + SAVE_DELAY_MS);
    }
    if (m == MODE_AUTO) {
        g_st.hold = false;
        apply_auto(now);                         /* follow the sensor at once */
    }
}

void lamp_manual(bool on, enum src src, int64_t now)
{
    lamp_set_mode(MODE_MANUAL, now);             /* manual use leaves AUTO */
    lamp_output(on, src, now);
    g_events |= BIT(DEV_LAMP);
}

/* Start-up: the mode and, in MANUAL, the lamp saved before the power cut */
void lamp_restore(int64_t now)
{
    g_st.mode = (enum mode)g_set.mode;
    printk("[LAMP] restored: mode %s\n", g_st.mode == MODE_AUTO ? "auto" : "manual");
    if (g_st.mode == MODE_MANUAL) lamp_output(g_set.lamp, SRC_RESTORE, now);
}

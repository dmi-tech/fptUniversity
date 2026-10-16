/* lamp.c — dusk-to-dawn lamp on Relay 3 (out0).
 *
 * AUTO   night starts when the ambient light stays below on_lux for
 *        confirm_s, ends when it stays above off_lux for confirm_s; the lamp
 *        is on during the night. The first reading decides at once. The
 *        lamp's own light on the sensor is measured SELF_MEASURE_MS after each
 *        switch-on and removed from the reading while the lamp is on.
 * MANUAL BTN1 / web switch the lamp; used in AUTO they switch to MANUAL.
 *        Back to AUTO, the lamp follows the sensor at once.
 * Timer  0..12 h, counted from the switch-on, both modes; in AUTO the lamp
 *        then stays off until the next night.
 * Fault  SENSOR_FAILS failed reads in a row: the lamp keeps its state.
 * All functions run in the main thread with g_lock held. */
#include <zephyr/sys/printk.h>

#include "driver_gpio.h"
#include "app.h"

static int     s_fails;
static int32_t s_lux_before;      /* reading just before the last switch-on */
static int64_t s_measure_at;      /* measure the lamp's own light then, 0 = no */

static void lamp_output(bool on, enum src src, int64_t now)
{
    if (on == g_st.on && src == g_st.by) return;
    g_events |= BIT(DEV_LAMP);
    if (on != g_st.on) {
        printk("[LAMP] %s (%s)\n", on ? "ON" : "OFF", src_str[src]);
        if (on) {
            g_st.on_since = now;
            s_lux_before  = g_st.lux;
            s_measure_at  = now + SELF_MEASURE_MS;
        } else {
            s_measure_at = 0;
        }
        driver_gpio_set(OUT_LAMP, on);
    }
    g_st.on = on;
    g_st.by = src;
}

static void set_night(bool night)
{
    g_st.night = night;
    g_st.night_known = true;
    g_st.cand_since = 0;
    g_events |= BIT(DEV_LIGHT);
    printk("[LAMP] %s (ambient %d lx)\n", night ? "night" : "day", g_st.ambient);
    if (!night) g_st.hold = false;   /* a bright period ends the timer hold */
}

/* AUTO: the lamp follows the night, unless held off by the timer */
static void apply_auto(int64_t now)
{
    if (g_st.mode != MODE_AUTO || !g_st.night_known) return;
    bool want = g_st.night && !g_st.hold;
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
            set_night(g_st.ambient < g_set.on_lux);
        } else if (!(g_st.on && s_measure_at)) {
            bool dark = g_st.night ? (g_st.ambient <= g_set.off_lux)
                                   : (g_st.ambient < g_set.on_lux);
            if (dark == g_st.night) {
                g_st.cand_since = 0;
            } else if (g_st.cand_since == 0) {
                g_st.cand_since = now;
            } else if (now - g_st.cand_since >= (int64_t)g_set.confirm_s * 1000) {
                set_night(dark);
            }
        }
    }

    /* Off timer, both modes */
    if (g_st.on && g_set.timer_h && now - g_st.on_since >= g_set.timer_h * HOUR_MS) {
        printk("[LAMP] timer %u h elapsed\n", g_set.timer_h);
        if (g_st.mode == MODE_AUTO && g_st.night) g_st.hold = true;
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

/* pump.c — mini pump on Relay 3 (out0).
 *
 * Automatic watering while ARMED and a watering situation has held
 * ENV_WATER_CONFIRM_MS (g_st.water_request): cycles PUMP_ON_S on /
 * PUMP_REST_S off, at most PUMP_MAX_CYCLES_PER_HOUR. A manual OFF during
 * watering (or while water is wanted) sets INHIBITED until "Re-enable auto"
 * on the web or the end of the watering situation. Manual ON stops after
 * PUMP_MANUAL_MAX_S. All functions run in the main thread with g_lock held. */
#include <zephyr/sys/printk.h>

#include "driver_gpio.h"
#include "app.h"

static int64_t s_hour_start;

static void pump_output(bool on, enum pump_src src)
{
    if (on != g_st.pump_on || src != g_st.pump_by) g_events |= BIT(DEV_PUMP);
    if (on != g_st.pump_on) {
        printk("[PUMP] %s (%s)\n", on ? "ON" : "OFF", src_str[src]);
        driver_gpio_set(OUT_PUMP, on);
        atomic_set(&g_pump_on, on);
    }
    g_st.pump_on = on;
    g_st.pump_by = src;
}

static void set_inhibited(bool inh)
{
    if (inh == g_st.inhibited) return;
    g_st.inhibited = inh;
    g_events |= BIT(DEV_PUMP);
    printk("[PUMP] auto %s\n", inh ? "inhibited" : "armed");
}

static void end_session(void)
{
    if (!g_st.session) return;
    g_st.session = false;
    g_st.resting = false;
    g_events |= BIT(DEV_PUMP);
    if (g_st.pump_on && g_st.pump_by == SRC_AUTO) pump_output(false, SRC_AUTO);
}

static void start_cycle(int64_t now)
{
    g_st.session   = true;
    g_st.resting   = false;
    g_st.phase_end = now + PUMP_ON_S * 1000;
    g_st.cycles++;
    pump_output(true, SRC_AUTO);
}

/* Start automatic watering when everything allows it */
static void pump_evaluate(int64_t now)
{
    bool manual_on = g_st.pump_on && g_st.pump_by != SRC_AUTO;

    if (!g_st.session && !g_st.inhibited && g_st.water_request && !manual_on &&
        g_st.cycles < PUMP_MAX_CYCLES_PER_HOUR) {
        start_cycle(now);
    }
}

void pump_start(int64_t now)
{
    s_hour_start = now;
}

/* Water request after a DHT11 read */
void pump_env_update(bool was_req, int64_t now)
{
    if (was_req && !g_st.water_request) {
        end_session();             /* the watering alert is over */
        set_inhibited(false);      /* and allows auto again */
    }
    pump_evaluate(now);
}

/* 1 s tick: cycle phases, manual run limit, hourly cycle count */
void pump_tick(int64_t now)
{
    if (now - s_hour_start >= HOUR_MS) {
        s_hour_start = now;
        g_st.cycles = 0;
    }
    if (g_st.pump_on && g_st.pump_by != SRC_AUTO && now >= g_st.phase_end) {
        printk("[PUMP] manual run limit reached\n");
        pump_output(false, SRC_TIMEOUT);
    }
    if (g_st.session && now >= g_st.phase_end) {
        if (!g_st.resting) {
            g_st.resting   = true;
            g_st.phase_end = now + PUMP_REST_S * 1000;
            pump_output(false, SRC_AUTO);
        } else if (!g_st.inhibited && g_st.water_request &&
                   g_st.cycles < PUMP_MAX_CYCLES_PER_HOUR) {
            start_cycle(now);
        } else {
            end_session();
        }
    }
    pump_evaluate(now);
}

/* Manual switch (BTN1 / web) */
void pump_manual(bool on, enum pump_src src, int64_t now)
{
    if (on) {
        /* a manual ON takes over: the automatic session ends */
        g_st.session = false;
        g_st.resting = false;
        g_st.phase_end = now + PUMP_MANUAL_MAX_S * 1000;
        pump_output(true, src);
        g_events |= BIT(DEV_PUMP);
    } else {
        /* stopping while water is wanted must not restart it at the next tick */
        if (g_st.session || g_st.water_request) set_inhibited(true);
        pump_output(false, src);
        end_session();
    }
}

/* Web "Re-enable auto" */
void pump_rearm(int64_t now)
{
    set_inhibited(false);
    pump_evaluate(now);
}

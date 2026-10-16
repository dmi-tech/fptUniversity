/* SmartEnvironment: greenhouse / garden monitor with mist watering.
 *
 * main.c: start-up, the main loop and the buttons. The main thread reads the
 * DHT11 every 2 s (env.c), runs the pump state machine every second
 * (pump.c) and applies the commands of the buttons and the web page
 * (g_cmdq), so the environment and pump state only change here. The servo
 * (motion.c) and the LCD (lcd_view.c) have their own threads; LED LIFE and
 * relay 1 blink by themselves (driver_gpio_blink). Overview: app.h;
 * behaviour: README.md. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_dht11.h"
#include "driver_gpio.h"
#include "driver_servo.h"

#include "app_config.h"
#include "app_state.h"
#include "boot_swap.h"
#include "mqtt_app.h"
#include "network.h"
#include "app.h"

K_MUTEX_DEFINE(g_lock);
struct state g_st = { .code = ENV_WAIT };
uint32_t     g_events;

atomic_t g_servo_mode;
atomic_t g_servo_mdeg = ATOMIC_INIT(MDEG(SERVO_PARK_DEG));
atomic_t g_servo_req = ATOMIC_INIT(-1);
atomic_t g_sweeping;
atomic_t g_pump_on;
atomic_t g_hold_left, g_hold_right;
atomic_t g_mode_press_ms;
atomic_t g_alert_half_ms;

const char *const src_str[] = { "", "auto", "button", "web", "timeout" };

/* ── Commands → main thread ──────────────────────────────────── */
struct cmd {
    uint8_t type;
    int16_t val;
};

K_MSGQ_DEFINE(g_cmdq, sizeof(struct cmd), 8, 4);

bool app_post(uint8_t type, int16_t val)
{
    struct cmd c = { .type = type, .val = val };

    return k_msgq_put(&g_cmdq, &c, K_NO_WAIT) == 0;
}

void app_snapshot(struct state *st)
{
    k_mutex_lock(&g_lock, K_FOREVER);
    *st = g_st;
    k_mutex_unlock(&g_lock);
}

/* g_lock held */
static void handle_cmd(const struct cmd *c, int64_t now)
{
    switch (c->type) {
    case CMD_PUMP_TOGGLE:   /* during a session (even resting): OFF */
        pump_manual(!(g_st.pump_on || g_st.session), SRC_BUTTON, now);
        break;
    case CMD_PUMP_SET:
        pump_manual(c->val != 0, SRC_WEB, now);
        break;
    case CMD_PUMP_REARM:
        pump_rearm(now);
        break;
    case CMD_SERVO_TOGGLE:
        servo_set_mode(atomic_get(&g_servo_mode) == SERVO_AUTO ? SERVO_MANUAL : SERVO_AUTO);
        break;
    case CMD_SERVO_SET_MODE:
        servo_set_mode((enum servo_mode)c->val);
        break;
    case CMD_SERVO_ANGLE:
        if (atomic_get(&g_servo_mode) == SERVO_MANUAL) {
            atomic_set(&g_servo_req, c->val);
            g_events |= BIT(DEV_SERVO);
        }
        break;
    }
}

/* ── Buttons (driver_gpio callbacks, system work queue) ──────── */
static void on_button(uint8_t id, bool pressed)
{
    switch (id) {
    case IN_BTN_PUMP:
        if (pressed) app_post(CMD_PUMP_TOGGLE, 0);
        break;
    case IN_BTN_MODE:       /* the 3 s hold is timed by the motion thread */
        atomic_set(&g_mode_press_ms, pressed ? MAX((uint32_t)k_uptime_get(), 1u) : 0);
        break;
    case IN_BTN_LEFT:
        atomic_set(&g_hold_left, pressed);
        break;
    case IN_BTN_RIGHT:
        atomic_set(&g_hold_right, pressed);
        break;
    }
}

/* LED LIFE (out2) for the fast blink of boot_swap_check() at start-up */
static void led_set(bool on)
{
    driver_gpio_set(OUT_LED, on);
}

static void publish_events(void)
{
    for (unsigned int d = 0; d < DEV_COUNT; d++) {
        if (g_events & BIT(d)) mqtt_app_notify(d);
    }
    g_events = 0;
}

/* ── main thread ─────────────────────────────────────────────── */
int main(void)
{
    static const uint16_t heartbeat[] = HEARTBEAT_PATTERN;
    static const struct driver_servo_cfg servo_cfg = {
        .min_pulse_us = SERVO_PULSE_MIN_US, .max_pulse_us = SERVO_PULSE_MAX_US,
        .max_angle = SERVO_MAX_DEG,
    };

    printk("\n== %s v%s ==\n", FW_NAME, FW_VERSION);

    if (config_load(&g_cfg) != 0) {
        printk("[CFG] No saved config — loading defaults\n");
        config_defaults(&g_cfg);
    }
    boot_swap_confirm_image();
    if (driver_gpio_out_init(OUT_LED) != 0)      printk("[LED] LED LIFE (out2) not ready\n");
    boot_swap_check(led_set);                    /* 3 s NRST window, LED blinks fast */
    driver_gpio_blink(OUT_LED, heartbeat, ARRAY_SIZE(heartbeat));

    /* Drivers */
    if (driver_dht11_init(DHT_ID) != 0)          printk("[DHT11] not ready (dht0)\n");
    if (driver_gpio_out_init(OUT_PUMP) != 0)     printk("[PUMP] relay (out0) not ready\n");
    if (driver_gpio_out_init(OUT_ALERT) != 0)    printk("[ALERT] relay (out1) not ready\n");
    for (uint8_t i = IN_BTN_PUMP; i <= IN_BTN_RIGHT; i++) {
        if (driver_gpio_in_init(i, on_button) != 0) printk("[BTN] in%u not ready\n", i);
    }
    if (driver_servo_init(SERVO_ID, &servo_cfg) != 0) printk("[SERVO] servo0 not ready\n");
    else driver_servo_set_angle(SERVO_ID, SERVO_PARK_DEG);
    lcd_view_start();

    /* Ethernet comes up in the background (DHCP or static per config) */
    network_start();

    /* Boot screen; it also covers the DHT11's ~1 s power-up delay */
    k_msleep(SPLASH_MS);
    k_thread_start(motion_tid);
    k_thread_start(lcd_tid);

    int64_t now = k_uptime_get();
    int64_t next_read = now, next_tick = now + TICK_MS, next_report = now;
    pump_start(now);

    while (1) {
        struct cmd c;

        now = k_uptime_get();
        int64_t due = MIN(next_read, next_tick);

        /* Commands until the next tick or reading is due */
        if (k_msgq_get(&g_cmdq, &c, K_MSEC(MAX(due - now, 0))) == 0) {
            k_mutex_lock(&g_lock, K_FOREVER);
            handle_cmd(&c, k_uptime_get());
            k_mutex_unlock(&g_lock);
        }

        now = k_uptime_get();
        if (now >= next_tick) {
            next_tick = now + TICK_MS;
            k_mutex_lock(&g_lock, K_FOREVER);
            pump_tick(now);
            k_mutex_unlock(&g_lock);
        }
        if (now >= next_read) {
            int t = 0, h = 0;
            int rc = driver_dht11_read(DHT_ID, &t, &h);   /* outside the lock */

            next_read = now + DHT_READ_PERIOD_MS;
            network_update_ip();
            k_mutex_lock(&g_lock, K_FOREVER);
            bool was_req = g_st.water_request;
            env_update(rc == 0, t, h, k_uptime_get());
            pump_env_update(was_req, k_uptime_get());
            k_mutex_unlock(&g_lock);

            if (rc == 0) printk("| T=%dC | H=%d%% | IP=%s | %s | PUMP:%s | SERVO:%s %d |\n",
                                t, h, g_device_ip, env_info[g_st.code].str,
                                g_st.pump_on ? "ON" : "OFF",
                                atomic_get(&g_servo_mode) == SERVO_AUTO ? "auto" : "manual",
                                servo_angle());
            else printk("| DHT11 read error %d | %s |\n", rc, env_info[g_st.code].str);

            if (now >= next_report) {
                next_report = now + REPORT_PERIOD_MS;
                mqtt_app_publish_now();
            }
        }
        publish_events();
    }
    return 0;
}

/* SmartLighting: dusk-to-dawn lamp.
 *
 * main.c: start-up, the main loop and the buttons. The main thread reads the
 * BH1750 every second (lamp.c) and applies the commands of the buttons and
 * the web page (g_cmdq), so the lamp state and the settings only change here;
 * settings are written to flash by settings.c. The LCD (lcd_view.c) has its
 * own thread; LED LIFE blinks by itself (driver_gpio_blink). Overview: app.h;
 * behaviour: README.md. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_bh1750.h"
#include "driver_gpio.h"

#include "app_config.h"
#include "app_state.h"
#include "boot_swap.h"
#include "mqtt_app.h"
#include "network.h"
#include "app.h"

K_MUTEX_DEFINE(g_lock);
struct state    g_st;
struct settings g_set;
uint32_t        g_events;

const char *const src_str[] = { "", "auto", "button", "web", "timer" };

K_MSGQ_DEFINE(g_cmdq, sizeof(struct cmd), 8, 4);

bool app_post(const struct cmd *c)
{
    return k_msgq_put(&g_cmdq, c, K_NO_WAIT) == 0;
}

void app_snapshot(struct state *st, struct settings *set)
{
    k_mutex_lock(&g_lock, K_FOREVER);
    *st = g_st;
    *set = g_set;
    k_mutex_unlock(&g_lock);
}

/* A command from a button or the web page (g_lock held) */
static void handle_cmd(const struct cmd *c, int64_t now)
{
    switch (c->type) {
    case CMD_LAMP_TOGGLE:
        lamp_manual(!g_st.on, SRC_BUTTON, now);
        break;
    case CMD_LAMP_SET:
        lamp_manual(c->val != 0, SRC_WEB, now);
        break;
    case CMD_MODE_TOGGLE:
        lamp_set_mode(g_st.mode == MODE_AUTO ? MODE_MANUAL : MODE_AUTO, now);
        break;
    case CMD_MODE_SET:
        lamp_set_mode((enum mode)c->val, now);
        break;
    case CMD_TIMER_STEP:
        g_set.timer_h = (uint8_t)CLAMP((int)g_set.timer_h + c->val, 0, SET_TIMER_MAX);
        settings_save_at(now + SAVE_DELAY_MS);   /* fewer writes on repeated clicks */
        g_events |= BIT(DEV_LAMP);
        printk("[BTN] timer %u h\n", g_set.timer_h);
        break;
    case CMD_TIMER_SET:
        g_set.timer_h = (uint8_t)c->val;
        settings_save_at(now);
        g_events |= BIT(DEV_LAMP);
        break;
    case CMD_SETTINGS:
        g_set.on_lux    = c->set.on_lux;
        g_set.off_lux   = c->set.off_lux;
        g_set.confirm_s = c->set.confirm_s;
        settings_save_at(now);
        g_events |= BIT(DEV_LIGHT);
        break;
    }
}

/* ── Buttons (driver_gpio callback, system work queue) ───────── */
static void on_button(uint8_t id, bool pressed)
{
    struct cmd c = { 0 };

    if (!pressed) return;
    switch (id) {
    case IN_BTN_LAMP:     c.type = CMD_LAMP_TOGGLE; break;
    case IN_BTN_MODE:     c.type = CMD_MODE_TOGGLE; break;
    case IN_BTN_TMR_UP:   c.type = CMD_TIMER_STEP; c.val = +1; break;
    case IN_BTN_TMR_DOWN: c.type = CMD_TIMER_STEP; c.val = -1; break;
    default: return;
    }
    app_post(&c);
}

/* LED LIFE (out1) for the fast blink of boot_swap_check() at start-up */
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

    printk("\n== %s v%s ==\n", FW_NAME, FW_VERSION);

    if (config_load(&g_cfg) != 0) {
        printk("[CFG] No saved config — loading defaults\n");
        config_defaults(&g_cfg);
    }
    settings_load();

    boot_swap_confirm_image();
    if (driver_gpio_out_init(OUT_LED) != 0)          printk("[LED] LED LIFE (out1) not ready\n");
    boot_swap_check(led_set);                        /* 3 s NRST window, LED blinks fast */
    driver_gpio_blink(OUT_LED, heartbeat, ARRAY_SIZE(heartbeat));

    /* Drivers */
    if (driver_bh1750_init() != 0)                   printk("[LIGHT] BH1750 not ready\n");
    if (driver_gpio_out_init(OUT_LAMP) != 0)         printk("[LAMP] relay (out0) not ready\n");
    for (uint8_t i = IN_BTN_LAMP; i <= IN_BTN_TMR_DOWN; i++) {
        if (driver_gpio_in_init(i, on_button) != 0)  printk("[BTN] in%u not ready\n", i);
    }
    lcd_view_start();

    /* Ethernet comes up in the background (DHCP or static per config) */
    network_start();

    k_msleep(SPLASH_MS);
    k_thread_start(lcd_tid);

    int64_t next_read   = k_uptime_get();
    int64_t next_report = next_read;              /* first report right away */
    int64_t next_log    = next_read;

    while (1) {
        int64_t now = k_uptime_get();
        struct cmd c;

        /* Wait for a command until the next reading is due */
        if (k_msgq_get(&g_cmdq, &c, K_MSEC(MAX(next_read - now, 0))) == 0) {
            k_mutex_lock(&g_lock, K_FOREVER);
            handle_cmd(&c, k_uptime_get());
            k_mutex_unlock(&g_lock);
        }

        now = k_uptime_get();
        if (now >= next_read) {
            float lux = 0;
            int rc = driver_bh1750_read_lux(&lux);   /* outside the lock: up to 180 ms */

            next_read = now + READ_PERIOD_MS;
            network_update_ip();
            k_mutex_lock(&g_lock, K_FOREVER);
            lamp_update(rc, lux, k_uptime_get());
            k_mutex_unlock(&g_lock);
            if (rc != 0) printk("[LIGHT] read error %d\n", rc);

            if (now >= next_log) {
                next_log = now + LOG_PERIOD_MS;
                printk("| LUX=%d (ambient %d, lamp %d) | %s | LAMP:%s %s | IP=%s |\n",
                       g_st.lux, g_st.ambient, g_st.lamp_lux,
                       !g_st.night_known ? "--" : g_st.night ? "DARK" : "BRIGHT",
                       g_st.on ? "ON" : "OFF", g_st.mode == MODE_AUTO ? "auto" : "manual",
                       g_device_ip);
            }
            if (now >= next_report) {
                next_report = now + REPORT_PERIOD_MS;
                mqtt_app_publish_now();
            }
        }

        settings_poll(now);
        publish_events();
    }
    return 0;
}

/* lcd_view.c — the 20x4 LCD (lcd thread, redraws every LCD_PERIOD_MS)
 *
 *   Boot (5 s)              Status                  Warning (sensor fault)
 *   |  Showcase v1.0.0   |  |IP:     192.168.1.50|  |  !!! Warning !!!   |
 *   |   Smart Lighting   |  |Light: 8 lx     Dark|  |                    |
 *   |BH1750 + web server |  |Lamp: ON   Mode:AUTO|  |BH1750 sensor error |
 *   |                    |  |MQTT: OK   Off 05:59|  |                    |
 *
 * Row 4 right: "Off hh:mm" left on the timer while the lamp is on, else
 * "Timer 6h" / "Timer off". During a sensor fault the warning and status
 * screens take turns, LCD_WARN_SWAP_MS each. driver_lcd_update() writes only
 * the rows that changed. */
#include <zephyr/sys/printk.h>
#include <stdio.h>

#include "driver_lcd.h"
#include "app_config.h"
#include "app_state.h"
#include "mqtt_app.h"
#include "network.h"
#include "app.h"

static struct driver_lcd s_lcd;

static void lcd_probe(void)
{
    int addr = driver_lcd_init_probe(&s_lcd, LCD_COLS, LCD_ROWS);

    if (addr > 0) printk("[LCD] 20x4 at 0x%02X\n", addr);
}

bool lcd_view_ok(void)
{
    return driver_lcd_is_ready(&s_lcd);
}

void lcd_view_start(void)
{
    char rows[LCD_ROWS][DRIVER_LCD_ROW_SIZE] = { 0 };

    lcd_probe();
    if (!lcd_view_ok()) {
        printk("[LCD] no PCF8574 backpack on I2C1\n");
        return;
    }
    driver_lcd_fmt_center(rows[0], LCD_COLS, "Showcase v" FW_VERSION);
    driver_lcd_fmt_center(rows[1], LCD_COLS, "Smart Lighting");
    driver_lcd_fmt_center(rows[2], LCD_COLS, "BH1750 + web server");
    driver_lcd_update(&s_lcd, rows);
}

static void build_rows(char rows[LCD_ROWS][DRIVER_LCD_ROW_SIZE], int64_t now)
{
    static int64_t warn_start;
    struct state st;
    struct settings set;
    char ip[16], a[24], b[16];

    app_snapshot(&st, &set);

    /* Sensor fault: warning and status screens take turns */
    if (st.fault) {
        if (warn_start == 0) warn_start = now;
        if (((now - warn_start) / LCD_WARN_SWAP_MS) % 2 == 0) {
            driver_lcd_fmt_center(rows[0], LCD_COLS, "!!! Warning !!!");
            rows[1][0] = '\0';
            driver_lcd_fmt_center(rows[2], LCD_COLS, "BH1750 sensor error");
            rows[3][0] = '\0';
            return;
        }
    } else {
        warn_start = 0;
    }

    /* The live IP once Ethernet is up, before that the saved static one */
    if (network_eth_connected()) snprintf(ip, sizeof(ip), "%s", g_device_ip);
    else                         ip4_to_str(g_cfg.static_ip, ip);
    snprintf(rows[0], DRIVER_LCD_ROW_SIZE, "IP:%17s", ip);

    if (st.have_data) snprintf(a, sizeof(a), "Light: %d lx", st.lux);
    else              snprintf(a, sizeof(a), "Light: -- lx");
    driver_lcd_fmt_left_right(rows[1], LCD_COLS, a,
                              !st.night_known ? "--" : st.night ? "Dark" : "Bright");

    snprintf(rows[2], DRIVER_LCD_ROW_SIZE, "Lamp: %-3s  Mode:%s", st.on ? "ON" : "OFF",
             st.mode == MODE_AUTO ? "AUTO" : "MAN");

    int64_t left = set.timer_h * HOUR_MS - (now - st.on_since);
    if (st.on && set.timer_h && left > 0) {
        uint32_t m = (uint32_t)((left + 59999) / 60000);
        snprintf(b, sizeof(b), "Off %02u:%02u", m / 60, m % 60);
    } else if (set.timer_h) {
        snprintf(b, sizeof(b), "Timer %uh", set.timer_h);
    } else {
        snprintf(b, sizeof(b), "Timer off");
    }
    driver_lcd_fmt_left_right(rows[3], LCD_COLS, network_eth_connected() && mqtt_app_is_connected()
                              ? "MQTT: OK" : "MQTT: N/A", b);
}

static void lcd_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    int64_t next_retry = 0;

    while (1) {
        int64_t now = k_uptime_get();

        /* Missing or unplugged LCD: probe again every LCD_RETRY_MS */
        if (!lcd_view_ok() && now >= next_retry) {
            next_retry = now + LCD_RETRY_MS;
            lcd_probe();
        }
        if (lcd_view_ok()) {
            char rows[LCD_ROWS][DRIVER_LCD_ROW_SIZE];

            build_rows(rows, now);
            driver_lcd_update(&s_lcd, rows);      /* writes the rows that changed */
        }
        k_msleep(LCD_PERIOD_MS);
    }
}

/* Started by main after the boot screen */
K_THREAD_DEFINE(lcd_tid, LCD_STACK, lcd_thread, NULL, NULL, NULL, LCD_PRIO, 0, SYS_FOREVER_MS);

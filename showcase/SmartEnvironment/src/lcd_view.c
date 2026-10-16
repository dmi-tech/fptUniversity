/* lcd_view.c — the 20x4 LCD (lcd thread, redraws every LCD_PERIOD_MS)
 *
 *   Boot (5 s)              Status                  Warning
 *   |  Showcase v1.0.0   |  |IP:     192.168.1.50|  |  !!! Warning !!!   |
 *   | Smart Environment  |  |T: 34*C       H: 35%|  |                    |
 *   |  DHT + web server  |  |Motor: ON  Mode:AUTO|  |  Hot and dry air   |
 *   |                    |  |MQTT: OK            |  |                    |
 *
 * Motor = pump, Mode = servo mode. While the environment is not OK the warning
 * and status screens take turns, LCD_WARN_SWAP_MS each. driver_lcd_update()
 * writes only the rows that changed. */
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
    driver_lcd_fmt_center(rows[1], LCD_COLS, "Smart Environment");
    driver_lcd_fmt_center(rows[2], LCD_COLS, "DHT + web server");
    driver_lcd_update(&s_lcd, rows);
}

static void build_rows(char rows[LCD_ROWS][DRIVER_LCD_ROW_SIZE], int64_t now)
{
    static int64_t warn_start;
    struct state st;
    char ip[16], t[12] = "T: --", h[12] = "H: --";

    app_snapshot(&st);

    if (env_level(st.code) != LVL_OK) {
        if (warn_start == 0) warn_start = now;
        if (((now - warn_start) / LCD_WARN_SWAP_MS) % 2 == 0) {
            driver_lcd_fmt_center(rows[0], LCD_COLS, "!!! Warning !!!");
            rows[1][0] = '\0';
            driver_lcd_fmt_center(rows[2], LCD_COLS, env_info[st.code].lcd);
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

    if (st.have_data) {
        snprintf(t, sizeof(t), "T: %d%cC", st.temp_c, 0xDF);
        snprintf(h, sizeof(h), "H: %d%%", st.humi_pct);
    }
    driver_lcd_fmt_left_right(rows[1], LCD_COLS, t, h);
    snprintf(rows[2], DRIVER_LCD_ROW_SIZE, "Motor: %-3s Mode:%s", st.pump_on ? "ON" : "OFF",
             atomic_get(&g_servo_mode) == SERVO_AUTO ? "AUTO" : "MAN");
    snprintf(rows[3], DRIVER_LCD_ROW_SIZE, "MQTT: %s",
             network_eth_connected() && mqtt_app_is_connected() ? "OK" : "N/A");
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

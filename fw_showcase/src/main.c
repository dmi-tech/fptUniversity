/* fw_showcase: DHT11 (PC2) -> 20x4 LCD (PCF8574, I2C1), live web page, MQTT
 * status every 30 s, abnormal value -> RS485 alarm + relay 1 blink + motor.
 * Motor (PA2): web switch, RS485 ON/OFF, PA10 button or the alarm.
 * Config stored in flash — load at boot, edit via browser */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <errno.h>

#include "alarm.h"
#include "alarm_notify.h"
#include "app_config.h"
#include "app_state.h"
#include "boot_swap.h"
#include "dht11.h"
#include "lcd_view.h"
#include "motor.h"
#include "mqtt_app.h"
#include "network.h"
#include "rs485.h"
#include "status_led.h"
#include "version.h"

#define DHT_READ_PERIOD_MS      2000    /* DHT11 needs >= 1 s between reads */
#define REPORT_PERIOD_MS       30000    /* MQTT status period (web: every read) */
#define ALARM_REPEAT_PERIOD_MS 10000    /* RS485 ALARM repeat while active */
#define SPLASH_MS               5000    /* boot screen, then the status screen */

/* Load config from flash; fall back to defaults */
static void load_config(void)
{
    if (config_load(&g_cfg) != 0) {
        printk("[CFG] No saved config — loading defaults\n");
        config_defaults(&g_cfg);
    } else {
        printk("[CFG] Config loaded from flash\n");
    }
    printk("[CFG] IP mode=%s\n", g_cfg.ip_mode ? "Static" : "DHCP");
}

int main(void)
{
    printk("\n== fw_showcase v%s ==\n", FW_VERSION);

    load_config();

    boot_swap_confirm_image();
    if (!status_led_init()) return -1;
    boot_swap_check();

    rs485_init();
    bool dht_ok = dht11_init();
    motor_init();
    alarm_init();
    lcd_view_init();

    /* Ethernet comes up in the background (DHCP or static per config) */
    network_start();

    /* Boot screen for SPLASH_MS; it also covers the DHT11's ~1 s power-up
     * delay */
    status_led_heartbeat_start();
    int64_t splash_end = k_uptime_get() + SPLASH_MS;
    while (k_uptime_get() < splash_end) {
        status_led_heartbeat();
        k_msleep(20);
    }

    lcd_view_clear();
    lcd_view_show(false, 0, 0);

    int t = 0, h = 0;
    bool have_data = false;
    int64_t next_read   = k_uptime_get();
    int64_t next_report = next_read;              /* first report right away */
    int64_t next_repeat = 0;

    while (1) {
        int64_t now = k_uptime_get();

        if (now >= next_read) {
            next_read = now + DHT_READ_PERIOD_MS;

            int nt, nh;
            int rc = dht_ok ? dht11_read(&nt, &nh) : -ENODEV;
            bool ok = (rc == 0);
            if (ok) { t = nt; h = nh; have_data = true; }

            network_update_ip();
            enum alarm_event ev = alarm_update(ok, t, h);

            if (ok) printk("| T=%dC | H=%d%% | IP=%s | %s | MOTOR:%s   |\n", t, h, g_device_ip,
                           alarm_active() ? alarm_reason_str(alarm_reason()) : "OK",
                           motor_is_on() ? "ON" : "OFF");
            else    printk("| DHT11 read error %d | %s |\n", rc,
                           alarm_active() ? alarm_reason_str(alarm_reason()) : "OK");

            /* RS485: at once on start/clear, then every 10 s while active */
            if (ev == ALARM_EVT_START) {
                alarm_notify_rs485(ok, t, h, false);
                next_repeat = now + ALARM_REPEAT_PERIOD_MS;
            } else if (ev == ALARM_EVT_CLEAR) {
                alarm_notify_rs485(ok, t, h, true);
            } else if (alarm_active() && now >= next_repeat) {
                alarm_notify_rs485(ok, t, h, false);
                next_repeat = now + ALARM_REPEAT_PERIOD_MS;
            }

            /* Web: every read. MQTT: every 30 s and at once on an alarm
             * change (motor changes are pushed by motor.c). */
            app_state_update(t, h, ok, have_data);
            if (ev != ALARM_EVT_NONE || now >= next_report) {
                next_report = now + REPORT_PERIOD_MS;
                mqtt_app_publish_now();
            }

            lcd_view_retry(now);
            lcd_view_show(have_data, dht11_last_temp_x10(), dht11_last_humi_x10());
        } else {
            lcd_view_refresh();
        }

        status_led_heartbeat();
        k_msleep(20);
    }
    return 0;
}

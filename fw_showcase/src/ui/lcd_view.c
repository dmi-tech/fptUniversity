/* lcd_view.c — what the 20x4 LCD shows */
#include "lcd_view.h"
#include "lcd_pcf8574.h"
#include "alarm.h"
#include "app_state.h"
#include "mqtt_app.h"
#include "version.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LCD_RETRY_PERIOD_MS 10000

static int64_t s_next_retry;

/* lcd_write_line() pads/truncates to the row width */
static void lcd_line(uint8_t row, const char *fmt, ...)
{
    char ln[LCD_COLS + 1];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(ln, sizeof(ln), fmt, ap);
    va_end(ap);
    lcd_write_line(row, ln);
}

/* Tenths -> "34.5" / "-5.3" */
static void fmt_x10(char *out, size_t len, int v)
{
    snprintf(out, len, "%s%d.%d", v < 0 ? "-" : "", abs(v) / 10, abs(v) % 10);
}

void lcd_view_init(void)
{
    if (lcd_init() == 0) {
        lcd_line(0, "fw_showcase " FW_VERSION);
        lcd_line(1, "DHT11 + ALARM");
        lcd_line(2, "NET WAIT");
    }
}

void lcd_view_clear(void)
{
    lcd_clear();
}

void lcd_view_show(bool have_data, int temp_x10, int humi_x10)
{
    char ts[16] = "--.-", hs[16] = "--.-", ln[48];

    if (strlen(g_device_ip) <= LCD_COLS - 3) lcd_line(0, "IP:%s", g_device_ip);
    else                                     lcd_line(0, "%s", g_device_ip);

    if (have_data) {
        fmt_x10(ts, sizeof(ts), temp_x10);
        fmt_x10(hs, sizeof(hs), humi_x10);
    }
    snprintf(ln, sizeof(ln), "T: %s%cC , H: %s%%", ts, 0xDF, hs);
    if (strlen(ln) > LCD_COLS)                    /* e.g. -10.5 / 100.0 */
        snprintf(ln, sizeof(ln), "T:%s%cC H:%s%%", ts, 0xDF, hs);
    lcd_line(1, "%s", ln);

    lcd_line(2, "MQTT:%s   Motor:%s", mqtt_app_is_connected() ? "OK" : "N/A",
             motor_is_on() ? "ON" : "OFF");

    lcd_line(3, "%s", alarm_reason() != ALARM_REASON_NONE
                      ? alarm_reason_text(alarm_reason()) : "");
}

void lcd_view_retry(int64_t now)
{
    if (!lcd_is_ready() && now >= s_next_retry) {
        s_next_retry = now + LCD_RETRY_PERIOD_MS;
        lcd_init();
    }
}

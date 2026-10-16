/* web_app.c — this showcase's part of the web page (hooks of web_core.h):
 * Device Information cells, the sections and script (web_page.h), and the
 * /api/... routes. The routes only post commands to the main thread; the
 * page shows the result at its next poll (d.app = MQTT status JSON). */
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "web_core.h"
#include "web_page.h"
#include "app.h"

void web_app_info_cells(void)
{
    struct state st;
    struct settings set;

    app_snapshot(&st, &set);
    web_td_ro("Light", "il", !st.night_known ? "--" : st.night ? "Dark" : "Bright");
    web_td_ro("Sensor", "is", st.fault ? "FAULT" : st.ok ? "OK" : "ERR");
    web_td_ro("LCD 20x4", "id", lcd_view_ok() ? "OK" : "ERR");
}

void web_app_sections(void)
{
    web_out_str(WEB_SECTIONS);
}

const char *web_app_js(void)
{
    return WEB_JS;
}

/* Unsigned integer form field; false when missing or not a number */
static bool form_uint(const char *body, const char *key, unsigned long *out)
{
    char v[12], *end;

    if (!http_form_field(body, key, v, sizeof(v)) || !v[0] || v[0] == '-') return false;
    *out = strtoul(v, &end, 10);
    return *end == '\0';
}

static void reply_post(int fd, const struct cmd *c, const char *ok_msg)
{
    bool ok = app_post(c);

    web_api_reply(fd, ok, ok ? ok_msg : "busy, try again");
}

bool web_app_api_post(int fd, const char *path, const char *body)
{
    struct cmd c = { 0 };
    char v[8];
    unsigned long on, off, cf, h;

    if (strcmp(path, "/api/lamp") == 0) {
        if (!http_form_field(body, "on", v, sizeof(v)) || (strcmp(v, "0") && strcmp(v, "1"))) {
            web_api_reply(fd, false, "on must be 0 or 1");
            return true;
        }
        c.type = CMD_LAMP_SET;
        c.val = v[0] == '1';
        reply_post(fd, &c, c.val ? "lamp ON (manual)" : "lamp OFF (manual)");
    } else if (strcmp(path, "/api/mode") == 0) {
        if (!http_form_field(body, "mode", v, sizeof(v)) ||
            (strcmp(v, "auto") && strcmp(v, "manual"))) {
            web_api_reply(fd, false, "mode must be auto or manual");
            return true;
        }
        c.type = CMD_MODE_SET;
        c.val = v[0] == 'm' ? MODE_MANUAL : MODE_AUTO;
        reply_post(fd, &c, c.val == MODE_MANUAL ? "mode MANUAL" : "mode AUTO");
    } else if (strcmp(path, "/api/timer") == 0) {
        if (!form_uint(body, "hours", &h) || h > SET_TIMER_MAX) {
            web_api_reply(fd, false, "timer must be 0..12 h");
            return true;
        }
        c.type = CMD_TIMER_SET;
        c.val = (int16_t)h;
        reply_post(fd, &c, h ? "timer set" : "timer off");
    } else if (strcmp(path, "/api/settings") == 0) {
        struct state st;
        const char *why;

        app_snapshot(&st, &c.set);              /* keeps the current timer */
        if (!form_uint(body, "on_lux", &on) || !form_uint(body, "off_lux", &off) ||
            !form_uint(body, "confirm_s", &cf) || on > 0xFFFF || off > 0xFFFF || cf > 0xFFFF) {
            web_api_reply(fd, false, "fill in every field");
            return true;
        }
        c.set.on_lux = (uint16_t)on;
        c.set.off_lux = (uint16_t)off;
        c.set.confirm_s = (uint16_t)cf;
        why = settings_check(&c.set);
        if (why) {
            web_api_reply(fd, false, why);
            return true;
        }
        c.type = CMD_SETTINGS;
        reply_post(fd, &c, "saved");
    } else {
        return false;
    }
    return true;
}

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

    app_snapshot(&st);
    web_td_ro("Environment", "ie", env_info[st.code].str);
    web_td_ro("Sensor", "is", st.code == ENV_SENSOR ? "FAULT" : st.sensor_ok ? "OK" : "ERR");
    web_td_ro("LCD 20x4", "il", lcd_view_ok() ? "OK" : "ERR");
}

void web_app_sections(void)
{
    web_out_str(WEB_SECTIONS);
}

const char *web_app_js(void)
{
    return WEB_JS;
}

static void reply_post(int fd, uint8_t type, int16_t val, const char *ok_msg)
{
    bool ok = app_post(type, val);

    web_api_reply(fd, ok, ok ? ok_msg : "busy, try again");
}

bool web_app_api_post(int fd, const char *path, const char *body)
{
    char v[8], *end = v;

    if (strcmp(path, "/api/pump") == 0) {
        if (!http_form_field(body, "on", v, sizeof(v)) || (strcmp(v, "0") && strcmp(v, "1"))) {
            web_api_reply(fd, false, "on must be 0 or 1");
        } else {
            reply_post(fd, CMD_PUMP_SET, v[0] == '1', v[0] == '1' ? "pump ON" : "pump OFF");
        }
    } else if (strcmp(path, "/api/pump/rearm") == 0) {
        reply_post(fd, CMD_PUMP_REARM, 0, "auto re-enabled");
    } else if (strcmp(path, "/api/servo") == 0) {
        if (!http_form_field(body, "mode", v, sizeof(v)) ||
            (strcmp(v, "auto") && strcmp(v, "manual"))) {
            web_api_reply(fd, false, "mode must be auto or manual");
        } else {
            reply_post(fd, CMD_SERVO_SET_MODE, v[0] == 'm' ? SERVO_MANUAL : SERVO_AUTO,
                       v[0] == 'm' ? "servo MANUAL" : "servo AUTO");
        }
    } else if (strcmp(path, "/api/servo/angle") == 0) {
        long deg = -1;

        if (http_form_field(body, "deg", v, sizeof(v)) && v[0]) deg = strtol(v, &end, 10);
        if (deg < SERVO_MIN_DEG || deg > SERVO_MAX_DEG || *end != '\0') {
            web_api_reply(fd, false, "deg must be 0..180");
        } else if (atomic_get(&g_servo_mode) != SERVO_MANUAL) {
            web_api_reply(fd, false, "switch the servo to MANUAL first");
        } else {
            reply_post(fd, CMD_SERVO_ANGLE, (int16_t)deg, "angle set");
        }
    } else {
        return false;
    }
    return true;
}

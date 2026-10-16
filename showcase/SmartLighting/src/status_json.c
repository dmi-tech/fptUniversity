/* status_json.c — JSON of the devices: MQTT status (every 30 s), one event
 * topic per device on a change, and "app" of the web page (/api/live, /status):
 * {"id":..,"fw":..,"name":..,"ip":..,"lcd":{..},"lamp":{..},"light":{..},
 *  "mode":{..},"sensor":{..}} */
#include <stdio.h>

#include "app_state.h"
#include "mqtt_app.h"
#include "app.h"

static const char *const dev_name[DEV_COUNT] = { "lamp", "light", "mode", "sensor" };

static int json_device(enum dev d, const struct state *st, const struct settings *set,
                       char *buf, size_t len)
{
    int64_t now = k_uptime_get();
    int64_t left = set->timer_h * HOUR_MS - (now - st->on_since);
    int64_t pend = st->cand_since ? (int64_t)set->confirm_s * 1000 - (now - st->cand_since) : 0;

    switch (d) {
    case DEV_LAMP:
        return snprintf(buf, len,
            "{\"on\":%d,\"by\":\"%s\",\"timer_h\":%u,\"timer_left_s\":%u,\"hold\":%d}",
            st->on, src_str[st->by], set->timer_h,
            st->on && set->timer_h && left > 0 ? (unsigned int)((left + 999) / 1000) : 0,
            st->hold);
    case DEV_LIGHT:
        return snprintf(buf, len,
            "{\"state\":\"%s\",\"lux\":%d,\"ambient_lux\":%d,\"lamp_lux\":%d,"
            "\"on_lux\":%u,\"off_lux\":%u,\"confirm_s\":%u,\"pending_s\":%u}",
            !st->night_known ? "unknown" : st->night ? "dark" : "bright",
            st->lux, st->ambient, st->lamp_lux, set->on_lux, set->off_lux,
            set->confirm_s, pend > 0 ? (unsigned int)((pend + 999) / 1000) : 0);
    case DEV_MODE:
        return snprintf(buf, len, "{\"mode\":\"%s\"}",
                        st->mode == MODE_AUTO ? "auto" : "manual");
    case DEV_SENSOR:
        return snprintf(buf, len, "{\"ok\":%d,\"fault\":%d,\"have\":%d,\"reads\":%u}",
                        st->ok, st->fault, st->have_data, st->reads);
    default:
        return snprintf(buf, len, "{}");
    }
}

/* ── MQTT hooks (mqtt_app.h) ─────────────────────────────────── */
int mqtt_app_build_status(char *buf, size_t len)
{
    struct state st;
    struct settings set;
    size_t n;

    app_snapshot(&st, &set);
    if (st.reads == 0) return 0;
    n = snprintf(buf, len, "{\"id\":\"Board %03d\",\"fw\":\"" FW_VERSION "\","
                 "\"name\":\"" FW_NAME "\",\"ip\":\"%s\",\"lcd\":{\"ok\":%d}",
                 mqtt_app_id_num(), g_device_ip, lcd_view_ok());
    for (int d = 0; d < DEV_COUNT && n < len; d++) {
        n += snprintf(buf + n, len - n, ",\"%s\":", dev_name[d]);
        if (n < len) n += json_device(d, &st, &set, buf + n, len - n);
    }
    if (n < len) n += snprintf(buf + n, len - n, "}");
    return (int)n;
}

int mqtt_app_build_event(unsigned int dev, char *name, size_t name_len,
                         char *buf, size_t len)
{
    struct state st;
    struct settings set;

    if (dev >= DEV_COUNT) return 0;
    app_snapshot(&st, &set);
    snprintf(name, name_len, "%s", dev_name[dev]);
    return json_device(dev, &st, &set, buf, len);
}

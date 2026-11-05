/* status_json.c — JSON of the devices.
 *
 * MQTT (status every 30 s, one event topic per device on a change): the main
 * values only,
 *   {"id":..,"fw":..,"lamp":{"on":..},"light":{"lux":..},"mode":{"mode":..},
 *    "sensor":{"ok":..}}
 * Web page ("app" of /api/live): every value the page shows,
 *   {"id":..,"fw":..,"name":..,"ip":..,"lcd":{..},"lamp":{..},"light":{..},
 *    "mode":{..},"sensor":{..}} */
#include <stdio.h>

#include "app_state.h"
#include "mqtt_app.h"
#include "web_core.h"
#include "app.h"

static const char *const dev_name[DEV_COUNT] = { "lamp", "light", "mode", "sensor" };

/* MQTT: the main value of each device */
static int json_brief(enum dev d, const struct state *st, char *buf, size_t len)
{
    switch (d) {
    case DEV_LAMP:
        return snprintf(buf, len, "{\"on\":%d}", st->on);
    case DEV_LIGHT:
        return snprintf(buf, len, "{\"lux\":%d}", st->lux);
    case DEV_MODE:
        return snprintf(buf, len, "{\"mode\":\"%s\"}",
                        st->mode == MODE_AUTO ? "auto" : "manual");
    case DEV_SENSOR:
        return snprintf(buf, len, "{\"ok\":%d}", st->have_data && !st->fault);
    default:
        return snprintf(buf, len, "{}");
    }
}

/* Web page: everything it shows */
static int json_full(enum dev d, const struct state *st, const struct settings *set,
                     char *buf, size_t len)
{
    int64_t now = k_uptime_get();
    int64_t left = (int64_t)set->timer_s * 1000 - (now - st->on_since);
    int min;
    int64_t pend = st->cand_since ? (int64_t)set->confirm_s * 1000 - (now - st->cand_since) : 0;

    switch (d) {
    case DEV_LAMP:
        return snprintf(buf, len,
            "{\"on\":%d,\"by\":\"%s\",\"timer_s\":%u,\"timer_left_s\":%u,\"hold\":%d}",
            st->on, src_str[st->by], set->timer_s,
            st->on && set->timer_s && left > 0 ? (unsigned int)((left + 999) / 1000) : 0,
            st->hold);
    case DEV_LIGHT:
        return snprintf(buf, len,
            "{\"state\":\"%s\",\"lux\":%d,"
            "\"on_lux\":%u,\"off_lux\":%u,\"confirm_s\":%u,\"pending_s\":%u}",
            !st->night_known ? "unknown" : st->night ? "dark" : "bright",
            st->lux, set->on_lux, set->off_lux,
            set->confirm_s, pend > 0 ? (unsigned int)((pend + 999) / 1000) : 0);
    case DEV_MODE:
        /* time: local minute of the day, -1 = not synced; quiet_*: minutes */
        return snprintf(buf, len,
            "{\"mode\":\"%s\",\"time\":%d,\"quiet_on\":%u,\"quiet_from\":%u,"
            "\"quiet_to\":%u,\"quiet\":%d}",
            st->mode == MODE_AUTO ? "auto" : "manual", clock_minute(&min) ? min : -1,
            set->quiet_on, set->quiet_from, set->quiet_to, st->quiet);
    case DEV_SENSOR:
        return snprintf(buf, len,
            "{\"ok\":%d,\"fault\":%d,\"have\":%d,\"fault_action\":\"%s\"}",
            st->ok, st->fault, st->have_data, fault_str[set->fault]);
    default:
        return snprintf(buf, len, "{}");
    }
}

/* Status of all the devices; full = the web page's version */
static int build_status(char *buf, size_t len, bool full)
{
    struct state st;
    struct settings set;
    size_t n;

    app_snapshot(&st, &set);
    if (st.reads == 0) return 0;
    n = snprintf(buf, len, "{\"id\":\"Board %03d\",\"fw\":\"" FW_VERSION "\"", mqtt_app_id_num());
    if (full && n < len) {
        n += snprintf(buf + n, len - n, ",\"name\":\"" FW_NAME "\",\"ip\":\"%s\",\"lcd\":{\"ok\":%d}",
                      g_device_ip, lcd_view_ok());
    }
    for (int d = 0; d < DEV_COUNT && n < len; d++) {
        n += snprintf(buf + n, len - n, ",\"%s\":", dev_name[d]);
        if (n < len) {
            n += full ? json_full(d, &st, &set, buf + n, len - n)
                      : json_brief(d, &st, buf + n, len - n);
        }
    }
    if (n < len) n += snprintf(buf + n, len - n, "}");
    return (int)n;
}

/* ── MQTT hooks (mqtt_app.h) ─────────────────────────────────── */
int mqtt_app_build_status(char *buf, size_t len)
{
    return build_status(buf, len, false);
}

int mqtt_app_build_event(unsigned int dev, char *name, size_t name_len,
                         char *buf, size_t len)
{
    struct state st;
    struct settings set;

    if (dev >= DEV_COUNT) return 0;
    app_snapshot(&st, &set);
    snprintf(name, name_len, "%s", dev_name[dev]);
    return json_brief(dev, &st, buf, len);
}

/* ── Web hook (web_core.h) ───────────────────────────────────── */
int web_app_build_status(char *buf, size_t len)
{
    return build_status(buf, len, true);
}

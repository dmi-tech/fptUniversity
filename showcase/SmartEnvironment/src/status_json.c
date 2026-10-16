/* status_json.c — JSON of the devices: MQTT status (every 30 s), one event
 * topic per device on a change, and "app" of the web page (/api/live, /status):
 * {"id":..,"fw":..,"ip":..,"lcd":{..},"pump":{..},"servo":{..},"env":{..},
 *  "sensor":{..},"relay":{..}} */
#include <stdio.h>

#include "app_state.h"
#include "mqtt_app.h"
#include "app.h"

static const char *const dev_name[DEV_COUNT] = { "pump", "servo", "env", "sensor", "relay" };

static uint32_t left_s(const struct state *st)
{
    int64_t left = st->phase_end - k_uptime_get();
    bool timed = st->session || (st->pump_on && st->pump_by != SRC_AUTO);

    return timed && left > 0 ? (uint32_t)((left + 999) / 1000) : 0;
}

static const char *alert_str(void)
{
    atomic_val_t h = atomic_get(&g_alert_half_ms);
    return h == 0 ? "off" : h == ALERT_FAST_HALF_MS ? "blink_fast" : "blink_slow";
}

static int json_device(enum dev d, const struct state *st, char *buf, size_t len)
{
    switch (d) {
    case DEV_PUMP:
        return snprintf(buf, len,
            "{\"on\":%d,\"by\":\"%s\",\"auto\":\"%s\",\"session\":%d,"
            "\"resting\":%d,\"left_s\":%u,\"cycles\":%u}",
            st->pump_on, src_str[st->pump_by], st->inhibited ? "inhibited" : "armed",
            st->session, st->resting, left_s(st), st->cycles);
    case DEV_SERVO:
        return snprintf(buf, len, "{\"mode\":\"%s\",\"angle\":%d,\"sweeping\":%d}",
                        atomic_get(&g_servo_mode) == SERVO_AUTO ? "auto" : "manual",
                        servo_angle(), (int)atomic_get(&g_sweeping));
    case DEV_ENV:
        return snprintf(buf, len,
            "{\"code\":\"%s\",\"level\":%d,\"level_str\":\"%s\",\"title\":\"%s\","
            "\"advice\":\"%s\",\"water_request\":%d}",
            env_info[st->code].str, env_level(st->code), level_str[env_level(st->code)],
            env_info[st->code].title, env_info[st->code].advice, st->water_request);
    case DEV_SENSOR:
        if (!st->have_data) {
            return snprintf(buf, len, "{\"ok\":%d,\"fault\":%d,\"reads\":%u}",
                            st->sensor_ok, st->code == ENV_SENSOR, st->reads);
        }
        return snprintf(buf, len,
            "{\"ok\":%d,\"fault\":%d,\"temp\":%d,\"humi\":%d,\"heat_index\":%d,"
            "\"hi_valid\":%d,\"reads\":%u}",
            st->sensor_ok, st->code == ENV_SENSOR, st->temp_c, st->humi_pct,
            st->heat_index_c, st->hi_valid, st->reads);
    case DEV_RELAY:
        return snprintf(buf, len, "{\"blink\":%d,\"mode\":\"%s\",\"half_period_ms\":%u}",
                        atomic_get(&g_alert_half_ms) != 0, alert_str(),
                        (unsigned int)atomic_get(&g_alert_half_ms));
    default:
        return snprintf(buf, len, "{}");
    }
}

/* ── MQTT hooks (mqtt_app.h) ─────────────────────────────────── */
int mqtt_app_build_status(char *buf, size_t len)
{
    struct state st;
    size_t n;

    app_snapshot(&st);
    if (st.reads == 0) return 0;
    n = snprintf(buf, len, "{\"id\":\"Board %03d\",\"fw\":\"" FW_VERSION "\",\"ip\":\"%s\","
                 "\"lcd\":{\"ok\":%d}", mqtt_app_id_num(), g_device_ip, lcd_view_ok());
    for (int d = 0; d < DEV_COUNT && n < len; d++) {
        n += snprintf(buf + n, len - n, ",\"%s\":", dev_name[d]);
        if (n < len) n += json_device(d, &st, buf + n, len - n);
    }
    if (n < len) n += snprintf(buf + n, len - n, "}");
    return (int)n;
}

int mqtt_app_build_event(unsigned int dev, char *name, size_t name_len,
                         char *buf, size_t len)
{
    struct state st;

    if (dev >= DEV_COUNT) return 0;
    app_snapshot(&st);
    snprintf(name, name_len, "%s", dev_name[dev]);
    return json_device(dev, &st, buf, len);
}

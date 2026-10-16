/* env.c — environment analysis: every DHT11 reading is placed in a 3x3
 * matrix (temperature COLD / NORMAL / HOT x humidity DRY / NORMAL / HUMID)
 * with hysteresis, plus a sensor fault. A new situation must be read
 * ENV_CONFIRM_READS times in a row. Each situation has a level, a Vietnamese
 * title / advice (web, MQTT), an English LCD reason and says whether watering
 * helps; relay 1 blinks with the level. */
#include <zephyr/sys/printk.h>
#include <math.h>

#include "driver_gpio.h"
#include "app.h"

const char *const level_str[] = { "ok", "warning", "critical" };

const struct env_info env_info[ENV_CODE_COUNT] = {
    [ENV_WAIT] = { "WAIT", LVL_OK, false, "Đang chờ dữ liệu",
        "Cảm biến đang khởi động.", "Reading sensor" },
    [ENV_OK] = { "OK", LVL_OK, false, "Môi trường tốt",
        "Nhiệt độ và độ ẩm trong ngưỡng phù hợp, không cần can thiệp.", "Environment OK" },
    [ENV_HOT_DRY] = { "HOT_DRY", LVL_CRIT, true, "Nóng và khô",
        "Cây mất nước nhanh, có nguy cơ héo. Phun sương làm mát, che nắng, tưới bổ sung.",
        "Hot and dry air" },
    [ENV_HOT] = { "HOT", LVL_WARN, false, "Nhiệt độ cao",
        "Che nắng, tăng thông gió. Hệ thống sẽ phun sương nếu nhiệt độ vượt 38 °C.",
        "High temperature" },
    [ENV_HOT_EXTREME] = { "HOT_EXTREME", LVL_CRIT, true, "Nhiệt độ rất cao",
        "Nguy cơ sốc nhiệt cho cây. Phun sương làm mát, che nắng, tăng thông gió ngay.",
        "Extreme heat" },
    [ENV_HOT_HUMID] = { "HOT_HUMID", LVL_CRIT, false, "Nóng và ẩm",
        "Chỉ số nóng cao, dễ phát sinh nấm bệnh. KHÔNG phun nước; mở thông gió hoặc bật quạt.",
        "Hot and humid air" },
    [ENV_DRY] = { "DRY", LVL_WARN, true, "Không khí khô hanh",
        "Độ ẩm thấp. Tưới hoặc phun sương để tăng độ ẩm.", "Dry air" },
    [ENV_HUMID] = { "HUMID", LVL_WARN, false, "Độ ẩm cao",
        "Nguy cơ nấm mốc. Thông gió, giảm tưới.", "High humidity" },
    [ENV_COLD_DRY] = { "COLD_DRY", LVL_WARN, false, "Lạnh và khô",
        "Che chắn gió, giữ ấm cho cây. Không phun nước (làm lạnh thêm).", "Cold and dry air" },
    [ENV_COLD] = { "COLD", LVL_WARN, false, "Nhiệt độ thấp",
        "Giữ ấm cho cây, hạn chế tưới.", "Low temperature" },
    [ENV_COLD_HUMID] = { "COLD_HUMID", LVL_CRIT, false, "Lạnh và ẩm",
        "Nguy cơ sương muối, thối rễ. Giữ ấm, thông gió, ngừng tưới.", "Cold and humid air" },
    [ENV_SENSOR] = { "SENSOR", LVL_CRIT, false, "Lỗi cảm biến",
        "DHT11 không trả lời. Kiểm tra dây và nguồn cảm biến. Tưới tự động tạm dừng.",
        "DHT11 sensor error" },
};

static const uint16_t ALERT_SLOW_MS[] = { ALERT_SLOW_HALF_MS, ALERT_SLOW_HALF_MS };
static const uint16_t ALERT_FAST_MS[] = { ALERT_FAST_HALF_MS, ALERT_FAST_HALF_MS };

/* Analysis state (main thread) */
static int       s_tb, s_hb;          /* temperature / humidity band: 0 normal, 1 low, 2 high */
static bool      s_extreme;
static int       s_fails;
static enum code s_cand;
static int       s_cand_reads;
static int64_t   s_water_since;

enum level env_level(enum code c) { return env_info[c].level; }

static int band(int cur, int v, int low, int high, int hyst)
{
    if (v > high) return 2;
    if (v < low)  return 1;
    if (cur == 2 && v > high - hyst) return 2;
    if (cur == 1 && v < low + hyst)  return 1;
    return 0;
}

static enum code classify(int t, int h)
{
    static const enum code m[3][3] = {
        /*           H normal  H dry         H humid */
        /* T normal */ { ENV_OK,   ENV_DRY,      ENV_HUMID },
        /* T cold   */ { ENV_COLD, ENV_COLD_DRY, ENV_COLD_HUMID },
        /* T hot    */ { ENV_HOT,  ENV_HOT_DRY,  ENV_HOT_HUMID },
    };

    s_tb = band(s_tb, t, ENV_T_COLD_C, ENV_T_HOT_C, ENV_T_HYST_C);
    s_hb = band(s_hb, h, ENV_H_DRY_PCT, ENV_H_HUMID_PCT, ENV_H_HYST_PCT);
    s_extreme = s_extreme ? (t > ENV_T_EXTREME_C - 2 * ENV_T_HYST_C) : (t >= ENV_T_EXTREME_C);
    enum code c = m[s_tb][s_hb];
    return (c == ENV_HOT && s_extreme) ? ENV_HOT_EXTREME : c;
}

/* NOAA heat index (Rothfusz regression), °C in and out */
static int heat_index_c(int t_c, int rh)
{
    float t = t_c * 9.0f / 5.0f + 32.0f;
    float hi = 0.5f * (t + 61.0f + (t - 68.0f) * 1.2f + rh * 0.094f);

    if ((hi + t) / 2.0f >= 80.0f) {
        hi = -42.379f + 2.04901523f * t + 10.14333127f * rh
             - 0.22475541f * t * rh - 0.00683783f * t * t
             - 0.05481717f * rh * rh + 0.00122874f * t * t * rh
             + 0.00085282f * t * rh * rh - 0.00000199f * t * t * rh * rh;
    }
    return (int)lroundf((hi - 32.0f) * 5.0f / 9.0f);
}

void env_update(bool ok, int t, int h, int64_t now)
{
    enum code raw, old = g_st.code;

    g_st.reads++;
    if (ok) {
        s_fails = 0;
        g_st.sensor_ok = true;
        g_st.have_data = true;
        g_st.temp_c = t;
        g_st.humi_pct = h;
        g_st.hi_valid = (t >= 27 && h >= 40);
        g_st.heat_index_c = g_st.hi_valid ? heat_index_c(t, h) : t;
        raw = classify(t, h);
    } else {
        if (s_fails < ENV_SENSOR_FAILS) s_fails++;
        g_st.sensor_ok = false;
        /* keep the current situation until the failure is persistent */
        raw = (s_fails >= ENV_SENSOR_FAILS) ? ENV_SENSOR : g_st.code;
    }

    /* New situation: ENV_CONFIRM_READS in a row; first reading and sensor
     * fault (start / end) apply at once */
    if (raw == g_st.code) {
        s_cand_reads = 0;
    } else if (g_st.code == ENV_WAIT || g_st.code == ENV_SENSOR || raw == ENV_SENSOR) {
        g_st.code = raw;
        s_cand_reads = 0;
    } else {
        if (raw != s_cand) {
            s_cand = raw;
            s_cand_reads = 0;
        }
        if (++s_cand_reads >= ENV_CONFIRM_READS) {
            g_st.code = raw;
            s_cand_reads = 0;
        }
    }
    if ((g_st.code == ENV_SENSOR) != (old == ENV_SENSOR)) g_events |= BIT(DEV_SENSOR);
    if (g_st.code != old) {
        g_events |= BIT(DEV_ENV);
        printk("[ENV] %s -> %s (%s)\n", env_info[old].str, env_info[g_st.code].str,
               level_str[env_level(g_st.code)]);
    }

    /* Watering request: a watering situation held ENV_WATER_CONFIRM_MS */
    bool waters = env_info[g_st.code].waters;
    if (!waters) {
        s_water_since = 0;
    } else if (s_water_since == 0 || !env_info[old].waters) {
        s_water_since = now;
    }
    bool req = waters && (now - s_water_since >= ENV_WATER_CONFIRM_MS);
    if (req != g_st.water_request) printk("[ENV] water request %s\n", req ? "ON" : "OFF");
    g_st.water_request = req;

    /* Relay 1 follows the level */
    enum level lv = env_level(g_st.code);
    atomic_val_t half = lv == LVL_CRIT ? ALERT_FAST_HALF_MS : lv == LVL_WARN ? ALERT_SLOW_HALF_MS : 0;
    if (atomic_set(&g_alert_half_ms, half) != half) {
        g_events |= BIT(DEV_RELAY);
        driver_gpio_blink(OUT_ALERT, half == ALERT_FAST_HALF_MS ? ALERT_FAST_MS : ALERT_SLOW_MS,
                          half ? 2 : 0);
    }
}

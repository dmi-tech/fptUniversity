/* settings.c — thresholds, confirm time, off timer, fault action, quiet
 * hours, and the mode / MANUAL lamp restored after a power cut, kept in flash
 * in g_cfg.app_data (with their own magic, see app_config.h). Written at once
 * from the web page, SAVE_DELAY_MS after the last change from the buttons or
 * a mode / lamp change (fewer flash writes). */
#include <zephyr/sys/printk.h>
#include <string.h>

#include "app_config.h"
#include "app.h"

struct stored {
    uint32_t magic;
    struct settings s;
};
BUILD_ASSERT(sizeof(struct stored) <= CFG_APP_DATA_SIZE, "settings do not fit app_data");

static int64_t s_save_at;     /* delayed write, 0 = none (main thread) */

const char *settings_check(const struct settings *s)
{
    if (s->on_lux < 1 || s->on_lux > SET_LUX_MAX)    return "on lux must be 1..5000";
    if (s->off_lux > SET_LUX_MAX)                     return "off lux must be at most 5000";
    if (s->off_lux < s->on_lux + SET_LUX_GAP_MIN)     return "off lux must be at least on lux + 5";
    if (s->confirm_s < SET_CONFIRM_MIN || s->confirm_s > SET_CONFIRM_MAX)
        return "confirm time must be 5..600 s";
    if (s->timer_s > SET_TIMER_MAX_S)                 return "timer must be 0..43200 s";
    if (s->fault > FAULT_OFF)                         return "fault action must be keep, on or off";
    if (s->quiet_on > 1 || s->quiet_from >= 24 * 60 || s->quiet_to >= 24 * 60)
        return "quiet hours must be 00:00..23:59";
    if (s->mode > MODE_MANUAL || s->lamp > 1)         return "bad mode";
    return NULL;
}

void settings_load(void)
{
    struct stored st;

    memcpy(&st, g_cfg.app_data, sizeof(st));
    if (st.magic == SET_MAGIC && settings_check(&st.s) == NULL) {
        g_set = st.s;
        printk("[SET] loaded\n");
    } else {
        g_set = (struct settings){
            .on_lux = SET_ON_LUX_DEF, .off_lux = SET_OFF_LUX_DEF, .confirm_s = SET_CONFIRM_S_DEF,
            .fault = FAULT_ON, .quiet_from = SET_QUIET_FROM_DEF, .quiet_to = SET_QUIET_TO_DEF,
            .mode = MODE_AUTO,
        };
        printk("[SET] defaults\n");
    }
}

void settings_save_at(int64_t at)
{
    s_save_at = at;
}

void settings_poll(int64_t now)
{
    struct stored st = { .magic = SET_MAGIC };

    if (!s_save_at || now < s_save_at) return;
    s_save_at = 0;
    k_mutex_lock(&g_lock, K_FOREVER);
    st.s = g_set;
    k_mutex_unlock(&g_lock);

    config_lock();
    memset(g_cfg.app_data, 0, sizeof(g_cfg.app_data));
    memcpy(g_cfg.app_data, &st, sizeof(st));
    int rc = config_save(&g_cfg);      /* both flash copies */
    config_unlock();
    printk("[SET] saved (%d): on<%u lx, off>%u lx, %u s, timer %u s, fault %s, "
           "quiet %s %02u:%02u-%02u:%02u, %s%s\n", rc,
           st.s.on_lux, st.s.off_lux, st.s.confirm_s, st.s.timer_s, fault_str[st.s.fault],
           st.s.quiet_on ? "on" : "off", st.s.quiet_from / 60, st.s.quiet_from % 60,
           st.s.quiet_to / 60, st.s.quiet_to % 60,
           st.s.mode == MODE_AUTO ? "auto" : "manual",
           st.s.mode == MODE_AUTO ? "" : st.s.lamp ? " lamp on" : " lamp off");
}

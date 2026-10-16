/* settings.c — thresholds, confirm time and off timer, kept in flash in
 * g_cfg.app_data (with their own magic, see app_config.h). Changed on the web
 * page (written at once) or with BTN3 / BTN4 (timer, written SAVE_DELAY_MS
 * after the last click). */
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
    if (s->timer_h > SET_TIMER_MAX)                   return "timer must be 0..12 h";
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
        g_set = (struct settings){ SET_ON_LUX_DEF, SET_OFF_LUX_DEF, SET_CONFIRM_S_DEF, 0 };
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

    memset(g_cfg.app_data, 0, sizeof(g_cfg.app_data));
    memcpy(g_cfg.app_data, &st, sizeof(st));
    int rc = config_save(&g_cfg);      /* both flash copies */
    printk("[SET] saved (%d): on<%u lx, off>%u lx, %u s, timer %u h\n", rc,
           st.s.on_lux, st.s.off_lux, st.s.confirm_s, st.s.timer_h);
}

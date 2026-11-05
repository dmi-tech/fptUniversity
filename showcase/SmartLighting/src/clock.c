/* clock.c — local time for the quiet hours (clock thread).
 *
 * Once Ethernet is up, the time is read from SNTP_SERVER, then again every
 * SNTP_RESYNC_MS (SNTP_RETRY_MS after a failed request). Between two reads
 * the time follows k_uptime_get(). Local time = UTC + TZ_OFFSET_S. Before
 * the first answer (no Internet) the clock is unknown: quiet hours are off. */
#include <zephyr/kernel.h>
#include <zephyr/net/sntp.h>
#include <zephyr/sys/printk.h>

#include "network.h"
#include "app.h"

static struct k_spinlock s_lock;
static int64_t s_epoch_ms;        /* local epoch ms at uptime 0, 0 = not synced */

bool clock_minute(int *min)
{
    k_spinlock_key_t key = k_spin_lock(&s_lock);
    int64_t base = s_epoch_ms;

    k_spin_unlock(&s_lock, key);
    if (base == 0) return false;
    *min = (int)(((base + k_uptime_get()) / 60000) % (24 * 60));
    return true;
}

static void clock_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    while (1) {
        struct sntp_time ts;
        int rc, m;

        if (!network_eth_connected()) {
            k_msleep(1000);
            continue;
        }
        rc = sntp_simple(SNTP_SERVER, SNTP_TIMEOUT_MS, &ts);
        if (rc == 0) {
            int64_t ms = (int64_t)ts.seconds * 1000 + (int64_t)((ts.fraction * 1000ULL) >> 32);
            k_spinlock_key_t key = k_spin_lock(&s_lock);

            s_epoch_ms = ms + (int64_t)TZ_OFFSET_S * 1000 - k_uptime_get();
            k_spin_unlock(&s_lock, key);
            clock_minute(&m);
            printk("[CLOCK] %02d:%02d (UTC%+d, %s)\n", m / 60, m % 60, TZ_OFFSET_S / 3600,
                   SNTP_SERVER);
            k_msleep(SNTP_RESYNC_MS);
        } else {
            printk("[CLOCK] SNTP error %d\n", rc);
            k_msleep(SNTP_RETRY_MS);
        }
    }
}

K_THREAD_DEFINE(clock_tid, CLOCK_STACK, clock_thread, NULL, NULL, NULL, CLOCK_PRIO, 0, 0);

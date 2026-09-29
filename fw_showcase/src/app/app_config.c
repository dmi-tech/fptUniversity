#include "app_config.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

app_config_t g_cfg;

/* ── CRC32 (IEEE 802.3 poly) ─────────────────────────────────── */
static uint32_t crc32_calc(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int b = 0; b < 8; b++)
            c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}

/* ── Defaults ────────────────────────────────────────────────── */
void config_defaults(app_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic   = CFG_MAGIC;
    cfg->version = CFG_VERSION;

    strncpy(cfg->device_name, "mini_GW", sizeof(cfg->device_name) - 1);

    cfg->ip_mode     = 0;                   /* DHCP */
    cfg->static_ip[0] = 192; cfg->static_ip[1] = 168;
    cfg->static_ip[2] = 1;   cfg->static_ip[3] = 100;
    cfg->subnet[0] = 255; cfg->subnet[1] = 255;
    cfg->subnet[2] = 255; cfg->subnet[3] = 0;
    cfg->gateway[0] = 192; cfg->gateway[1] = 168;
    cfg->gateway[2] = 1;   cfg->gateway[3] = 1;
    cfg->dns[0] = 8; cfg->dns[1] = 8; cfg->dns[2] = 8; cfg->dns[3] = 8;
    cfg->web_port = 80;

    /* CAN / Modbus: unused, kept so the flash layout stays compatible */
    cfg->can_bitrate = 500000;
    cfg->can_tx_id   = 0x785;

    cfg->mb_baud = 9600;
    cfg->mb_addr = 1;

    /* MQTT broker, topic and credentials are empty until set on /config;
     * port 8883 selects TLS, any other port plain TCP. */
    cfg->mqtt_port = 1883;
    strncpy(cfg->mqtt_client_id, "mini_GW",           sizeof(cfg->mqtt_client_id) - 1);

    strncpy(cfg->web_password, "123456", sizeof(cfg->web_password) - 1);
}

/* ── One config slot: read + validate (magic/version/CRC) ───────── */
static int slot_read(uint8_t id, app_config_t *cfg)
{
    const struct flash_area *fa;
    int r = flash_area_open(id, &fa);
    if (r) return r;

    app_config_t tmp;
    r = flash_area_read(fa, 0, &tmp, sizeof(tmp));
    flash_area_close(fa);
    if (r) return r;

    if (tmp.magic != CFG_MAGIC || tmp.version != CFG_VERSION)
        return -ENODATA;

    uint32_t calc = crc32_calc((const uint8_t *)&tmp,
                               sizeof(tmp) - sizeof(uint32_t));
    if (calc != tmp.crc)
        return -EBADMSG;

    memcpy(cfg, &tmp, sizeof(*cfg));
    return 0;
}

/* ── One config slot: erase + write ────────────────────────────── */
static int slot_write(uint8_t id, const app_config_t *tmp)
{
    const struct flash_area *fa;
    int r = flash_area_open(id, &fa);
    if (r) return r;

    r = flash_area_erase(fa, 0, 8192);
    if (!r) r = flash_area_write(fa, 0, tmp, sizeof(*tmp));
    flash_area_close(fa);
    return r;
}

/* ── Flash load: try slot A, fall back to slot B (rollback) ────── */
int config_load(app_config_t *cfg)
{
    if (slot_read(PARTITION_ID(storage_partition), cfg) == 0) {
        printk("[CFG] loaded from slot A\n");
        return 0;
    }
    printk("[CFG] slot A invalid — trying backup slot B\n");
    if (slot_read(PARTITION_ID(storage_b_partition), cfg) == 0) {
        printk("[CFG] loaded from backup slot B\n");
        return 0;
    }
    return -ENODATA;
}

/* ── Flash save: write slot A then slot B (backup) ─────────────── */
int config_save(const app_config_t *cfg)
{
    app_config_t tmp;
    memcpy(&tmp, cfg, sizeof(tmp));
    tmp.magic   = CFG_MAGIC;
    tmp.version = CFG_VERSION;
    tmp.crc     = crc32_calc((const uint8_t *)&tmp,
                              sizeof(tmp) - sizeof(uint32_t));

    int ra = slot_write(PARTITION_ID(storage_partition),   &tmp);
    int rb = slot_write(PARTITION_ID(storage_b_partition), &tmp);
    if (ra) printk("[CFG] slot A write err %d\n", ra);
    if (rb) printk("[CFG] slot B write err %d\n", rb);
    return ra ? ra : rb;   /* success only if both wrote OK */
}

/* ── IP helpers ──────────────────────────────────────────────── */
void ip4_to_str(const uint8_t ip[4], char out[16])
{
    snprintf(out, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

int str_to_ip4(const char *s, uint8_t ip[4])
{
    unsigned a, b, c, d;
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return -1;
    if (a > 255 || b > 255 || c > 255 || d > 255) return -1;
    ip[0] = a; ip[1] = b; ip[2] = c; ip[3] = d;
    return 0;
}

#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include <stdint.h>
#include <stdbool.h>

#define CFG_MAGIC   0xCF6742A0U
#define CFG_VERSION 2

/* All configurable parameters, stored in the config-a / config-b flash
 * partitions. Do not reorder or resize fields without bumping CFG_VERSION. */
typedef struct {
    uint32_t magic;
    uint8_t  version;
    /* Network */
    char     device_name[16];
    uint8_t  ip_mode;        /* 0=DHCP, 1=Static */
    uint8_t  static_ip[4];
    uint8_t  subnet[4];
    uint8_t  gateway[4];
    uint8_t  dns[4];
    uint16_t web_port;
    /* CAN (unused, kept for flash layout compatibility) */
    uint32_t can_bitrate;
    uint32_t can_tx_id;
    /* Modbus RTU (unused, kept for flash layout compatibility) */
    uint32_t mb_baud;
    uint8_t  mb_addr;
    /* MQTT */
    char     mqtt_host[64];
    uint16_t mqtt_port;
    char     mqtt_client_id[32];
    char     mqtt_topic[64];     /* unused since 3.1.0 (status goes to "users/admin@example.com/<NNN>/status"), kept for the flash layout */
    char     mqtt_user[64];
    char     mqtt_pass[256];
    /* Web login */
    char     web_password[32];
    /* Padding to align CRC */
    uint8_t  _pad[3];
    /* CRC32 of all fields above */
    uint32_t crc;
} app_config_t;

extern app_config_t g_cfg;

void config_defaults(app_config_t *cfg);
int  config_load(app_config_t *cfg);
int  config_save(const app_config_t *cfg);

/* Utility: convert 4-byte IP to "a.b.c.d" string */
void ip4_to_str(const uint8_t ip[4], char out[16]);
/* Parse "a.b.c.d" into 4-byte array; returns 0 on success */
int  str_to_ip4(const char *s, uint8_t ip[4]);

#endif /* APP_CONFIG_H */

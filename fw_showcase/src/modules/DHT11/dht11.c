/* dht11.c — DHT11 through the Zephyr "aosong,dht" sensor driver */
#include "dht11.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

static const struct device *const dht = DEVICE_DT_GET(DT_ALIAS(dht0));
static int s_temp_x10, s_humi_x10;

/* val2 is in millionths and carries the same sign as val1 */
static int to_x10(const struct sensor_value *v)
{
    return v->val1 * 10 + v->val2 / 100000;
}

int dht11_last_temp_x10(void) { return s_temp_x10; }
int dht11_last_humi_x10(void) { return s_humi_x10; }

bool dht11_init(void)
{
    if (!device_is_ready(dht)) {
        printk("[DHT11] device not ready (PC2)\n");
        return false;
    }
    printk("[DHT11] PC2 OK\n");
    return true;
}

int dht11_read(int *temp_c, int *humi_pct)
{
    struct sensor_value t, h;
    int rc = sensor_sample_fetch(dht);

    if (rc == 0) rc = sensor_channel_get(dht, SENSOR_CHAN_AMBIENT_TEMP, &t);
    if (rc == 0) rc = sensor_channel_get(dht, SENSOR_CHAN_HUMIDITY, &h);
    if (rc != 0) return rc;

    s_temp_x10 = to_x10(&t);
    s_humi_x10 = to_x10(&h);
    *temp_c   = t.val1;
    *humi_pct = h.val1;
    return 0;
}

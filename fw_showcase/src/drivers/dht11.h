#ifndef DHT11_H
#define DHT11_H
#include <stdbool.h>

/* DHT11 on PC2 (U16-5), see boards/stm32h573ri_custom.overlay */
bool dht11_init(void);

/* One measurement. Returns 0 on success (temperature in °C, humidity in %),
 * a negative errno on a timeout/checksum failure. The DHT11 must not be
 * read more often than once per second. */
int dht11_read(int *temp_c, int *humi_pct);

/* Last successful measurement in tenths (345 = 34.5 °C / 34.5 %) */
int dht11_last_temp_x10(void);
int dht11_last_humi_x10(void);

#endif /* DHT11_H */

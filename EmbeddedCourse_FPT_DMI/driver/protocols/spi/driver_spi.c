/**
 * @file driver_spi.c
 * @brief SPI driver on top of the Zephyr SPI API (one device, node label "spidev").
 */
#include <errno.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

#include "driver_spi.h"

#define DUMMY_CHUNK 64

#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(spidev))

/* "vnd,spi-device" has no Zephyr driver: take the SPI specification, never the device. */
static const struct spi_dt_spec spec = SPI_DT_SPEC_GET(DT_NODELABEL(spidev), SPI_WORD_SET(8));
static const uint32_t max_freq = DT_PROP(DT_NODELABEL(spidev), spi_max_frequency);

static struct spi_config cfg;
static bool ready;
static K_MUTEX_DEFINE(spi_lock);
static uint8_t dummy[DUMMY_CHUNK];

int driver_spi_init(uint32_t freq_hz, uint8_t mode)
{
	if (mode > 3 || freq_hz == 0 || freq_hz > max_freq) {
		return -EINVAL;
	}
	if (!spi_is_ready_dt(&spec)) {
		return -ENODEV;
	}

	k_mutex_lock(&spi_lock, K_FOREVER);
	cfg = spec.config;
	cfg.frequency = freq_hz;
	cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER |
			((mode & 2) ? SPI_MODE_CPOL : 0) | ((mode & 1) ? SPI_MODE_CPHA : 0);
	memset(dummy, 0xFF, sizeof(dummy));
	ready = true;
	k_mutex_unlock(&spi_lock);
	return 0;
}

/* tx may be NULL (zeros); rx may be NULL (ignored). Both lengths are in bytes. */
static int do_transceive(const struct spi_buf *tx, size_t tx_count, const struct spi_buf *rx,
			 size_t rx_count)
{
	const struct spi_buf_set tx_set = {.buffers = tx, .count = tx_count};
	const struct spi_buf_set rx_set = {.buffers = rx, .count = rx_count};
	int ret;

	if (!ready) {
		return -EACCES;
	}
	k_mutex_lock(&spi_lock, K_FOREVER);
	ret = spi_transceive(spec.bus, &cfg, tx ? &tx_set : NULL, rx ? &rx_set : NULL);
	k_mutex_unlock(&spi_lock);
	return ret;
}

int driver_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
	struct spi_buf tx_buf = {.buf = (void *)tx, .len = len};
	struct spi_buf rx_buf = {.buf = rx, .len = len};

	if (len == 0 || (tx == NULL && rx == NULL)) {
		return -EINVAL;
	}
	/* A NULL buffer pointer makes the driver send zeros / skip received bytes */
	return do_transceive(&tx_buf, 1, &rx_buf, 1);
}

int driver_spi_write(const uint8_t *tx, size_t len)
{
	if (tx == NULL) {
		return -EINVAL;
	}
	return driver_spi_transfer(tx, NULL, len);
}

/* Fill @p bufs with entries that point to the 0xFF block, @p total bytes in all. */
static size_t dummy_bufs(struct spi_buf *bufs, size_t total)
{
	size_t n = 0;

	while (total > 0) {
		size_t chunk = MIN(total, sizeof(dummy));

		bufs[n].buf = dummy;
		bufs[n].len = chunk;
		n++;
		total -= chunk;
	}
	return n;
}

int driver_spi_read(uint8_t *rx, size_t len)
{
	struct spi_buf tx_bufs[DRIVER_SPI_MAX_READ / DUMMY_CHUNK];
	struct spi_buf rx_buf = {.buf = rx, .len = len};
	size_t n;

	if (rx == NULL || len == 0 || len > DRIVER_SPI_MAX_READ) {
		return -EINVAL;
	}
	n = dummy_bufs(tx_bufs, len);
	return do_transceive(tx_bufs, n, &rx_buf, 1);
}

int driver_spi_write_then_read(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
	struct spi_buf tx_bufs[1 + DRIVER_SPI_MAX_READ / DUMMY_CHUNK];
	struct spi_buf rx_bufs[2];
	size_t n;

	if (tx == NULL || tx_len == 0 || rx == NULL || rx_len == 0 ||
	    rx_len > DRIVER_SPI_MAX_READ) {
		return -EINVAL;
	}
	tx_bufs[0].buf = (void *)tx;
	tx_bufs[0].len = tx_len;
	n = 1 + dummy_bufs(&tx_bufs[1], rx_len);

	rx_bufs[0].buf = NULL; /* skip what comes back during the command */
	rx_bufs[0].len = tx_len;
	rx_bufs[1].buf = rx;
	rx_bufs[1].len = rx_len;
	return do_transceive(tx_bufs, n, rx_bufs, 2);
}

#else /* node "spidev" not present */

int driver_spi_init(uint32_t freq_hz, uint8_t mode)
{
	if (mode > 3 || freq_hz == 0) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
	if (len == 0 || (tx == NULL && rx == NULL)) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_spi_write(const uint8_t *tx, size_t len)
{
	return driver_spi_transfer(tx, NULL, len);
}

int driver_spi_read(uint8_t *rx, size_t len)
{
	if (rx == NULL || len == 0 || len > DRIVER_SPI_MAX_READ) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_spi_write_then_read(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
	if (tx == NULL || tx_len == 0 || rx == NULL || rx_len == 0 ||
	    rx_len > DRIVER_SPI_MAX_READ) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

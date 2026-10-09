/**
 * @file driver_spi.h
 * @brief SPI2 master with one chip select, using the devicetree node labelled "spidev".
 *
 * Pins: SCK PA9, MOSI PC1, MISO PC2, CS PC0 (see README for the overlay).
 */
#ifndef DRIVER_SPI_H_
#define DRIVER_SPI_H_

#include <stddef.h>
#include <stdint.h>

/** Largest number of bytes sent as dummy 0xFF in driver_spi_read() / write_then_read(). */
#define DRIVER_SPI_MAX_READ 1024

/**
 * @brief Set clock and mode. Call before any transfer; may be called again to change them.
 *
 * @param freq_hz Wanted clock in Hz; the hardware uses the nearest value not above it
 * @param mode    SPI mode 0..3 (bit 1 = CPOL, bit 0 = CPHA)
 *
 * @retval 0       Success
 * @retval -EINVAL mode above 3, freq_hz 0 or above the spi-max-frequency of the node
 * @retval -ENODEV Node "spidev" missing or SPI controller not ready
 */
int driver_spi_init(uint32_t freq_hz, uint8_t mode);

/**
 * @brief Send and receive @p len bytes at the same time. CS is low during the transfer.
 *
 * @param tx  Bytes to send, or NULL to send zeros
 * @param rx  Buffer for received bytes, or NULL to ignore them
 * @retval 0       Success
 * @retval -EINVAL len is 0 or both buffers are NULL
 * @retval -EACCES driver_spi_init() not called
 */
int driver_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len);

/** @brief Send only. */
int driver_spi_write(const uint8_t *tx, size_t len);

/**
 * @brief Receive only (sends 0xFF). At most DRIVER_SPI_MAX_READ bytes.
 * @retval -EINVAL len is 0 or above DRIVER_SPI_MAX_READ
 */
int driver_spi_read(uint8_t *rx, size_t len);

/**
 * @brief Send a command, then read the answer, with CS low during both phases.
 *
 * Like reading a register: the bytes received while the command is sent are discarded.
 * @retval -EINVAL A length is 0 or @p rx_len is above DRIVER_SPI_MAX_READ
 */
int driver_spi_write_then_read(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);

#endif /* DRIVER_SPI_H_ */

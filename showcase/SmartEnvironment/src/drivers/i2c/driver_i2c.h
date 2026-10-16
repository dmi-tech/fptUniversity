/**
 * @file driver_i2c.h
 * @brief I2C master access to I2C1 (header U16) and I2C2 (on-board SHT41, RTC).
 *
 * Addresses are 7 bit (0x08..0x77), not shifted left.
 */
#ifndef DRIVER_I2C_H_
#define DRIVER_I2C_H_

#include <stddef.h>
#include <stdint.h>

/** I2C buses. */
enum driver_i2c_bus {
	DRIVER_I2C1 = 0, /**< PB9 SDA, PB6 SCL (U16-9/10) */
	DRIVER_I2C2 = 1, /**< PB11 SDA, PB10 SCL (on board) */
};

/**
 * @brief Check that a bus is ready.
 * @retval 0       Ready
 * @retval -EINVAL Unknown bus
 * @retval -ENODEV Bus disabled in devicetree or not ready
 */
int driver_i2c_init(enum driver_i2c_bus bus);

/**
 * @brief Probe every address 0x08..0x77 and print those that answer with printk.
 * @return Number of devices found, or negative errno (-EINVAL, -ENODEV)
 */
int driver_i2c_scan(enum driver_i2c_bus bus);

/**
 * @brief Write @p len bytes.
 * @retval 0       Success
 * @retval -EINVAL Bad bus, address or NULL data
 * @retval -ENODEV Bus not available
 * @retval -EIO    The device did not acknowledge
 */
int driver_i2c_write(enum driver_i2c_bus bus, uint8_t addr, const uint8_t *data, size_t len);

/** @brief Read @p len bytes. Return values as driver_i2c_write(). */
int driver_i2c_read(enum driver_i2c_bus bus, uint8_t addr, uint8_t *data, size_t len);

/** @brief Write then read with a repeated START (no STOP in between). */
int driver_i2c_write_read(enum driver_i2c_bus bus, uint8_t addr, const uint8_t *wr, size_t wlen,
			  uint8_t *rd, size_t rlen);

/** @brief Write one register: sends @p reg then @p value. */
int driver_i2c_reg_write(enum driver_i2c_bus bus, uint8_t addr, uint8_t reg, uint8_t value);

/** @brief Read one register: sends @p reg, then reads one byte. */
int driver_i2c_reg_read(enum driver_i2c_bus bus, uint8_t addr, uint8_t reg, uint8_t *value);

#endif /* DRIVER_I2C_H_ */

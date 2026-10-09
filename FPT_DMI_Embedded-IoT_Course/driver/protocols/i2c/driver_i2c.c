/**
 * @file driver_i2c.c
 * @brief I2C driver on top of the Zephyr I2C API.
 */
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_i2c.h"

#define I2C_ADDR_MIN 0x08
#define I2C_ADDR_MAX 0x77

/* NULL when the bus is disabled in devicetree. */
static const struct device *const bus_dev[] = {
	[DRIVER_I2C1] = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(i2c1)),
	[DRIVER_I2C2] = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(i2c2)),
};

static int get_bus(enum driver_i2c_bus bus, const struct device **dev)
{
	if ((unsigned int)bus >= ARRAY_SIZE(bus_dev)) {
		return -EINVAL;
	}
	if (bus_dev[bus] == NULL || !device_is_ready(bus_dev[bus])) {
		return -ENODEV;
	}
	*dev = bus_dev[bus];
	return 0;
}

static bool addr_ok(uint8_t addr)
{
	return addr >= I2C_ADDR_MIN && addr <= I2C_ADDR_MAX;
}

int driver_i2c_init(enum driver_i2c_bus bus)
{
	const struct device *dev;

	return get_bus(bus, &dev);
}

int driver_i2c_scan(enum driver_i2c_bus bus)
{
	const struct device *dev;
	int found = 0;
	int ret = get_bus(bus, &dev);

	if (ret < 0) {
		return ret;
	}
	for (uint8_t addr = I2C_ADDR_MIN; addr <= I2C_ADDR_MAX; addr++) {
		/* A zero-length write only checks for an acknowledge */
		if (i2c_write(dev, NULL, 0, addr) == 0) {
			printk("  0x%02x\n", addr);
			found++;
		}
	}
	return found;
}

int driver_i2c_write(enum driver_i2c_bus bus, uint8_t addr, const uint8_t *data, size_t len)
{
	const struct device *dev;
	int ret;

	if (!addr_ok(addr) || (data == NULL && len > 0)) {
		return -EINVAL;
	}
	ret = get_bus(bus, &dev);
	if (ret < 0) {
		return ret;
	}
	return i2c_write(dev, data, len, addr);
}

int driver_i2c_read(enum driver_i2c_bus bus, uint8_t addr, uint8_t *data, size_t len)
{
	const struct device *dev;
	int ret;

	if (!addr_ok(addr) || data == NULL || len == 0) {
		return -EINVAL;
	}
	ret = get_bus(bus, &dev);
	if (ret < 0) {
		return ret;
	}
	return i2c_read(dev, data, len, addr);
}

int driver_i2c_write_read(enum driver_i2c_bus bus, uint8_t addr, const uint8_t *wr, size_t wlen,
			  uint8_t *rd, size_t rlen)
{
	const struct device *dev;
	int ret;

	if (!addr_ok(addr) || wr == NULL || wlen == 0 || rd == NULL || rlen == 0) {
		return -EINVAL;
	}
	ret = get_bus(bus, &dev);
	if (ret < 0) {
		return ret;
	}
	return i2c_write_read(dev, addr, wr, wlen, rd, rlen);
}

int driver_i2c_reg_write(enum driver_i2c_bus bus, uint8_t addr, uint8_t reg, uint8_t value)
{
	const uint8_t buf[2] = {reg, value};

	return driver_i2c_write(bus, addr, buf, sizeof(buf));
}

int driver_i2c_reg_read(enum driver_i2c_bus bus, uint8_t addr, uint8_t reg, uint8_t *value)
{
	return driver_i2c_write_read(bus, addr, &reg, 1, value, 1);
}

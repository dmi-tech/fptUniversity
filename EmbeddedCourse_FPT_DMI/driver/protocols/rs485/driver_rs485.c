/**
 * @file driver_rs485.c
 * @brief RS485 driver: UART4 plus an optional DE/RE direction pin.
 */
#include <errno.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "driver_rs485.h"
#include "driver_uart.h"

#define RS485_PORT      DRIVER_UART4
#define TX_DONE_TIMEOUT K_SECONDS(1)

/* Empty spec (port == NULL) when zephyr,user has no rs485-de-gpios. */
static const struct gpio_dt_spec de_pin =
	GPIO_DT_SPEC_GET_OR(DT_PATH(zephyr_user), rs485_de_gpios, {0});

static bool ready;
static K_MUTEX_DEFINE(bus_lock);

int driver_rs485_init(uint32_t baud)
{
	int ret;

	if (baud == 0) {
		return -EINVAL;
	}
	if (de_pin.port != NULL) {
		if (!gpio_is_ready_dt(&de_pin)) {
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&de_pin, GPIO_OUTPUT_INACTIVE); /* receive */
		if (ret < 0) {
			return ret;
		}
	}
	ret = driver_uart_init(RS485_PORT, baud);
	if (ret < 0) {
		return ret;
	}
	ready = true;
	return 0;
}

int driver_rs485_send(const uint8_t *data, size_t len)
{
	int ret;
	int flush_ret;

	if (data == NULL && len > 0) {
		return -EINVAL;
	}
	if (!ready) {
		return -EACCES;
	}

	k_mutex_lock(&bus_lock, K_FOREVER);
	if (de_pin.port != NULL) {
		gpio_pin_set_dt(&de_pin, 1);
	}
	ret = driver_uart_write(RS485_PORT, data, len);
	/* The last byte must be completely out before the bus is released */
	flush_ret = driver_uart_flush(RS485_PORT, TX_DONE_TIMEOUT);
	if (de_pin.port != NULL) {
		gpio_pin_set_dt(&de_pin, 0);
	}
	k_mutex_unlock(&bus_lock);

	return ret < 0 ? ret : flush_ret;
}

int driver_rs485_print(const char *str)
{
	if (str == NULL) {
		return -EINVAL;
	}
	return driver_rs485_send((const uint8_t *)str, strlen(str));
}

int driver_rs485_receive(uint8_t *buf, size_t len, k_timeout_t timeout)
{
	if (!ready) {
		return -EACCES;
	}
	return driver_uart_read(RS485_PORT, buf, len, timeout);
}

int driver_rs485_receive_frame(uint8_t *buf, size_t len, uint32_t gap_ms, k_timeout_t timeout)
{
	size_t total = 0;
	int n;

	if (buf == NULL || len == 0) {
		return -EINVAL;
	}
	if (!ready) {
		return -EACCES;
	}

	n = driver_uart_read(RS485_PORT, buf, len, timeout);
	if (n <= 0) {
		return n;
	}
	total = (size_t)n;

	while (total < len) {
		n = driver_uart_read(RS485_PORT, buf + total, len - total, K_MSEC(gap_ms));
		if (n < 0) {
			return n;
		}
		if (n == 0) {
			break; /* line silent for gap_ms: end of frame */
		}
		total += (size_t)n;
	}
	return (int)total;
}

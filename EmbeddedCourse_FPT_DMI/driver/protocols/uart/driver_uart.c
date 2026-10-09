/**
 * @file driver_uart.c
 * @brief UART driver: interrupt-driven receive into a ring buffer, polled transmit.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

#include "driver_uart.h"

#define PRINTF_MAX 128

struct port_ctx {
	const struct device *dev; /* NULL when the port is disabled in devicetree */
	struct ring_buf rx_ring;
	uint8_t rx_storage[DRIVER_UART_RX_BUF_SIZE];
	struct k_sem rx_sem;
	struct k_mutex tx_lock;
	driver_uart_rx_cb_t cb;
	enum driver_uart_port port;
	bool ready;
};

static struct port_ctx ports[] = {
	[DRIVER_UART6] = {.dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(usart6)), .port = DRIVER_UART6},
	[DRIVER_UART1] = {.dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(usart1)), .port = DRIVER_UART1},
	[DRIVER_UART4] = {.dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart4)), .port = DRIVER_UART4},
};

static int get_port(enum driver_uart_port port, struct port_ctx **ctx)
{
	if ((unsigned int)port >= ARRAY_SIZE(ports)) {
		return -EINVAL;
	}
	if (ports[port].dev == NULL || !device_is_ready(ports[port].dev)) {
		return -ENODEV;
	}
	*ctx = &ports[port];
	return 0;
}

/* Same as get_port(), and the port must have been initialised. */
static int get_ready_port(enum driver_uart_port port, struct port_ctx **ctx)
{
	int ret = get_port(port, ctx);

	if (ret == 0 && !(*ctx)->ready) {
		return -EACCES;
	}
	return ret;
}

static void uart_isr(const struct device *dev, void *user_data)
{
	struct port_ctx *ctx = user_data;
	uint8_t byte;

	/* uart_irq_update() must be called once at the start of every interrupt */
	uart_irq_update(dev);
	if (!uart_irq_rx_ready(dev)) {
		return;
	}
	while (uart_fifo_read(dev, &byte, 1) == 1) {
		/* When the buffer is full the new byte is dropped */
		ring_buf_put(&ctx->rx_ring, &byte, 1);
		if (ctx->cb != NULL) {
			ctx->cb(ctx->port, byte);
		}
	}
	k_sem_give(&ctx->rx_sem);
}

int driver_uart_init(enum driver_uart_port port, uint32_t baud)
{
	struct port_ctx *ctx;
	struct uart_config cfg = {
		.baudrate = baud,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};
	int ret;

	if ((unsigned int)port >= ARRAY_SIZE(ports) || baud == 0) {
		return -EINVAL;
	}
	ret = get_port(port, &ctx);
	if (ret < 0) {
		return ret;
	}

	if (!ctx->ready) {
		ring_buf_init(&ctx->rx_ring, sizeof(ctx->rx_storage), ctx->rx_storage);
		k_sem_init(&ctx->rx_sem, 0, 1);
		k_mutex_init(&ctx->tx_lock);
	}

	ret = uart_configure(ctx->dev, &cfg);
	if (ret < 0) {
		return ret;
	}

	if (!ctx->ready) {
		uart_irq_callback_user_data_set(ctx->dev, uart_isr, ctx);
		uart_irq_rx_enable(ctx->dev);
		ctx->ready = true;
	}
	return 0;
}

int driver_uart_write(enum driver_uart_port port, const uint8_t *data, size_t len)
{
	struct port_ctx *ctx;
	int ret;

	if (data == NULL && len > 0) {
		return -EINVAL;
	}
	ret = get_ready_port(port, &ctx);
	if (ret < 0) {
		return ret;
	}

	k_mutex_lock(&ctx->tx_lock, K_FOREVER);
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(ctx->dev, data[i]);
	}
	k_mutex_unlock(&ctx->tx_lock);
	return 0;
}

int driver_uart_print(enum driver_uart_port port, const char *str)
{
	if (str == NULL) {
		return -EINVAL;
	}
	return driver_uart_write(port, (const uint8_t *)str, strlen(str));
}

int driver_uart_printf(enum driver_uart_port port, const char *fmt, ...)
{
	char buf[PRINTF_MAX];
	va_list ap;
	int n;

	if (fmt == NULL) {
		return -EINVAL;
	}
	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0) {
		return -EINVAL;
	}
	if (n >= (int)sizeof(buf)) {
		n = sizeof(buf) - 1; /* output was cut */
	}
	return driver_uart_write(port, (const uint8_t *)buf, (size_t)n);
}

int driver_uart_read(enum driver_uart_port port, uint8_t *buf, size_t len, k_timeout_t timeout)
{
	struct port_ctx *ctx;
	k_timepoint_t end = sys_timepoint_calc(timeout);
	int ret;

	if (buf == NULL || len == 0) {
		return -EINVAL;
	}
	ret = get_ready_port(port, &ctx);
	if (ret < 0) {
		return ret;
	}

	while (ring_buf_is_empty(&ctx->rx_ring)) {
		if (k_sem_take(&ctx->rx_sem, sys_timepoint_timeout(end)) != 0) {
			return 0;
		}
	}
	return (int)ring_buf_get(&ctx->rx_ring, buf, len);
}

int driver_uart_read_line(enum driver_uart_port port, char *buf, size_t len, k_timeout_t timeout)
{
	k_timepoint_t end = sys_timepoint_calc(timeout);
	size_t n = 0;

	if (buf == NULL || len < 2) {
		return -EINVAL;
	}
	buf[0] = '\0';

	while (1) {
		uint8_t c;
		int ret = driver_uart_read(port, &c, 1, sys_timepoint_timeout(end));

		if (ret < 0) {
			return ret;
		}
		if (ret == 0) {
			return -ETIMEDOUT;
		}
		if (c == '\n' || c == '\r') {
			buf[n] = '\0';
			return (int)n;
		}
		if (n < len - 1) {
			buf[n++] = (char)c;
		}
	}
}

int driver_uart_available(enum driver_uart_port port)
{
	struct port_ctx *ctx;
	int ret = get_ready_port(port, &ctx);

	if (ret < 0) {
		return ret;
	}
	return (int)ring_buf_size_get(&ctx->rx_ring);
}

int driver_uart_set_rx_callback(enum driver_uart_port port, driver_uart_rx_cb_t cb)
{
	struct port_ctx *ctx;
	int ret = get_ready_port(port, &ctx);

	if (ret < 0) {
		return ret;
	}
	ctx->cb = cb;
	return 0;
}

int driver_uart_flush(enum driver_uart_port port, k_timeout_t timeout)
{
	struct port_ctx *ctx;
	k_timepoint_t end = sys_timepoint_calc(timeout);
	int ret = get_ready_port(port, &ctx);

	if (ret < 0) {
		return ret;
	}
	while (!uart_irq_tx_complete(ctx->dev)) {
		if (sys_timepoint_expired(end)) {
			return -ETIMEDOUT;
		}
		k_busy_wait(50);
	}
	return 0;
}

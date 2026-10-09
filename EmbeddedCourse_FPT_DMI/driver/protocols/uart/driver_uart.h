/**
 * @file driver_uart.h
 * @brief Serial ports with an interrupt-driven receive buffer (256 bytes), 8N1.
 */
#ifndef DRIVER_UART_H_
#define DRIVER_UART_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

/** Serial ports. UART5 is the MCUboot console and is not available here. */
enum driver_uart_port {
	DRIVER_UART6 = 0, /**< USART6, PC6 TX / PC7 RX (U16-14/13) */
	DRIVER_UART1 = 1, /**< USART1, PB14 TX / PB15 RX (U16-16/15) */
	DRIVER_UART4 = 2, /**< UART4, RS485 (used by driver_rs485, not by students) */
};

/** Size of the receive buffer of each port, in bytes. */
#define DRIVER_UART_RX_BUF_SIZE 256

/**
 * @brief Receive callback, called for every received byte from interrupt context.
 * Keep it very short.
 */
typedef void (*driver_uart_rx_cb_t)(enum driver_uart_port port, uint8_t byte);

/**
 * @brief Configure a port (8 data bits, no parity, 1 stop bit) and start receiving.
 *
 * @retval 0       Success
 * @retval -EINVAL Unknown port or baud is 0
 * @retval -ENODEV Port disabled in devicetree, or not ready
 */
int driver_uart_init(enum driver_uart_port port, uint32_t baud);

/**
 * @brief Send bytes; returns when all of them have been handed to the hardware.
 * @return 0, or negative errno (-EINVAL, -ENODEV, -EACCES when not initialised)
 */
int driver_uart_write(enum driver_uart_port port, const uint8_t *data, size_t len);

/** @brief Send a zero-terminated string. */
int driver_uart_print(enum driver_uart_port port, const char *str);

/** @brief printf into the port. At most 127 characters are sent; longer output is cut. */
int driver_uart_printf(enum driver_uart_port port, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/**
 * @brief Read up to @p len bytes.
 *
 * Waits until at least one byte is available (or @p timeout), then returns what the
 * buffer holds, up to @p len.
 *
 * @return Number of bytes read (0 on timeout), or negative errno
 */
int driver_uart_read(enum driver_uart_port port, uint8_t *buf, size_t len, k_timeout_t timeout);

/**
 * @brief Read one line, ended by '\n' or '\r' (not stored). The result is zero-terminated.
 *
 * A line longer than @p len - 1 is cut, the rest of it is dropped. On timeout the bytes
 * already received for this line are discarded.
 *
 * @return Length of the line (0 for an empty line), -ETIMEDOUT on timeout, or other
 *         negative errno
 */
int driver_uart_read_line(enum driver_uart_port port, char *buf, size_t len, k_timeout_t timeout);

/** @brief Number of bytes waiting in the receive buffer, or negative errno. */
int driver_uart_available(enum driver_uart_port port);

/**
 * @brief Call @p cb for each received byte (interrupt context). NULL removes it.
 * @return 0, or negative errno
 */
int driver_uart_set_rx_callback(enum driver_uart_port port, driver_uart_rx_cb_t cb);

/**
 * @brief Wait until the last byte has left the wire (transmission complete flag).
 * Used by driver_rs485 before it releases the bus.
 * @retval 0          Done
 * @retval -ETIMEDOUT Still sending after @p timeout
 */
int driver_uart_flush(enum driver_uart_port port, k_timeout_t timeout);

#endif /* DRIVER_UART_H_ */

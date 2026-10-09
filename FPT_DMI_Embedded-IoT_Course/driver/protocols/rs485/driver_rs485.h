/**
 * @file driver_rs485.h
 * @brief Half-duplex RS485 on UART4 (isolated transceiver, connector CN1).
 *
 * Uses driver_uart (DRIVER_UART4). The direction pin DE/RE (PC3) is driven when the
 * devicetree node zephyr,user has the property rs485-de-gpios; without it the board is
 * assumed to switch direction by itself (resistor R87 fitted).
 */
#ifndef DRIVER_RS485_H_
#define DRIVER_RS485_H_

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

/**
 * @brief Configure UART4 (8N1) at @p baud and the direction pin.
 *
 * @retval 0       Success
 * @retval -EINVAL baud is 0
 * @retval -ENODEV UART4 disabled or not ready, or DE GPIO controller not ready
 */
int driver_rs485_init(uint32_t baud);

/**
 * @brief Send bytes: DE on, send, wait until the last byte left the wire, DE off.
 * @retval -EACCES driver_rs485_init() not called
 * @retval -ETIMEDOUT The transmitter did not finish in 1 s
 */
int driver_rs485_send(const uint8_t *data, size_t len);

/** @brief Send a zero-terminated string. */
int driver_rs485_print(const char *str);

/**
 * @brief Receive up to @p len bytes; waits for at least one (or @p timeout).
 * @return Number of bytes received (0 on timeout), or negative errno
 */
int driver_rs485_receive(uint8_t *buf, size_t len, k_timeout_t timeout);

/**
 * @brief Receive one frame: wait for the first byte (up to @p timeout), then keep reading
 *        until the line has been silent for @p gap_ms milliseconds or @p buf is full.
 * @return Number of bytes received (0 on timeout), or negative errno
 */
int driver_rs485_receive_frame(uint8_t *buf, size_t len, uint32_t gap_ms, k_timeout_t timeout);

#endif /* DRIVER_RS485_H_ */

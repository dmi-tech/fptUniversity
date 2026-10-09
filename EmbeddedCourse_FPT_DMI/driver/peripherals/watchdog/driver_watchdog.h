/**
 * @file driver_watchdog.h
 * @brief Independent watchdog (IWDG, alias watchdog0) and reset cause.
 */
#ifndef DRIVER_WATCHDOG_H_
#define DRIVER_WATCHDOG_H_

#include <stdbool.h>
#include <stdint.h>

/** Longest timeout accepted by driver_watchdog_init(), in milliseconds. */
#define DRIVER_WATCHDOG_MAX_TIMEOUT_MS 32000

/**
 * @brief Configure and start the watchdog. Call once. It cannot be stopped afterwards.
 *
 * The watchdog pauses while the CPU is halted by a debugger.
 *
 * @param timeout_ms Time between two driver_watchdog_feed() calls that triggers a reset,
 *                   1..DRIVER_WATCHDOG_MAX_TIMEOUT_MS
 *
 * @retval 0        Success
 * @retval -EINVAL  timeout_ms out of range
 * @retval -ENODEV  Alias watchdog0 missing or device not ready
 * @retval -EALREADY Already started
 */
int driver_watchdog_init(uint32_t timeout_ms);

/**
 * @brief Feed the watchdog. Must be called more often than the timeout.
 * @retval 0        Success
 * @retval -ENODEV  Watchdog not started
 */
int driver_watchdog_feed(void);

/**
 * @brief true when the last reset was caused by the watchdog.
 *
 * Call it at the start of main(): the answer is read once and remembered, the hardware
 * reset flags are cleared afterwards. Returns false when CONFIG_HWINFO is disabled.
 */
bool driver_watchdog_caused_reset(void);

#endif /* DRIVER_WATCHDOG_H_ */

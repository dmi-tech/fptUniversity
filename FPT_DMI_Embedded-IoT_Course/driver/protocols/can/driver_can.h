/**
 * @file driver_can.h
 * @brief Classic CAN on FDCAN1 (isolated transceiver, connector CN5), with loopback mode.
 *
 * The transceiver standby pin (PB0) is handled by Zephyr (can-transceiver-gpio).
 */
#ifndef DRIVER_CAN_H_
#define DRIVER_CAN_H_

#include <stdbool.h>
#include <stdint.h>

/** Most receive filters that can be added. */
#define DRIVER_CAN_MAX_FILTERS 8

/**
 * @brief Receive callback, called from interrupt context.
 * Copy the data or set a flag; do not print or block here.
 *
 * @param id   CAN identifier of the frame
 * @param data Payload (valid only during the call)
 * @param len  Payload length, 0..8
 */
typedef void (*driver_can_rx_cb_t)(uint32_t id, const uint8_t *data, uint8_t len);

/**
 * @brief Set the bitrate and mode, then start the controller.
 *
 * @param bitrate  125000, 250000, 500000 or 1000000 (any value the hardware can make)
 * @param loopback true: frames sent are received by this node and do not reach the wire
 *
 * @retval 0       Success
 * @retval -ENODEV No CAN controller (CONFIG_CAN off, or zephyr,canbus missing / not ready)
 * @retval other   Negative errno from the CAN driver (for example -EINVAL, unusable bitrate)
 */
int driver_can_init(uint32_t bitrate, bool loopback);

/**
 * @brief Send a standard frame (11 bit id, at most 0x7FF), 0..8 data bytes.
 * Waits up to 100 ms for a free transmit mailbox.
 *
 * @retval 0        Queued
 * @retval -EINVAL  id above 0x7FF, len above 8, or data NULL with len > 0
 * @retval -EACCES  driver_can_init() not called
 * @retval -EAGAIN  No free mailbox within 100 ms
 * @retval -ENETDOWN Controller stopped or bus-off
 */
int driver_can_send(uint32_t id, const uint8_t *data, uint8_t len);

/** @brief Send an extended frame (29 bit id, at most 0x1FFFFFFF). Return values as above. */
int driver_can_send_ext(uint32_t id, const uint8_t *data, uint8_t len);

/**
 * @brief Receive standard frames with (rx_id & mask) == (id & mask).
 *
 * mask 0x7FF matches exactly one id, mask 0 matches every id.
 *
 * @retval 0        Added
 * @retval -EINVAL  id or mask above 0x7FF, or cb NULL
 * @retval -EACCES  driver_can_init() not called
 * @retval -ENOSPC  DRIVER_CAN_MAX_FILTERS filters already in use
 */
int driver_can_add_rx(uint32_t id, uint32_t mask, driver_can_rx_cb_t cb);

/**
 * @brief Controller state.
 * @retval 0         Normal (including error-active and error-passive)
 * @retval -ENETDOWN Bus-off or stopped
 * @retval -ENODEV   No CAN controller
 */
int driver_can_get_state(void);

/**
 * @brief Heartbeat state callback.
 *
 * Called from the system workqueue (not from an interrupt) when the state changes.
 *
 * @param alive true when a heartbeat arrived, false when none arrived within the timeout
 *              (also called with false once if the very first heartbeat never comes)
 */
typedef void (*driver_can_hb_cb_t)(bool alive);

/**
 * @brief Watch a heartbeat frame (fail-safe pattern: lose the master, go to a safe state).
 *
 * Adds a receive filter for the standard id @p id (one filter slot) and starts a timeout.
 * Every frame with that id restarts the timeout. @p cb is called with false when the timeout
 * expires and with true when the heartbeat comes back. Only one watch is supported.
 *
 * @param id         Heartbeat id, at most 0x7FF
 * @param timeout_ms Timeout in ms, at least 1
 * @param cb         State callback, not NULL
 *
 * @retval 0        Watching
 * @retval -EINVAL  id above 0x7FF, timeout_ms 0, or cb NULL
 * @retval -EACCES  driver_can_init() not called
 * @retval -EALREADY A watch is already running
 * @retval -ENOSPC  DRIVER_CAN_MAX_FILTERS filters already in use
 */
int driver_can_heartbeat_watch(uint32_t id, uint32_t timeout_ms, driver_can_hb_cb_t cb);

/**
 * @brief Current heartbeat state.
 * @return 1 alive, 0 lost or no heartbeat seen yet, -EACCES when no watch is running
 */
int driver_can_heartbeat_alive(void);

#endif /* DRIVER_CAN_H_ */

/**
 * @file driver_ethernet.h
 * @brief Wired network through the W5500 (SPI1): link state, DHCP or static IPv4.
 */
#ifndef DRIVER_ETHERNET_H_
#define DRIVER_ETHERNET_H_

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/kernel.h>

/** Smallest buffer for driver_ethernet_get_ip() ("255.255.255.255" + NUL). */
#define DRIVER_ETHERNET_IP_STR_LEN 16

/**
 * @brief Bring the interface up and start DHCP.
 * @retval 0       Started (the address arrives later, see driver_ethernet_wait_ip())
 * @retval -ENODEV No network interface (CONFIG_NETWORKING / W5500 not enabled)
 */
int driver_ethernet_init(void);

/**
 * @brief Wait until an IPv4 address is configured.
 * @retval 0       Address available
 * @retval -EAGAIN Timeout
 * @retval -ENODEV Network not available
 */
int driver_ethernet_wait_ip(k_timeout_t timeout);

/** @brief true when the interface has an IPv4 address. */
bool driver_ethernet_has_ip(void);

/** @brief true when the cable is plugged in and the link is up. */
bool driver_ethernet_link_up(void);

/**
 * @brief The IPv4 address as text, for example "192.168.1.57".
 * @param buf Output
 * @param len Size of @p buf, at least DRIVER_ETHERNET_IP_STR_LEN
 * @retval 0       Success
 * @retval -EINVAL buf NULL or len too small
 * @retval -ENOENT No address yet
 */
int driver_ethernet_get_ip(char *buf, size_t len);

/**
 * @brief Use a fixed address instead of DHCP. Call it instead of waiting for DHCP.
 * @retval 0       Success
 * @retval -EINVAL A string is not a valid IPv4 address
 * @retval -ENODEV Network not available
 */
int driver_ethernet_set_static(const char *ip, const char *mask, const char *gw);

#endif /* DRIVER_ETHERNET_H_ */

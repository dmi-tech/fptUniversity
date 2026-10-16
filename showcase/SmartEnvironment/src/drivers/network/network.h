#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

/* Bring up the W5500 Ethernet interface in a background thread and get an
 * IPv4 address, per g_cfg: the saved static IP, or DHCP — wait for the cable
 * (no limit), then up to 30 s for a lease, else the saved static IP.
 * Returns at once; fills g_device_ip and g_device_mac when ready. */
void network_start(void);

/* Link up (cable + PHY) and an address assigned */
bool network_eth_connected(void);

/* Refresh g_device_ip from the interface (e.g. after a DHCP renewal) */
void network_update_ip(void);

#endif /* NETWORK_H */

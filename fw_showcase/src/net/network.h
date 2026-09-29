#ifndef NETWORK_H
#define NETWORK_H

/* Bring up the W5500 Ethernet interface and get an IPv4 address: DHCP (up to
 * 30 s, falls back to the saved static IP) or the saved static IP, per g_cfg.
 * Blocks until an address is set or the timeouts expire; fills g_device_ip
 * and g_device_mac. */
void network_start(void);

/* Refresh g_device_ip from the interface (e.g. after a DHCP renewal) */
void network_update_ip(void);

#endif /* NETWORK_H */

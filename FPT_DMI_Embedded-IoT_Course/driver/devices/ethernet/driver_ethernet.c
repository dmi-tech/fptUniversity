/**
 * @file driver_ethernet.c
 * @brief Network bring-up on top of the Zephyr net_if, DHCPv4 and net_mgmt APIs.
 */
#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "driver_ethernet.h"

#if IS_ENABLED(CONFIG_NETWORKING) && IS_ENABLED(CONFIG_NET_IPV4)

#include <zephyr/net/net_if.h> /* must come before dhcpv4.h */
#include <zephyr/net/net_ip.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>

static struct net_mgmt_event_callback ip_cb;
static K_SEM_DEFINE(ip_sem, 0, 1);
static bool started;

static void ip_event(struct net_mgmt_event_callback *cb, uint64_t event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);
	if (event == NET_EVENT_IPV4_ADDR_ADD) {
		k_sem_give(&ip_sem);
	}
}

/* The global (non link-local) address of the interface, or NULL */
static const void *global_addr(void)
{
	struct net_if *iface = net_if_get_default();

	if (iface == NULL) {
		return NULL;
	}
	return net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
}

int driver_ethernet_init(void)
{
	struct net_if *iface = net_if_get_default();

	if (iface == NULL) {
		return -ENODEV;
	}
	if (started) {
		return 0;
	}

	net_mgmt_init_event_callback(&ip_cb, ip_event, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ip_cb);

	net_if_up(iface);
	if (IS_ENABLED(CONFIG_NET_DHCPV4)) {
		net_dhcpv4_start(iface);
	}
	started = true;
	return 0;
}

int driver_ethernet_wait_ip(k_timeout_t timeout)
{
	if (net_if_get_default() == NULL) {
		return -ENODEV;
	}
	if (global_addr() != NULL) {
		return 0;
	}
	/* The event may arrive before the wait, so check again after each wake-up */
	while (k_sem_take(&ip_sem, timeout) == 0) {
		if (global_addr() != NULL) {
			return 0;
		}
	}
	return global_addr() != NULL ? 0 : -EAGAIN;
}

bool driver_ethernet_has_ip(void)
{
	return global_addr() != NULL;
}

bool driver_ethernet_link_up(void)
{
	struct net_if *iface = net_if_get_default();

	return iface != NULL && net_if_is_up(iface) && net_if_is_carrier_ok(iface);
}

int driver_ethernet_get_ip(char *buf, size_t len)
{
	const void *addr;

	if (buf == NULL || len < DRIVER_ETHERNET_IP_STR_LEN) {
		return -EINVAL;
	}
	addr = global_addr();
	if (addr == NULL) {
		return -ENOENT;
	}
	return net_addr_ntop(NET_AF_INET, addr, buf, len) != NULL ? 0 : -ENOENT;
}

int driver_ethernet_set_static(const char *ip, const char *mask, const char *gw)
{
	struct net_if *iface = net_if_get_default();
	/* Same type as the addresses of the net API, whatever its name in this Zephyr version */
	__typeof__(*net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED)) addr, nm, gateway;

	if (ip == NULL || mask == NULL || gw == NULL) {
		return -EINVAL;
	}
	if (net_addr_pton(NET_AF_INET, ip, &addr) < 0 || net_addr_pton(NET_AF_INET, mask, &nm) < 0 ||
	    net_addr_pton(NET_AF_INET, gw, &gateway) < 0) {
		return -EINVAL;
	}
	if (iface == NULL) {
		return -ENODEV;
	}

	if (!started) {
		net_mgmt_init_event_callback(&ip_cb, ip_event, NET_EVENT_IPV4_ADDR_ADD);
		net_mgmt_add_event_callback(&ip_cb);
		net_if_up(iface);
		started = true;
	}
	if (IS_ENABLED(CONFIG_NET_DHCPV4)) {
		net_dhcpv4_stop(iface);
	}
	if (net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0) == NULL) {
		return -ENOMEM;
	}
	net_if_ipv4_set_netmask_by_addr(iface, &addr, &nm);
	net_if_ipv4_set_gw(iface, &gateway);
	return 0;
}

#else /* networking not enabled */

int driver_ethernet_init(void)
{
	return -ENODEV;
}

int driver_ethernet_wait_ip(k_timeout_t timeout)
{
	ARG_UNUSED(timeout);
	return -ENODEV;
}

bool driver_ethernet_has_ip(void)
{
	return false;
}

bool driver_ethernet_link_up(void)
{
	return false;
}

int driver_ethernet_get_ip(char *buf, size_t len)
{
	if (buf == NULL || len < DRIVER_ETHERNET_IP_STR_LEN) {
		return -EINVAL;
	}
	return -ENOENT;
}

int driver_ethernet_set_static(const char *ip, const char *mask, const char *gw)
{
	if (ip == NULL || mask == NULL || gw == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

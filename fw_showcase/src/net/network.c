/* network.c — W5500 Ethernet bring-up, DHCP or static IPv4 */
#include "network.h"
#include "app_config.h"
#include "app_state.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/dhcpv4.h>
#include <stdio.h>
#include <string.h>

#define DHCP_TIMEOUT_MS 30000   /* after link up, then static fallback */

/* Apply the static IP / netmask / gateway saved in config */
static void apply_static_ip(struct net_if *iface)
{
    struct in_addr addr, nm, gw;

    memcpy(addr.s4_addr, g_cfg.static_ip, 4);
    memcpy(nm.s4_addr,   g_cfg.subnet,    4);
    memcpy(gw.s4_addr,   g_cfg.gateway,   4);
    net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
    net_if_ipv4_set_netmask_by_addr(iface, &addr, &nm);
    net_if_ipv4_set_gw(iface, &gw);
}

void network_update_ip(void)
{
    struct net_if *iface = net_if_get_default();
    if (!iface) return;
    struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
    if (!ipv4) return;

    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        /* Zephyr 4.x: unicast[] entries are struct net_if_addr_ipv4,
         * the actual net_if_addr is nested under .ipv4 */
        if (ipv4->unicast[i].ipv4.is_used) {
            char tmp[16];
            net_addr_ntop(AF_INET, &ipv4->unicast[i].ipv4.address.in_addr,
                          tmp, sizeof(tmp));
            strncpy(g_device_ip, tmp, sizeof(g_device_ip) - 1);
            return;
        }
    }
}

/* Publish the real interface MAC for the web UI */
static void read_mac(struct net_if *iface)
{
    struct net_linkaddr *la = net_if_get_link_addr(iface);

    if (la && la->len == 6) {
        snprintf(g_device_mac, sizeof(g_device_mac),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 la->addr[0], la->addr[1], la->addr[2],
                 la->addr[3], la->addr[4], la->addr[5]);
        printk("[NET] MAC = %s\n", g_device_mac);
    }
}

bool network_eth_connected(void)
{
    struct net_if *iface = net_if_get_default();

    return iface && net_if_is_up(iface) && app_state_has_ip();
}

/* Static: address applied at once. DHCP: wait for the cable (no limit),
 * then up to DHCP_TIMEOUT_MS for a lease, else the saved static address. */
static void net_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    struct net_if *iface = net_if_get_default();
    if (!iface) { printk("[NET] no interface\n"); return; }

    net_if_up(iface);
    read_mac(iface);

    if (g_cfg.ip_mode == 0) {
        printk("[NET] DHCP mode — waiting for link...\n");
        net_dhcpv4_start(iface);
        while (!net_if_is_up(iface)) {
            k_msleep(100);
        }
        printk("[NET] link up — requesting lease...\n");
        for (int i = 0; i < DHCP_TIMEOUT_MS / 100; i++) {
            network_update_ip();
            if (app_state_has_ip()) break;
            k_msleep(100);
        }
        if (!app_state_has_ip()) {
            printk("[NET] DHCP got no lease — falling back to static\n");
            net_dhcpv4_stop(iface);
            apply_static_ip(iface);
        }
    } else {
        printk("[NET] Static IP mode\n");
        apply_static_ip(iface);
    }

    for (int i = 0; i < 15; i++) {                    /* settle */
        network_update_ip();
        if (app_state_has_ip()) break;
        k_msleep(200);
    }
    printk("[NET] IP = %s  (%s)\n", g_device_ip,
           g_cfg.ip_mode ? "static" : "dhcp");
    printk("[APP] Running — http://%s/\n\n", g_device_ip);
}

K_THREAD_STACK_DEFINE(net_stack, 2048);
static struct k_thread net_thread_data;

void network_start(void)
{
    k_thread_create(&net_thread_data, net_stack, K_THREAD_STACK_SIZEOF(net_stack),
                    net_thread, NULL, NULL, NULL, 6, 0, K_NO_WAIT);
    k_thread_name_set(&net_thread_data, "net_up");
}

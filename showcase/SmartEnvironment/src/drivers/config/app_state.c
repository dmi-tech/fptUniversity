/* app_state.c — network state shared between the main loop, web and MQTT */
#include "app_state.h"
#include <string.h>

char g_device_ip[16]  = "0.0.0.0";
char g_device_mac[18] = "00:08:DC:01:02:03";

bool app_state_has_ip(void)
{
    return strcmp(g_device_ip, "0.0.0.0") != 0;
}

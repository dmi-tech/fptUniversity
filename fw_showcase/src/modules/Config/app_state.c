/* app_state.c — runtime state shared between the main loop, web and MQTT */
#include "app_state.h"
#include <string.h>

char g_device_ip[16]  = "0.0.0.0";
char g_device_mac[18] = "00:08:DC:01:02:03";

volatile int32_t  g_web_temp_mc;
volatile int32_t  g_web_humi_mp;
volatile uint32_t g_web_cnt;
volatile bool     g_web_sensor_ok;
volatile bool     g_web_have_data;

bool app_state_has_ip(void)
{
    return strcmp(g_device_ip, "0.0.0.0") != 0;
}

void app_state_update(int temp_c, int humi_pct, bool sensor_ok, bool have_data)
{
    g_web_temp_mc   = temp_c * 1000;
    g_web_humi_mp   = humi_pct * 1000;
    g_web_sensor_ok = sensor_ok;
    g_web_have_data = have_data;
    g_web_cnt++;
}

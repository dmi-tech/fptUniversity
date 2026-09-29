/* rs485.c — transmit-only RS485 on UART4 with the DE/RE line on PC3 */
#include "rs485.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define RS485_DIR_PIN 3   /* PC3 */

static const struct device *const s_uart = DEVICE_DT_GET(DT_NODELABEL(uart4));
static const struct device *const s_dir  = DEVICE_DT_GET(DT_NODELABEL(gpioc));
static K_MUTEX_DEFINE(s_lock);
static bool s_ready;

bool rs485_is_ready(void) { return s_ready; }

bool rs485_init(void)
{
    if (!device_is_ready(s_uart) || !device_is_ready(s_dir)) {
        printk("[RS485] UART4/PC3 not ready\n");
        return false;
    }
    if (gpio_pin_configure(s_dir, RS485_DIR_PIN, GPIO_OUTPUT_LOW)) {
        printk("[RS485] PC3 configure failed\n");
        return false;
    }
    s_ready = true;
    printk("[RS485] UART4 PA0/PA1, DIR PC3 OK\n");
    return true;
}

void rs485_send(const char *s)
{
    if (!s_ready) return;
    size_t len = strlen(s);

    k_mutex_lock(&s_lock, K_FOREVER);
    gpio_pin_set(s_dir, RS485_DIR_PIN, 1);
    k_busy_wait(50);
    for (size_t i = 0; i < len; i++) uart_poll_out(s_uart, (uint8_t)s[i]);
    /* poll_out returns once the byte is queued: wait for the last frame
     * (10 bits @9600 = ~1.05 ms per byte) before releasing the bus. */
    k_usleep((uint32_t)len * 1100 + 500);
    gpio_pin_set(s_dir, RS485_DIR_PIN, 0);
    k_mutex_unlock(&s_lock);
    printk("[RS485] TX %s", s);
}

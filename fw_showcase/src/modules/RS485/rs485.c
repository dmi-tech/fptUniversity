/* rs485.c — half-duplex RS485 on UART4 with the DE/RE line on PC3.
 * TX is polled; RX is interrupt driven. A line ends on CR/LF or, for senders
 * that add no terminator, after RS485_IDLE_MS without a new byte. */
#include "rs485.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define RS485_DIR_PIN 3   /* PC3 */
#define RS485_IDLE_MS 50  /* gap that ends a line without CR/LF (a byte takes ~1 ms) */

static const struct device *const s_uart = DEVICE_DT_GET(DT_NODELABEL(uart4));
static const struct device *const s_dir  = DEVICE_DT_GET(DT_NODELABEL(gpioc));
static K_MUTEX_DEFINE(s_lock);
static bool s_ready;

/* RX line assembly: touched by the UART ISR, the idle timer and rs485_send(),
 * always with interrupts locked */
static char   s_line[RS485_LINE_MAX];
static size_t s_len;
static bool   s_overflow;
static struct k_timer s_idle;
K_MSGQ_DEFINE(s_rxq, RS485_LINE_MAX, 4, 1);

bool rs485_is_ready(void) { return s_ready; }

/* Interrupts locked */
static void rx_reset(void)
{
    s_len = 0;
    s_overflow = false;
}

/* Interrupts locked: hand the collected line over and start a new one */
static void rx_flush(void)
{
    if (s_len > 0 && !s_overflow) {
        s_line[s_len] = '\0';
        k_msgq_put(&s_rxq, s_line, K_NO_WAIT);   /* dropped when full */
    }
    rx_reset();
}

static void idle_expiry(struct k_timer *t)
{
    ARG_UNUSED(t);
    unsigned int key = irq_lock();
    rx_flush();
    irq_unlock(key);
}

static void rx_byte(uint8_t c)
{
    unsigned int key = irq_lock();

    if (c == '\r' || c == '\n') {
        k_timer_stop(&s_idle);
        rx_flush();
    } else {
        if (s_len < RS485_LINE_MAX - 1) s_line[s_len++] = (char)c;
        else                            s_overflow = true;
        k_timer_start(&s_idle, K_MSEC(RS485_IDLE_MS), K_NO_WAIT);
    }
    irq_unlock(key);
}

static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);
    uint8_t buf[16];

    uart_irq_update(dev);
    while (uart_irq_rx_ready(dev)) {
        int n = uart_fifo_read(dev, buf, sizeof(buf));
        if (n <= 0) break;
        for (int i = 0; i < n; i++) rx_byte(buf[i]);
    }
}

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
    k_timer_init(&s_idle, idle_expiry, NULL);
    if (uart_irq_callback_user_data_set(s_uart, uart_isr, NULL)) {
        printk("[RS485] UART4 IRQ setup failed\n");
        return false;
    }
    uart_irq_rx_enable(s_uart);
    s_ready = true;
    printk("[RS485] UART4 PA0/PA1, DIR PC3 OK (TX + RX)\n");
    return true;
}

void rs485_send(const char *s)
{
    if (!s_ready) return;
    size_t len = strlen(s);
    uint8_t c;

    k_mutex_lock(&s_lock, K_FOREVER);
    uart_irq_rx_disable(s_uart);
    gpio_pin_set(s_dir, RS485_DIR_PIN, 1);
    k_busy_wait(50);
    for (size_t i = 0; i < len; i++) uart_poll_out(s_uart, (uint8_t)s[i]);
    /* poll_out returns once the byte is queued: wait for the last frame
     * (10 bits @9600 = ~1.05 ms per byte) before releasing the bus. */
    k_usleep((uint32_t)len * 1100 + 500);
    gpio_pin_set(s_dir, RS485_DIR_PIN, 0);
    /* The receiver is off while DE/RE is high: drop whatever the floating
     * RX line produced and restart the line assembly. */
    while (uart_poll_in(s_uart, &c) == 0) {
    }
    unsigned int key = irq_lock();
    k_timer_stop(&s_idle);
    rx_reset();
    irq_unlock(key);
    uart_irq_rx_enable(s_uart);
    k_mutex_unlock(&s_lock);
    printk("[RS485] TX %s", s);
}

int rs485_read_line(char out[RS485_LINE_MAX], k_timeout_t timeout)
{
    return k_msgq_get(&s_rxq, out, timeout);
}

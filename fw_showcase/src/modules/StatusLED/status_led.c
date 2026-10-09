/* status_led.c — LED LIFE (PA8): heartbeat double blink */
#include "status_led.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec s_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static uint32_t s_step;
static int64_t  s_next;

bool status_led_init(void)
{
    if (!gpio_is_ready_dt(&s_led)) return false;
    gpio_pin_configure_dt(&s_led, GPIO_OUTPUT_INACTIVE);
    return true;
}

void status_led_set(bool on) { gpio_pin_set_dt(&s_led, on); }
void status_led_toggle(void) { gpio_pin_toggle_dt(&s_led); }

void status_led_heartbeat_start(void)
{
    s_step = 0;
    s_next = k_uptime_get();
}

/* on 80 ms, off 80 ms, on 80 ms, off 760 ms -> one double blink per second */
void status_led_heartbeat(void)
{
    static const struct { bool on; uint16_t ms; } seq[] = {
        { true, 80 }, { false, 80 }, { true, 80 }, { false, 760 },
    };
    int64_t now = k_uptime_get();

    if (now < s_next) return;
    status_led_set(seq[s_step].on);
    s_next = now + seq[s_step].ms;
    s_step = (s_step + 1) % ARRAY_SIZE(seq);
}

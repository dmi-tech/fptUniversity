/* motor.c — motor output (Relay 3, PA15) shared by the web page, RS485, the PA10
 * button and the alarm (ownership rules in motor.h) */
#include "motor.h"
#include "mqtt_app.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define BUTTON_POLL_MS   10
#define BUTTON_INTEG_MAX 5    /* x BUTTON_POLL_MS = 50 ms to change level */

static const struct gpio_dt_spec s_motor  = GPIO_DT_SPEC_GET(DT_ALIAS(motor0), gpios);
static const struct gpio_dt_spec s_button = GPIO_DT_SPEC_GET(DT_NODELABEL(user_btn), gpios);

static K_MUTEX_DEFINE(s_lock);
static bool           s_on;
static enum motor_src s_owner;      /* who started it, NONE while off */
static enum motor_src s_last_src;   /* who changed it last */

static struct k_work_delayable s_btn_work;

/* Caller holds s_lock. Returns true when the output changed. */
static bool apply(bool on, enum motor_src src)
{
    bool changed = (on != s_on);

    s_on       = on;
    s_owner    = on ? src : MOTOR_SRC_NONE;
    s_last_src = src;
    gpio_pin_set_dt(&s_motor, on);
    return changed;
}

static void report(bool changed, bool on, enum motor_src src)
{
    printk("[MOTOR] %s (%s)\n", on ? "ON" : "OFF", motor_src_str(src));
    /* Push the new state to MQTT at once; the web page polls it */
    if (changed) mqtt_app_publish_now();
}

void motor_set(bool on, enum motor_src src)
{
    k_mutex_lock(&s_lock, K_FOREVER);
    bool changed = apply(on, src);
    k_mutex_unlock(&s_lock);
    report(changed, on, src);
}

void motor_alarm_start(void)
{
    k_mutex_lock(&s_lock, K_FOREVER);
    bool changed = !s_on && apply(true, MOTOR_SRC_ALARM);
    k_mutex_unlock(&s_lock);
    if (changed) report(true, true, MOTOR_SRC_ALARM);
}

void motor_alarm_clear(void)
{
    k_mutex_lock(&s_lock, K_FOREVER);
    bool changed = s_on && s_owner == MOTOR_SRC_ALARM && apply(false, MOTOR_SRC_ALARM);
    k_mutex_unlock(&s_lock);
    if (changed) report(true, false, MOTOR_SRC_ALARM);
}

bool           motor_is_on(void)    { return s_on; }
enum motor_src motor_last_src(void) { return s_last_src; }

const char *motor_src_str(enum motor_src src)
{
    switch (src) {
    case MOTOR_SRC_ALARM:  return "alarm";
    case MOTOR_SRC_BUTTON: return "button";
    case MOTOR_SRC_WEB:    return "web";
    case MOTOR_SRC_RS485:  return "RS485";
    default:               return "";
    }
}

/* ── Button PA10: one click = press then release ─────────────── */
/* Polled, not interrupt driven: a running brushed motor couples noise into
 * PA10, and edge interrupts then fire continuously. Every BUTTON_POLL_MS one
 * sample moves an integrator up (pressed) or down (released); the level only
 * counts as changed once the integrator reaches an end, so short spikes are
 * absorbed. A click is a stable press followed by a stable release. */
static uint8_t s_btn_integ;   /* 0 = released .. BUTTON_INTEG_MAX = pressed */
static bool    s_btn_down;    /* last stable level, true = pressed */
static bool    s_btn_skip;    /* held since boot: its release is not a click */

static void btn_work(struct k_work *w)
{
    ARG_UNUSED(w);
    k_work_schedule(&s_btn_work, K_MSEC(BUTTON_POLL_MS));

    if (gpio_pin_get_dt(&s_button) > 0) {
        if (s_btn_integ < BUTTON_INTEG_MAX) s_btn_integ++;
    } else if (s_btn_integ > 0) {
        s_btn_integ--;
    }

    if (!s_btn_down && s_btn_integ == BUTTON_INTEG_MAX) {
        s_btn_down = true;           /* pressed: wait for the release */
        return;
    }
    if (!s_btn_down || s_btn_integ != 0) return;
    s_btn_down = false;
    if (s_btn_skip) { s_btn_skip = false; return; }

    k_mutex_lock(&s_lock, K_FOREVER);
    bool on = !s_on;
    apply(on, MOTOR_SRC_BUTTON);
    k_mutex_unlock(&s_lock);
    report(true, on, MOTOR_SRC_BUTTON);
}

bool motor_init(void)
{
    if (!gpio_is_ready_dt(&s_motor) || !gpio_is_ready_dt(&s_button)) {
        printk("[MOTOR] motor/button GPIO not ready\n");
        return false;
    }
    gpio_pin_configure_dt(&s_motor, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&s_button, GPIO_INPUT);

    /* Held at boot: start as pressed and swallow that release */
    s_btn_down  = (gpio_pin_get_dt(&s_button) > 0);
    s_btn_skip  = s_btn_down;
    s_btn_integ = s_btn_down ? BUTTON_INTEG_MAX : 0;
    k_work_init_delayable(&s_btn_work, btn_work);
    k_work_schedule(&s_btn_work, K_MSEC(BUTTON_POLL_MS));

    printk("[MOTOR] MOTOR RL3 PA15, BTN PA10 OK\n");
    return true;
}

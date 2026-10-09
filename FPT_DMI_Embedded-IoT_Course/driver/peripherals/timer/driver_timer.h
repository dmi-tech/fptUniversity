/**
 * @file driver_timer.h
 * @brief Software timers (k_timer running callbacks in a thread), stopwatch, and a
 *        one-shot hardware timer (TIM5 through the Counter API).
 */
#ifndef DRIVER_TIMER_H_
#define DRIVER_TIMER_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

/**
 * @brief Callback of a timer.
 * @param user_data Pointer given when the timer was started
 */
typedef void (*driver_timer_cb_t)(void *user_data);

/**
 * @brief One software timer. Declare it static or global, never as a local variable.
 *
 * The fields are private. A zero-initialised variable is ready to use.
 */
struct driver_timer {
	struct k_timer timer;
	struct k_work work;
	driver_timer_cb_t cb;
	void *user_data;
	bool initialised;
};

/** @brief Start time of a stopwatch. The field is private. */
struct driver_stopwatch {
	uint32_t start_cycles;
};

/**
 * @brief Start (or restart) a software timer.
 *
 * The callback runs in the system workqueue thread, so it may use printk and talk to
 * sensors, but it should not block longer than the period.
 *
 * @param t         Timer variable
 * @param period_ms Period (periodic) or delay (one shot) in milliseconds, at least 1
 * @param periodic  true: repeat every period_ms, false: run once
 * @param cb        Function to call
 * @param user_data Passed to @p cb
 *
 * @retval 0       Success
 * @retval -EINVAL NULL argument or period_ms is 0
 */
int driver_timer_start(struct driver_timer *t, uint32_t period_ms, bool periodic,
		       driver_timer_cb_t cb, void *user_data);

/**
 * @brief Stop a timer. Safe to call on a timer that is not running.
 * @retval 0       Success
 * @retval -EINVAL NULL argument
 */
int driver_timer_stop(struct driver_timer *t);

/** @brief true while the timer is counting down. */
bool driver_timer_is_running(const struct driver_timer *t);

/** @brief Remember the current time. */
void driver_timer_stopwatch_start(struct driver_stopwatch *sw);

/**
 * @brief Microseconds elapsed since driver_timer_stopwatch_start().
 *
 * Resolution is one CPU cycle; the value wraps after about 17 seconds at 250 MHz.
 */
uint32_t driver_timer_stopwatch_us(const struct driver_stopwatch *sw);

/**
 * @brief Initialise and start the hardware timer (alias hwtimer0, TIM5 at 1 MHz).
 *
 * @retval 0       Success
 * @retval -ENODEV Alias hwtimer0 missing, CONFIG_COUNTER disabled, or device not ready
 */
int driver_timer_hw_init(void);

/**
 * @brief Call @p cb once after @p us microseconds. Only one alarm can be pending.
 *
 * The callback runs in interrupt context: do not call printk with long text, k_msleep,
 * I2C or SPI there. Set a flag, toggle a GPIO or k_sem_give().
 *
 * @retval 0       Success
 * @retval -EINVAL cb is NULL or us is too large for the counter
 * @retval -ENODEV driver_timer_hw_init() not done or hardware timer missing
 * @retval -EBUSY  An alarm is already pending
 */
int driver_timer_hw_alarm_us(uint32_t us, driver_timer_cb_t cb, void *user_data);

/**
 * @brief Current value of the hardware timer in microseconds.
 * @return Microseconds (wraps after about 71 minutes), 0 if the timer is not available
 */
uint32_t driver_timer_hw_now_us(void);

#endif /* DRIVER_TIMER_H_ */

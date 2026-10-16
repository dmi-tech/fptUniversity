/**
 * @file driver_lm393.c
 * @brief LM393 comparator modules: DO with fast detection and a hold time, optional AO.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include "driver_lm393.h"

#define USER_NODE DT_PATH(zephyr_user)

#define AO_RAW_MAX 4095
#define AO_VREF_MV 3300
#define HOLD_MS_MAX 60000

/* DO of module <idx>: entry <idx> of zephyr,user lm393-gpios, or an empty spec. */
#define DO_SPEC(idx) GPIO_DT_SPEC_GET_BY_IDX_OR(USER_NODE, lm393_gpios, idx, {0})

/* AO of module <n>: the io-channels entry named "lm393_<n>" (only with CONFIG_ADC). */
#define HAS_AO(n) (IS_ENABLED(CONFIG_ADC) && DT_PROP_HAS_NAME(USER_NODE, io_channels, lm393_##n))

struct lm393 {
	struct gpio_dt_spec spec;
	driver_lm393_cb_t cb;
	struct gpio_callback gpio_cb;
	struct k_work_delayable work;
	atomic_t seen_active; /* DO was active at an edge since the last work run */
	uint32_t hold_ms;
	uint8_t id;
	bool ready;
	bool reported;
};

static struct lm393 modules[DRIVER_LM393_MAX] = {
	{.spec = DO_SPEC(0), .id = 0, .hold_ms = DRIVER_LM393_HOLD_MS_DEFAULT},
	{.spec = DO_SPEC(1), .id = 1, .hold_ms = DRIVER_LM393_HOLD_MS_DEFAULT},
};

/* ---- Digital output DO -------------------------------------------------------------- */

/*
 * Runs in the system workqueue. "Detected" is reported at once; "not detected" only after
 * a full hold time without any active edge and with DO inactive.
 */
static void lm393_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct lm393 *m = CONTAINER_OF(dwork, struct lm393, work);
	bool seen = atomic_clear(&m->seen_active) != 0;
	int level = gpio_pin_get_dt(&m->spec);

	if (level < 0) {
		return;
	}
	if (!m->reported) {
		if (seen || level > 0) {
			m->reported = true;
			if (m->cb != NULL) {
				m->cb(m->id, true);
			}
			k_work_reschedule(&m->work, K_MSEC(m->hold_ms));
		}
		return;
	}
	if (level == 0 && !seen) {
		m->reported = false;
		if (m->cb != NULL) {
			m->cb(m->id, false);
		}
	} else {
		/* Still active (or active again): check again after the hold time */
		k_work_reschedule(&m->work, K_MSEC(m->hold_ms));
	}
}

/* Interrupt context: note an active level, then let the work item decide. */
static void lm393_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	struct lm393 *m = CONTAINER_OF(cb, struct lm393, gpio_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);
	if (gpio_pin_get_dt(&m->spec) > 0) {
		atomic_set(&m->seen_active, 1);
	}
	k_work_reschedule(&m->work, m->reported ? K_MSEC(m->hold_ms) : K_NO_WAIT);
}

int driver_lm393_init(uint8_t id, driver_lm393_cb_t cb)
{
	struct lm393 *m;
	int ret;

	if (id >= DRIVER_LM393_MAX) {
		return -EINVAL;
	}
	m = &modules[id];
	if (m->spec.port == NULL || !gpio_is_ready_dt(&m->spec)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&m->spec, GPIO_INPUT);
	if (ret < 0) {
		return ret;
	}

	m->cb = cb;
	k_work_init_delayable(&m->work, lm393_work_handler);
	atomic_clear(&m->seen_active);
	m->reported = gpio_pin_get_dt(&m->spec) > 0;

	if (cb != NULL) {
		gpio_init_callback(&m->gpio_cb, lm393_isr, BIT(m->spec.pin));
		ret = gpio_add_callback(m->spec.port, &m->gpio_cb);
		if (ret < 0) {
			return ret;
		}
		ret = gpio_pin_interrupt_configure_dt(&m->spec, GPIO_INT_EDGE_BOTH);
		if (ret < 0) {
			gpio_remove_callback(m->spec.port, &m->gpio_cb);
			return ret;
		}
		if (m->reported) {
			/* Already detecting at start: watch for the release */
			k_work_reschedule(&m->work, K_MSEC(m->hold_ms));
		}
	}

	m->ready = true;
	return 0;
}

int driver_lm393_get(uint8_t id)
{
	if (id >= DRIVER_LM393_MAX) {
		return -EINVAL;
	}
	if (modules[id].spec.port == NULL) {
		return -ENODEV;
	}
	if (!modules[id].ready) {
		return -EACCES;
	}
	return gpio_pin_get_dt(&modules[id].spec);
}

int driver_lm393_set_hold(uint8_t id, uint32_t ms)
{
	if (id >= DRIVER_LM393_MAX || ms == 0 || ms > HOLD_MS_MAX) {
		return -EINVAL;
	}
	modules[id].hold_ms = ms;
	return 0;
}

/* ---- Analog output AO --------------------------------------------------------------- */

#if HAS_AO(0) || HAS_AO(1)

struct lm393_ao {
	struct adc_dt_spec spec;
	bool present;
	bool ready;
};

static struct lm393_ao ao[DRIVER_LM393_MAX] = {
#if HAS_AO(0)
	{.spec = ADC_DT_SPEC_GET_BY_NAME(USER_NODE, lm393_0), .present = true},
#else
	{.present = false},
#endif
#if HAS_AO(1)
	{.spec = ADC_DT_SPEC_GET_BY_NAME(USER_NODE, lm393_1), .present = true},
#else
	{.present = false},
#endif
};

static K_MUTEX_DEFINE(ao_lock);

int driver_lm393_read_raw(uint8_t id, int16_t *raw)
{
	struct lm393_ao *a;
	int16_t buf;
	struct adc_sequence seq = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};
	int ret;

	if (id >= DRIVER_LM393_MAX || raw == NULL) {
		return -EINVAL;
	}
	a = &ao[id];
	if (!a->present || !adc_is_ready_dt(&a->spec)) {
		return -ENODEV;
	}

	k_mutex_lock(&ao_lock, K_FOREVER);
	ret = 0;
	if (!a->ready) {
		ret = adc_channel_setup_dt(&a->spec);
		a->ready = ret == 0;
	}
	if (ret == 0) {
		ret = adc_sequence_init_dt(&a->spec, &seq);
	}
	if (ret == 0) {
		ret = adc_read_dt(&a->spec, &seq);
	}
	k_mutex_unlock(&ao_lock);
	if (ret < 0) {
		return ret;
	}

	*raw = buf < 0 ? 0 : buf; /* single-ended: a negative value is noise around 0 */
	return 0;
}

#else /* no AO declared */

int driver_lm393_read_raw(uint8_t id, int16_t *raw)
{
	if (id >= DRIVER_LM393_MAX || raw == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

int driver_lm393_read_mv(uint8_t id, int32_t *mv)
{
	int16_t raw;
	int ret;

	if (mv == NULL) {
		return -EINVAL;
	}
	ret = driver_lm393_read_raw(id, &raw);
	if (ret < 0) {
		return ret;
	}
	*mv = (int32_t)raw * AO_VREF_MV / AO_RAW_MAX;
	return 0;
}

int driver_lm393_read_avg_mv(uint8_t id, uint8_t samples, int32_t *mv)
{
	int32_t sum = 0;

	if (mv == NULL || samples == 0 || samples > DRIVER_LM393_MAX_AVG) {
		return -EINVAL;
	}
	for (uint8_t i = 0; i < samples; i++) {
		int16_t raw;
		int ret = driver_lm393_read_raw(id, &raw);

		if (ret < 0) {
			return ret;
		}
		sum += raw;
	}
	*mv = sum / samples * AO_VREF_MV / AO_RAW_MAX;
	return 0;
}

int driver_lm393_read_percent(uint8_t id, uint8_t *percent)
{
	int16_t raw;
	int ret;

	if (percent == NULL) {
		return -EINVAL;
	}
	ret = driver_lm393_read_raw(id, &raw);
	if (ret < 0) {
		return ret;
	}
	*percent = (uint8_t)(((int32_t)raw * 100 + AO_RAW_MAX / 2) / AO_RAW_MAX);
	return 0;
}

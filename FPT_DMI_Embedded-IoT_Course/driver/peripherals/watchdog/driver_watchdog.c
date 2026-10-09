/**
 * @file driver_watchdog.c
 * @brief IWDG watchdog driver on top of the Zephyr watchdog and hwinfo APIs.
 */
#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "driver_watchdog.h"

#if IS_ENABLED(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif

#if IS_ENABLED(CONFIG_WATCHDOG) && DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(watchdog0))
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

static const struct device *const wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int wdt_channel = -1;

int driver_watchdog_init(uint32_t timeout_ms)
{
	struct wdt_timeout_cfg cfg = {
		.window.min = 0,
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};
	int ret;

	if (timeout_ms == 0 || timeout_ms > DRIVER_WATCHDOG_MAX_TIMEOUT_MS) {
		return -EINVAL;
	}
	if (!device_is_ready(wdt_dev)) {
		return -ENODEV;
	}
	if (wdt_channel >= 0) {
		return -EALREADY;
	}

	cfg.window.max = timeout_ms;
	ret = wdt_install_timeout(wdt_dev, &cfg);
	if (ret < 0) {
		return ret;
	}
	wdt_channel = ret;

	ret = wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (ret < 0) {
		wdt_channel = -1;
		return ret;
	}
	return 0;
}

int driver_watchdog_feed(void)
{
	if (wdt_channel < 0) {
		return -ENODEV;
	}
	return wdt_feed(wdt_dev, wdt_channel);
}

#else /* watchdog not available */

int driver_watchdog_init(uint32_t timeout_ms)
{
	if (timeout_ms == 0 || timeout_ms > DRIVER_WATCHDOG_MAX_TIMEOUT_MS) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_watchdog_feed(void)
{
	return -ENODEV;
}

#endif

bool driver_watchdog_caused_reset(void)
{
#if IS_ENABLED(CONFIG_HWINFO)
	static bool done;
	static bool by_watchdog;

	if (!done) {
		uint32_t cause = 0;

		if (hwinfo_get_reset_cause(&cause) == 0) {
			by_watchdog = (cause & RESET_WATCHDOG) != 0;
		}
		hwinfo_clear_reset_cause();
		done = true;
	}
	return by_watchdog;
#else
	return false;
#endif
}

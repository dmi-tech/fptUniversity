/**
 * @file driver_rtc.c
 * @brief RTC driver on top of the Zephyr RTC API.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include "driver_rtc.h"

#if IS_ENABLED(CONFIG_RTC) && DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(rtc0))

#include <zephyr/device.h>

static const struct device *const rtc_dev = DEVICE_DT_GET(DT_ALIAS(rtc0));

static bool is_leap(int year)
{
	return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int days_in_month(int year, int month)
{
	static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	return (month == 2 && is_leap(year)) ? 29 : days[month - 1];
}

/* Day of the week, 0 = Sunday (Sakamoto's method) */
static int day_of_week(int year, int month, int day)
{
	static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

	if (month < 3) {
		year--;
	}
	return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

int driver_rtc_init(void)
{
	struct rtc_time t;

	if (!device_is_ready(rtc_dev)) {
		return -ENODEV;
	}
	/* -ENODATA means the time has never been set */
	return rtc_get_time(rtc_dev, &t);
}

int driver_rtc_set(int year, int month, int day, int hour, int min, int sec)
{
	struct rtc_time t;

	if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 ||
	    day > days_in_month(year, month) || hour < 0 || hour > 23 || min < 0 || min > 59 ||
	    sec < 0 || sec > 59) {
		return -EINVAL;
	}
	if (!device_is_ready(rtc_dev)) {
		return -ENODEV;
	}

	memset(&t, 0, sizeof(t));
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = min;
	t.tm_sec = sec;
	t.tm_wday = day_of_week(year, month, day);
	t.tm_yday = -1;
	t.tm_isdst = -1;
	return rtc_set_time(rtc_dev, &t);
}

int driver_rtc_get(struct rtc_time *t)
{
	if (t == NULL) {
		return -EINVAL;
	}
	if (!device_is_ready(rtc_dev)) {
		return -ENODEV;
	}
	return rtc_get_time(rtc_dev, t);
}

#else /* RTC not available */

int driver_rtc_init(void)
{
	return -ENODEV;
}

int driver_rtc_set(int year, int month, int day, int hour, int min, int sec)
{
	if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
	    hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59) {
		return -EINVAL;
	}
	return -ENODEV;
}

int driver_rtc_get(struct rtc_time *t)
{
	if (t == NULL) {
		return -EINVAL;
	}
	return -ENODEV;
}

#endif

int driver_rtc_to_string(char *buf, size_t len)
{
	struct rtc_time t;
	int ret;

	if (buf == NULL || len < DRIVER_RTC_STRING_LEN) {
		return -EINVAL;
	}
	ret = driver_rtc_get(&t);
	if (ret < 0) {
		return ret;
	}
	snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon + 1,
		 t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	return 0;
}

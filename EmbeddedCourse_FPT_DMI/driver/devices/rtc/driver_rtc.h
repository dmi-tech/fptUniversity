/**
 * @file driver_rtc.h
 * @brief Real-time clock BM8563 (PCF8563 compatible) on I2C2, alias "rtc0".
 */
#ifndef DRIVER_RTC_H_
#define DRIVER_RTC_H_

#include <stddef.h>

#include <zephyr/drivers/rtc.h>

/** Smallest buffer accepted by driver_rtc_to_string() ("YYYY-MM-DD hh:mm:ss" + NUL). */
#define DRIVER_RTC_STRING_LEN 20

/**
 * @brief Check that the chip is ready and holds a valid time.
 *
 * @retval 0        Ready and time is valid
 * @retval -ENODATA The time has never been set (or the backup battery ran out)
 * @retval -ENODEV  Alias rtc0 missing, CONFIG_RTC off, or chip not ready
 */
int driver_rtc_init(void);

/**
 * @brief Set the date and time.
 *
 * @param year  2000..2099
 * @param month 1..12
 * @param day   1..31 (checked against the month and leap years)
 * @param hour  0..23
 * @param min   0..59
 * @param sec   0..59
 *
 * @retval 0       Success
 * @retval -EINVAL A value is out of range
 * @retval -ENODEV RTC not available
 */
int driver_rtc_set(int year, int month, int day, int hour, int min, int sec);

/**
 * @brief Read the time. Note: tm_year is the year minus 1900 and tm_mon is 0..11.
 * @retval 0        Success
 * @retval -ENODATA The time has never been set
 * @retval -ENODEV  RTC not available
 */
int driver_rtc_get(struct rtc_time *t);

/**
 * @brief Format the time as "YYYY-MM-DD hh:mm:ss".
 * @param buf Output buffer
 * @param len Size of @p buf, at least DRIVER_RTC_STRING_LEN
 * @retval 0       Success
 * @retval -EINVAL buf NULL or len too small
 * @retval other   Errors of driver_rtc_get()
 */
int driver_rtc_to_string(char *buf, size_t len);

#endif /* DRIVER_RTC_H_ */

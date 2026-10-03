#ifndef LCD_VIEW_H
#define LCD_VIEW_H
#include <stdbool.h>
#include <stdint.h>

/* Initialise the 20x4 LCD and show the boot splash */
void lcd_view_init(void);

/* Clear the boot splash before the first lcd_view_show() */
void lcd_view_clear(void);

/* Draw the status screen:
 *   1: IP:192.168.100.112        (the saved static IP until Ethernet is up)
 *   2: T: 34.5*C , H: 56.9%     (* = HD44780 degree glyph 0xDF)
 *   3: MQTT:OK   Motor:ON       (MQTT:OK only with the broker and the cable)
 *   4: alarm text, blank unless an alarm is active
 * Temperature/humidity are in tenths; "--.-" is shown until have_data. */
void lcd_view_show(bool have_data, int temp_x10, int humi_x10);

/* Redraw at once when the link comes up / goes down, the address changes or
 * MQTT connects / drops; call it often between two lcd_view_show() */
void lcd_view_refresh(void);

/* Re-probe a missing/unplugged LCD, at most once every 10 s */
void lcd_view_retry(int64_t now);

#endif /* LCD_VIEW_H */

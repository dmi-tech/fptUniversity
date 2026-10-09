/* Calls every public driver function so that all of them are compiled and linked. */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "driver_adc.h"
#include "driver_bh1750.h"
#include "driver_can.h"
#include "driver_dht11.h"
#include "driver_encoder.h"
#include "driver_ethernet.h"
#include "driver_gpio.h"
#include "driver_hcsr04.h"
#include "driver_i2c.h"
#include "driver_lcd.h"
#include "driver_motor.h"
#include "driver_mqtt.h"
#include "driver_pwm.h"
#include "driver_rs485.h"
#include "driver_rtc.h"
#include "driver_servo.h"
#include "driver_sht41.h"
#include "driver_spi.h"
#include "driver_timer.h"
#include "driver_uart.h"
#include "driver_watchdog.h"

static struct driver_timer sw_timer;

static void on_timer(void *user_data)
{
	ARG_UNUSED(user_data);
}

static void on_can_rx(uint32_t id, const uint8_t *data, uint8_t len)
{
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	ARG_UNUSED(id);
}

static void on_uart_rx(enum driver_uart_port port, uint8_t byte)
{
	ARG_UNUSED(port);
	ARG_UNUSED(byte);
}

static void on_mqtt(const char *topic, const uint8_t *payload, size_t len)
{
	ARG_UNUSED(topic);
	ARG_UNUSED(payload);
	ARG_UNUSED(len);
}

static void on_encoder(int32_t position, int8_t step)
{
	ARG_UNUSED(position);
	ARG_UNUSED(step);
}

static void on_encoder_button(bool pressed)
{
	ARG_UNUSED(pressed);
}

static void on_input(uint8_t id, bool active)
{
	printk("in%u=%d\n", id, active);
}

int main(void)
{
	struct driver_stopwatch sw;
	int16_t raw;
	int32_t value;

	/* gpio */
	printk("gpio %d %d\n", driver_gpio_out_init(0), driver_gpio_in_init(0, on_input));
	driver_gpio_set(0, true);
	driver_gpio_toggle(0);
	printk("in0=%d\n", driver_gpio_get(0));

	/* timer */
	driver_timer_start(&sw_timer, 100, true, on_timer, NULL);
	printk("running=%d\n", driver_timer_is_running(&sw_timer));
	driver_timer_stop(&sw_timer);
	driver_timer_stopwatch_start(&sw);
	printk("us=%u\n", driver_timer_stopwatch_us(&sw));
	printk("hw %d\n", driver_timer_hw_init());
	driver_timer_hw_alarm_us(100, on_timer, NULL);
	printk("now=%u\n", driver_timer_hw_now_us());

	/* watchdog */
	printk("wdt reset=%d\n", driver_watchdog_caused_reset());
	printk("wdt %d\n", driver_watchdog_init(2000));
	driver_watchdog_feed();

	/* adc */
	printk("adc %d\n", driver_adc_init());
	driver_adc_read_raw(&raw);
	driver_adc_read_mv(&value);
	driver_adc_read_avg_mv(8, &value);
	driver_adc_read_die_temp(&value);
	driver_adc_read_vdda(&value);

	/* pwm */
	printk("pwm %d\n", driver_pwm_init(0));
	driver_pwm_set_freq_duty(0, 1000, 50);
	driver_pwm_set_pulse(0, 1000000, 250000);
	driver_pwm_stop(0);


	/* i2c */
	uint8_t reg;

	printk("i2c %d\n", driver_i2c_init(DRIVER_I2C1));
	printk("scan %d\n", driver_i2c_scan(DRIVER_I2C2));
	driver_i2c_write(DRIVER_I2C1, 0x27, &reg, 1);
	driver_i2c_read(DRIVER_I2C1, 0x27, &reg, 1);
	driver_i2c_write_read(DRIVER_I2C1, 0x27, &reg, 1, &reg, 1);
	driver_i2c_reg_write(DRIVER_I2C2, 0x51, 0x00, 0x00);
	driver_i2c_reg_read(DRIVER_I2C2, 0x51, 0x02, &reg);

	/* spi */
	uint8_t spi_tx[4] = {1, 2, 3, 4};
	uint8_t spi_rx[4];

	printk("spi %d\n", driver_spi_init(1000000, 0));
	driver_spi_transfer(spi_tx, spi_rx, sizeof(spi_tx));
	driver_spi_write(spi_tx, sizeof(spi_tx));
	driver_spi_read(spi_rx, sizeof(spi_rx));
	driver_spi_write_then_read(spi_tx, 1, spi_rx, sizeof(spi_rx));

	/* uart */
	char line[32];
	uint8_t byte = 0;

	printk("uart %d\n", driver_uart_init(DRIVER_UART6, 115200));
	driver_uart_set_rx_callback(DRIVER_UART6, on_uart_rx);
	driver_uart_print(DRIVER_UART6, "hello\r\n");
	driver_uart_printf(DRIVER_UART6, "n=%d\r\n", 42);
	driver_uart_write(DRIVER_UART6, &byte, 1);
	driver_uart_read(DRIVER_UART6, &byte, 1, K_MSEC(10));
	driver_uart_read_line(DRIVER_UART6, line, sizeof(line), K_MSEC(10));
	printk("avail %d\n", driver_uart_available(DRIVER_UART6));
	driver_uart_flush(DRIVER_UART6, K_MSEC(10));

	/* rs485 */
	uint8_t frame[16] = {0};

	printk("rs485 %d\n", driver_rs485_init(9600));
	driver_rs485_send(frame, 1);
	driver_rs485_print("ok");
	driver_rs485_receive(frame, sizeof(frame), K_MSEC(10));
	driver_rs485_receive_frame(frame, sizeof(frame), 20, K_MSEC(10));

	/* can */
	printk("can %d\n", driver_can_init(500000, true));
	driver_can_add_rx(0x123, 0x7FF, on_can_rx);
	driver_can_send(0x123, spi_tx, sizeof(spi_tx));
	driver_can_send_ext(0x1234567, spi_tx, sizeof(spi_tx));
	printk("can state %d\n", driver_can_get_state());


	/* sht41, bh1750, dht11, hcsr04 */
	float f1, f2;
	int i1, i2;
	uint32_t mm;

	printk("sht41 %d\n", driver_sht41_init());
	driver_sht41_read(&f1, &f2);
	printk("bh1750 %d\n", driver_bh1750_init());
	driver_bh1750_read_lux(&f1);
	printk("dht11 %d\n", driver_dht11_init(0));
	driver_dht11_read(0, &i1, &i2);
	printk("hcsr04 %d\n", driver_hcsr04_init());
	driver_hcsr04_read_mm(&mm);
	driver_hcsr04_read_avg_mm(5, &mm);

	/* rtc */
	struct rtc_time tm;
	char stamp[DRIVER_RTC_STRING_LEN];

	printk("rtc %d\n", driver_rtc_init());
	driver_rtc_set(2026, 10, 6, 14, 5, 9);
	driver_rtc_get(&tm);
	driver_rtc_to_string(stamp, sizeof(stamp));

	/* lcd */
	static struct driver_lcd lcd;
	static const struct driver_lcd_cfg lcd_cfg = DRIVER_LCD_CFG_20X4(0x27);
	static const uint8_t glyph[8] = {0x04, 0x0E, 0x1F, 0x0E, 0x04, 0x00, 0x00, 0x00};
	uint8_t cols, rows;

	printk("lcd %d\n", driver_lcd_init(&lcd, &lcd_cfg));
	driver_lcd_clear(&lcd);
	driver_lcd_home(&lcd);
	driver_lcd_set_cursor(&lcd, 0, 1);
	driver_lcd_print(&lcd, "hello\nworld");
	driver_lcd_print_at(&lcd, 2, 2, "x");
	driver_lcd_printf(&lcd, "%d", 42);
	driver_lcd_clear_row(&lcd, 1);
	driver_lcd_backlight(&lcd, true);
	driver_lcd_cursor(&lcd, true, false);
	driver_lcd_create_char(&lcd, 0, glyph);
	driver_lcd_putc(&lcd, 0);
	driver_lcd_get_size(&lcd, &cols, &rows);

	/* encoder */
	printk("encoder %d\n", driver_encoder_init());
	driver_encoder_set_limits(0, 100);
	driver_encoder_set_position(10);
	driver_encoder_set_callback(on_encoder);
	driver_encoder_set_button_callback(on_encoder_button);
	printk("pos %d btn %d\n", driver_encoder_get_position(), driver_encoder_button_is_pressed());

	/* motor */
	printk("motor %d\n", driver_motor_init());
	driver_motor_set_min_duty(25);
	driver_motor_on();
	driver_motor_set_speed(40);
	driver_motor_ramp_to(100, 100);
	printk("motor on=%d speed=%u\n", driver_motor_is_on(), driver_motor_get_speed());
	driver_motor_off();

	/* servo */
	const struct driver_servo_cfg servo_cfg = DRIVER_SERVO_CFG_DEFAULT;

	printk("servo %d\n", driver_servo_init(0, &servo_cfg));
	driver_servo_set_angle(0, 90);
	driver_servo_set_pulse_us(0, 1500);
	driver_servo_sweep(0, 0, 180, 200);
	driver_servo_release(0);

	/* ethernet + mqtt */
	char ip[DRIVER_ETHERNET_IP_STR_LEN];

	printk("eth %d\n", driver_ethernet_init());
	driver_ethernet_wait_ip(K_MSEC(10));
	printk("ip %d link %d\n", driver_ethernet_has_ip(), driver_ethernet_link_up());
	driver_ethernet_get_ip(ip, sizeof(ip));
	driver_ethernet_set_static("192.168.1.50", "255.255.255.0", "192.168.1.1");

	printk("mqtt %d\n", driver_mqtt_init("192.168.1.20", 1883, "board-test"));
	driver_mqtt_subscribe("fpt/test/cmd", 1, on_mqtt);
	printk("connect %d\n", driver_mqtt_connect(K_SECONDS(1)));
	driver_mqtt_publish("fpt/test/temp", "25", 0);
	driver_mqtt_publishf("fpt/test/hum", 1, "%d", 60);
	printk("up %d\n", driver_mqtt_is_connected());
	driver_mqtt_process(K_MSEC(100));

	return 0;
}

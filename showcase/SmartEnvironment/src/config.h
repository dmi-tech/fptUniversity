#ifndef CONFIG_H
#define CONFIG_H

/* Settings of SmartEnvironment: thresholds, periods, timings, driver ids.
 * Change the behaviour here; the logic is in the .c files next to main.c. */

/* ── Timing ──────────────────────────────────────────────────── */
#define DHT_READ_PERIOD_MS     2000   /* DHT11 needs >= 2 s between reads */
#define TICK_MS                1000   /* pump state machine */
#define REPORT_PERIOD_MS      30000   /* MQTT status */
#define SPLASH_MS              5000
#define MOTION_TICK_MS           20
#define LCD_PERIOD_MS           200
#define LCD_WARN_SWAP_MS       2000
#define LCD_RETRY_MS          10000

/* ── Environment thresholds (°C / %RH; DHT11: ±2 °C, ±5 %RH, 20..90 %RH) ── */
#define ENV_T_HOT_C              32   /* HOT above, back at <= 32 - hyst */
#define ENV_T_EXTREME_C          38   /* HOT and >= this: watering even if not dry */
#define ENV_T_COLD_C             15   /* COLD below, back at >= 15 + hyst */
#define ENV_T_HYST_C              1
#define ENV_H_DRY_PCT            40   /* DRY below */
#define ENV_H_HUMID_PCT          80   /* HUMID above */
#define ENV_H_HYST_PCT            3
#define ENV_SENSOR_FAILS          3   /* failed reads in a row => sensor fault */
#define ENV_CONFIRM_READS         3   /* same situation N reads in a row */
#define ENV_WATER_CONFIRM_MS  30000   /* watering situation held => request */

/* ── Pump ────────────────────────────────────────────────────── */
#define PUMP_ON_S                20
#define PUMP_REST_S              60
#define PUMP_MAX_CYCLES_PER_HOUR 10
#define PUMP_MANUAL_MAX_S       600
#define HOUR_MS          (3600LL * 1000)

/* ── Servo (0..180° = 500..2400 us) ─────────────────────────── */
#define SERVO_MIN_DEG             0
#define SERVO_MAX_DEG           180
#define SERVO_AUTO_MIN_DEG       30
#define SERVO_AUTO_MAX_DEG      150
#define SERVO_PARK_DEG           90
#define SERVO_AUTO_SPEED_DPS     15   /* one 30..150° sweep in 8 s */
#define SERVO_MANUAL_SPEED_DPS   30
#define SERVO_PULSE_MIN_US      500
#define SERVO_PULSE_MAX_US     2400
#define MODE_HOLD_MS           3000   /* BTN2 held this long switches the mode */
#define MDEG(d)        ((int32_t)(d) * 1000)

/* ── Relay 1 blink (driver_gpio_blink patterns: ms on, ms off) ── */
#define ALERT_SLOW_HALF_MS     1000   /* warning */
#define ALERT_FAST_HALF_MS      500   /* critical */
#define HEARTBEAT_PATTERN   { 80, 80, 80, 760 }   /* LED LIFE: double blink / s */

/* ── Driver ids (aliases in the overlay) ─────────────────────── */
#define OUT_PUMP      0   /* out0 = Relay 3 */
#define OUT_ALERT     1   /* out1 = Relay 1 */
#define OUT_LED       2   /* out2 = LED LIFE (PA8) */
#define IN_BTN_PUMP   0   /* in0 = BTN1 PA10 */
#define IN_BTN_MODE   1   /* in1 = BTN2 PA9  */
#define IN_BTN_LEFT   2   /* in2 = BTN3 PA3  */
#define IN_BTN_RIGHT  3   /* in3 = BTN4 PA2  */
#define DHT_ID        0   /* dht0 */
#define SERVO_ID      0   /* servo0 */

/* ── Threads (priority: lower number runs first) ─────────────────
 *   prio  thread            period / wakes on        work
 *   coop  net RX, sysworkq  packets, timers          Zephyr: network, buttons, blink
 *    2    W5500 RX, TCP     W5500 IRQ                Zephyr Ethernet driver
 *    4    motion            20 ms                    servo step, BTN2 hold (short)
 *    5    main              command / 1 s / 2 s      control loop, DHT11 read
 *    6    net_up            once at start            DHCP / static IP
 *    7    lcd               200 ms                   LCD rows over I2C (~10 ms)
 *    8    web               HTTP request             web page, /api
 *   10    mqtt              socket / 200 ms          MQTT, TLS handshake (long)
 * Shorter period => higher priority; network I/O and long computations
 * below the device threads. Shared state: g_lock (priority inheritance)
 * or atomics. Stack use: build with scripts/build.sh --threads.
 * Note: a DHT11 read locks the interrupts ~23 ms every 2 s
 * (CONFIG_DHT_LOCK_IRQS, 18 ms start pulse + 40 bits): every thread may be
 * late by that much once per reading; the servo PWM (timer) is not. */
#define MOTION_PRIO     4
#define MOTION_STACK 1536
#define LCD_PRIO        7
#define LCD_STACK    2048

/* ── LCD ─────────────────────────────────────────────────────── */
#define LCD_COLS     20
#define LCD_ROWS      4

#endif /* CONFIG_H */

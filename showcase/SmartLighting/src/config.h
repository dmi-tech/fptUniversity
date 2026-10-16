#ifndef CONFIG_H
#define CONFIG_H

/* Settings of SmartLighting: periods, defaults and limits of the settings
 * saved in flash, driver ids. The logic is in the .c files next to main.c. */

/* ── Timing ──────────────────────────────────────────────────── */
#define READ_PERIOD_MS      1000    /* one BH1750 measurement takes up to 180 ms */
#define REPORT_PERIOD_MS   30000    /* MQTT status */
#define LOG_PERIOD_MS       5000    /* console line */
#define SPLASH_MS           5000
#define SELF_MEASURE_MS     2000    /* lamp's own light measured after switch-on */
#define SENSOR_FAILS           3    /* failed reads in a row => sensor fault */
#define SAVE_DELAY_MS       5000    /* timer from the buttons: flash write delay */
#define LCD_PERIOD_MS        200
#define LCD_WARN_SWAP_MS    2000
#define LCD_RETRY_MS       10000
#define HOUR_MS            (3600LL * 1000)
#define HEARTBEAT_PATTERN  { 80, 80, 80, 760 }   /* LED LIFE: double blink / s */

/* ── Settings (g_cfg.app_data, flash): defaults and limits ───── */
#define SET_MAGIC           0x4C494731U   /* "LIG1" */
#define SET_ON_LUX_DEF      10            /* lamp on below (lux) */
#define SET_OFF_LUX_DEF     30            /* lamp off above (lux, own light removed) */
#define SET_CONFIRM_S_DEF   30            /* a condition must last this long */
#define SET_LUX_MAX       5000
#define SET_LUX_GAP_MIN      5            /* off_lux >= on_lux + 5 */
#define SET_CONFIRM_MIN      5
#define SET_CONFIRM_MAX    600
#define SET_TIMER_MAX       12            /* off timer, hours (0 = none) */

/* ── Driver ids (aliases in the overlay) ─────────────────────── */
#define OUT_LAMP        0   /* out0 = Relay 3 */
#define OUT_LED         1   /* out1 = LED LIFE (PA8) */
#define IN_BTN_LAMP     0   /* in0 = BTN1 PA10 */
#define IN_BTN_MODE     1   /* in1 = BTN2 PA9  */
#define IN_BTN_TMR_UP   2   /* in2 = BTN3 PA3  */
#define IN_BTN_TMR_DOWN 3   /* in3 = BTN4 PA2  */

/* ── Threads (priority: lower number runs first) ─────────────────
 *   prio  thread            period / wakes on        work
 *   coop  net RX, sysworkq  packets, timers          Zephyr: network, buttons, blink
 *    2    W5500 RX, TCP     W5500 IRQ                Zephyr Ethernet driver
 *    5    main              command / 1 s            control loop, BH1750 read (180 ms)
 *    6    net_up            once at start            DHCP / static IP
 *    7    lcd               200 ms                   LCD rows over I2C (~10 ms)
 *    8    web               HTTP request             web page, /api
 *   10    mqtt              socket / 200 ms          MQTT, TLS handshake (long)
 * Shorter period => higher priority; network I/O and long computations
 * below the device threads. Shared state: g_lock (priority inheritance)
 * or atomics. Stack use: build with scripts/build.sh --threads. */
#define LCD_PRIO        7
#define LCD_STACK    2048

/* ── LCD ─────────────────────────────────────────────────────── */
#define LCD_COLS       20
#define LCD_ROWS        4

#endif /* CONFIG_H */

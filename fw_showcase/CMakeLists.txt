cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fw_showcase VERSION 3.0.0)

# Headers the Zephyr libraries must see (Mbed TLS user config)
zephyr_include_directories(src/config)

target_include_directories(app PRIVATE
  src/app
  src/drivers
  src/net
  src/system
  src/ui
)

target_sources(app PRIVATE
  src/main.c

  # Application logic
  src/app/alarm.c
  src/app/alarm_notify.c
  src/app/app_config.c
  src/app/app_state.c

  # Peripheral drivers
  src/drivers/dht11.c
  src/drivers/lcd_pcf8574.c
  src/drivers/rs485.c

  # Networking
  src/net/network.c
  src/net/http.c
  src/net/web_server.c
  src/net/mqtt_app.c

  # System
  src/system/boot_swap.c
  src/system/status_led.c

  # User interface
  src/ui/lcd_view.c
)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fw_showcase VERSION 3.1.0)

# Headers the Zephyr libraries must see (Mbed TLS user config)
zephyr_include_directories(src/modules/MQTT/TLS)

# One folder per module under src/modules
set(MODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/modules)

target_include_directories(app PRIVATE
  ${MODULES_DIR}/Alarm
  ${MODULES_DIR}/Boot
  ${MODULES_DIR}/Config
  ${MODULES_DIR}/DHT11
  ${MODULES_DIR}/LCD
  ${MODULES_DIR}/Motor
  ${MODULES_DIR}/MQTT
  ${MODULES_DIR}/Network
  ${MODULES_DIR}/RS485
  ${MODULES_DIR}/StatusLED
  ${MODULES_DIR}/WebServer
)

target_sources(app PRIVATE
  src/main.c

  ${MODULES_DIR}/Alarm/alarm.c
  ${MODULES_DIR}/Alarm/alarm_notify.c
  ${MODULES_DIR}/Boot/boot_swap.c
  ${MODULES_DIR}/Config/app_config.c
  ${MODULES_DIR}/Config/app_state.c
  ${MODULES_DIR}/DHT11/dht11.c
  ${MODULES_DIR}/LCD/lcd_pcf8574.c
  ${MODULES_DIR}/LCD/lcd_view.c
  ${MODULES_DIR}/Motor/motor.c
  ${MODULES_DIR}/MQTT/mqtt_app.c
  ${MODULES_DIR}/Network/network.c
  ${MODULES_DIR}/RS485/rs485.c
  ${MODULES_DIR}/RS485/rs485_cmd.c
  ${MODULES_DIR}/StatusLED/status_led.c
  ${MODULES_DIR}/WebServer/http.c
  ${MODULES_DIR}/WebServer/web_server.c
)

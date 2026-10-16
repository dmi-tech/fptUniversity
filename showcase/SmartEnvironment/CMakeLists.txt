cmake_minimum_required(VERSION 3.20.0)

# Board definition in boards/st/ (scripts/build.sh also passes it for sysbuild)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(SmartEnvironment)

# Firmware identity: FW_VERSION comes from the VERSION file (MCUboot signs the
# image with the same version); FW_DEVICE_NAME is the default device name and
# MQTT client ID (at most 15 characters)
if(NOT APP_VERSION_STRING)
  message(FATAL_ERROR "${CMAKE_CURRENT_SOURCE_DIR}/VERSION is missing")
endif()
target_compile_definitions(app PRIVATE
  FW_NAME="SmartEnvironment"
  FW_VERSION="${APP_VERSION_STRING}"
  FW_DEVICE_NAME="SmartEnv"
)

# Drivers in src/drivers/<name>: their .c files are built, their folder is on
# the include path
set(DRIVERS
  boot          # MCUboot image confirm, NRST swap gesture
  config        # configuration in flash (A/B + CRC), IP / MAC
  network       # W5500 Ethernet, DHCP / static IP
  mqtt          # MQTT client (TLS), status + per-device topics
  web           # web server: login, configuration page, /api
  gpio          # relays, buttons
  pwm           # used by the servo driver
  i2c           # bus of the LCD
  lcd           # 20x4 LCD (PCF8574)
  dht11         # temperature / humidity
  servo         # spray nozzle
)
foreach(drv ${DRIVERS})
  file(GLOB srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/${drv}/*.c)
  target_sources(app PRIVATE ${srcs})
  target_include_directories(app PRIVATE src/drivers/${drv})
endforeach()

# Mbed TLS user config (src/drivers/mqtt/mbedtls_user_config.h) must be seen
# by the Zephyr libraries too
zephyr_include_directories(src/drivers/mqtt)

# Application: main.c and the logic files next to it (overview in src/app.h)
target_include_directories(app PRIVATE src)
target_sources(app PRIVATE
  src/main.c          # start-up, main loop, buttons
  src/env.c           # environment analysis, relay 1
  src/pump.c          # pump state machine
  src/motion.c        # servo thread, BTN2 hold
  src/lcd_view.c      # LCD thread and screens
  src/status_json.c   # JSON for MQTT and the web page
  src/web_app.c       # web page hooks, /api routes
)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(FPT_DMI_Embedded-IoT_Course VERSION 1.0.0)

# Thư mục chứa toàn bộ driver của môn học
set(DRIVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/driver)

# 1. Thư mục chứa file .h: MQTT cần thêm driver Ethernet
target_include_directories(app PRIVATE
  src
  ${DRIVER_DIR}/devices/ethernet
  ${DRIVER_DIR}/services/mqtt
  ${DRIVER_DIR}/peripherals/gpio
  ${DRIVER_DIR}/devices/dht11
)

# 2. Các file .c cần biên dịch
target_sources(app PRIVATE
  src/main.c
  ${DRIVER_DIR}/devices/ethernet/driver_ethernet.c
  ${DRIVER_DIR}/services/mqtt/driver_mqtt.c
  ${DRIVER_DIR}/peripherals/gpio/driver_gpio.c
  ${DRIVER_DIR}/devices/dht11/driver_dht11.c
)

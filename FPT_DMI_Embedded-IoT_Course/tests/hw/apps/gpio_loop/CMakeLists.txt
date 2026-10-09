cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(example)
set(DRIVER_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../driver)
target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/gpio
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/gpio/driver_gpio.c
)

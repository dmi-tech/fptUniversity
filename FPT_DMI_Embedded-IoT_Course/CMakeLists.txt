cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(FPT_DMI_Embedded-IoT_Course VERSION 1.0.0)

# Thư mục chứa toàn bộ driver của môn học
set(DRIVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/driver)

# 1. Thư mục chứa file .h. Mỗi driver cần thêm thư mục của nó
#    VÀ thư mục của driver mà nó phụ thuộc (xem README của driver).
target_include_directories(app PRIVATE
  src
  # TODO (Sinh viên): thêm thư mục header của driver, ví dụ:
  # ${DRIVER_DIR}/peripherals/gpio
)

# 2. Các file .c cần biên dịch.
target_sources(app PRIVATE
  src/main.c
  # TODO (Sinh viên): thêm file .c của driver, ví dụ:
  # ${DRIVER_DIR}/peripherals/gpio/driver_gpio.c
)

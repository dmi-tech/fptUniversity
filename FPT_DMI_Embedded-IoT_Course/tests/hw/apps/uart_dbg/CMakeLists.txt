cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(example)
set(DRIVER_DIR /home/dmi/zephyr-boards/stm32h573ri/firmware/tests/EmbeddedCourse_FPT_DMI/driver)
target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/protocols/uart
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/protocols/uart/driver_uart.c
)

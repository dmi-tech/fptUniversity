cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(example)
set(DRIVER_DIR /home/dmi/zephyr-boards/stm32h573ri/firmware/tests/EmbeddedCourse_FPT_DMI/driver)
target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/pwm
  ${DRIVER_DIR}/devices/servo
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/pwm/driver_pwm.c
  ${DRIVER_DIR}/devices/servo/driver_servo.c
)

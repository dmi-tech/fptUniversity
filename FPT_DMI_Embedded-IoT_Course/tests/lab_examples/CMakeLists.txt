# Builds one lab example of docs/examples/ with the drivers it needs. Used by run.sh.
#   -DLAB_MAIN=<file in docs/examples>  -DLAB_DRIVERS="peripherals/gpio;protocols/can"
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lab_example)

set(DRIVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../driver)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../docs/examples/${LAB_MAIN})
foreach(d ${LAB_DRIVERS})
  get_filename_component(name ${d} NAME)
  file(GLOB srcs ${DRIVER_DIR}/${d}/driver_*.c)
  target_sources(app PRIVATE ${srcs})
  target_include_directories(app PRIVATE ${DRIVER_DIR}/${d})
endforeach()

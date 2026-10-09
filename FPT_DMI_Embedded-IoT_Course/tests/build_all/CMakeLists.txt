# Links EVERY driver of driver/ into one image, to prove that all of them build.
# Configurations (Kconfig + overlay) live in configs/; run them with ./run.sh.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(build_all)

set(DRIVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../driver)
file(GLOB_RECURSE DRIVER_SOURCES ${DRIVER_DIR}/*.c)
target_sources(app PRIVATE src/main.c ${DRIVER_SOURCES})

foreach(src ${DRIVER_SOURCES})
  get_filename_component(dir ${src} DIRECTORY)
  target_include_directories(app PRIVATE ${dir})
endforeach()

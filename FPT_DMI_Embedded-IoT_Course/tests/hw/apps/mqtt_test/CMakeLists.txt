cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mqtt_test)
set(DRIVER_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../driver)
target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/devices/ethernet
  ${DRIVER_DIR}/services/mqtt
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/devices/ethernet/driver_ethernet.c
  ${DRIVER_DIR}/services/mqtt/driver_mqtt.c
)
# Broker settings from the environment at build time, so no credential is stored in the source:
# MQTT_BROKER_IP, MQTT_USER, MQTT_PASS, MQTT_TOPIC (defaults: local anonymous broker, fpt/HWTEST)
foreach(v MQTT_BROKER_IP MQTT_USER MQTT_PASS MQTT_TOPIC)
  if(DEFINED ENV{${v}})
    target_compile_definitions(app PRIVATE ${v}="$ENV{${v}}")
  endif()
endforeach()

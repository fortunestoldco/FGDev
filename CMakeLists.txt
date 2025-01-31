set(BOARD xiao_esp32c6)
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/xiao_esp32c6.overlay")

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(FGDev)

target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_sources(app PRIVATE
    src/main.c
    handlers/ble_provisioning.c
    handlers/aws_mqtt.c
    handlers/button_handler.c
    drivers/aht10_driver.c
    drivers/max17043_driver.c
    drivers/soil_moisture_sensor.c
)

# ESP32 HomeKit Sensor Switch Project

cmake_minimum_required(VERSION 3.5)

# Set component directories to use our local components
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/components ${CMAKE_CURRENT_LIST_DIR}/components/homekit ${CMAKE_CURRENT_LIST_DIR}/components/common)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-homekit-sensor-switch)

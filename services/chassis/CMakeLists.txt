cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Gpio/inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/I2c/inc)

add_subdirectory(Gpio)
add_subdirectory(I2c)
add_subdirectory(Buttons)
add_subdirectory(ChassisControl)
add_subdirectory(PowerControl)

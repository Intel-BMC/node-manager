cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
project(gpio CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

add_library(${PROJECT_NAME} SHARED src/Gpio.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION "0.1.0")
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION "0")

install (TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})

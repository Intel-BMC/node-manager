cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
project(pfr CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(ExternalProject)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

add_library(${PROJECT_NAME} SHARED src/pfr.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION "0.1.0")
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION "0")
target_link_libraries(${PROJECT_NAME} phosphor_logging)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})

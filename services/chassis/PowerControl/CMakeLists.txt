cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
project(PowerControl CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
include(GNUInstallDirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(DBUS_OBJECT_NAME "xyz/openbmc_project/Chassis/Control/Power")
set(DBUS_INTF_NAME "xyz.openbmc_project.Chassis.Control.Power")

add_definitions(-DDBUS_OBJECT_NAME="/${DBUS_OBJECT_NAME}")
add_definitions(-DDBUS_INTF_NAME="${DBUS_INTF_NAME}")
set(SRC_FILES
    src/PowerControl.cpp
    src/timer.cpp
    src/main.cpp
)

set(GENERATED_SRC_FILES
    ${DBUS_OBJECT_NAME}/error.cpp
    ${DBUS_OBJECT_NAME}/server.cpp
)

# import libsystemd
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD libsystemd REQUIRED)
include_directories(${SYSTEMD_INCLUDE_DIRS})
link_directories(${SYSTEMD_LIBRARY_DIRS})

# import sdbusplus
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUSPLUSPLUS sdbusplus REQUIRED)
include_directories(${SDBUSPLUSPLUS_INCLUDE_DIRS})
link_directories(${SDBUSPLUSPLUS_LIBRARY_DIRS})
find_program(SDBUSPLUSPLUS sdbus++)

# import phosphor-logging
find_package(PkgConfig REQUIRED)
pkg_check_modules(LOGGING phosphor-logging REQUIRED)
include_directories(${LOGGING_INCLUDE_DIRS})
link_directories(${LOGGING_LIBRARY_DIRS})

# phosphor-dbus-interfaces
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUSINTERFACE phosphor-dbus-interfaces REQUIRED)
include_directories(${DBUSINTERFACE_INCLUDE_DIRS})
link_directories(${DBUSINTERFACE_LIBRARY_DIRS})

option(ENABLE_GTEST "Enable Google Test" OFF)
if(ENABLE_GTEST)
    enable_testing()
    find_package(Threads REQUIRED)
    find_package(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tests/inc)
    add_executable(timertest tests/src/timer_test.cpp src/timer.cpp)
    target_link_libraries(timertest ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${SDBUSPLUSPLUS_LIBRARIES} ${DBUSINTERFACE_LIBRARIES})
    add_test(NAME timertest COMMAND timertest)
endif()

add_custom_command(
    OUTPUT ${DBUS_OBJECT_NAME}/error.hpp
    OUTPUT ${DBUS_OBJECT_NAME}/error.cpp
    COMMAND mkdir -p ${DBUS_OBJECT_NAME}
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} error exception-header ${DBUS_INTF_NAME} > ${DBUS_OBJECT_NAME}/error.hpp
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} error exception-cpp ${DBUS_INTF_NAME} > ${DBUS_OBJECT_NAME}/error.cpp
    DEPENDS ${DBUS_OBJECT_NAME}.errors.yaml
)
set_source_files_properties(
    ${DBUS_OBJECT_NAME}/error.hpp
    ${DBUS_OBJECT_NAME}/error.cpp
    PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${DBUS_OBJECT_NAME}/server.hpp
    OUTPUT ${DBUS_OBJECT_NAME}/server.cpp
    COMMAND mkdir -p ${DBUS_OBJECT_NAME}
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} interface server-header ${DBUS_INTF_NAME} > ${DBUS_OBJECT_NAME}/server.hpp
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} interface server-cpp ${DBUS_INTF_NAME} > ${DBUS_OBJECT_NAME}/server.cpp
    DEPENDS ${DBUS_OBJECT_NAME}.interface.yaml
)
set_source_files_properties(
    ${DBUS_OBJECT_NAME}/server.hpp
    ${DBUS_OBJECT_NAME}/server.cpp
    PROPERTIES GENERATED TRUE)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${GENERATED_SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${SYSTEMD_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${SDBUSPLUSPLUS_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${DBUSINTERFACE_LIBRARIES})

install (TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

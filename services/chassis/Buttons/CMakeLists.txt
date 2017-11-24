cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
project(Buttons CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include(GNUInstallDirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(POWER_DBUS_OBJECT_NAME "xyz/openbmc_project/Chassis/Buttons/Power")
set(POWER_DBUS_INTF_NAME "xyz.openbmc_project.Chassis.Buttons.Power")

set(RESET_DBUS_OBJECT_NAME "xyz/openbmc_project/Chassis/Buttons/Reset")
set(RESET_DBUS_INTF_NAME "xyz.openbmc_project.Chassis.Buttons.Reset")

add_definitions(-DPOWER_DBUS_OBJECT_NAME="/${POWER_DBUS_OBJECT_NAME}")

add_definitions(-DRESET_DBUS_OBJECT_NAME="/${RESET_DBUS_OBJECT_NAME}")

set(SRC_FILES
    src/PowerButton.cpp
    src/ResetButton.cpp
    src/main.cpp
)
set(GENERATED_SRC_FILES
    ${POWER_DBUS_OBJECT_NAME}/error.cpp
    ${POWER_DBUS_OBJECT_NAME}/server.cpp
    ${RESET_DBUS_OBJECT_NAME}/error.cpp
    ${RESET_DBUS_OBJECT_NAME}/server.cpp
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

#include_directories(${CMAKE_CURRENT_BINARY_DIR}/${DBUS_OBJECT_NAME})

add_custom_command(
    OUTPUT ${POWER_DBUS_OBJECT_NAME}/error.hpp
    OUTPUT ${POWER_DBUS_OBJECT_NAME}/error.cpp
    COMMAND mkdir -p ${POWER_DBUS_OBJECT_NAME}
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} error exception-header ${POWER_DBUS_INTF_NAME} > ${POWER_DBUS_OBJECT_NAME}/error.hpp
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} error exception-cpp ${POWER_DBUS_INTF_NAME} > ${POWER_DBUS_OBJECT_NAME}/error.cpp
    DEPENDS ${POWER_DBUS_OBJECT_NAME}.errors.yaml
)
set_source_files_properties(
    ${POWER_DBUS_OBJECT_NAME}/error.hpp
    ${POWER_DBUS_OBJECT_NAME}/error.cpp
    PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${RESET_DBUS_OBJECT_NAME}/error.hpp
    OUTPUT ${RESET_DBUS_OBJECT_NAME}/error.cpp
    COMMAND mkdir -p ${RESET_DBUS_OBJECT_NAME}
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} error exception-header ${RESET_DBUS_INTF_NAME} > ${RESET_DBUS_OBJECT_NAME}/error.hpp
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} error exception-cpp ${RESET_DBUS_INTF_NAME} > ${RESET_DBUS_OBJECT_NAME}/error.cpp
    DEPENDS ${RESET_DBUS_OBJECT_NAME}.errors.yaml
)
set_source_files_properties(
    ${RESET_DBUS_OBJECT_NAME}/error.hpp
    ${RESET_DBUS_OBJECT_NAME}/error.cpp
    PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${POWER_DBUS_OBJECT_NAME}/server.hpp
    OUTPUT ${POWER_DBUS_OBJECT_NAME}/server.cpp
    COMMAND mkdir -p ${POWER_DBUS_OBJECT_NAME}
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} interface server-header ${POWER_DBUS_INTF_NAME} > ${POWER_DBUS_OBJECT_NAME}/server.hpp
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} interface server-cpp ${POWER_DBUS_INTF_NAME} > ${POWER_DBUS_OBJECT_NAME}/server.cpp
    DEPENDS ${POWER_DBUS_OBJECT_NAME}.interface.yaml
)
set_source_files_properties(
    ${POWER_DBUS_OBJECT_NAME}/server.hpp
    ${POWER_DBUS_OBJECT_NAME}/server.cpp
    PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${RESET_DBUS_OBJECT_NAME}/server.hpp
    OUTPUT ${RESET_DBUS_OBJECT_NAME}/server.cpp
    COMMAND mkdir -p ${RESET_DBUS_OBJECT_NAME}
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} interface server-header ${RESET_DBUS_INTF_NAME} > ${RESET_DBUS_OBJECT_NAME}/server.hpp
    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR} interface server-cpp ${RESET_DBUS_INTF_NAME} > ${RESET_DBUS_OBJECT_NAME}/server.cpp
    DEPENDS ${RESET_DBUS_OBJECT_NAME}.interface.yaml
)
set_source_files_properties(
    ${RESET_DBUS_OBJECT_NAME}/server.hpp
    ${RESET_DBUS_OBJECT_NAME}/server.cpp
    PROPERTIES GENERATED TRUE)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${GENERATED_SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${SYSTEMD_LIBRARIES} gpio)
target_link_libraries(${PROJECT_NAME} "${SDBUSPLUSPLUS_LIBRARIES} -lstdc++fs")

install (TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

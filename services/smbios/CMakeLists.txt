cmake_minimum_required (VERSION 3.2 FATAL_ERROR)
include (ExternalProject)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set (DBUS_OBJECT_NAME "xyz/openbmc_project/Smbios/MDR_V1")
set (DBUS_INTF_NAME "xyz.openbmc_project.Smbios.MDR_V1")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/include/phosphor-dbus-interfaces
)
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include/GSL/include)
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
link_directories (${DBUSINTERFACE_LIBRARIES})

find_package (PkgConfig REQUIRED)
pkg_check_modules (SYSTEMD libsystemd REQUIRED)
include_directories (${SYSTEMD_INCLUDE_DIRS})
link_directories (${SYSTEMD_LIBRARY_DIRS})

# import sdbusplus
find_package (PkgConfig REQUIRED)
pkg_check_modules (SDBUSPLUSPLUS sdbusplus REQUIRED)
include_directories (${SDBUSPLUSPLUS_INCLUDE_DIRS})
link_directories (${SDBUSPLUSPLUS_LIBRARY_DIRS})

# phosphor-dbus-interfaces
find_package (PkgConfig REQUIRED)
pkg_check_modules (DBUSINTERFACE phosphor-dbus-interfaces REQUIRED)
include_directories (${DBUSINTERFACE_INCLUDE_DIRS})
link_directories (${DBUSINTERFACE_LIBRARY_DIRS})

set (SRC_FILES src/manager.cpp src/smbios-main.cpp src/timer.cpp src/cpu.cpp
     src/dimm.cpp ${DBUS_OBJECT_NAME}/server.cpp ${DBUS_OBJECT_NAME}/error.cpp)

include_directories (${CMAKE_CURRENT_BINARY_DIR})

find_package (PkgConfig REQUIRED)
pkg_check_modules (SDBUSPLUSPLUS sdbusplus REQUIRED)
include_directories (${SDBUSPLUSPLUS_INCLUDE_DIRS})
link_directories (${SDBUSPLUSPLUS_LIBRARY_DIRS})
find_program (SDBUSPLUSPLUS sdbus++)

add_custom_command (OUTPUT ${DBUS_OBJECT_NAME}/server.hpp
                    OUTPUT ${DBUS_OBJECT_NAME}/server.cpp
                    COMMAND mkdir -p ${DBUS_OBJECT_NAME}
                    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR}
                            interface server-header ${DBUS_INTF_NAME} >
                            ${DBUS_OBJECT_NAME}/server.hpp
                    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR}
                            interface server-cpp ${DBUS_INTF_NAME} >
                            ${DBUS_OBJECT_NAME}/server.cpp
                    DEPENDS ${DBUS_OBJECT_NAME}.interface.yaml)
set_source_files_properties (${DBUS_OBJECT_NAME}/server.hpp
                             ${DBUS_OBJECT_NAME}/server.cpp PROPERTIES
                             GENERATED TRUE)

add_custom_command (OUTPUT ${DBUS_OBJECT_NAME}/error.hpp
                    OUTPUT ${DBUS_OBJECT_NAME}/error.cpp
                    COMMAND mkdir -p ${DBUS_OBJECT_NAME}
                    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR}
                            error exception-header ${DBUS_INTF_NAME} >
                            ${DBUS_OBJECT_NAME}/error.hpp
                    COMMAND ${SDBUSPLUSPLUS} -r ${CMAKE_CURRENT_SOURCE_DIR}
                            error exception-cpp ${DBUS_INTF_NAME} >
                            ${DBUS_OBJECT_NAME}/error.cpp
                    DEPENDS ${DBUS_OBJECT_NAME}.errors.yaml)
set_source_files_properties (${DBUS_OBJECT_NAME}/error.hpp
                             ${DBUS_OBJECT_NAME}/error.cpp PROPERTIES GENERATED
                             TRUE)

add_executable (smbiosapp ${SRC_FILES})
target_link_libraries (smbiosapp ${SYSTEMD_LIBRARIES})
target_link_libraries (smbiosapp ${DBUSINTERFACE_LIBRARIES})
target_link_libraries (smbiosapp ${SDBUSPLUSPLUS_LIBRARIES})
target_link_libraries (smbiosapp phosphor_logging)

install (TARGETS smbiosapp DESTINATION sbin)

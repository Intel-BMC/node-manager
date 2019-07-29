cmake_minimum_required (VERSION 2.8.10 FATAL_ERROR)
set (BUILD_SHARED_LIBRARIES OFF)
include (ExternalProject)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++fs -Werror")

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/include/phosphor-dbus-interfaces
)

project (psumanager CXX)

set (PSU_CR_SRC_FILES src/utility.cpp src/coldRedundancy.cpp)

set (EXTERNAL_PACKAGES Boost sdbusplus-project nlohmann-json)
set (CR_LINK_LIBS -lsystemd stdc++fs sdbusplus)

add_definitions (-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions (-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions (-DBOOST_ALL_NO_LIB)
add_definitions (-DBOOST_NO_RTTI)
add_definitions (-DBOOST_NO_TYPEID)
add_definitions (-DBOOST_ASIO_DISABLE_THREADS)

link_directories (${EXTERNAL_INSTALL_LOCATION}/lib)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

include_directories (${SDBUSPLUSPLUS_INCLUDE_DIRS})
link_directories (${SDBUSPLUSPLUS_LIBRARY_DIRS})

include_directories (${DBUSINTERFACE_INCLUDE_DIRS})
link_directories (${DBUSINTERFACE_LIBRARY_DIRS})

add_executable (psuredundancy ${PSU_CR_SRC_FILES} src/redundancyMain.cpp)
add_dependencies (psuredundancy sdbusplus-project)
target_link_libraries (psuredundancy ${CR_LINK_LIBS})
target_link_libraries (psuredundancy i2c)
target_link_libraries (psuredundancy phosphor_logging)
target_link_libraries (psuredundancy phosphor_dbus)

include_directories (${LOGGING_INCLUDE_DIRS})
link_directories (${LOGGING_LIBRARY_DIRS})

set (SERVICE_FILE_SRC_DIR ${PROJECT_SOURCE_DIR}/service_files)
set (SERVICE_FILE_INSTALL_DIR /lib/systemd/system/)

install (TARGETS psuredundancy DESTINATION bin)
install (FILES
             ${SERVICE_FILE_SRC_DIR}/xyz.openbmc_project.coldredundancy.service
             DESTINATION ${SERVICE_FILE_INSTALL_DIR})

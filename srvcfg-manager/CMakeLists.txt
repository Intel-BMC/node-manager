cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(phosphor-srvcfg-manager CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-rtti")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include(GNUInstallDirs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions(-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-DBOOST_NO_RTTI)
add_definitions(-DBOOST_NO_TYPEID)
add_definitions(-DBOOST_ASIO_DISABLE_THREADS)

set(SRC_FILES src/main.cpp src/srvcfg_manager.cpp src/utils.cpp)

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

# phosphor-dbus-interfaces
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUSINTERFACE phosphor-dbus-interfaces REQUIRED)
include_directories(${DBUSINTERFACE_INCLUDE_DIRS})
link_directories(${DBUSINTERFACE_LIBRARY_DIRS})

# import phosphor-logging
find_package(PkgConfig REQUIRED)
pkg_check_modules(LOGGING phosphor-logging REQUIRED)
include_directories(${LOGGING_INCLUDE_DIRS})
link_directories(${LOGGING_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} systemd)
target_link_libraries(${PROJECT_NAME} "${SDBUSPLUSPLUS_LIBRARIES} -lstdc++fs")
target_link_libraries(${PROJECT_NAME} ${DBUSINTERFACE_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
target_link_libraries(${PROJECT_NAME} phosphor_logging)

set(SERVICE_FILES ${PROJECT_SOURCE_DIR}/srvcfg-manager.service)

install(TARGETS ${PROJECT_NAME} DESTINATION sbin)
install(FILES ${SERVICE_FILES} DESTINATION /lib/systemd/system/)

cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libpfr/inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/service/inc)

add_subdirectory(libpfr)
add_subdirectory(service)

set(SERVICE_FILES ${PROJECT_SOURCE_DIR}/xyz.openbmc_project.PFR.Manager.service)
install(FILES ${SERVICE_FILES} DESTINATION /lib/systemd/system/)

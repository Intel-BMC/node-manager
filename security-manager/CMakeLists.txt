cmake_minimum_required (VERSION 3.6 FATAL_ERROR)
project (security-manager CXX)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++fs \
    -Werror \
    -Wall \
    -Wextra \
    -Wshadow \
    -Wnon-virtual-dtor \
    -Wold-style-cast \
    -Wunused \
    -Woverloaded-virtual \
    -Wpedantic \
    -Wconversion \
    -Wmisleading-indentation \
    -Wduplicated-cond \
    -Wduplicated-branches \
    -Wlogical-op \
    -Wnull-dereference \
    -Wuseless-cast \
    -Wdouble-promotion \
    -Wformat=2 \
")

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable (security-manager src/security-manager.cpp)

target_include_directories (security-manager PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries (${PROJECT_NAME} -lstdc++fs)
target_link_libraries (${PROJECT_NAME} gpiodcxx)
target_link_libraries (${PROJECT_NAME} systemd)
target_link_libraries (${PROJECT_NAME} sdbusplus)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

install (
    TARGETS security-manager
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)

find_package (Boost 1.66 REQUIRED)
include_directories (${BOOST_SRC_DIR})

add_definitions (-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions (-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions (-DBOOST_ALL_NO_LIB)
add_definitions (-DBOOST_NO_RTTI)
add_definitions (-DBOOST_NO_TYPEID)
add_definitions (-DBOOST_ASIO_DISABLE_THREADS)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-rtti")

set (
    SERVICE_FILES
    ${PROJECT_SOURCE_DIR}/service_files/xyz.openbmc_project.SecurityManager.service
)
install (FILES ${SERVICE_FILES} DESTINATION /lib/systemd/system/)

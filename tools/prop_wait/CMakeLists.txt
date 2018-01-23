cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
project(prop_wait CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRC_FILES
    src/prop_wait.cpp
    src/argument.cpp
)

# import phosphor-logging
find_package(PkgConfig REQUIRED)
pkg_check_modules(LOGGING phosphor-logging REQUIRED)
include_directories(${LOGGING_INCLUDE_DIRS})
link_directories(${LOGGING_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME}  ${LOGGING_LIBRARIES})

install (TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
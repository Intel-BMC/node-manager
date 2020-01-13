cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(VirtualMedia CXX)

cmake_policy(SET CMP0054 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-rtti")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -flto")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

option(YOCTO_DEPENDENCIES "Use YOCTO dependencies system" OFF)

option(VM_USE_VALGRIND "Build VirtualMedia to work with valgrind" OFF)

option(VM_VERBOSE_NBDKIT_LOGS "Include detailed logs from nbdkit" OFF)

if(NOT ${YOCTO_DEPENDENCIES})
  include(ExternalProject)

  externalproject_add(sdbusplus-project
                      PREFIX
                      ${CMAKE_BINARY_DIR}/sdbusplus-project
                      GIT_REPOSITORY
                      https://github.com/openbmc/sdbusplus.git
                      GIT_TAG
                      c08cf5283b80a071d19506d9a462f6c69e1797f1
                      SOURCE_DIR
                      ${CMAKE_BINARY_DIR}/sdbusplus-src
                      BINARY_DIR
                      ${CMAKE_BINARY_DIR}/sdbusplus-build
                      CONFIGURE_COMMAND
                      ""
                      BUILD_COMMAND
                      cd
                      ${CMAKE_BINARY_DIR}/sdbusplus-src
                      &&
                      ./bootstrap.sh
                      &&
                      ./configure
                      --enable-transaction
                      &&
                      make
                      -j
                      libsdbusplus.la
                      INSTALL_COMMAND
                      ""
                      LOG_DOWNLOAD
                      ON
                      UPDATE_COMMAND
                      "")

  externalproject_add(nlohmann-json
                      GIT_REPOSITORY
                      "https://github.com/nlohmann/json.git"
                      GIT_TAG
                      aafad2be1f3cd259a1e79d2f6fcf267d1ede9ec7
                      SOURCE_DIR
                      "${CMAKE_BINARY_DIR}/nlohmann-json-src"
                      BINARY_DIR
                      "${CMAKE_BINARY_DIR}/nlohmann-json-build"
                      CONFIGURE_COMMAND
                      ""
                      BUILD_COMMAND
                      ""
                      INSTALL_COMMAND
                      mkdir
                      -p
                      "${CMAKE_BINARY_DIR}/prefix/include/nlohmann"
                      &&
                      cp
                      -r
                      "${CMAKE_BINARY_DIR}/nlohmann-json-src/include/nlohmann"
                      "${CMAKE_BINARY_DIR}/prefix/include"
                      UPDATE_COMMAND
                      "")

  include_directories(${CMAKE_BINARY_DIR}/prefix/include)
  include_directories(${CMAKE_BINARY_DIR}/sdbusplus-src)

  link_directories(${CMAKE_BINARY_DIR}/sdbusplus-src/.libs)

endif()

# Include UDEV library
find_package(udev REQUIRED)
include_directories(${UDEV_INCLUDE_DIRS})
link_directories(${UDEV_LIBRARIES})

# Include Boost library This allows specify exact version of BOOST to be used,
# especially important while using valgrind, to point BOOST that is compiled
# with valgrind support
if(${BOOST_VERSION})
  find_package(Boost ${BOOST_VERSION} EXACT)
else()
  find_package(Boost 1.69 REQUIRED)
endif()
message("++ Using Boost version: " ${Boost_VERSION})

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

# Boost related definitions
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions(-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-DBOOST_NO_RTTI)
add_definitions(-DBOOST_NO_TYPEID)
add_definitions(-DBOOST_ASIO_DISABLE_THREADS)

# Define source files
set(SRC_FILES src/main.cpp)

# Executables
add_executable(virtual-media ${SRC_FILES} ${HEADER_FILES})
if(NOT ${YOCTO_DEPENDENCIES})
  add_dependencies(virtual-media nlohmann-json sdbusplus-project)
endif()

# Default linkage
target_link_libraries(virtual-media systemd)
target_link_libraries(virtual-media -lsdbusplus)
target_link_libraries(virtual-media -ludev)
target_link_libraries(virtual-media -lboost_coroutine)
target_link_libraries(virtual-media -lboost_context)
install(TARGETS virtual-media DESTINATION sbin)

# Options based compile definitions
target_compile_definitions(virtual-media
                           PRIVATE
                           $<$<BOOL:${VM_USE_VALGRIND}>:
                           -DBOOST_USE_VALGRIND>
                           $<$<BOOL:${VM_VERBOSE_NBDKIT_LOGS}>:
                           -DVM_VERBOSE_NBDKIT_LOGS>
                           $<$<BOOL:${CUSTOM_DBUS_PATH}>:
                           -DCUSTOM_DBUS_PATH="${CUSTOM_DBUS_PATH}">)

if(CMAKE_INSTALL_SYSCONFDIR)
  install(FILES ${PROJECT_SOURCE_DIR}/virtual-media.json DESTINATION
                ${CMAKE_INSTALL_SYSCONFDIR})
endif()
install(FILES ${PROJECT_SOURCE_DIR}/xyz.openbmc_project.VirtualMedia.service
              DESTINATION /lib/systemd/system/)

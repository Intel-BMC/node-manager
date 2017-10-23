cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
SET(BUILD_SHARED_LIBRARIES OFF)
include(ExternalProject)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++fs -Os")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

option(HUNTER_ENABLED "Enable hunter package pulling" OFF)
include("cmake/HunterGate.cmake")

HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.18.64.tar.gz"
    SHA1 "baf9c8cc4f65306f0e442b5419967b4c4c04589a"
)

project(fans CXX)

hunter_add_package(Boost)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions(-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-DBOOST_NO_RTTI)
add_definitions(-DBOOST_NO_TYPEID)
hunter_add_package(dbus)
find_package(dbus REQUIRED) # Include functions provided by PkgConfig module.
include_directories(${DBUS_INCLUDE_DIRS})

set(SRC_FILES
    src/TachSensor.cpp
    src/Utils.cpp
)

set(TEST_FILES
    tests/test_TachSensor.cpp
)


option(EXTERNAL_PROJECT "Enable Cloning External Projects" OFF)
if(EXTERNAL_PROJECT)
    option(ENABLE_TEST "Enable Google Test" ON)
    ExternalProject_Add(boost-dbus
            PREFIX ${CMAKE_CURRENT_BINARY_DIR}/boost-dbus
            GIT_REPOSITORY ssh://git-amr-2.devtools.intel.com:29418/openbmc-boost-dbus
            GIT_TAG 6fb503a087a0334736375e9aa1ceeaa5d2d434f9
            CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION} -DBOOST_ROOT=${BOOST_ROOT}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            LOG_DOWNLOAD ON
            )
    ExternalProject_Get_Property(boost-dbus install_dir)
    include_directories(${install_dir}/src/boost-dbus/include)

    ExternalProject_Add(nlohmann-json
            PREFIX ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG c0d511ea500e698061a4b288bd77ce43bd76fa70
            CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/src/nlohmann-json/src/nlohmann && cp ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/src/nlohmann-json/src/json.hpp ${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json/src/nlohmann-json/src/nlohmann/json.hpp
            INSTALL_COMMAND ""
            LOG_DOWNLOAD ON
            )
    ExternalProject_Get_Property(nlohmann-json install_dir)
    include_directories(${install_dir}/src/nlohmann-json/src)

    if(ENABLE_TEST)
	hunter_add_package(GTest)
	find_package(GTest CONFIG REQUIRED)

	enable_testing()

	add_executable(runUnitTests ${TEST_FILES} ${SRC_FILES})
	add_test(NAME test_fansensor COMMAND runUnitTests)
	target_link_libraries(runUnitTests GTest::main GTest::gtest pthread ${DBUS_LIBRARIES} stdc++fs)
    endif()

endif()

link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)

include_directories(include)

add_executable(fansensor src/FanMain.cpp ${SRC_FILES})
add_dependencies(fansensor boost-dbus)
add_dependencies(fansensor nlohmann-json)
target_link_libraries(fansensor  ${DBUS_LIBRARIES})
target_link_libraries(fansensor stdc++fs)
target_link_libraries(fansensor pthread)
install (TARGETS fansensor DESTINATION bin)


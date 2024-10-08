

cmake_minimum_required (VERSION 3.18)

set(SDK_DIRECTORY ${CMAKE_SOURCE_DIR}/libs)
set(CMSDK_LOCAL_PATH ${CMAKE_SOURCE_DIR}/sysroot)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/libs/CMakeSDK/cmake)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_MODULE_PATH}/cmsdk-toolchain.cmake)
include(cmsdk)

project(sl
	LANGUAGES CXX C
	VERSION 2.0
	)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE INTERNAL "Use MacOS version 10.15")

include(CTest)

set(CMSDK_PULL_TARGET pull)

add_custom_target(${CMSDK_PULL_TARGET}
	COMMAND git pull
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(libs/API)
add_subdirectory(libs/CloudAPI)
add_subdirectory(libs/HalAPI)
add_subdirectory(libs/InetAPI)
add_subdirectory(libs/JsonAPI)
add_subdirectory(libs/ServiceAPI)
add_subdirectory(libs/SosAPI)
add_subdirectory(libs/StratifyOS)
add_subdirectory(libs/SwdAPI)
add_subdirectory(libs/UsbAPI)

add_subdirectory(sl2/app)
add_subdirectory(sl2/tests)


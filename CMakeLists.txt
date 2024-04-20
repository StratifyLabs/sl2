cmake_minimum_required (VERSION 3.18)

set(SDK_DIRECTORY ${CMAKE_SOURCE_DIR}/SDK)
set(CMSDK_LOCAL_PATH ${SDK_DIRECTORY}/local)

set(CMAKE_MODULE_PATH ${CMSDK_LOCAL_PATH}/cmake)
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

add_subdirectory(SDK SDK EXCLUDE_FROM_ALL)
add_subdirectory(app)
add_subdirectory(tests)


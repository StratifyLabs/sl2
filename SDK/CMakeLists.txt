cmake_minimum_required (VERSION 3.12)

project(sl-SDK)

set(API_IS_SDK TRUE CACHE INTERNAL "Build SDK projects")
option(IS_PULL "Pull SDK projects" ON)

set(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dependencies)
file(MAKE_DIRECTORY ${BASE_DIRECTORY})

message(STATUS "Base directory ${BASE_DIRECTORY}")

macro(sdk_add_subdirectory NAME BRANCH)
	cmsdk_git_clone_or_pull_branch(${BASE_DIRECTORY} ${NAME} https://github.com/StratifyLabs/${NAME}.git ${BRANCH})
	add_subdirectory(${BASE_DIRECTORY}/${NAME} ${NAME} EXCLUDE_FROM_ALL)
endmacro()

include(CTest)

set(STRATIFYOS_LWIP_PATH
	${CMAKE_CURRENT_SOURCE_DIR}/../InetAPI/lwip/include
	${CMAKE_CURRENT_SOURCE_DIR}/../InetAPI/lwip/lwip-2.1.2/src/include
	CACHE INTERNAL "LWIP INCLUDE PATHS")


sdk_add_subdirectory(API v1.7)
include(${BASE_DIRECTORY}/API/libraries/API.cmake)

set(MBEDTLS_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/mbedtls/config.h CACHE INTERNAL "MBEDTLS config file")

sdk_add_subdirectory(StratifyOS v4.3)
sdk_add_subdirectory(JsonAPI v1.5)
sdk_add_subdirectory(InetAPI v1.5)
sdk_add_subdirectory(CryptoAPI v1.4)
sdk_add_subdirectory(SosAPI v1.4)
sdk_add_subdirectory(HalAPI v1.3)
sdk_add_subdirectory(CloudAPI v1.3)
sdk_add_subdirectory(SwdAPI v1.2)
sdk_add_subdirectory(ServiceAPI v1.2)
sdk_add_subdirectory(UsbAPI v1.3)




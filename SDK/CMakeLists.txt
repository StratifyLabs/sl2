cmake_minimum_required (VERSION 3.12)

project(sl-SDK)

set(API_IS_SDK TRUE CACHE INTERNAL "Build SDK projects")
option(IS_PULL "Pull SDK projects" ON)

set(BASE_DIRECTORY $ENV{SPACES_WORKSPACE}/libs)

message(STATUS "Base directory ${BASE_DIRECTORY}")

macro(sdk_add_subdirectory NAME BRANCH)
	cmsdk_git_clone_or_pull_branch(${BASE_DIRECTORY} ${NAME} https://github.com/StratifyLabs/${NAME}.git ${BRANCH})
	add_subdirectory(${BASE_DIRECTORY}/${NAME} ${NAME} EXCLUDE_FROM_ALL)
endmacro()

include(CTest)

set(STRATIFYOS_LWIP_PATH
	${BASE_DIRECTORY}/InetAPI/lwip/include
	${BASE_DIRECTORY}/InetAPI/lwip/lwip-2.1.2/src/include
	CACHE INTERNAL "LWIP INCLUDE PATHS")

add_subdirectory(${BASE_DIRECTORY}/API API)
include(${BASE_DIRECTORY}/API/libraries/API.cmake)

set(MBEDTLS_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/mbedtls/config.h CACHE INTERNAL "MBEDTLS config file")

add_subdirectory(${BASE_DIRECTORY}/StratifyOS StratifyOS)
add_subdirectory(${BASE_DIRECTORY}/JsonAPI JsonAPI)
add_subdirectory(${BASE_DIRECTORY}/InetAPI InetAPI)
add_subdirectory(${BASE_DIRECTORY}/CryptoAPI CryptoAPI)
add_subdirectory(${BASE_DIRECTORY}/SosAPI SosAPI)
add_subdirectory(${BASE_DIRECTORY}/HalAPI HalAPI)
add_subdirectory(${BASE_DIRECTORY}/CloudAPI CloudAPI)
add_subdirectory(${BASE_DIRECTORY}/SwdAPI SwdAPI)
add_subdirectory(${BASE_DIRECTORY}/ServiceAPI ServiceAPI)
add_subdirectory(${BASE_DIRECTORY}/UsbAPI UsbAPI)






cmsdk2_add_executable(
	NAME sl
	CONFIG release
	ARCH ${CMSDK_ARCH}
	TARGET RELEASE_TARGET)

cmsdk2_add_sources(TARGET ${RELEASE_TARGET}
	DIRECTORY src)

target_compile_definitions(${RELEASE_TARGET}
	PRIVATE
	SL_CLOUD_API_KEY=\"${SL_CLOUD_API_KEY}\"
	VERSION=\"${CMAKE_PROJECT_VERSION}\"
	)

target_include_directories(${RELEASE_TARGET}
	PRIVATE
	src)


if(WIN32)
	target_link_libraries(${RELEASE_TARGET} PUBLIC -static)
endif()

set_property(TARGET ${RELEASE_TARGET} PROPERTY CXX_STANDARD 17)

set(DEPENDENCIES
	ServiceAPI
	HalAPI
	SosAPI
	TestAPI
	FsAPI
	VarAPI
	ThreadAPI
	UsbAPI)

cmsdk2_app_add_dependencies(
	TARGET ${RELEASE_TARGET}
	DEPENDENCIES ${DEPENDENCIES})

cmsdk2_add_executable(
	NAME sl
	OPTION admin
	CONFIG release
	ARCH ${CMSDK_ARCH}
	TARGET ADMIN_TARGET)

cmsdk2_copy_target(
	SOURCE ${RELEASE_TARGET}
	DESTINATION ${ADMIN_TARGET})

target_compile_definitions(${ADMIN_TARGET}
	PRIVATE
	-D__admin_release=1)

install(
	PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/build_release_link/${RELEASE_TARGET}${CMSDK_EXEC_SUFFIX}
	DESTINATION ${CMSDK_LOCAL_PATH}/bin RENAME sl${CMSDK_EXEC_SUFFIX})

install(
	PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/build_release_link/${ADMIN_TARGET}${CMSDK_EXEC_SUFFIX}
	DESTINATION ${CMSDK_LOCAL_PATH}/bin RENAME sla${CMSDK_EXEC_SUFFIX})




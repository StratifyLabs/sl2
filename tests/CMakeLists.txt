
set(PROGRAM_NAME ${CMAKE_CURRENT_SOURCE_DIR}/../app/build_release_link/sl_release_link)
set(TMP_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../tmp)
set(LOCAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../SDK/local)

set(TEST_LIST "")

macro(add_sl_test TEST_NAME WILL_FAIL IS_JSON ARGUMENTS)
	add_test(NAME ${TEST_NAME}
		COMMAND ${PROGRAM_NAME} ${ARGUMENTS}
		WORKING_DIRECTORY ${TMP_DIRECTORY}
		)

	set_tests_properties(${TEST_NAME} PROPERTIES
		WILL_FAIL ${WILL_FAIL}
		)

	set_tests_properties(${TEST_NAME} PROPERTIES
		RUN_SERIAL TRUE
		)

	if(${IS_JSON})
		add_test(NAME ${TEST_NAME}_json
			COMMAND ${PROGRAM_NAME} ${ARGUMENTS} --json
			WORKING_DIRECTORY ${TMP_DIRECTORY}
			)

		set_tests_properties(${TEST_NAME}_json PROPERTIES
			WILL_FAIL ${WILL_FAIL}
			)

		set_tests_properties(${TEST_NAME}_json PROPERTIES
			RUN_SERIAL TRUE
			)
	endif()

endmacro()

set(SIGN signkey=RhnmvxQ8D4tlh02L8693)
set(SIGN_APP signkey=162ZEPiD33bF1T8diV0t,signkeypassword=4AC673981E969BBC9C33933800960A7F57EC0F9036CAABB2E1CF09402E9B391E)

add_sl_test(initialize FALSE FALSE "--initialize")
add_sl_test(connection_connect FALSE TRUE "conn.connect")
add_sl_test(connection_connect_usb FALSE TRUE "conn.connect:path=/usb")
add_sl_test(connection_connect_usb_vid FALSE TRUE "conn.connect:path=/usb/20A0")
add_sl_test(connection_connect_usb_vid_pid FALSE TRUE "conn.connect:path=/usb/20A0/41D5")
add_sl_test(connection_connect_usb_vid_pid_interfacenumber_serialnumber FALSE TRUE "conn.connect:path=/usb/20A0/41D5/00/000000003037303133345103003F0016")
add_sl_test(connection_connect_usb_vid_pid_interface_number FALSE TRUE "conn.connect:path=/usb/20A0/41D5/00")
add_sl_test(connection_connect_bad TRUE TRUE "conn.connect:path=badpath")
add_sl_test(cloud_install_FFxXbp1ExySM7DaLrBA4 FALSE TRUE cloud.install:id=FFxXbp1ExySM7DaLrBA4,${SIGN})
add_sl_test(cloud_install_Kvp7xXzdO94kyCWAAcmW_sign FALSE FALSE cloud.install:id=Kvp7xXzdO94kyCWAAcmW,${SIGN_APP})
add_sl_test(cloud_install_Kvp7xXzdO94kyCWAAcmW TRUE TRUE cloud.install:id=Kvp7xXzdO94kyCWAAcmW)
add_sl_test(cloud_install_Kvp7xXzdO94kyCWAAcmW_dest_host FALSE TRUE cloud.install:id=Kvp7xXzdO94kyCWAAcmW,dest=host@.,arch=v7em_f4sh)
add_sl_test(cloud_install_Kvp7xXzdO94kyCWAAcmW_dest_host_hello_world FALSE TRUE cloud.install:id=Kvp7xXzdO94kyCWAAcmW,dest=host@hello_world_v7em_f4sh,arch=v7em_f4sh)
add_sl_test(cloud_install_Kvp7xXzdO94kyCWAAcmW_dest_host_no_arch TRUE TRUE cloud.install:id=Kvp7xXzdO94kyCWAAcmW,dest=host@.)
add_sl_test(cloud_install_Kvp7xXzdO94kyCWAAcmW_dest_host_helloworld FALSE TRUE cloud.install:id=Kvp7xXzdO94kyCWAAcmW,dest=host@HelloWorld_build_release_v7em_f4sh,arch=v7em_f4sh)
add_sl_test(cloud_install_Kvp7xXzdO94kyCWAAcmW_dest_host_helloworld_no_arch FALSE TRUE cloud.install:id=Kvp7xXzdO94kyCWAAcmW,dest=host@HelloWorld_build_release_v7em_f4sh)
add_sl_test(cloud_app_install_helloworld_release_v7em_f4sh TRUE TRUE app.install:path=HelloWorld_build_release_v7em_f4sh)
add_sl_test(cloud_app_install_helloworld_release_v7em_f4sh_sign FALSE TRUE app.install:path=HelloWorld_build_release_v7em_f4sh,${SIGN_APP})
add_sl_test(cloud_run_app_HelloWorld FALSE FALSE app.run:path=HelloWorld,terminal)
add_sl_test(cloud_fs_exists_app_flash_hello_world FALSE TRUE fs.exists:path=device@/app/flash/HelloWorld)
add_sl_test(cloud_app_run_HelloWorld FALSE TRUE app.run:path=HelloWorld)
add_sl_test(cloud_app_run_app_flash_HelloWorld FALSE TRUE app.run:path=device@/app/flash/HelloWorld)
add_sl_test(cloud_terminal_run_helloworld FALSE TRUE terminal.run:duration=1)
add_sl_test(app_clean FALSE FALSE app.clean:path=device@/app/flash)
add_sl_test(cloud_app_install_helloworld_release_v7em_f4sh_sign_rename FALSE TRUE app.install:path=HelloWorld_build_release_v7em_f4sh,name=Test,${SIGN_APP})
add_sl_test(app_ping_path_device_app_flash_Test FALSE TRUE app.ping:path=device@/app/flash/Test)
add_sl_test(app_clean_test FALSE FALSE app.clean)
add_sl_test(os_ping FALSE TRUE "os.ping")
add_sl_test(cloud_refresh FALSE TRUE "cloud.refresh")
add_sl_test(user_ping FALSE TRUE "user.ping")
add_sl_test(user_ping_bad_id TRUE TRUE "user.ping:id=1235678910111213")
add_sl_test(team_ping_stratifylabs FALSE TRUE "team.ping:id=41zVSUkV7jVX9bviWjpR")
add_sl_test(thing_ping_000000003037303133345103003F0016 FALSE TRUE "thing.ping:id=000000003037303133345103003F0016")
add_sl_test(thing_ping_000000003037303133345103003F TRUE TRUE "thing.ping:id=000000003037303133345103003F")
add_sl_test(fs_list_app FALSE TRUE "fs.list:path=device@/app")
add_sl_test(fs_list_app_hide_false FALSE TRUE "fs.list:path=device@/app,hide=false")
add_sl_test(fs_list_app_hide_true FALSE TRUE "fs.list:path=device@/app,hide=true")
add_sl_test(fs_list_app_ram_hide_true FALSE TRUE "fs.list:path=device@/app/ram,hide=true")
add_sl_test(fs_list_app_ram_hide_false FALSE TRUE "fs.list:path=device@/app/ram,hide=false")
add_sl_test(fs_list_app_ram0_hide_true TRUE TRUE "fs.list:path=device@/app/ram0,hide=true")
add_sl_test(fs_list_app_ram0_hide_false TRUE TRUE "fs.list:path=device@/app/ram0,hide=false")
add_sl_test(fs_list_go TRUE TRUE "fs.list:path=device@/app,go")
add_sl_test(fs_list_host_dot FALSE TRUE "fs.list:path=host@.")
add_sl_test(fs_list_dev FALSE TRUE "fs.list:path=device@/dev")
add_sl_test(fs_list_dev_sys FALSE TRUE "fs.list:path=device@/dev/sys")
add_sl_test(fs_list_dev_nosys TRUE TRUE "fs.list:path=device@/dev/none")
add_sl_test(fs_remove_dev_sys TRUE TRUE "fs.remove:path=device@/dev/sys")
add_sl_test(fs_copy_settings FALSE FALSE "fs.copy:source=host@sl_workspace_settings.json,dest=device@/app/flash/settings.json")
add_sl_test(fs_copy_settings_bad_dest TRUE TRUE "fs.copy:source=host@sl_workspace_settings.json,dest=device@/dev/settings.json")
add_sl_test(fs_copy_settings_back FALSE TRUE "fs.copy:dest=host@dev_settings.json,source=device@/app/flash/settings.json")
add_sl_test(fs_copy_settings_back_validate FALSE TRUE "fs.validate:path=host@dev_settings.json")
add_sl_test(fs_copy_settings_verify FALSE TRUE "fs.verify:source=host@dev_settings.json,dest=device@/app/flash/settings.json")
add_sl_test(fs_copy_settings_back_overwrite_false TRUE FALSE "fs.copy:dest=host@dev_settings.json,source=device@/app/flash/settings.json,overwrite=false")
add_sl_test(fs_exists_settings FALSE FALSE "fs.exists:path=device@/app/flash/settings.json")
add_sl_test(fs_remove_settings FALSE FALSE "fs.remove:path=device@/app/flash/settings.json")
add_sl_test(fs_remove_settings_will_fail TRUE TRUE "fs.exists:path=device@/app/flash/settings.json")
add_sl_test(fs_copy_settings_result_json FALSE FALSE fs.copy:source=host@sl_workspace_settings.json,dest=host@result.json)
add_sl_test(fs_copy_result_json FALSE FALSE fs.copy:source=host@result.json,dest=device@/app/flash/result.json)
add_sl_test(fs_verify_result_json FALSE TRUE fs.verify:source=host@result.json,dest=device@/app/flash/result.json)
add_sl_test(fs_validate_result_json FALSE TRUE fs.validate:path=device@/app/flash/result.json)
add_sl_test(fs_copy_result_result2 FALSE TRUE fs.copy:source=device@/app/flash/result.json,dest=device@/app/flash/result2.json)
add_sl_test(fs_remove_result2 FALSE FALSE "fs.remove:path=device@/app/flash/result2.json")
add_sl_test(fs_copy_result_json_no_overwrite TRUE TRUE fs.copy:source=host@result.json,dest=device@/app/flash/result.json,overwrite=false)
add_sl_test(fs_mkdir_tmp0 FALSE TRUE fs.mkdir:path=host@tmp0)
add_sl_test(fs_mkdir_tmp0_repeat FALSE TRUE fs.mkdir:path=host@tmp0)
add_sl_test(fs_remove_tmp0 FALSE FALSE fs.remove:path=host@tmp0,recursive)
add_sl_test(task_list FALSE TRUE task.list)
add_sl_test(task_list_name_sys FALSE TRUE task.list:name=sys)
add_sl_test(task_list_name_none FALSE TRUE task.list:name=none)
add_sl_test(task_list_pid_0 FALSE TRUE task.list:pid=0)
add_sl_test(task_list_pid_1 FALSE TRUE task.list:pid=1)
add_sl_test(task_bad TRUE TRUE task.bad:pid=1)
add_sl_test(task_list_bad TRUE TRUE task.list:bad)
add_sl_test(task_analyze FALSE TRUE task.analyze:duration=1)
add_sl_test(task_analyze_period_100 FALSE TRUE task.analyze:duration=1,period=100)
add_sl_test(task_analyze_name_sys FALSE TRUE task.analyze:duration=1,name=sys)
add_sl_test(debug_analyze FALSE TRUE debug.analyze)
add_sl_test(report_ping_BIQQQkh045yGLXJXnAF9 FALSE TRUE report.ping:id=BIQQQkh045yGLXJXnAF9)
add_sl_test(report_ping_display_BIQQQkh045yGLXJXnAF9 FALSE FALSE report.ping:id=BIQQQkh045yGLXJXnAF9,display)
add_sl_test(report_ping_display_encode_BIQQQkh045yGLXJXnAF9 FALSE TRUE report.ping:id=BIQQQkh045yGLXJXnAF9,display,encode)
add_sl_test(report_download_BIQQQkh045yGLXJXnAF9 FALSE TRUE report.download:id=BIQQQkh045yGLXJXnAF9)
add_sl_test(report_download_BIQQQkh045yGLXJXn TRUE TRUE report.download:id=BIQQQkh045yGLXJX)
add_sl_test(report_ping_BIQQQkh045yGLXJXn TRUE TRUE report.ping:id=BIQQQkh045yGLXJX)

add_sl_test(key_ping_RhnmvxQ8D4tlh02L8693 FALSE TRUE key.ping:id=RhnmvxQ8D4tlh02L8693)
add_sl_test(key_sign_RhnmvxQ8D4tlh02L8693 FALSE TRUE key.sign:id=RhnmvxQ8D4tlh02L8693,path=dev_settings.json)
add_sl_test(key_sign_append_RhnmvxQ8D4tlh02L8693 FALSE TRUE key.sign:id=RhnmvxQ8D4tlh02L8693,path=dev_settings.json,append)
add_sl_test(key_verify_RhnmvxQ8D4tlh02L8693 FALSE TRUE key.verify:id=RhnmvxQ8D4tlh02L8693,path=dev_settings.json.signed)
add_sl_test(key_sign_suffix_RhnmvxQ8D4tlh02L8693 FALSE TRUE key.sign:id=RhnmvxQ8D4tlh02L8693,path=dev_settings.json,append,suffix=.sgn)
add_sl_test(key_verify_suffix_RhnmvxQ8D4tlh02L8693 FALSE TRUE key.verify:id=RhnmvxQ8D4tlh02L8693,path=dev_settings.json.sgn)
add_sl_test(key_download_RhnmvxQ8D4tlh02L8693 FALSE TRUE key.download:id=RhnmvxQ8D4tlh02L8693)
add_sl_test(fs_validate_RhnmvxQ8D4tlh02L8693_json FALSE TRUE fs.validate:path=host@RhnmvxQ8D4tlh02L8693.json)
add_sl_test(key_sign_RhnmvxQ8D4tlh02L8693_suffix_json FALSE TRUE key.sign:id=RhnmvxQ8D4tlh02L8693.json,path=dev_settings.json)
add_sl_test(key_sign_append_RhnmvxQ8D4tlh02L8693_suffix_json FALSE TRUE key.sign:id=RhnmvxQ8D4tlh02L8693.json,path=dev_settings.json,append)
add_sl_test(key_verify_RhnmvxQ8D4tlh02L8693_suffix_json FALSE TRUE key.verify:id=RhnmvxQ8D4tlh02L8693.json,path=dev_settings.json.signed)
add_sl_test(key_verify_RhnmvxQ8D4tlh02L8693_suffix_json_badsignature TRUE TRUE key.verify:id=RhnmvxQ8D4tlh02L8693.json,path=dev_settings.json,signature=2FDB792D474B2B98BC6BA6E55AD4A2B0CAE2D3E2DC9E94CC4C27E09B17A0588BB00B07897FAA0D329FDFFC7AF31AA40F4A572E4D46E96C15BEEA400E60889B00)

add_sl_test(key_ping_162ZEPiD33bF1T8diV0t FALSE TRUE key.ping:id=162ZEPiD33bF1T8diV0t)
add_sl_test(key_sign_162ZEPiD33bF1T8diV0t FALSE TRUE key.sign:id=162ZEPiD33bF1T8diV0t,password=4AC673981E969BBC9C33933800960A7F57EC0F9036CAABB2E1CF09402E9B391E,path=dev_settings.json)
add_sl_test(key_sign_append_162ZEPiD33bF1T8diV0t FALSE TRUE key.sign:id=162ZEPiD33bF1T8diV0t,password=4AC673981E969BBC9C33933800960A7F57EC0F9036CAABB2E1CF09402E9B391E,path=dev_settings.json,append)
add_sl_test(key_verify_162ZEPiD33bF1T8diV0t FALSE TRUE key.verify:id=162ZEPiD33bF1T8diV0t,path=dev_settings.json.signed)
add_sl_test(key_sign_suffix_162ZEPiD33bF1T8diV0t FALSE TRUE key.sign:id=162ZEPiD33bF1T8diV0t,password=4AC673981E969BBC9C33933800960A7F57EC0F9036CAABB2E1CF09402E9B391E,path=dev_settings.json,append,suffix=.sgn)
add_sl_test(key_verify_suffix_162ZEPiD33bF1T8diV0t FALSE TRUE key.verify:id=162ZEPiD33bF1T8diV0t,path=dev_settings.json.sgn)


if(APPLE)
	add_sl_test(compiler_install_no_hash FALSE TRUE cloud.install:compiler,url=https://github.com/StratifyLabs/StratifyOS/releases/download/compilerv8/compiler.macosx_x86_64_sblob,dest=${LOCAL_DIR})
	add_sl_test(compiler_install FALSE TRUE cloud.install:compiler,url=https://github.com/StratifyLabs/StratifyOS/releases/download/compilerv8/compiler.macosx_x86_64_sblob,hash=60BB5EDFCB9CC3DB3FF409870F6D94BB4E7C084E2D2449499F044B6350641823,dest=${LOCAL_DIR})
	add_sl_test(compiler_install_bad_url TRUE TRUE cloud.install:compiler,url=https://github.com/StratifyLabs/StratifyOS/releases/download/compilerv8/compiler,dest=${LOCAL_DIR})
	add_sl_test(compiler_install_bad_hash TRUE TRUE cloud.install:compiler,url=https://github.com/StratifyLabs/StratifyOS/releases/download/compilerv8/compiler.macosx_x86_64_sblob,hash=60BB5EDFCB9CC3DB3FF409870F6D94BB4E7C084E2D2449499F044B6350641800,dest=${LOCAL_DIR})
endif()

#[[
add_sl_test(rm_helloworld FALSE FALSE "fs.execute:command='rm -Rf ./HelloWorld'")
add_sl_test(clone_helloworld FALSE FALSE "fs.execute:command='git clone https://github.com/StratifyLabs/HelloWorld.git'")
add_sl_test(mkdir_cmake_arm_helloworld FALSE FALSE "fs.mkdir:path=host@HelloWorld/cmake_arm")
add_sl_test(cmake_helloworld FALSE FALSE "fs.execute:directory='HelloWorld/cmake_arm',command='cmake .. -GNinja'")
add_sl_test(build_helloworld FALSE FALSE "fs.execute:directory='HelloWorld/cmake_arm',command='ninja'")
add_sl_test(app_publish_helloworld_provision FALSE FALSE "app.publish:path=HelloWorld,fork")
add_sl_test(cloud_remove_helloworld_bad_argument TRUE FALSE "cloud.remove:path=HelloWorld,fork")
add_sl_test(cloud_remove_helloworld FALSE FALSE "cloud.remove:path=HelloWorld")
add_sl_test(cloud_remove_helloworld_no_entity TRUE FALSE "cloud.remove:path=HelloWorld")
add_sl_test(app_install_helloworld1 FALSE TRUE "app.install:path=HelloWorld,${SIGN_APP}")
add_sl_test(app_install_helloworld2 FALSE TRUE "app.ping:path=device@/app/flash/HelloWorld")
add_sl_test(app_install_helloworld3 TRUE TRUE "app.ping:path=device@/app/ram/HelloWorld")
add_sl_test(app_install_helloworld4 FALSE TRUE "app.install:path=HelloWorld,ram,${SIGN_APP}")
add_sl_test(app_install_helloworld5 TRUE TRUE "app.ping:path=device@/app/flash/HelloWorld")
add_sl_test(app_install_helloworld6 FALSE TRUE "app.ping:path=device@/app/ram/HelloWorld")
add_sl_test(app_install_helloworld7 FALSE TRUE "app.install:path=HelloWorld,ramsize=8192,${SIGN_APP}")
add_sl_test(app_install_helloworld8 FALSE TRUE "app.install:path=HelloWorld,datasize=4096,${SIGN_APP}")
add_sl_test(app_install_helloworld9 FALSE FALSE "app.install:path=HelloWorld,run,terminal,${SIGN_APP}")
add_sl_test(app_install_helloworld10 FALSE TRUE "app.install:path=HelloWorld,startup,${SIGN_APP}")
add_sl_test(app_install_helloworld11 FALSE TRUE "app.install:path=HelloWorld,run,${SIGN_APP}")
#]]

# next -- do authentication testing
#sl os.authenticate
#sl app.install:authenticated
#sl cloud.install:id=<os id>,key -- make sure key is inserted to binary, fetched from thing

#[[
# Cmake has problems running this sequence
# Haven't seen problems running it manually
add_sl_test(os_reset1 FALSE TRUE "os.reset")
add_sl_test(os_reset2 FALSE TRUE "os.ping")
add_sl_test(os_reset3 TRUE TRUE "os.ping:boot")
add_sl_test(os_reset4 FALSE TRUE "os.reset:bootloader")
add_sl_test(os_reset5 FALSE TRUE "os.ping:boot")
add_sl_test(os_reset6 TRUE TRUE "os.ping")
add_sl_test(os_reset7 FALSE TRUE "os.reset:boot")
add_sl_test(os_reset8 FALSE TRUE "os.ping:boot")
add_sl_test(os_reset9 TRUE TRUE "os.ping")
add_sl_test(os_reset10 FALSE TRUE "os.reset")
add_sl_test(os_reset11 TRUE TRUE "os.ping:boot")
add_sl_test(os_reset12 FALSE TRUE "os.ping")
#]]

#add_sl_test(os_reset_2 FALSE TRUE "os.reset")


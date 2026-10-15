add_executable(Bugdom2ScriptGameplayTests test_script_gameplay.c)
add_executable(Bugdom2ScriptDialogTests test_script_dialog_arbitration.c)

foreach(test_target IN ITEMS Bugdom2ScriptGameplayTests Bugdom2ScriptDialogTests)
	target_include_directories(${test_target} PRIVATE
		$<TARGET_PROPERTY:${GAME_TARGET},INCLUDE_DIRECTORIES>
		${CMAKE_SOURCE_DIR}/extern/Pomme/src)
	target_compile_definitions(${test_target} PRIVATE
		$<TARGET_PROPERTY:${GAME_TARGET},COMPILE_DEFINITIONS>)
	target_compile_options(${test_target} PRIVATE
		-Wall -Wextra -Werror -Wno-multichar -Wno-unknown-pragmas
		-ffunction-sections -fdata-sections)
	target_link_options(${test_target} PRIVATE -Wl,--gc-sections)
	if(SDL_STATIC)
		target_link_libraries(${test_target} PRIVATE SDL3::SDL3-static m)
	else()
		target_link_libraries(${test_target} PRIVATE SDL3::SDL3 m)
	endif()
	add_test(NAME ${test_target} COMMAND ${test_target})
	set_tests_properties(${test_target} PROPERTIES TIMEOUT 15)
endforeach()

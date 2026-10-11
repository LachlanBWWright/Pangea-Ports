cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(PangeaScript LANGUAGES C)
	include(CTest)
endif()

include(FetchContent)

FetchContent_Declare(
	pangea_lua
	URL https://www.lua.org/ftp/lua-5.4.6.tar.gz
	URL_HASH SHA256=7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(pangea_lua)

file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${pangea_lua_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM LUA_SOURCES "${pangea_lua_SOURCE_DIR}/src/lua.c" "${pangea_lua_SOURCE_DIR}/src/luac.c" "${pangea_lua_SOURCE_DIR}/src/onelua.c")
add_library(PangeaLua STATIC ${LUA_SOURCES})
target_include_directories(PangeaLua PUBLIC "${pangea_lua_SOURCE_DIR}/src")
target_compile_features(PangeaLua PUBLIC c_std_17)

add_library(PangeaScript STATIC
	pangea_script.c
	pangea_script_config.c
	pangea_script_backend_lua.c
)

target_include_directories(PangeaScript PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(PangeaScript PUBLIC c_std_17)
target_link_libraries(PangeaScript PUBLIC PangeaLua)

if(UNIX AND NOT EMSCRIPTEN)
	target_link_libraries(PangeaLua PUBLIC m)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND BUILD_TESTING)
	find_package(Python3 REQUIRED COMPONENTS Interpreter)
	add_test(NAME ScriptHookCoverage
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/check_script_coverage.py
		--json ${CMAKE_CURRENT_BINARY_DIR}/script-hook-coverage.json)
	add_test(NAME ScriptCoverageCheckerTests
		COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tests -p test_script_coverage.py -v)
	set_tests_properties(ScriptCoverageCheckerTests PROPERTIES
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
	add_test(NAME NativeScriptSmokeHarnessTests
		COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tests -p test_native_script_smoke.py -v)
	set_tests_properties(NativeScriptSmokeHarnessTests PROPERTIES
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
	add_test(NAME NativeScriptRunnerTests
		COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tests -p test_native_script_runners.py -v)
	set_tests_properties(NativeScriptRunnerTests PROPERTIES
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
	# Exercise the runtime independently of Lua using the controllable mock backend.
	add_executable(PangeaScriptCoreTests ../../tests/test_pangea_script.c
		pangea_script.c pangea_script_config.c)
	target_include_directories(PangeaScriptCoreTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_compile_features(PangeaScriptCoreTests PRIVATE c_std_17)
	if(UNIX AND NOT EMSCRIPTEN)
		target_link_libraries(PangeaScriptCoreTests PRIVATE m)
	endif()
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/core-tests")
	add_test(NAME PangeaScriptCoreTests COMMAND PangeaScriptCoreTests)
	set_tests_properties(PangeaScriptCoreTests PROPERTIES
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/core-tests")
	add_executable(PangeaScriptLuaTests ../../tests/test_pangea_script_lua.c)
	target_link_libraries(PangeaScriptLuaTests PRIVATE PangeaScript)
	target_compile_features(PangeaScriptLuaTests PRIVATE c_std_17)
	add_executable(PangeaTransactionalLuaTests ../../tests/test_transactional_lua.c)
	target_link_libraries(PangeaTransactionalLuaTests PRIVATE PangeaScript)
	target_compile_features(PangeaTransactionalLuaTests PRIVATE c_std_17)
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/transactional-tests")
	add_test(NAME PangeaTransactionalLuaTests COMMAND PangeaTransactionalLuaTests)
	set_tests_properties(PangeaTransactionalLuaTests PROPERTIES
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/transactional-tests")
	add_executable(PangeaGameScriptSampleTests ../../tests/test_game_script_samples.c)
	target_link_libraries(PangeaGameScriptSampleTests PRIVATE PangeaScript)
	target_compile_features(PangeaGameScriptSampleTests PRIVATE c_std_17)
	if(EMSCRIPTEN)
		# The conformance fixtures exercise nested Lua/native callbacks and exceed Emscripten's 64 KiB default stack.
		target_link_options(PangeaScriptLuaTests PRIVATE "-sSTACK_SIZE=262144")
		target_link_options(PangeaTransactionalLuaTests PRIVATE "-sSTACK_SIZE=262144")
		target_link_options(PangeaGameScriptSampleTests PRIVATE "-sSTACK_SIZE=262144")
	endif()
	if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_options(PangeaScriptLuaTests PRIVATE -Wall -Wextra -Werror)
		target_compile_options(PangeaGameScriptSampleTests PRIVATE -Wall -Wextra -Werror)
	endif()
	add_test(NAME PangeaScriptLuaTests COMMAND PangeaScriptLuaTests)
	add_test(NAME OttoMaticScriptSamples COMMAND PangeaGameScriptSampleTests ottomatic)
	add_test(NAME BugdomScriptSamples COMMAND PangeaGameScriptSampleTests bugdom)
	add_test(NAME Bugdom2ScriptSamples COMMAND PangeaGameScriptSampleTests bugdom2)
	add_test(NAME CroMagRallyScriptSamples COMMAND PangeaGameScriptSampleTests cromag)
	add_test(NAME NanosaurScriptSamples COMMAND PangeaGameScriptSampleTests nanosaur)
	add_test(NAME Nanosaur2ScriptSamples COMMAND PangeaGameScriptSampleTests nanosaur2)
	add_test(NAME BillyFrontierScriptSamples COMMAND PangeaGameScriptSampleTests billy)
	add_test(NAME MightyMikeScriptSamples COMMAND PangeaGameScriptSampleTests mightymike)
	# These fixtures create modules with shared filenames; parallel CTest runs
	# need separate working directories for each game.
	foreach(sample_test IN ITEMS OttoMaticScriptSamples BugdomScriptSamples
		Bugdom2ScriptSamples CroMagRallyScriptSamples NanosaurScriptSamples
		Nanosaur2ScriptSamples BillyFrontierScriptSamples MightyMikeScriptSamples)
		file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${sample_test}")
		set_tests_properties(${sample_test} PROPERTIES
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${sample_test}")
	endforeach()
	if(NOT EMSCRIPTEN)
		add_executable(PangeaShippedGameScriptTests ../../tests/test_shipped_game_scripts.c)
		target_link_libraries(PangeaShippedGameScriptTests PRIVATE PangeaScript)
		target_compile_definitions(PangeaShippedGameScriptTests PRIVATE
			PANGEA_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../..")
		if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
			target_compile_options(PangeaShippedGameScriptTests PRIVATE -Wall -Wextra -Werror)
		endif()
		foreach(game IN ITEMS OttoMatic-Android Bugdom-android Bugdom2-Android
			CroMagRally-Android Nanosaur-android Nanosaur2-Android
			BillyFrontier-Android MightyMike-Android)
			file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shipped-${game}")
			add_test(NAME ShippedScripts-${game} COMMAND PangeaShippedGameScriptTests ${game})
			set_tests_properties(ShippedScripts-${game} PROPERTIES
				WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shipped-${game}")
		endforeach()
	endif()
	# Assertions are executable conformance checks, including in Release builds.
	foreach(test_target IN ITEMS PangeaScriptCoreTests PangeaScriptLuaTests
		PangeaGameScriptSampleTests PangeaShippedGameScriptTests PangeaTransactionalLuaTests)
		if(TARGET ${test_target})
			if(MSVC)
				target_compile_options(${test_target} PRIVATE /UNDEBUG)
			else()
				target_compile_options(${test_target} PRIVATE -UNDEBUG)
			endif()
			if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
				target_compile_options(${test_target} PRIVATE -Wall -Wextra -Werror)
			endif()
		endif()
	endforeach()
endif()

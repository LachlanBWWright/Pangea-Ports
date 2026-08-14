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
list(REMOVE_ITEM LUA_SOURCES "${pangea_lua_SOURCE_DIR}/src/lua.c" "${pangea_lua_SOURCE_DIR}/src/luac.c")
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
	add_executable(PangeaScriptLuaTests ../../tests/test_pangea_script_lua.c)
	target_link_libraries(PangeaScriptLuaTests PRIVATE PangeaScript)
	target_compile_features(PangeaScriptLuaTests PRIVATE c_std_17)
	add_executable(PangeaGameScriptSampleTests ../../tests/test_game_script_samples.c)
	target_link_libraries(PangeaGameScriptSampleTests PRIVATE PangeaScript)
	target_compile_features(PangeaGameScriptSampleTests PRIVATE c_std_17)
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
endif()

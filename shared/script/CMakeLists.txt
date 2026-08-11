cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(PangeaScript LANGUAGES C)
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

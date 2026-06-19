set(PANGEA_LUA_VERSION "5.4.7")

set(PANGEA_SCRIPT_SOURCES
	pangea_script.c
	pangea_script_config.c
	pangea_script_backend_lua.c
	third_party/lua/src/lapi.c
	third_party/lua/src/lauxlib.c
	third_party/lua/src/lbaselib.c
	third_party/lua/src/lcode.c
	third_party/lua/src/lcorolib.c
	third_party/lua/src/lctype.c
	third_party/lua/src/ldebug.c
	third_party/lua/src/ldo.c
	third_party/lua/src/ldump.c
	third_party/lua/src/lfunc.c
	third_party/lua/src/lgc.c
	third_party/lua/src/llex.c
	third_party/lua/src/lmem.c
	third_party/lua/src/lobject.c
	third_party/lua/src/lopcodes.c
	third_party/lua/src/lparser.c
	third_party/lua/src/lstate.c
	third_party/lua/src/lstring.c
	third_party/lua/src/ltable.c
	third_party/lua/src/ltablib.c
	third_party/lua/src/ltm.c
	third_party/lua/src/lundump.c
	third_party/lua/src/lutf8lib.c
	third_party/lua/src/lvm.c
	third_party/lua/src/lzio.c
	third_party/lua/src/lmathlib.c
	third_party/lua/src/lstrlib.c
)

add_library(PangeaScript STATIC ${PANGEA_SCRIPT_SOURCES})

target_include_directories(PangeaScript PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(PangeaScript PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/src
)

target_compile_features(PangeaScript PUBLIC c_std_17)
target_compile_definitions(PangeaScript PRIVATE LUA_USE_C89=1 PANGEA_SCRIPT_LUA_VERSION="${PANGEA_LUA_VERSION}")

if(NOT WIN32)
	target_link_libraries(PangeaScript PUBLIC m)
endif()

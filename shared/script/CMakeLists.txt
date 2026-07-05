set(PANGEA_LUA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.8")
set(PANGEA_LUA_SRC "${PANGEA_LUA_ROOT}/src")

set(PANGEA_LUA_SOURCES
	"${PANGEA_LUA_SRC}/lapi.c"
	"${PANGEA_LUA_SRC}/lauxlib.c"
	"${PANGEA_LUA_SRC}/lbaselib.c"
	"${PANGEA_LUA_SRC}/lcode.c"
	"${PANGEA_LUA_SRC}/lcorolib.c"
	"${PANGEA_LUA_SRC}/lctype.c"
	"${PANGEA_LUA_SRC}/ldebug.c"
	"${PANGEA_LUA_SRC}/ldo.c"
	"${PANGEA_LUA_SRC}/ldump.c"
	"${PANGEA_LUA_SRC}/lfunc.c"
	"${PANGEA_LUA_SRC}/lgc.c"
	"${PANGEA_LUA_SRC}/llex.c"
	"${PANGEA_LUA_SRC}/lmathlib.c"
	"${PANGEA_LUA_SRC}/lmem.c"
	"${PANGEA_LUA_SRC}/lobject.c"
	"${PANGEA_LUA_SRC}/lopcodes.c"
	"${PANGEA_LUA_SRC}/lparser.c"
	"${PANGEA_LUA_SRC}/lstate.c"
	"${PANGEA_LUA_SRC}/lstring.c"
	"${PANGEA_LUA_SRC}/lstrlib.c"
	"${PANGEA_LUA_SRC}/ltable.c"
	"${PANGEA_LUA_SRC}/ltablib.c"
	"${PANGEA_LUA_SRC}/ltm.c"
	"${PANGEA_LUA_SRC}/lundump.c"
	"${PANGEA_LUA_SRC}/lutf8lib.c"
	"${PANGEA_LUA_SRC}/lvm.c"
	"${PANGEA_LUA_SRC}/lzio.c"
)

add_library(PangeaLua STATIC ${PANGEA_LUA_SOURCES})

target_include_directories(PangeaLua PUBLIC
	"${PANGEA_LUA_SRC}"
)

target_compile_features(PangeaLua PUBLIC c_std_17)

if(UNIX)
	target_link_libraries(PangeaLua PUBLIC m)
endif()

set(PANGEA_SCRIPT_SOURCES
	pangea_script.c
	pangea_script_config.c
	pangea_script_backend_lua.c
)

add_library(PangeaScript STATIC ${PANGEA_SCRIPT_SOURCES})

target_include_directories(PangeaScript PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(PangeaScript PUBLIC c_std_17)

target_link_libraries(PangeaScript PUBLIC PangeaLua)

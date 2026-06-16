option(PANGEA_SCRIPT_USE_DUKTAPE "Use Duktape for native script execution when available" ON)

set(PANGEA_SCRIPT_SOURCES
	pangea_script.c
	pangea_script_config.c
)

if(EMSCRIPTEN)
	list(APPEND PANGEA_SCRIPT_SOURCES pangea_script_backend_emscripten.c)
elseif(PANGEA_SCRIPT_USE_DUKTAPE AND NOT ANDROID)
	find_library(PANGEA_SCRIPT_DUKTAPE_LIBRARY NAMES duktape libduktape.so.207)
	find_path(PANGEA_SCRIPT_DUKTAPE_INCLUDE_DIR NAMES duktape.h)
endif()

if(NOT EMSCRIPTEN)
	if(PANGEA_SCRIPT_DUKTAPE_LIBRARY AND PANGEA_SCRIPT_DUKTAPE_INCLUDE_DIR)
		list(APPEND PANGEA_SCRIPT_SOURCES pangea_script_backend_duktape.c)
	else()
		list(APPEND PANGEA_SCRIPT_SOURCES pangea_script_backend_none.c)
	endif()
endif()

add_library(PangeaScript STATIC ${PANGEA_SCRIPT_SOURCES})

target_include_directories(PangeaScript PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(PangeaScript PUBLIC c_std_17)

if(PANGEA_SCRIPT_DUKTAPE_LIBRARY AND PANGEA_SCRIPT_DUKTAPE_INCLUDE_DIR)
	target_include_directories(PangeaScript PRIVATE ${PANGEA_SCRIPT_DUKTAPE_INCLUDE_DIR})
	target_link_libraries(PangeaScript PUBLIC ${PANGEA_SCRIPT_DUKTAPE_LIBRARY})
	target_compile_definitions(PangeaScript PRIVATE PANGEA_SCRIPT_USE_DUKTAPE=1)
endif()

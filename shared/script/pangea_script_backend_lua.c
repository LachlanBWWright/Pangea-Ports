#include "pangea_script_backend.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PangeaScriptBackend
{
	lua_State* lua;
	PangeaScriptGameInfo gameInfo;
	int entryReference;
};

static void copy_error(char* error, int capacity, const char* message)
{
	if (error && capacity > 0) snprintf(error, (size_t) capacity, "%s", message ? message : "Lua error");
}

static void instruction_budget_hook(lua_State* lua, lua_Debug* debug)
{
	(void) debug;
	luaL_error(lua, "script instruction budget exceeded");
}

static int protected_call(lua_State* lua, int arguments, int results, char* error, int errorCapacity)
{
	lua_sethook(lua, instruction_budget_hook, LUA_MASKCOUNT, 100000);
	int status = lua_pcall(lua, arguments, results, 0);
	lua_sethook(lua, NULL, 0, 0);
	if (status == LUA_OK) return PANGEA_SCRIPT_OK;
	copy_error(error, errorCapacity, lua_tostring(lua, -1));
	lua_pop(lua, 1);
	return strstr(error ? error : "", "instruction budget") ? PANGEA_SCRIPT_BUDGET_EXCEEDED : PANGEA_SCRIPT_RUNTIME_ERROR;
}

static void push_vector(lua_State* lua, float x, float y, float z)
{
	lua_createtable(lua, 0, 3);
	lua_pushnumber(lua, x); lua_setfield(lua, -2, "x");
	lua_pushnumber(lua, y); lua_setfield(lua, -2, "y");
	lua_pushnumber(lua, z); lua_setfield(lua, -2, "z");
}

static bool read_vector(lua_State* lua, int index, PangeaScriptVector3* vector)
{
	if (!lua_istable(lua, index) || !vector) return false;
	lua_getfield(lua, index, "x"); vector->x = (float) lua_tonumber(lua, -1); lua_pop(lua, 1);
	lua_getfield(lua, index, "y"); vector->y = (float) lua_tonumber(lua, -1); lua_pop(lua, 1);
	lua_getfield(lua, index, "z"); vector->z = (float) lua_tonumber(lua, -1); lua_pop(lua, 1);
	return true;
}

static void push_handle(lua_State* lua, PangeaScriptObjectHandle handle)
{
	lua_createtable(lua, 0, 2);
	lua_pushinteger(lua, handle.id); lua_setfield(lua, -2, "id");
	lua_pushinteger(lua, handle.generation); lua_setfield(lua, -2, "generation");
}

static bool read_handle(lua_State* lua, int index, PangeaScriptObjectHandle* handle)
{
	if (!lua_istable(lua, index) || !handle) return false;
	lua_getfield(lua, index, "id"); handle->id = (int) lua_tointeger(lua, -1); lua_pop(lua, 1);
	lua_getfield(lua, index, "generation"); handle->generation = (uint32_t) lua_tointeger(lua, -1); lua_pop(lua, 1);
	return handle->id > 0;
}

static int lua_log(lua_State* lua)
{
	PangeaScript_Log((PangeaScriptLogLevel) lua_tointeger(lua, lua_upvalueindex(1)), "Lua", luaL_checkstring(lua, 1));
	return 0;
}

static int lua_spawn_native(lua_State* lua)
{
	char id[96];
	if (lua_isinteger(lua, 1)) snprintf(id, sizeof(id), "%lld", (long long) lua_tointeger(lua, 1));
	else snprintf(id, sizeof(id), "%s", luaL_checkstring(lua, 1));
	PangeaScriptVector3 position;
	if (!read_vector(lua, 2, &position)) return luaL_error(lua, "position must be a Vector3 table");
	int params[4] = {0, 0, 0, 0};
	if (lua_istable(lua, 3))
	{
		lua_getfield(lua, 3, "subtype"); if (lua_isnumber(lua, -1)) params[0] = (int) lua_tointeger(lua, -1); lua_pop(lua, 1);
		lua_getfield(lua, 3, "amount"); if (lua_isnumber(lua, -1)) params[1] = (int) lua_tointeger(lua, -1); lua_pop(lua, 1);
		for (int i = 0; i < 4; i++)
		{
			char key[8]; snprintf(key, sizeof(key), "param%d", i);
			lua_getfield(lua, 3, key); if (lua_isnumber(lua, -1)) params[i] = (int) lua_tointeger(lua, -1); lua_pop(lua, 1);
		}
	}
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status = PangeaScript_SpawnNative(id, position.x, position.y, position.z, params, &handle);
	if (status == PANGEA_SCRIPT_OK && handle.id > 0) push_handle(lua, handle); else lua_pushnil(lua);
	return 1;
}

static int lua_spawn_native_result(lua_State* lua)
{
	lua_spawn_native(lua);
	bool hasHandle = !lua_isnil(lua, -1);
	lua_createtable(lua, 0, 4);
	lua_pushboolean(lua, PangeaScript_GetLastStatus() == PANGEA_SCRIPT_OK); lua_setfield(lua, -2, "ok");
	lua_pushinteger(lua, PangeaScript_GetLastStatus()); lua_setfield(lua, -2, "code");
	lua_pushstring(lua, PangeaScript_GetLastError()); lua_setfield(lua, -2, "message");
	if (hasHandle) { lua_pushvalue(lua, -2); lua_setfield(lua, -2, "primary"); }
	lua_remove(lua, -2);
	return 1;
}

static int lua_spawn_scripted(lua_State* lua)
{
	const char* id = luaL_checkstring(lua, 1);
	PangeaScriptVector3 position;
	if (!read_vector(lua, 2, &position)) return luaL_error(lua, "position must be a Vector3 table");
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status = PangeaScript_RegisterScriptedObject(id, position.x, position.y, position.z, &handle);
	if (status == PANGEA_SCRIPT_OK && handle.id > 0) push_handle(lua, handle); else lua_pushnil(lua);
	return 1;
}

static int lua_object_position(lua_State* lua)
{
	PangeaScriptObjectHandle handle; PangeaScriptVector3 position;
	if (read_handle(lua, 1, &handle) && PangeaScript_GetObjectPosition(handle, &position)) push_vector(lua, position.x, position.y, position.z); else lua_pushnil(lua);
	return 1;
}

static int lua_object_set_position(lua_State* lua)
{
	PangeaScriptObjectHandle handle; PangeaScriptVector3 position;
	lua_pushboolean(lua, read_handle(lua, 1, &handle) && read_vector(lua, 2, &position) && PangeaScript_SetObjectPosition(handle, &position)); return 1;
}

static int lua_object_set_velocity(lua_State* lua)
{
	PangeaScriptObjectHandle handle; PangeaScriptVector3 velocity;
	lua_pushboolean(lua, read_handle(lua, 1, &handle) && read_vector(lua, 2, &velocity) && PangeaScript_SetObjectVelocity(handle, &velocity)); return 1;
}

static int lua_object_delete(lua_State* lua)
{
	PangeaScriptObjectHandle handle; lua_pushboolean(lua, read_handle(lua, 1, &handle) && PangeaScript_DeleteObject(handle)); return 1;
}

static int lua_object_exists(lua_State* lua)
{
	PangeaScriptObjectHandle handle; lua_pushboolean(lua, read_handle(lua, 1, &handle) && PangeaScript_ObjectExists(handle)); return 1;
}

static int lua_object_tags(lua_State* lua)
{
	PangeaScriptObjectHandle handle;
	if (!read_handle(lua, 1, &handle)) { lua_createtable(lua, 0, 0); return 1; }
	int count = PangeaScript_GetObjectTagCount(handle); lua_createtable(lua, count, 0);
	for (int i = 0; i < count; i++) { lua_pushstring(lua, PangeaScript_GetObjectTag(handle, i)); lua_rawseti(lua, -2, i + 1); }
	return 1;
}

static int lua_object_has_tag(lua_State* lua)
{
	PangeaScriptObjectHandle handle; const char* expected = luaL_checkstring(lua, 2); bool found = false;
	if (read_handle(lua, 1, &handle)) for (int i = 0; i < PangeaScript_GetObjectTagCount(handle); i++) found |= strcmp(PangeaScript_GetObjectTag(handle, i), expected) == 0;
	lua_pushboolean(lua, found); return 1;
}

static int lua_object_state(lua_State* lua)
{
	PangeaScriptObjectHandle handle; if (!read_handle(lua, 1, &handle) || !PangeaScript_ObjectExists(handle)) { lua_pushnil(lua); return 1; }
	char key[48]; snprintf(key, sizeof(key), "%d:%u", handle.id, handle.generation);
	lua_getfield(lua, LUA_REGISTRYINDEX, "PangeaObjectStates");
	if (!lua_istable(lua, -1)) { lua_pop(lua, 1); lua_createtable(lua, 0, 32); lua_pushvalue(lua, -1); lua_setfield(lua, LUA_REGISTRYINDEX, "PangeaObjectStates"); }
	lua_getfield(lua, -1, key);
	if (!lua_istable(lua, -1)) { lua_pop(lua, 1); lua_createtable(lua, 0, 8); lua_pushvalue(lua, -1); lua_setfield(lua, -3, key); }
	lua_remove(lua, -2); return 1;
}

static int lua_object_set_rotation(lua_State* lua)
{
	PangeaScriptObjectHandle handle; PangeaScriptVector3 rotation; lua_pushboolean(lua, read_handle(lua, 1, &handle) && read_vector(lua, 2, &rotation) && PangeaScript_SetObjectRotation(handle, &rotation)); return 1;
}

static int lua_object_set_scale(lua_State* lua)
{
	PangeaScriptObjectHandle handle; lua_pushboolean(lua, read_handle(lua, 1, &handle) && PangeaScript_SetObjectScale(handle, (float) luaL_checknumber(lua, 2))); return 1;
}

static int lua_object_set_animation(lua_State* lua)
{
	PangeaScriptObjectHandle handle; if (!read_handle(lua, 1, &handle)) { lua_pushboolean(lua, false); return 1; }
	float speed = (float) luaL_optnumber(lua, 3, 1.0); float blend = (float) luaL_optnumber(lua, 4, 0.0);
	bool result = lua_isinteger(lua, 2) ? PangeaScript_SetObjectAnimation(handle, (int) lua_tointeger(lua, 2), speed, blend) : PangeaScript_SetObjectAnimationNamed(handle, luaL_checkstring(lua, 2), speed, blend);
	lua_pushboolean(lua, result); return 1;
}

static int lua_level_setting(lua_State* lua)
{
	const char* key = luaL_checkstring(lua, 1); float floatValue; int intValue; bool boolValue; char stringValue[256];
	if (PangeaScript_GetLevelStringSetting(key, stringValue, sizeof(stringValue))) lua_pushstring(lua, stringValue);
	else if (PangeaScript_GetLevelBoolSetting(key, &boolValue)) lua_pushboolean(lua, boolValue);
	else if (PangeaScript_GetLevelIntSetting(key, &intValue)) lua_pushinteger(lua, intValue);
	else if (PangeaScript_GetLevelFloatSetting(key, &floatValue)) lua_pushnumber(lua, floatValue);
	else lua_pushnil(lua);
	return 1;
}

static int lua_capabilities(lua_State* lua)
{
	lua_createtable(lua, 0, 5);
	const char* names[] = {"levelSettings", "objectMutation", "objectPosition", "spawnNative", "spawnScripted"};
	for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++) { lua_pushboolean(lua, true); lua_setfield(lua, -2, names[i]); }
	return 1;
}

static void set_function(lua_State* lua, const char* name, lua_CFunction function)
{
	lua_pushcfunction(lua, function); lua_setfield(lua, -2, name);
}

static int load_pangea_module(lua_State* lua)
{
	lua_pushvalue(lua, lua_upvalueindex(1));
	return 1;
}

static void open_library(lua_State* lua, const char* name, lua_CFunction function)
{
	luaL_requiref(lua, name, function, 1);
	lua_pop(lua, 1);
}

static void open_safe_libraries(lua_State* lua)
{
	open_library(lua, LUA_GNAME, luaopen_base);
	open_library(lua, LUA_LOADLIBNAME, luaopen_package);
	open_library(lua, LUA_COLIBNAME, luaopen_coroutine);
	open_library(lua, LUA_TABLIBNAME, luaopen_table);
	open_library(lua, LUA_STRLIBNAME, luaopen_string);
	open_library(lua, LUA_MATHLIBNAME, luaopen_math);
	open_library(lua, LUA_UTF8LIBNAME, luaopen_utf8);
}

static void install_pangea(lua_State* lua, const PangeaScriptGameInfo* gameInfo)
{
	lua_getglobal(lua, "package");
	lua_pushstring(lua, "Data/Scripts/dist/modules/?.lua;Data/Scripts/dist/?.lua");
	lua_setfield(lua, -2, "path");
	lua_pop(lua, 1);
	lua_createtable(lua, 0, 6);
	lua_createtable(lua, 0, 2); lua_pushinteger(lua, 1); lua_setfield(lua, -2, "version"); set_function(lua, "capabilities", lua_capabilities); lua_setfield(lua, -2, "api");
	lua_createtable(lua, 0, 3);
	for (int level = PANGEA_LOG_INFO; level <= PANGEA_LOG_ERROR; level++)
	{
		lua_pushinteger(lua, level); lua_pushcclosure(lua, lua_log, 1);
		lua_setfield(lua, -2, level == PANGEA_LOG_INFO ? "info" : level == PANGEA_LOG_WARN ? "warn" : "error");
	}
	lua_setfield(lua, -2, "log");
	lua_createtable(lua, 0, 3); set_function(lua, "native", lua_spawn_native); set_function(lua, "nativeResult", lua_spawn_native_result); set_function(lua, "scripted", lua_spawn_scripted); lua_setfield(lua, -2, "spawn");
	lua_createtable(lua, 0, 12); set_function(lua, "exists", lua_object_exists); set_function(lua, "position", lua_object_position); set_function(lua, "setPosition", lua_object_set_position); set_function(lua, "setVelocity", lua_object_set_velocity); set_function(lua, "setRotation", lua_object_set_rotation); set_function(lua, "setScale", lua_object_set_scale); set_function(lua, "setAnimation", lua_object_set_animation); set_function(lua, "tags", lua_object_tags); set_function(lua, "hasTag", lua_object_has_tag); set_function(lua, "state", lua_object_state); set_function(lua, "delete", lua_object_delete); lua_setfield(lua, -2, "object");
	lua_createtable(lua, 0, 1); set_function(lua, "setting", lua_level_setting); lua_setfield(lua, -2, "level");
	lua_createtable(lua, 0, 2); lua_pushstring(lua, gameInfo->gameId); lua_setfield(lua, -2, "id"); lua_pushstring(lua, gameInfo->gameName); lua_setfield(lua, -2, "name"); lua_setfield(lua, -2, "game");
	lua_pushvalue(lua, -1); lua_setglobal(lua, "pangea");
	lua_getglobal(lua, "package"); lua_getfield(lua, -1, "preload"); lua_pushvalue(lua, -3); lua_pushcclosure(lua, load_pangea_module, 1); lua_setfield(lua, -2, "pangea"); lua_pop(lua, 3);
}

static void sandbox_lua(lua_State* lua)
{
	const char* blockedGlobals[] = {"debug", "dofile", "io", "loadfile", "os"};
	for (int i = 0; i < (int)(sizeof(blockedGlobals) / sizeof(blockedGlobals[0])); i++)
	{
		lua_pushnil(lua);
		lua_setglobal(lua, blockedGlobals[i]);
	}
	lua_getglobal(lua, "package");
	lua_pushstring(lua, ""); lua_setfield(lua, -2, "cpath");
	lua_pushnil(lua); lua_setfield(lua, -2, "loadlib");
	lua_pop(lua, 1);
}

static bool reset_lua(PangeaScriptBackend* backend)
{
	if (backend->lua)
	{
		lua_close(backend->lua);
	}

	backend->lua = luaL_newstate();
	backend->entryReference = LUA_NOREF;
	if (!backend->lua)
	{
		return false;
	}

	open_safe_libraries(backend->lua);
	sandbox_lua(backend->lua);
	install_pangea(backend->lua, &backend->gameInfo);
	return true;
}

static bool push_hook(PangeaScriptBackend* backend, const char* name)
{
	lua_rawgeti(backend->lua, LUA_REGISTRYINDEX, backend->entryReference);
	lua_getfield(backend->lua, -1, name);
	lua_remove(backend->lua, -2);
	if (lua_isfunction(backend->lua, -1)) return true;
	lua_pop(backend->lua, 1); return false;
}

static const char* hook_name(PangeaScriptBackend* backend, PangeaScriptHook hook)
{
	static const char* names[] = {"onGameStart", "onLevelLoad", "onLevelStart", "onFrame", "onLevelComplete", "onLevelUnload", "onGameShutdown", "onTerrainItem", "onSplineItem", "onMapItem", "onObjectFrame"};
	static const char* raceNames[] = {"onGameStart", "onRaceLoad", "onRaceStart", "onRaceFrame", "onRaceComplete", "onRaceUnload", "onGameShutdown", "onTerrainItem", "onSplineItem", "onMapItem", "onObjectFrame"};
	static const char* areaNames[] = {"onGameStart", "onAreaLoad", "onAreaStart", "onAreaFrame", "onAreaComplete", "onAreaUnload", "onGameShutdown", "onTerrainItem", "onSplineItem", "onMapItem", "onObjectFrame"};
	const char* const* selected = names;
	if (strstr(backend->gameInfo.gameId, "CroMag")) selected = raceNames;
	else if (strstr(backend->gameInfo.gameId, "Billy") || strstr(backend->gameInfo.gameId, "MightyMike")) selected = areaNames;
	return hook >= 0 && hook < (int)(sizeof(names) / sizeof(names[0])) ? selected[hook] : "";
}

static void push_base_context(lua_State* lua, int levelNum)
{
	lua_createtable(lua, 0, 8); lua_pushinteger(lua, levelNum); lua_setfield(lua, -2, "levelNum");
}

static void push_params(lua_State* lua, const unsigned char* params, int count)
{
	lua_createtable(lua, count, 0); for (int i = 0; i < count; i++) { lua_pushinteger(lua, params[i]); lua_rawseti(lua, -2, i + 1); } lua_setfield(lua, -2, "params");
}

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo)
{
	if (!gameInfo) return NULL;
	PangeaScriptBackend* backend = calloc(1, sizeof(*backend)); if (!backend) return NULL;
	backend->gameInfo = *gameInfo;
	if (!reset_lua(backend))
	{
		free(backend);
		return NULL;
	}
	return backend;
}

void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend) { if (backend) { if (backend->lua) lua_close(backend->lua); free(backend); } }

PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity)
{
	if (!backend || !source) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!reset_lua(backend))
	{
		copy_error(error, errorCapacity, "Unable to allocate the Lua state");
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	if (luaL_loadbuffer(backend->lua, source, strlen(source), "@Data/Scripts/dist/main.lua") != LUA_OK) { copy_error(error, errorCapacity, lua_tostring(backend->lua, -1)); lua_pop(backend->lua, 1); return PANGEA_SCRIPT_PARSE_ERROR; }
	PangeaScriptStatus status = protected_call(backend->lua, 0, 1, error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	if (!lua_istable(backend->lua, -1)) { lua_pop(backend->lua, 1); copy_error(error, errorCapacity, "Lua bundle must return an entry table"); return PANGEA_SCRIPT_PARSE_ERROR; }
	backend->entryReference = luaL_ref(backend->lua, LUA_REGISTRYINDEX); return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, hook_name(backend, hook))) return PANGEA_SCRIPT_OK;
	push_base_context(backend->lua, context->levelNum); lua_pushstring(backend->lua, context->levelName ? context->levelName : ""); lua_setfield(backend->lua, -2, "levelName"); return protected_call(backend->lua, 1, 0, error, errorCapacity);
}

PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, hook_name(backend, PANGEA_SCRIPT_HOOK_FRAME))) return PANGEA_SCRIPT_OK;
	push_base_context(backend->lua, context->levelNum); lua_pushinteger(backend->lua, context->frameNum); lua_setfield(backend->lua, -2, "frameNum"); lua_pushnumber(backend->lua, context->deltaSeconds); lua_setfield(backend->lua, -2, "deltaSeconds"); lua_pushnumber(backend->lua, context->levelTimeSeconds); lua_setfield(backend->lua, -2, "levelTimeSeconds"); return protected_call(backend->lua, 1, 0, error, errorCapacity);
}

PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onTerrainItem")) return PANGEA_SCRIPT_OK;
	push_base_context(backend->lua, context->levelNum); lua_pushinteger(backend->lua, context->itemType); lua_setfield(backend->lua, -2, "itemType"); lua_pushinteger(backend->lua, context->playerNum); lua_setfield(backend->lua, -2, "playerNum"); lua_pushboolean(backend->lua, context->networked); lua_setfield(backend->lua, -2, "networked"); lua_pushnumber(backend->lua, context->x); lua_setfield(backend->lua, -2, "x"); lua_pushnumber(backend->lua, context->z); lua_setfield(backend->lua, -2, "z"); lua_pushinteger(backend->lua, context->flags); lua_setfield(backend->lua, -2, "flags"); push_params(backend->lua, context->params, context->paramCount);
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	if (lua_istable(backend->lua, -1)) { lua_getfield(backend->lua, -1, "handled"); context->handled = lua_toboolean(backend->lua, -1); lua_pop(backend->lua, 1); lua_getfield(backend->lua, -1, "markInUse"); context->markInUse = lua_toboolean(backend->lua, -1); lua_pop(backend->lua, 1); lua_getfield(backend->lua, -1, "remappedItemType"); if (lua_isinteger(backend->lua, -1)) context->remappedItemType = (int) lua_tointeger(backend->lua, -1); lua_pop(backend->lua, 1); } lua_pop(backend->lua, 1); return status;
}

PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onSplineItem")) return PANGEA_SCRIPT_OK;
	push_base_context(backend->lua, context->levelNum);
	lua_pushinteger(backend->lua, context->itemType);
	lua_setfield(backend->lua, -2, "itemType");
	lua_pushinteger(backend->lua, context->splineNum);
	lua_setfield(backend->lua, -2, "splineNum");
	lua_pushnumber(backend->lua, context->placement);
	lua_setfield(backend->lua, -2, "placement");
	push_params(backend->lua, context->params, context->paramCount);
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK && lua_istable(backend->lua, -1))
	{
		lua_getfield(backend->lua, -1, "handled");
		context->handled = lua_toboolean(backend->lua, -1);
		lua_pop(backend->lua, 1);
		lua_getfield(backend->lua, -1, "markInUse");
		context->markInUse = lua_toboolean(backend->lua, -1);
		lua_pop(backend->lua, 1);
	}
	if (status == PANGEA_SCRIPT_OK) lua_pop(backend->lua, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onMapItem")) return PANGEA_SCRIPT_OK;
	push_base_context(backend->lua, context->levelNum);
	lua_pushinteger(backend->lua, context->itemType);
	lua_setfield(backend->lua, -2, "itemType");
	lua_pushinteger(backend->lua, context->sceneNum);
	lua_setfield(backend->lua, -2, "sceneNum");
	lua_pushinteger(backend->lua, context->areaNum);
	lua_setfield(backend->lua, -2, "areaNum");
	lua_pushnumber(backend->lua, context->x);
	lua_setfield(backend->lua, -2, "x");
	lua_pushnumber(backend->lua, context->y);
	lua_setfield(backend->lua, -2, "y");
	push_params(backend->lua, context->params, context->paramCount);
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK && lua_istable(backend->lua, -1))
	{
		lua_getfield(backend->lua, -1, "handled");
		context->handled = lua_toboolean(backend->lua, -1);
		lua_pop(backend->lua, 1);
		lua_getfield(backend->lua, -1, "markInUse");
		context->markInUse = lua_toboolean(backend->lua, -1);
		lua_pop(backend->lua, 1);
	}
	if (status == PANGEA_SCRIPT_OK) lua_pop(backend->lua, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity)
{
	if (!backend || !context || !result) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onObjectFrame")) return PANGEA_SCRIPT_OK;
	push_base_context(backend->lua, context->levelNum);
	lua_pushinteger(backend->lua, context->frameNum);
	lua_setfield(backend->lua, -2, "frameNum");
	lua_pushnumber(backend->lua, context->deltaSeconds);
	lua_setfield(backend->lua, -2, "deltaSeconds");
	lua_pushnumber(backend->lua, context->levelTimeSeconds);
	lua_setfield(backend->lua, -2, "levelTimeSeconds");
	push_handle(backend->lua, context->object);
	lua_setfield(backend->lua, -2, "object");
	push_vector(backend->lua, context->position.x, context->position.y, context->position.z);
	lua_setfield(backend->lua, -2, "position");
	lua_pushstring(backend->lua, context->objectType ? context->objectType : "");
	lua_setfield(backend->lua, -2, "objectType");
	lua_pushstring(backend->lua, context->event ? context->event : "frame");
	lua_setfield(backend->lua, -2, "event");
	lua_createtable(backend->lua, context->tagCount, 0);
	for (int i = 0; i < context->tagCount; i++)
	{
		lua_pushstring(backend->lua, context->tags[i]);
		lua_rawseti(backend->lua, -2, i + 1);
	}
	lua_setfield(backend->lua, -2, "tags");
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, error, errorCapacity); if (status == PANGEA_SCRIPT_OK && lua_istable(backend->lua, -1)) { lua_getfield(backend->lua, -1, "positionOffset"); result->hasPositionOffset = read_vector(backend->lua, -1, &result->positionOffset); lua_pop(backend->lua, 1); } if (status == PANGEA_SCRIPT_OK) lua_pop(backend->lua, 1); return status;
}

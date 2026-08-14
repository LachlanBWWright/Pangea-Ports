#include "pangea_script_backend.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PangeaScriptBackend
{
	lua_State* lua;
	PangeaScriptGameInfo gameInfo;
	int entryReference;
	size_t allocatedBytes;
	size_t memoryLimitBytes;
	int currentLevelNum;
	unsigned int currentFrameNum;
	float currentDeltaSeconds;
	float currentLevelTimeSeconds;
	uint32_t randomState;
	struct
	{
		bool active;
		int id;
		int callbackReference;
		float dueTimeSeconds;
		float intervalSeconds;
		bool repeating;
	} timers[128];
	int nextTimerId;
	struct
	{
		bool active;
		int id;
		int threadReference;
		lua_State* thread;
		float dueTimeSeconds;
	} tasks[64];
	int nextTaskId;
	struct
	{
		bool active;
		int id;
		char eventName[64];
		int callbackReference;
		bool once;
	} subscriptions[128];
	int nextSubscriptionId;
};

enum
{
	PANGEA_LUA_MEMORY_LIMIT = 16 * 1024 * 1024,
	PANGEA_LUA_LOAD_BUDGET = 1000000,
	PANGEA_LUA_EVENT_BUDGET = 100000,
	PANGEA_LUA_FRAME_BUDGET = 50000,
	PANGEA_LUA_API_VERSION = 1,
	PANGEA_LUA_MIN_API_VERSION = 1,
};

static int reject_context_write(lua_State* lua)
{
	return luaL_error(lua, "callback contexts are read-only");
}

static int iterate_read_only_context(lua_State* lua)
{
	lua_getglobal(lua, "next");
	lua_pushvalue(lua, lua_upvalueindex(1));
	lua_pushnil(lua);
	return 3;
}

static int read_only_context_length(lua_State* lua)
{
	lua_len(lua, lua_upvalueindex(1));
	return 1;
}

static void freeze_table(lua_State* lua, int index, int depth)
{
	if (depth > 8 || !lua_istable(lua, index)) return;
	int tableIndex = lua_absindex(lua, index);
	lua_pushnil(lua);
	while (lua_next(lua, tableIndex) != 0)
	{
		if (lua_istable(lua, -1))
		{
			freeze_table(lua, -1, depth + 1);
			lua_pushvalue(lua, -2);
			lua_pushvalue(lua, -2);
			lua_rawset(lua, tableIndex);
		}
		lua_pop(lua, 1);
	}
	lua_createtable(lua, 0, 0);
	lua_createtable(lua, 0, 5);
	lua_pushvalue(lua, tableIndex); lua_setfield(lua, -2, "__index");
	lua_pushcfunction(lua, reject_context_write); lua_setfield(lua, -2, "__newindex");
	lua_pushvalue(lua, tableIndex); lua_pushcclosure(lua, iterate_read_only_context, 1); lua_setfield(lua, -2, "__pairs");
	lua_pushvalue(lua, tableIndex); lua_pushcclosure(lua, read_only_context_length, 1); lua_setfield(lua, -2, "__len");
	lua_pushboolean(lua, false); lua_setfield(lua, -2, "__metatable");
	lua_setmetatable(lua, -2);
	lua_replace(lua, tableIndex);
}

static void freeze_context_argument(lua_State* lua, int functionIndex, int arguments)
{
	if (arguments != 1 || !lua_istable(lua, functionIndex + 1)) return;
	freeze_table(lua, functionIndex + 1, 0);
}

static void* limited_allocator(void* userData, void* pointer, size_t oldSize, size_t newSize)
{
	PangeaScriptBackend* backend = userData;
	size_t accountedOldSize = pointer ? oldSize : 0;
	if (newSize == 0)
	{
		free(pointer);
		backend->allocatedBytes = accountedOldSize <= backend->allocatedBytes ? backend->allocatedBytes - accountedOldSize : 0;
		return NULL;
	}

	size_t retainedBytes = accountedOldSize <= backend->allocatedBytes ? backend->allocatedBytes - accountedOldSize : 0;
	if (newSize > backend->memoryLimitBytes - retainedBytes)
		return NULL;

	void* resized = realloc(pointer, newSize);
	if (resized)
		backend->allocatedBytes = retainedBytes + newSize;
	return resized;
}

static void copy_error(char* error, int capacity, const char* message)
{
	if (error && capacity > 0) snprintf(error, (size_t) capacity, "%s", message ? message : "Lua error");
}

static void instruction_budget_hook(lua_State* lua, lua_Debug* debug)
{
	(void) debug;
	luaL_error(lua, "script instruction budget exceeded");
}

static int traceback_handler(lua_State* lua)
{
	const char* message = lua_tostring(lua, 1);
	luaL_traceback(lua, lua, message ? message : "Lua error", 1);
	return 1;
}

static int protected_call(lua_State* lua, int arguments, int results, int budget, char* error, int errorCapacity)
{
	int functionIndex = lua_gettop(lua) - arguments;
	freeze_context_argument(lua, functionIndex, arguments);
	lua_pushcfunction(lua, traceback_handler);
	lua_insert(lua, functionIndex);
	lua_sethook(lua, instruction_budget_hook, LUA_MASKCOUNT, budget);
	int status = lua_pcall(lua, arguments, results, functionIndex);
	lua_sethook(lua, NULL, 0, 0);
	lua_remove(lua, functionIndex);
	if (status == LUA_OK) return PANGEA_SCRIPT_OK;
	copy_error(error, errorCapacity, lua_tostring(lua, -1));
	lua_pop(lua, 1);
	if (status == LUA_ERRMEM) return PANGEA_SCRIPT_BUDGET_EXCEEDED;
	return strstr(error ? error : "", "instruction budget") ? PANGEA_SCRIPT_BUDGET_EXCEEDED : PANGEA_SCRIPT_RUNTIME_ERROR;
}

static PangeaScriptStatus validate_hook_result(lua_State* lua, const char* hookName, char* error, int errorCapacity)
{
	if (lua_isnil(lua, -1) || lua_istable(lua, -1)) return PANGEA_SCRIPT_OK;
	char message[160];
	snprintf(message, sizeof(message), "%s must return a table or nil, received %s", hookName, luaL_typename(lua, -1));
	copy_error(error, errorCapacity, message);
	lua_pop(lua, 1);
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

typedef struct LuaResultField
{
	const char* name;
	int type;
	bool integerOnly;
} LuaResultField;

static const LuaResultField kItemResultFields[] = {{"handled", LUA_TBOOLEAN, false}, {"markInUse", LUA_TBOOLEAN, false}, {"remappedItemType", LUA_TNUMBER, true}};
static const LuaResultField kObjectResultFields[] = {{"positionOffset", LUA_TTABLE, false}};
static const LuaResultField kTriggerResultFields[] = {{"handled", LUA_TBOOLEAN, false}, {"solid", LUA_TBOOLEAN, false}, {"deleteSelf", LUA_TBOOLEAN, false}, {"deleteOther", LUA_TBOOLEAN, false}, {"damagePlayer", LUA_TNUMBER, false}, {"healthDelta", LUA_TNUMBER, false}, {"scoreDelta", LUA_TNUMBER, true}};
static const LuaResultField kPickupResultFields[] = {{"handled", LUA_TBOOLEAN, false}, {"consumePickup", LUA_TBOOLEAN, false}, {"healthDelta", LUA_TNUMBER, false}, {"scoreDelta", LUA_TNUMBER, true}};
static const LuaResultField kWeaponResultFields[] = {{"handled", LUA_TBOOLEAN, false}, {"applyDamage", LUA_TBOOLEAN, false}, {"destroyTarget", LUA_TBOOLEAN, false}, {"damage", LUA_TNUMBER, false}, {"scoreDelta", LUA_TNUMBER, true}};

static PangeaScriptStatus validate_result_fields(lua_State* lua, const char* hookName, const LuaResultField* fields, int fieldCount, char* error, int errorCapacity)
{
	if (lua_isnil(lua, -1)) return PANGEA_SCRIPT_OK;
	for (int i = 0; i < fieldCount; i++)
	{
		lua_getfield(lua, -1, fields[i].name);
		int actualType = lua_type(lua, -1);
		bool finiteNumber = actualType != LUA_TNUMBER || isfinite((double) lua_tonumber(lua, -1));
		bool validInteger = !fields[i].integerOnly || (lua_isinteger(lua, -1) && lua_tointeger(lua, -1) >= INT_MIN && lua_tointeger(lua, -1) <= INT_MAX);
		lua_pop(lua, 1);
		if (actualType == LUA_TNIL || (actualType == fields[i].type && finiteNumber && validInteger)) continue;
		char message[192];
		snprintf(message, sizeof(message), fields[i].integerOnly
			? "%s result field '%s' must be an integer or nil"
			: fields[i].type == LUA_TNUMBER
			? "%s result field '%s' must be a finite %s or nil"
			: "%s result field '%s' must be %s or nil", hookName, fields[i].name, lua_typename(lua, fields[i].type));
		copy_error(error, errorCapacity, message);
		lua_pop(lua, 1);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	return PANGEA_SCRIPT_OK;
}

static void push_vector(lua_State* lua, float x, float y, float z)
{
	lua_createtable(lua, 0, 3);
	lua_pushnumber(lua, x); lua_setfield(lua, -2, "x");
	lua_pushnumber(lua, y); lua_setfield(lua, -2, "y");
	lua_pushnumber(lua, z); lua_setfield(lua, -2, "z");
}

static void push_vector2(lua_State* lua, float x, float y)
{
	lua_createtable(lua, 0, 2);
	lua_pushnumber(lua, x); lua_setfield(lua, -2, "x");
	lua_pushnumber(lua, y); lua_setfield(lua, -2, "y");
}

static bool read_vector(lua_State* lua, int index, PangeaScriptVector3* vector)
{
	if (!lua_istable(lua, index) || !vector) return false;
	float values[3];
	const char* fields[] = {"x", "y", "z"};
	for (int i = 0; i < 3; i++)
	{
		lua_getfield(lua, index, fields[i]);
		if (!lua_isnumber(lua, -1) || !isfinite((double) lua_tonumber(lua, -1)))
		{
			lua_pop(lua, 1);
			return false;
		}
		values[i] = (float) lua_tonumber(lua, -1);
		lua_pop(lua, 1);
	}
	*vector = (PangeaScriptVector3){values[0], values[1], values[2]};
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
	lua_getfield(lua, index, "id");
	if (!lua_isinteger(lua, -1)) { lua_pop(lua, 1); return false; }
	handle->id = (int) lua_tointeger(lua, -1); lua_pop(lua, 1);
	lua_getfield(lua, index, "generation");
	if (!lua_isinteger(lua, -1)) { lua_pop(lua, 1); return false; }
	handle->generation = (uint32_t) lua_tointeger(lua, -1); lua_pop(lua, 1);
	return handle->id > 0 && handle->generation > 0;
}

static int lua_log(lua_State* lua)
{
	PangeaScript_Log((PangeaScriptLogLevel) lua_tointeger(lua, lua_upvalueindex(1)), "Lua", luaL_checkstring(lua, 1));
	return 0;
}

static int read_optional_native_byte(lua_State* lua, int tableIndex, const char* key, int* value)
{
	lua_getfield(lua, tableIndex, key);
	if (lua_isnil(lua, -1)) { lua_pop(lua, 1); return 0; }
	if (!lua_isinteger(lua, -1)) { lua_pop(lua, 1); return -1; }
	lua_Integer number = lua_tointeger(lua, -1);
	lua_pop(lua, 1);
	if (number < 0 || number > 255) return -1;
	*value = (int) number;
	return 1;
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
		if (read_optional_native_byte(lua, 3, "subtype", &params[0]) < 0 || read_optional_native_byte(lua, 3, "amount", &params[1]) < 0)
			return luaL_error(lua, "native spawn subtype and amount must be integers from 0 through 255");
		for (int i = 0; i < 4; i++)
		{
			char key[8]; snprintf(key, sizeof(key), "param%d", i);
			if (read_optional_native_byte(lua, 3, key, &params[i]) < 0)
				return luaL_error(lua, "%s must be an integer from 0 through 255", key);
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
	static const char* statusNames[] = {"ok", "not-enabled", "file-not-found", "parse-error", "runtime-error", "bad-argument", "budget-exceeded", "incompatible-item", "config-error"};
	int status = PangeaScript_GetLastStatus();
	const char* statusName = status >= 0 && status < (int)(sizeof(statusNames) / sizeof(statusNames[0])) ? statusNames[status] : "unknown";
	lua_pushstring(lua, statusName); lua_setfield(lua, -2, "reason");
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
	if (status != PANGEA_SCRIPT_OK || handle.id <= 0) { lua_pushnil(lua); return 1; }
	if (lua_istable(lua, 3))
	{
		lua_getfield(lua, 3, "scale");
		if (!lua_isnil(lua, -1))
		{
			float scale = (float) luaL_checknumber(lua, -1);
			if (!isfinite(scale) || scale <= 0.0f) return luaL_error(lua, "scripted spawn scale must be finite and greater than zero");
			(void) PangeaScript_SetObjectScale(handle, scale);
		}
		lua_pop(lua, 1);
		lua_getfield(lua, 3, "animation");
		if (!lua_isnil(lua, -1))
		{
			lua_getfield(lua, 3, "animationSpeed"); float speed = (float) luaL_optnumber(lua, -1, 1.0); lua_pop(lua, 1);
			lua_getfield(lua, 3, "blendSeconds"); float blend = (float) luaL_optnumber(lua, -1, 0.0); lua_pop(lua, 1);
			if (!isfinite(speed) || !isfinite(blend) || blend < 0.0f) return luaL_error(lua, "scripted spawn animation values must be finite and blend must be non-negative");
			if (lua_isinteger(lua, -1)) (void) PangeaScript_SetObjectAnimation(handle, (int) lua_tointeger(lua, -1), speed, blend);
			else if (lua_isstring(lua, -1)) (void) PangeaScript_SetObjectAnimationNamed(handle, lua_tostring(lua, -1), speed, blend);
			else return luaL_error(lua, "scripted spawn animation must be a string or integer");
		}
		lua_pop(lua, 1);
	}
	push_handle(lua, handle);
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

static bool object_has_tag(PangeaScriptObjectHandle handle, const char* expected)
{
	for (int i = 0; i < PangeaScript_GetObjectTagCount(handle); i++)
		if (strcmp(PangeaScript_GetObjectTag(handle, i), expected) == 0) return true;
	return false;
}

static int lua_object_all(lua_State* lua)
{
	int count = PangeaScript_GetRegisteredObjectCount();
	lua_createtable(lua, count, 0);
	int outputIndex = 1;
	for (int i = 0; i < count; i++)
	{
		PangeaScriptObjectHandle handle;
		if (!PangeaScript_GetRegisteredObjectHandle(i, &handle)) continue;
		push_handle(lua, handle);
		lua_rawseti(lua, -2, outputIndex++);
	}
	return 1;
}

static int lua_object_find_by_tag(lua_State* lua)
{
	const char* tag = luaL_checkstring(lua, 1);
	int count = PangeaScript_GetRegisteredObjectCount();
	lua_createtable(lua, count, 0);
	int outputIndex = 1;
	for (int i = 0; i < count; i++)
	{
		PangeaScriptObjectHandle handle;
		if (!PangeaScript_GetRegisteredObjectHandle(i, &handle) || !object_has_tag(handle, tag)) continue;
		push_handle(lua, handle);
		lua_rawseti(lua, -2, outputIndex++);
	}
	return 1;
}

static int lua_object_nearest(lua_State* lua)
{
	PangeaScriptVector3 origin;
	if (!read_vector(lua, 1, &origin)) return luaL_error(lua, "origin must be a finite Vector3 table");
	const char* tag = luaL_optstring(lua, 2, NULL);
	PangeaScriptObjectHandle nearest = {0};
	double nearestDistanceSquared = 0.0;
	int count = PangeaScript_GetRegisteredObjectCount();
	for (int i = 0; i < count; i++)
	{
		PangeaScriptObjectHandle handle;
		PangeaScriptVector3 position;
		if (!PangeaScript_GetRegisteredObjectHandle(i, &handle) || (tag && !object_has_tag(handle, tag)) || !PangeaScript_GetObjectPosition(handle, &position)) continue;
		double dx = (double) position.x - origin.x;
		double dy = (double) position.y - origin.y;
		double dz = (double) position.z - origin.z;
		double distanceSquared = dx * dx + dy * dy + dz * dz;
		if (nearest.id > 0 && distanceSquared >= nearestDistanceSquared) continue;
		nearest = handle;
		nearestDistanceSquared = distanceSquared;
	}
	if (nearest.id > 0) push_handle(lua, nearest); else lua_pushnil(lua);
	return 1;
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

void PangeaScriptBackend_ClearObjectState(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle)
{
	if (!backend || !backend->lua) return;
	char key[48];
	snprintf(key, sizeof(key), "%d:%u", handle.id, handle.generation);
	lua_getfield(backend->lua, LUA_REGISTRYINDEX, "PangeaObjectStates");
	if (lua_istable(backend->lua, -1))
	{
		lua_pushnil(backend->lua);
		lua_setfield(backend->lua, -2, key);
	}
	lua_pop(backend->lua, 1);
}

void PangeaScriptBackend_ResetObjectStates(PangeaScriptBackend* backend)
{
	if (!backend || !backend->lua) return;
	lua_pushnil(backend->lua);
	lua_setfield(backend->lua, LUA_REGISTRYINDEX, "PangeaObjectStates");
}

static int lua_object_set_rotation(lua_State* lua)
{
	PangeaScriptObjectHandle handle; PangeaScriptVector3 rotation; lua_pushboolean(lua, read_handle(lua, 1, &handle) && read_vector(lua, 2, &rotation) && PangeaScript_SetObjectRotation(handle, &rotation)); return 1;
}

static int lua_object_set_scale(lua_State* lua)
{
	PangeaScriptObjectHandle handle;
	float scale = (float) luaL_checknumber(lua, 2);
	if (!isfinite(scale) || scale <= 0.0f) return luaL_error(lua, "scale must be finite and greater than zero");
	lua_pushboolean(lua, read_handle(lua, 1, &handle) && PangeaScript_SetObjectScale(handle, scale)); return 1;
}

static int lua_object_set_animation(lua_State* lua)
{
	PangeaScriptObjectHandle handle; if (!read_handle(lua, 1, &handle)) { lua_pushboolean(lua, false); return 1; }
	float speed = (float) luaL_optnumber(lua, 3, 1.0); float blend = (float) luaL_optnumber(lua, 4, 0.0);
	if (!isfinite(speed) || !isfinite(blend) || blend < 0.0f) return luaL_error(lua, "animation speed and blend must be finite, and blend must be non-negative");
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

static PangeaScriptBackend* current_backend(lua_State* lua)
{
	return lua_touserdata(lua, lua_upvalueindex(1));
}

static int lua_player_count(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	int count = backend->gameInfo.getPlayerCount ? backend->gameInfo.getPlayerCount() : 0;
	lua_pushinteger(lua, count);
	return 1;
}

static int lua_player_get(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	int playerNum = (int) luaL_checkinteger(lua, 1);
	PangeaScriptPlayerSnapshot player = {0};
	if (!backend->gameInfo.getPlayer || !backend->gameInfo.getPlayer(playerNum, &player) || !player.active)
	{
		lua_pushnil(lua);
		return 1;
	}
	lua_createtable(lua, 0, 3);
	lua_pushinteger(lua, playerNum); lua_setfield(lua, -2, "playerNum");
	push_vector(lua, player.position.x, player.position.y, player.position.z); lua_setfield(lua, -2, "position");
	if (player.hasHealth)
	{
		lua_pushnumber(lua, player.health);
		lua_setfield(lua, -2, "health");
	}
	return 1;
}

static int lua_level_current(lua_State* lua)
{
	lua_pushinteger(lua, current_backend(lua)->currentLevelNum);
	return 1;
}

static int lua_time_frame(lua_State* lua)
{
	lua_pushinteger(lua, current_backend(lua)->currentFrameNum);
	return 1;
}

static int lua_time_delta(lua_State* lua)
{
	lua_pushnumber(lua, current_backend(lua)->currentDeltaSeconds);
	return 1;
}

static int lua_time_level(lua_State* lua)
{
	lua_pushnumber(lua, current_backend(lua)->currentLevelTimeSeconds);
	return 1;
}

static int schedule_timer(lua_State* lua, bool repeating)
{
	PangeaScriptBackend* backend = current_backend(lua);
	float delaySeconds = (float) luaL_checknumber(lua, 1);
	luaL_checktype(lua, 2, LUA_TFUNCTION);
	if (!isfinite(delaySeconds) || delaySeconds < 0.0f || (repeating && delaySeconds == 0.0f))
		return luaL_error(lua, repeating ? "interval must be finite and greater than zero" : "delay must be finite and zero or greater");
	for (int i = 0; i < (int)(sizeof(backend->timers) / sizeof(backend->timers[0])); i++)
	{
		if (backend->timers[i].active) continue;
		backend->nextTimerId++;
		if (backend->nextTimerId <= 0) backend->nextTimerId = 1;
		lua_pushvalue(lua, 2);
		backend->timers[i].active = true;
		backend->timers[i].id = backend->nextTimerId;
		backend->timers[i].callbackReference = luaL_ref(lua, LUA_REGISTRYINDEX);
		backend->timers[i].dueTimeSeconds = backend->currentLevelTimeSeconds + delaySeconds;
		backend->timers[i].intervalSeconds = delaySeconds;
		backend->timers[i].repeating = repeating;
		lua_pushinteger(lua, backend->timers[i].id);
		return 1;
	}
	return luaL_error(lua, "script timer capacity exceeded");
}

static int lua_time_after(lua_State* lua) { return schedule_timer(lua, false); }

static int lua_time_every(lua_State* lua) { return schedule_timer(lua, true); }

static int lua_api_require_version(lua_State* lua)
{
	int minimum = (int) luaL_checkinteger(lua, 1);
	int maximum = (int) luaL_optinteger(lua, 2, minimum);
	if (minimum > maximum) return luaL_error(lua, "minimum API version must not exceed maximum");
	if (PANGEA_LUA_API_VERSION < minimum || PANGEA_LUA_API_VERSION > maximum)
		return luaL_error(lua, "incompatible Pangea API version: runtime=%d requested=%d..%d", PANGEA_LUA_API_VERSION, minimum, maximum);
	lua_pushboolean(lua, true);
	return 1;
}

static int lua_time_cancel(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	int timerId = (int) luaL_checkinteger(lua, 1);
	for (int i = 0; i < (int)(sizeof(backend->timers) / sizeof(backend->timers[0])); i++)
	{
		if (!backend->timers[i].active || backend->timers[i].id != timerId) continue;
		luaL_unref(lua, LUA_REGISTRYINDEX, backend->timers[i].callbackReference);
		backend->timers[i].active = false;
		lua_pushboolean(lua, true);
		return 1;
	}
	lua_pushboolean(lua, false);
	return 1;
}

static int lua_time_is_active(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	int timerId = (int) luaL_checkinteger(lua, 1);
	for (int i = 0; i < (int)(sizeof(backend->timers) / sizeof(backend->timers[0])); i++)
		if (backend->timers[i].active && backend->timers[i].id == timerId) { lua_pushboolean(lua, true); return 1; }
	lua_pushboolean(lua, false);
	return 1;
}

static int lua_task_wait(lua_State* lua)
{
	float delaySeconds = (float) luaL_checknumber(lua, 1);
	if (!isfinite(delaySeconds) || delaySeconds < 0.0f)
		return luaL_error(lua, "task delay must be finite and zero or greater");
	lua_settop(lua, 1);
	return lua_yield(lua, 1);
}

static PangeaScriptStatus resume_task(PangeaScriptBackend* backend, int taskIndex, char* error, int errorCapacity)
{
	lua_State* thread = backend->tasks[taskIndex].thread;
	int resultCount = 0;
	lua_sethook(thread, instruction_budget_hook, LUA_MASKCOUNT, PANGEA_LUA_EVENT_BUDGET);
	int status = lua_resume(thread, backend->lua, 0, &resultCount);
	lua_sethook(thread, NULL, 0, 0);
	if (status == LUA_YIELD)
	{
		if (resultCount != 1 || !lua_isnumber(thread, -1) || !isfinite((double) lua_tonumber(thread, -1)) || lua_tonumber(thread, -1) < 0.0)
		{
			copy_error(error, errorCapacity, "pangea.task.wait must yield one finite, non-negative delay");
			lua_settop(thread, 0);
			return PANGEA_SCRIPT_RUNTIME_ERROR;
		}
		backend->tasks[taskIndex].dueTimeSeconds = backend->currentLevelTimeSeconds + (float) lua_tonumber(thread, -1);
		lua_settop(thread, 0);
		return PANGEA_SCRIPT_OK;
	}
	if (status == LUA_OK)
	{
		luaL_unref(backend->lua, LUA_REGISTRYINDEX, backend->tasks[taskIndex].threadReference);
		backend->tasks[taskIndex].active = false;
		return PANGEA_SCRIPT_OK;
	}
	const char* message = lua_tostring(thread, -1);
	luaL_traceback(backend->lua, thread, message ? message : "Lua task error", 1);
	copy_error(error, errorCapacity, lua_tostring(backend->lua, -1));
	lua_pop(backend->lua, 1);
	lua_settop(thread, 0);
	luaL_unref(backend->lua, LUA_REGISTRYINDEX, backend->tasks[taskIndex].threadReference);
	backend->tasks[taskIndex].active = false;
	if (status == LUA_ERRMEM) return PANGEA_SCRIPT_BUDGET_EXCEEDED;
	return strstr(error ? error : "", "instruction budget") ? PANGEA_SCRIPT_BUDGET_EXCEEDED : PANGEA_SCRIPT_RUNTIME_ERROR;
}

static int lua_task_start(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	luaL_checktype(lua, 1, LUA_TFUNCTION);
	for (int i = 0; i < (int)(sizeof(backend->tasks) / sizeof(backend->tasks[0])); i++)
	{
		if (backend->tasks[i].active) continue;
		backend->nextTaskId++;
		if (backend->nextTaskId <= 0) backend->nextTaskId = 1;
		lua_State* thread = lua_newthread(lua);
		int threadReference = luaL_ref(lua, LUA_REGISTRYINDEX);
		lua_pushvalue(lua, 1);
		lua_xmove(lua, thread, 1);
		backend->tasks[i].active = true;
		backend->tasks[i].id = backend->nextTaskId;
		backend->tasks[i].threadReference = threadReference;
		backend->tasks[i].thread = thread;
		char taskError[512] = {0};
		PangeaScriptStatus taskStatus = resume_task(backend, i, taskError, sizeof(taskError));
		if (taskStatus != PANGEA_SCRIPT_OK)
			return luaL_error(lua, "%s", taskError[0] ? taskError : "task failed while starting");
		lua_pushinteger(lua, backend->tasks[i].id);
		return 1;
	}
	return luaL_error(lua, "script task capacity exceeded");
}

static int lua_task_cancel(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	int taskId = (int) luaL_checkinteger(lua, 1);
	for (int i = 0; i < (int)(sizeof(backend->tasks) / sizeof(backend->tasks[0])); i++)
	{
		if (!backend->tasks[i].active || backend->tasks[i].id != taskId) continue;
		luaL_unref(lua, LUA_REGISTRYINDEX, backend->tasks[i].threadReference);
		backend->tasks[i].active = false;
		lua_pushboolean(lua, true);
		return 1;
	}
	lua_pushboolean(lua, false);
	return 1;
}

static int lua_task_is_active(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	int taskId = (int) luaL_checkinteger(lua, 1);
	for (int i = 0; i < (int)(sizeof(backend->tasks) / sizeof(backend->tasks[0])); i++)
		if (backend->tasks[i].active && backend->tasks[i].id == taskId) { lua_pushboolean(lua, true); return 1; }
	lua_pushboolean(lua, false);
	return 1;
}

static int subscribe_event(lua_State* lua, bool once)
{
	PangeaScriptBackend* backend = current_backend(lua);
	const char* eventName = luaL_checkstring(lua, 1);
	luaL_checktype(lua, 2, LUA_TFUNCTION);
	if (!eventName[0] || strlen(eventName) >= sizeof(backend->subscriptions[0].eventName))
		return luaL_error(lua, "event name must contain 1-63 bytes");
	for (int i = 0; i < (int)(sizeof(backend->subscriptions) / sizeof(backend->subscriptions[0])); i++)
	{
		if (backend->subscriptions[i].active) continue;
		backend->nextSubscriptionId++;
		if (backend->nextSubscriptionId <= 0) backend->nextSubscriptionId = 1;
		backend->subscriptions[i].active = true;
		backend->subscriptions[i].id = backend->nextSubscriptionId;
		backend->subscriptions[i].once = once;
		snprintf(backend->subscriptions[i].eventName, sizeof(backend->subscriptions[i].eventName), "%s", eventName);
		lua_pushvalue(lua, 2);
		backend->subscriptions[i].callbackReference = luaL_ref(lua, LUA_REGISTRYINDEX);
		lua_pushinteger(lua, backend->subscriptions[i].id);
		return 1;
	}
	return luaL_error(lua, "script event subscription capacity exceeded");
}

static int lua_events_on(lua_State* lua) { return subscribe_event(lua, false); }

static int lua_events_once(lua_State* lua) { return subscribe_event(lua, true); }

static int lua_events_off(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	int subscriptionId = (int) luaL_checkinteger(lua, 1);
	for (int i = 0; i < (int)(sizeof(backend->subscriptions) / sizeof(backend->subscriptions[0])); i++)
	{
		if (!backend->subscriptions[i].active || backend->subscriptions[i].id != subscriptionId) continue;
		luaL_unref(lua, LUA_REGISTRYINDEX, backend->subscriptions[i].callbackReference);
		backend->subscriptions[i].active = false;
		lua_pushboolean(lua, true);
		return 1;
	}
	lua_pushboolean(lua, false);
	return 1;
}

static int lua_events_emit(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	const char* eventName = luaL_checkstring(lua, 1);
	int lastSubscriptionId = backend->nextSubscriptionId;
	int delivered = 0;
	for (int i = 0; i < (int)(sizeof(backend->subscriptions) / sizeof(backend->subscriptions[0])); i++)
	{
		if (!backend->subscriptions[i].active || backend->subscriptions[i].id > lastSubscriptionId || strcmp(backend->subscriptions[i].eventName, eventName) != 0) continue;
		int callbackReference = backend->subscriptions[i].callbackReference;
		bool once = backend->subscriptions[i].once;
		lua_rawgeti(lua, LUA_REGISTRYINDEX, callbackReference);
		if (once)
		{
			luaL_unref(lua, LUA_REGISTRYINDEX, callbackReference);
			backend->subscriptions[i].active = false;
		}
		if (lua_gettop(lua) >= 2) lua_pushvalue(lua, 2); else lua_pushnil(lua);
		char eventError[512] = {0};
		PangeaScriptStatus status = protected_call(lua, 1, 0, PANGEA_LUA_EVENT_BUDGET, eventError, sizeof(eventError));
		if (status != PANGEA_SCRIPT_OK) return luaL_error(lua, "%s", eventError);
		delivered++;
	}
	lua_pushinteger(lua, delivered);
	return 1;
}

static uint32_t next_random(PangeaScriptBackend* backend)
{
	uint32_t value = backend->randomState ? backend->randomState : 0x6d2b79f5u;
	value ^= value << 13;
	value ^= value >> 17;
	value ^= value << 5;
	backend->randomState = value;
	return value;
}

static int lua_random_number(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	lua_pushnumber(lua, (lua_Number) next_random(backend) / (lua_Number) UINT32_MAX);
	return 1;
}

static int lua_random_integer(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	lua_Integer minimum = luaL_checkinteger(lua, 1);
	lua_Integer maximum = luaL_checkinteger(lua, 2);
	if (maximum < minimum) return luaL_error(lua, "maximum must be greater than or equal to minimum");
	uint64_t range = (uint64_t)(maximum - minimum) + 1;
	lua_pushinteger(lua, minimum + (lua_Integer)(next_random(backend) % range));
	return 1;
}

static int lua_random_seed(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	backend->randomState = (uint32_t) luaL_checkinteger(lua, 1);
	if (!backend->randomState) backend->randomState = 0x6d2b79f5u;
	return 0;
}

static int lua_capabilities(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	lua_createtable(lua, 0, 19);
	lua_pushboolean(lua, true); lua_setfield(lua, -2, "levelSettings");
	lua_pushboolean(lua, true); lua_setfield(lua, -2, "objectMutation");
	lua_pushboolean(lua, true); lua_setfield(lua, -2, "objectPosition");
	lua_pushboolean(lua, backend->gameInfo.spawnNative != NULL); lua_setfield(lua, -2, "spawnNative");
	lua_pushboolean(lua, true); lua_setfield(lua, -2, "spawnScripted");
	lua_pushboolean(lua, true); lua_setfield(lua, -2, "objectQueries");
	lua_pushboolean(lua, true); lua_setfield(lua, -2, "timers");
	lua_pushboolean(lua, true); lua_setfield(lua, -2, "tasks");
	lua_pushboolean(lua, true); lua_setfield(lua, -2, "events");
	lua_pushboolean(lua, backend->gameInfo.getPlayerCount != NULL && backend->gameInfo.getPlayer != NULL); lua_setfield(lua, -2, "playerLookup");
	lua_pushboolean(lua, strstr(backend->gameInfo.gameId, "MightyMike") == NULL); lua_setfield(lua, -2, "terrainItems");
	lua_pushboolean(lua, strstr(backend->gameInfo.gameId, "Nanosaur-android") == NULL && strstr(backend->gameInfo.gameId, "CroMag") == NULL && strstr(backend->gameInfo.gameId, "MightyMike") == NULL); lua_setfield(lua, -2, "splineItems");
	lua_pushboolean(lua, strstr(backend->gameInfo.gameId, "MightyMike") != NULL); lua_setfield(lua, -2, "mapItems");
	lua_pushinteger(lua, PANGEA_LUA_MEMORY_LIMIT); lua_setfield(lua, -2, "memoryLimitBytes");
	lua_pushinteger(lua, PANGEA_LUA_LOAD_BUDGET); lua_setfield(lua, -2, "loadInstructionBudget");
	lua_pushinteger(lua, PANGEA_LUA_EVENT_BUDGET); lua_setfield(lua, -2, "eventInstructionBudget");
	lua_pushinteger(lua, PANGEA_LUA_FRAME_BUDGET); lua_setfield(lua, -2, "frameInstructionBudget");
	lua_pushinteger(lua, (lua_Integer)(sizeof(backend->timers) / sizeof(backend->timers[0]))); lua_setfield(lua, -2, "timerLimit");
	lua_pushinteger(lua, (lua_Integer)(sizeof(backend->tasks) / sizeof(backend->tasks[0]))); lua_setfield(lua, -2, "taskLimit");
	lua_pushinteger(lua, (lua_Integer)(sizeof(backend->subscriptions) / sizeof(backend->subscriptions[0]))); lua_setfield(lua, -2, "subscriptionLimit");
	return 1;
}

static int lua_diagnostics(lua_State* lua)
{
	PangeaScriptBackend* backend = current_backend(lua);
	int activeTimers = 0;
	int activeTasks = 0;
	int activeSubscriptions = 0;
	for (int i = 0; i < (int)(sizeof(backend->timers) / sizeof(backend->timers[0])); i++)
		if (backend->timers[i].active) activeTimers++;
	for (int i = 0; i < (int)(sizeof(backend->tasks) / sizeof(backend->tasks[0])); i++)
		if (backend->tasks[i].active) activeTasks++;
	for (int i = 0; i < (int)(sizeof(backend->subscriptions) / sizeof(backend->subscriptions[0])); i++)
		if (backend->subscriptions[i].active) activeSubscriptions++;
	lua_createtable(lua, 0, 6);
	lua_pushinteger(lua, (lua_Integer) backend->allocatedBytes); lua_setfield(lua, -2, "memoryUsedBytes");
	lua_pushinteger(lua, (lua_Integer) backend->memoryLimitBytes); lua_setfield(lua, -2, "memoryLimitBytes");
	lua_pushinteger(lua, activeTimers); lua_setfield(lua, -2, "activeTimers");
	lua_pushinteger(lua, activeTasks); lua_setfield(lua, -2, "activeTasks");
	lua_pushinteger(lua, activeSubscriptions); lua_setfield(lua, -2, "activeSubscriptions");
	lua_pushinteger(lua, (lua_Integer) backend->currentFrameNum); lua_setfield(lua, -2, "frameNum");
	return 1;
}

static void set_function(lua_State* lua, const char* name, lua_CFunction function)
{
	lua_pushcfunction(lua, function); lua_setfield(lua, -2, name);
}

static void set_backend_function(lua_State* lua, const char* name, lua_CFunction function, PangeaScriptBackend* backend)
{
	lua_pushlightuserdata(lua, backend);
	lua_pushcclosure(lua, function, 1);
	lua_setfield(lua, -2, name);
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

static void push_supported_hooks(lua_State* lua, const char* gameId)
{
	static const char* levelHooks[] = {"onGameStart", "onGameShutdown", "onLevelLoad", "onLevelStart", "onFrame", "onLevelComplete", "onLevelUnload", "onTerrainItem", "onSplineItem", "onObjectFrame", "onPickupCollected", "onWeaponHit", "onTriggerEnter"};
	static const char* raceHooks[] = {"onGameStart", "onGameShutdown", "onRaceLoad", "onRaceStart", "onRaceFrame", "onRaceComplete", "onRaceUnload", "onTerrainItem", "onObjectFrame", "onPickupCollected", "onWeaponHit", "onTriggerEnter"};
	static const char* areaHooks[] = {"onGameStart", "onGameShutdown", "onAreaLoad", "onAreaStart", "onAreaFrame", "onAreaComplete", "onAreaUnload", "onTerrainItem", "onSplineItem", "onObjectFrame", "onPickupCollected", "onWeaponHit", "onTriggerEnter"};
	static const char* mapAreaHooks[] = {"onGameStart", "onGameShutdown", "onAreaLoad", "onAreaStart", "onAreaFrame", "onAreaComplete", "onAreaUnload", "onMapItem", "onObjectFrame", "onPickupCollected", "onWeaponHit", "onTriggerEnter"};
	static const char* nanosaurHooks[] = {"onGameStart", "onGameShutdown", "onLevelLoad", "onLevelStart", "onFrame", "onLevelComplete", "onLevelUnload", "onTerrainItem", "onObjectFrame", "onPickupCollected", "onWeaponHit", "onTriggerEnter"};
	const char* const* hooks = levelHooks;
	int count = (int)(sizeof(levelHooks) / sizeof(levelHooks[0]));
	if (strstr(gameId, "CroMag"))
	{
		hooks = raceHooks;
		count = (int)(sizeof(raceHooks) / sizeof(raceHooks[0]));
	}
	else if (strstr(gameId, "MightyMike"))
	{
		hooks = mapAreaHooks;
		count = (int)(sizeof(mapAreaHooks) / sizeof(mapAreaHooks[0]));
	}
	else if (strstr(gameId, "Billy"))
	{
		hooks = areaHooks;
		count = (int)(sizeof(areaHooks) / sizeof(areaHooks[0]));
	}
	else if (strstr(gameId, "Nanosaur-android"))
	{
		hooks = nanosaurHooks;
		count = (int)(sizeof(nanosaurHooks) / sizeof(nanosaurHooks[0]));
	}
	lua_createtable(lua, count, 0);
	for (int i = 0; i < count; i++)
	{
		lua_pushstring(lua, hooks[i]);
		lua_rawseti(lua, -2, i + 1);
	}
}

static void install_pangea(PangeaScriptBackend* backend)
{
	lua_State* lua = backend->lua;
	const PangeaScriptGameInfo* gameInfo = &backend->gameInfo;
	lua_getglobal(lua, "package");
	lua_pushstring(lua, "Data/Scripts/dist/modules/?.lua;Data/Scripts/dist/?.lua");
	lua_setfield(lua, -2, "path");
	lua_pop(lua, 1);
	lua_createtable(lua, 0, 6);
	lua_createtable(lua, 0, 5); lua_pushinteger(lua, PANGEA_LUA_API_VERSION); lua_setfield(lua, -2, "version"); lua_pushinteger(lua, PANGEA_LUA_MIN_API_VERSION); lua_setfield(lua, -2, "minimumVersion"); set_function(lua, "requireVersion", lua_api_require_version); set_backend_function(lua, "capabilities", lua_capabilities, backend); set_backend_function(lua, "diagnostics", lua_diagnostics, backend); lua_setfield(lua, -2, "api");
	lua_createtable(lua, 0, 3);
	for (int level = PANGEA_LOG_INFO; level <= PANGEA_LOG_ERROR; level++)
	{
		lua_pushinteger(lua, level); lua_pushcclosure(lua, lua_log, 1);
		lua_setfield(lua, -2, level == PANGEA_LOG_INFO ? "info" : level == PANGEA_LOG_WARN ? "warn" : "error");
	}
	lua_setfield(lua, -2, "log");
	lua_createtable(lua, 0, 3); set_function(lua, "native", lua_spawn_native); set_function(lua, "nativeResult", lua_spawn_native_result); set_function(lua, "scripted", lua_spawn_scripted); lua_setfield(lua, -2, "spawn");
	lua_createtable(lua, 0, 15); set_function(lua, "exists", lua_object_exists); set_function(lua, "all", lua_object_all); set_function(lua, "findByTag", lua_object_find_by_tag); set_function(lua, "nearest", lua_object_nearest); set_function(lua, "position", lua_object_position); set_function(lua, "setPosition", lua_object_set_position); set_function(lua, "setVelocity", lua_object_set_velocity); set_function(lua, "setRotation", lua_object_set_rotation); set_function(lua, "setScale", lua_object_set_scale); set_function(lua, "setAnimation", lua_object_set_animation); set_function(lua, "tags", lua_object_tags); set_function(lua, "hasTag", lua_object_has_tag); set_function(lua, "state", lua_object_state); set_function(lua, "delete", lua_object_delete); lua_setfield(lua, -2, "object");
	lua_createtable(lua, 0, 2); set_function(lua, "setting", lua_level_setting); set_backend_function(lua, "current", lua_level_current, backend); lua_setfield(lua, -2, "level");
	lua_createtable(lua, 0, 7); set_backend_function(lua, "frame", lua_time_frame, backend); set_backend_function(lua, "delta", lua_time_delta, backend); set_backend_function(lua, "level", lua_time_level, backend); set_backend_function(lua, "after", lua_time_after, backend); set_backend_function(lua, "every", lua_time_every, backend); set_backend_function(lua, "cancel", lua_time_cancel, backend); set_backend_function(lua, "isActive", lua_time_is_active, backend); lua_setfield(lua, -2, "time");
	lua_createtable(lua, 0, 4); set_backend_function(lua, "start", lua_task_start, backend); set_function(lua, "wait", lua_task_wait); set_backend_function(lua, "cancel", lua_task_cancel, backend); set_backend_function(lua, "isActive", lua_task_is_active, backend); lua_setfield(lua, -2, "task");
	lua_createtable(lua, 0, 4); set_backend_function(lua, "on", lua_events_on, backend); set_backend_function(lua, "once", lua_events_once, backend); set_backend_function(lua, "off", lua_events_off, backend); set_backend_function(lua, "emit", lua_events_emit, backend); lua_setfield(lua, -2, "events");
	lua_createtable(lua, 0, 3); set_backend_function(lua, "number", lua_random_number, backend); set_backend_function(lua, "integer", lua_random_integer, backend); set_backend_function(lua, "seed", lua_random_seed, backend); lua_setfield(lua, -2, "random");
	lua_createtable(lua, 0, 2); set_backend_function(lua, "count", lua_player_count, backend); set_backend_function(lua, "get", lua_player_get, backend); lua_setfield(lua, -2, "player");
	lua_createtable(lua, 0, 4); lua_pushstring(lua, gameInfo->gameId); lua_setfield(lua, -2, "id"); lua_pushstring(lua, gameInfo->gameName); lua_setfield(lua, -2, "name"); push_supported_hooks(lua, gameInfo->gameId); lua_setfield(lua, -2, "supportedHooks"); lua_createtable(lua, 0, 0); lua_setfield(lua, -2, "tags"); lua_setfield(lua, -2, "game");
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
	lua_getglobal(lua, "math");
	lua_pushnil(lua); lua_setfield(lua, -2, "random");
	lua_pushnil(lua); lua_setfield(lua, -2, "randomseed");
	lua_pop(lua, 1);
}

static bool reset_lua(PangeaScriptBackend* backend)
{
	if (backend->lua)
	{
		lua_close(backend->lua);
	}

	backend->allocatedBytes = 0;
	backend->memoryLimitBytes = PANGEA_LUA_MEMORY_LIMIT;
	backend->randomState = 0x6d2b79f5u;
	backend->currentLevelNum = 0;
	backend->currentFrameNum = 0;
	backend->currentDeltaSeconds = 0.0f;
	backend->currentLevelTimeSeconds = 0.0f;
	backend->nextTimerId = 0;
	backend->nextTaskId = 0;
	backend->nextSubscriptionId = 0;
	memset(backend->timers, 0, sizeof(backend->timers));
	memset(backend->tasks, 0, sizeof(backend->tasks));
	memset(backend->subscriptions, 0, sizeof(backend->subscriptions));
	backend->lua = lua_newstate(limited_allocator, backend);
	backend->entryReference = LUA_NOREF;
	if (!backend->lua)
	{
		return false;
	}

	open_safe_libraries(backend->lua);
	sandbox_lua(backend->lua);
	install_pangea(backend);
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

static void clear_timers(PangeaScriptBackend* backend)
{
	for (int i = 0; i < (int)(sizeof(backend->timers) / sizeof(backend->timers[0])); i++)
	{
		if (backend->timers[i].active)
			luaL_unref(backend->lua, LUA_REGISTRYINDEX, backend->timers[i].callbackReference);
		backend->timers[i].active = false;
	}
}

static void clear_tasks(PangeaScriptBackend* backend)
{
	for (int i = 0; i < (int)(sizeof(backend->tasks) / sizeof(backend->tasks[0])); i++)
	{
		if (backend->tasks[i].active)
			luaL_unref(backend->lua, LUA_REGISTRYINDEX, backend->tasks[i].threadReference);
		backend->tasks[i].active = false;
	}
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

static void push_base_context(PangeaScriptBackend* backend, int levelNum)
{
	lua_State* lua = backend->lua;
	backend->currentLevelNum = levelNum;
	lua_createtable(lua, 0, 8);
	lua_pushstring(lua, backend->gameInfo.gameId); lua_setfield(lua, -2, "gameId");
	lua_pushstring(lua, backend->gameInfo.gameName); lua_setfield(lua, -2, "gameName");
	lua_pushinteger(lua, levelNum); lua_setfield(lua, -2, "levelNum");
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
	PangeaScriptStatus status = protected_call(backend->lua, 0, 1, PANGEA_LUA_LOAD_BUDGET, error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	if (!lua_istable(backend->lua, -1)) { lua_pop(backend->lua, 1); copy_error(error, errorCapacity, "Lua bundle must return an entry table"); return PANGEA_SCRIPT_PARSE_ERROR; }
	backend->entryReference = luaL_ref(backend->lua, LUA_REGISTRYINDEX); return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (hook == PANGEA_SCRIPT_HOOK_LEVEL_LOAD)
	{
		clear_timers(backend);
		clear_tasks(backend);
		backend->currentFrameNum = 0;
		backend->currentDeltaSeconds = 0.0f;
		backend->currentLevelTimeSeconds = 0.0f;
	}
	if (!push_hook(backend, hook_name(backend, hook))) return PANGEA_SCRIPT_OK;
	push_base_context(backend, context->levelNum); lua_pushstring(backend->lua, context->levelName ? context->levelName : ""); lua_setfield(backend->lua, -2, "levelName"); return protected_call(backend->lua, 1, 0, PANGEA_LUA_EVENT_BUDGET, error, errorCapacity);
}

static PangeaScriptStatus call_due_timers(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	for (int i = 0; i < (int)(sizeof(backend->timers) / sizeof(backend->timers[0])); i++)
	{
		if (!backend->timers[i].active || backend->timers[i].dueTimeSeconds > backend->currentLevelTimeSeconds) continue;
		int callbackReference = backend->timers[i].callbackReference;
		bool repeating = backend->timers[i].repeating;
		if (repeating)
		{
			do backend->timers[i].dueTimeSeconds += backend->timers[i].intervalSeconds;
			while (backend->timers[i].dueTimeSeconds <= backend->currentLevelTimeSeconds);
		}
		else backend->timers[i].active = false;
		lua_rawgeti(backend->lua, LUA_REGISTRYINDEX, callbackReference);
		if (!repeating) luaL_unref(backend->lua, LUA_REGISTRYINDEX, callbackReference);
		PangeaScriptStatus status = protected_call(backend->lua, 0, 0, PANGEA_LUA_EVENT_BUDGET, error, errorCapacity);
		if (status != PANGEA_SCRIPT_OK)
		{
			if (repeating && backend->timers[i].active)
			{
				luaL_unref(backend->lua, LUA_REGISTRYINDEX, callbackReference);
				backend->timers[i].active = false;
			}
			return status;
		}
	}
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus call_due_tasks(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	for (int i = 0; i < (int)(sizeof(backend->tasks) / sizeof(backend->tasks[0])); i++)
	{
		if (!backend->tasks[i].active || backend->tasks[i].dueTimeSeconds > backend->currentLevelTimeSeconds) continue;
		PangeaScriptStatus status = resume_task(backend, i, error, errorCapacity);
		if (status != PANGEA_SCRIPT_OK) return status;
	}
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	backend->currentFrameNum = context->frameNum; backend->currentDeltaSeconds = context->deltaSeconds; backend->currentLevelTimeSeconds = context->levelTimeSeconds;
	backend->currentLevelNum = context->levelNum;
	PangeaScriptStatus timerStatus = call_due_timers(backend, error, errorCapacity);
	if (timerStatus != PANGEA_SCRIPT_OK) return timerStatus;
	PangeaScriptStatus taskStatus = call_due_tasks(backend, error, errorCapacity);
	if (taskStatus != PANGEA_SCRIPT_OK) return taskStatus;
	if (!push_hook(backend, hook_name(backend, PANGEA_SCRIPT_HOOK_FRAME))) return PANGEA_SCRIPT_OK;
	push_base_context(backend, context->levelNum); lua_pushinteger(backend->lua, context->frameNum); lua_setfield(backend->lua, -2, "frameNum"); lua_pushnumber(backend->lua, context->deltaSeconds); lua_setfield(backend->lua, -2, "deltaSeconds"); lua_pushnumber(backend->lua, context->levelTimeSeconds); lua_setfield(backend->lua, -2, "levelTimeSeconds"); return protected_call(backend->lua, 1, 0, PANGEA_LUA_FRAME_BUDGET, error, errorCapacity);
}

PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onTerrainItem")) return PANGEA_SCRIPT_OK;
	push_base_context(backend, context->levelNum); lua_pushinteger(backend->lua, context->itemType); lua_setfield(backend->lua, -2, "itemType"); lua_pushinteger(backend->lua, context->playerNum); lua_setfield(backend->lua, -2, "playerNum"); lua_pushboolean(backend->lua, context->networked); lua_setfield(backend->lua, -2, "networked"); push_vector(backend->lua, context->x, 0.0f, context->z); lua_setfield(backend->lua, -2, "position"); lua_pushinteger(backend->lua, context->flags); lua_setfield(backend->lua, -2, "flags"); push_params(backend->lua, context->params, context->paramCount);
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, PANGEA_LUA_EVENT_BUDGET, error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	status = validate_hook_result(backend->lua, "onTerrainItem", error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	status = validate_result_fields(backend->lua, "onTerrainItem", kItemResultFields, 3, error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	if (lua_istable(backend->lua, -1)) { lua_getfield(backend->lua, -1, "handled"); context->handled = lua_toboolean(backend->lua, -1); lua_pop(backend->lua, 1); lua_getfield(backend->lua, -1, "markInUse"); context->markInUse = lua_toboolean(backend->lua, -1); lua_pop(backend->lua, 1); lua_getfield(backend->lua, -1, "remappedItemType"); if (lua_isinteger(backend->lua, -1)) context->remappedItemType = (int) lua_tointeger(backend->lua, -1); lua_pop(backend->lua, 1); } lua_pop(backend->lua, 1); return status;
}

PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onSplineItem")) return PANGEA_SCRIPT_OK;
	push_base_context(backend, context->levelNum);
	lua_pushinteger(backend->lua, context->itemType);
	lua_setfield(backend->lua, -2, "itemType");
	lua_pushinteger(backend->lua, context->splineNum);
	lua_setfield(backend->lua, -2, "splineNum");
	lua_pushnumber(backend->lua, context->placement);
	lua_setfield(backend->lua, -2, "placement");
	push_params(backend->lua, context->params, context->paramCount);
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, PANGEA_LUA_EVENT_BUDGET, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK) status = validate_hook_result(backend->lua, "onSplineItem", error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK) status = validate_result_fields(backend->lua, "onSplineItem", kItemResultFields, 2, error, errorCapacity);
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
	push_base_context(backend, context->levelNum);
	lua_pushinteger(backend->lua, context->itemType);
	lua_setfield(backend->lua, -2, "itemType");
	lua_pushinteger(backend->lua, context->sceneNum);
	lua_setfield(backend->lua, -2, "scene");
	lua_pushinteger(backend->lua, context->areaNum);
	lua_setfield(backend->lua, -2, "area");
	push_vector2(backend->lua, context->x, context->y);
	lua_setfield(backend->lua, -2, "position");
	push_params(backend->lua, context->params, context->paramCount);
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, PANGEA_LUA_EVENT_BUDGET, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK) status = validate_hook_result(backend->lua, "onMapItem", error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK) status = validate_result_fields(backend->lua, "onMapItem", kItemResultFields, 2, error, errorCapacity);
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
	push_base_context(backend, context->levelNum);
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
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, PANGEA_LUA_FRAME_BUDGET, error, errorCapacity); if (status == PANGEA_SCRIPT_OK) status = validate_hook_result(backend->lua, "onObjectFrame", error, errorCapacity); if (status == PANGEA_SCRIPT_OK) status = validate_result_fields(backend->lua, "onObjectFrame", kObjectResultFields, 1, error, errorCapacity); if (status == PANGEA_SCRIPT_OK && lua_istable(backend->lua, -1)) { lua_getfield(backend->lua, -1, "positionOffset"); result->hasPositionOffset = read_vector(backend->lua, -1, &result->positionOffset); lua_pop(backend->lua, 1); } if (status == PANGEA_SCRIPT_OK) lua_pop(backend->lua, 1); return status;
}

static bool read_optional_boolean(lua_State* lua, int tableIndex, const char* field, bool* value)
{
	lua_getfield(lua, tableIndex, field);
	bool present = lua_isboolean(lua, -1);
	if (present) *value = lua_toboolean(lua, -1);
	lua_pop(lua, 1);
	return present;
}

static void read_common_result(lua_State* lua, bool* handled, int* scoreDelta)
{
	read_optional_boolean(lua, -1, "handled", handled);
	lua_getfield(lua, -1, "scoreDelta");
	if (lua_isinteger(lua, -1)) *scoreDelta = (int) lua_tointeger(lua, -1);
	lua_pop(lua, 1);
}

PangeaScriptStatus PangeaScriptBackend_CallTriggerHook(PangeaScriptBackend* backend, const PangeaScriptTriggerContext* context, PangeaScriptTriggerResult* result, char* error, int errorCapacity)
{
	if (!backend || !context || !result) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onTriggerEnter")) return PANGEA_SCRIPT_OK;
	push_base_context(backend, context->levelNum);
	lua_pushinteger(backend->lua, context->playerNum); lua_setfield(backend->lua, -2, "playerNum");
	lua_pushstring(backend->lua, context->triggerId ? context->triggerId : ""); lua_setfield(backend->lua, -2, "triggerId");
	lua_pushinteger(backend->lua, context->triggerType); lua_setfield(backend->lua, -2, "triggerType");
	lua_pushinteger(backend->lua, context->otherType); lua_setfield(backend->lua, -2, "otherType");
	lua_pushinteger(backend->lua, context->sideBits); lua_setfield(backend->lua, -2, "sideBits");
	lua_pushinteger(backend->lua, context->otherFlags); lua_setfield(backend->lua, -2, "otherFlags");
	push_handle(backend->lua, context->self); lua_setfield(backend->lua, -2, "self");
	push_handle(backend->lua, context->other); lua_setfield(backend->lua, -2, "other");
	push_vector(backend->lua, context->position.x, context->position.y, context->position.z); lua_setfield(backend->lua, -2, "position");
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, PANGEA_LUA_EVENT_BUDGET, error, errorCapacity);
	if (status != PANGEA_SCRIPT_OK) return status;
	status = validate_hook_result(backend->lua, "onTriggerEnter", error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	status = validate_result_fields(backend->lua, "onTriggerEnter", kTriggerResultFields, 7, error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	if (lua_istable(backend->lua, -1))
	{
		read_common_result(backend->lua, &result->handled, &result->scoreDelta);
		result->hasSolid = read_optional_boolean(backend->lua, -1, "solid", &result->solid);
		read_optional_boolean(backend->lua, -1, "deleteSelf", &result->deleteSelf);
		read_optional_boolean(backend->lua, -1, "deleteOther", &result->deleteOther);
		lua_getfield(backend->lua, -1, "damagePlayer"); if (lua_isnumber(backend->lua, -1)) result->damagePlayer = (float) lua_tonumber(backend->lua, -1); lua_pop(backend->lua, 1);
		lua_getfield(backend->lua, -1, "healthDelta"); if (lua_isnumber(backend->lua, -1)) result->healthDelta = (float) lua_tonumber(backend->lua, -1); lua_pop(backend->lua, 1);
	}
	lua_pop(backend->lua, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallPickupHook(PangeaScriptBackend* backend, const PangeaScriptPickupContext* context, PangeaScriptPickupResult* result, char* error, int errorCapacity)
{
	if (!backend || !context || !result) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onPickupCollected")) return PANGEA_SCRIPT_OK;
	push_base_context(backend, context->levelNum);
	lua_pushinteger(backend->lua, context->playerNum); lua_setfield(backend->lua, -2, "playerNum");
	lua_pushstring(backend->lua, context->pickupId ? context->pickupId : ""); lua_setfield(backend->lua, -2, "pickupId");
	lua_pushinteger(backend->lua, context->pickupType); lua_setfield(backend->lua, -2, "pickupType");
	lua_pushnumber(backend->lua, context->amount); lua_setfield(backend->lua, -2, "amount");
	push_handle(backend->lua, context->pickup); lua_setfield(backend->lua, -2, "pickup");
	push_handle(backend->lua, context->player); lua_setfield(backend->lua, -2, "player");
	push_vector(backend->lua, context->position.x, context->position.y, context->position.z); lua_setfield(backend->lua, -2, "position");
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, PANGEA_LUA_EVENT_BUDGET, error, errorCapacity);
	if (status != PANGEA_SCRIPT_OK) return status;
	status = validate_hook_result(backend->lua, "onPickupCollected", error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	status = validate_result_fields(backend->lua, "onPickupCollected", kPickupResultFields, 4, error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	if (lua_istable(backend->lua, -1))
	{
		read_common_result(backend->lua, &result->handled, &result->scoreDelta);
		result->hasConsumePickup = read_optional_boolean(backend->lua, -1, "consumePickup", &result->consumePickup);
		lua_getfield(backend->lua, -1, "healthDelta"); if (lua_isnumber(backend->lua, -1)) result->healthDelta = (float) lua_tonumber(backend->lua, -1); lua_pop(backend->lua, 1);
	}
	lua_pop(backend->lua, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallWeaponHitHook(PangeaScriptBackend* backend, const PangeaScriptWeaponHitContext* context, PangeaScriptWeaponHitResult* result, char* error, int errorCapacity)
{
	if (!backend || !context || !result) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onWeaponHit")) return PANGEA_SCRIPT_OK;
	push_base_context(backend, context->levelNum);
	lua_pushinteger(backend->lua, context->playerNum); lua_setfield(backend->lua, -2, "playerNum");
	lua_pushstring(backend->lua, context->weaponId ? context->weaponId : ""); lua_setfield(backend->lua, -2, "weaponId");
	lua_pushinteger(backend->lua, context->weaponType); lua_setfield(backend->lua, -2, "weaponType");
	lua_pushinteger(backend->lua, context->targetType); lua_setfield(backend->lua, -2, "targetType");
	lua_pushinteger(backend->lua, context->targetFlags); lua_setfield(backend->lua, -2, "targetFlags");
	lua_pushnumber(backend->lua, context->damage); lua_setfield(backend->lua, -2, "damage");
	push_handle(backend->lua, context->weapon); lua_setfield(backend->lua, -2, "weapon");
	push_handle(backend->lua, context->target); lua_setfield(backend->lua, -2, "target");
	push_vector(backend->lua, context->position.x, context->position.y, context->position.z); lua_setfield(backend->lua, -2, "position");
	PangeaScriptStatus status = protected_call(backend->lua, 1, 1, PANGEA_LUA_EVENT_BUDGET, error, errorCapacity);
	if (status != PANGEA_SCRIPT_OK) return status;
	status = validate_hook_result(backend->lua, "onWeaponHit", error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	status = validate_result_fields(backend->lua, "onWeaponHit", kWeaponResultFields, 5, error, errorCapacity); if (status != PANGEA_SCRIPT_OK) return status;
	result->damage = context->damage;
	if (lua_istable(backend->lua, -1))
	{
		read_common_result(backend->lua, &result->handled, &result->scoreDelta);
		result->hasApplyDamage = read_optional_boolean(backend->lua, -1, "applyDamage", &result->applyDamage);
		read_optional_boolean(backend->lua, -1, "destroyTarget", &result->destroyTarget);
		lua_getfield(backend->lua, -1, "damage"); if (lua_isnumber(backend->lua, -1)) result->damage = (float) lua_tonumber(backend->lua, -1); lua_pop(backend->lua, 1);
	}
	lua_pop(backend->lua, 1);
	return status;
}

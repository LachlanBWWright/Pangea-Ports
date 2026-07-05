#include "pangea_script_backend.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PANGEA_LUA_LOADED_KEY "PangeaScript.loaded"
#define PANGEA_LUA_PATH_CAPACITY 512

struct PangeaScriptBackend
{
	lua_State* state;
	PangeaScriptGameInfo gameInfo;
	int entryRef;
	int objectStateRef;
};

static void push_tags_array(lua_State* state, const char* const* tags, int tagCount);

static void copy_error(char* dest, int capacity, const char* message)
{
	if (!dest || capacity <= 0)
		return;
	snprintf(dest, (size_t) capacity, "%s", message ? message : "Unknown Lua script error");
}

static PangeaScriptBackend* get_backend(lua_State* state)
{
	return (PangeaScriptBackend*) lua_touserdata(state, lua_upvalueindex(1));
}

static bool read_number_field(lua_State* state, int index, const char* key, lua_Number* outValue)
{
	index = lua_absindex(state, index);
	lua_getfield(state, index, key);
	if (!lua_isnumber(state, -1))
	{
		lua_pop(state, 1);
		return false;
	}

	*outValue = lua_tonumber(state, -1);
	lua_pop(state, 1);
	return true;
}

static bool read_object_handle(lua_State* state, int index, PangeaScriptObjectHandle* outHandle)
{
	lua_Number id = 0.0;
	lua_Number generation = 0.0;
	if (!outHandle || !lua_istable(state, index))
		return false;

	if (!read_number_field(state, index, "id", &id) || !read_number_field(state, index, "generation", &generation))
		return false;

	outHandle->id = (int) id;
	outHandle->generation = (uint32_t) generation;
	return true;
}

static bool read_vector3(lua_State* state, int index, PangeaScriptVector3* outVector)
{
	lua_Number x = 0.0;
	lua_Number y = 0.0;
	lua_Number z = 0.0;
	if (!outVector || !lua_istable(state, index))
		return false;

	if (!read_number_field(state, index, "x", &x) || !read_number_field(state, index, "y", &y) || !read_number_field(state, index, "z", &z))
		return false;

	outVector->x = (float) x;
	outVector->y = (float) y;
	outVector->z = (float) z;
	return true;
}

static void push_object_handle(lua_State* state, PangeaScriptObjectHandle handle)
{
	lua_createtable(state, 0, 2);
	lua_pushinteger(state, handle.id);
	lua_setfield(state, -2, "id");
	lua_pushinteger(state, (lua_Integer) handle.generation);
	lua_setfield(state, -2, "generation");
}

static void push_vector3(lua_State* state, const PangeaScriptVector3* vector)
{
	lua_createtable(state, 0, 3);
	lua_pushnumber(state, vector->x);
	lua_setfield(state, -2, "x");
	lua_pushnumber(state, vector->y);
	lua_setfield(state, -2, "y");
	lua_pushnumber(state, vector->z);
	lua_setfield(state, -2, "z");
}

static void push_object_info(lua_State* state, const PangeaScriptObjectInfo* info)
{
	lua_createtable(state, 0, 9);
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_TYPE)
	{
		lua_pushinteger(state, info->type);
		lua_setfield(state, -2, "type");
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_KIND)
	{
		lua_pushinteger(state, info->kind);
		lua_setfield(state, -2, "kind");
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_MODE)
	{
		lua_pushinteger(state, info->mode);
		lua_setfield(state, -2, "mode");
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_FLAGS)
	{
		lua_pushinteger(state, (lua_Integer) info->statusBits);
		lua_setfield(state, -2, "statusBits");
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_COLLISION)
	{
		lua_pushinteger(state, (lua_Integer) info->cType);
		lua_setfield(state, -2, "cType");
		lua_pushinteger(state, (lua_Integer) info->cBits);
		lua_setfield(state, -2, "cBits");
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_HEALTH)
	{
		lua_pushnumber(state, info->health);
		lua_setfield(state, -2, "health");
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_DAMAGE)
	{
		lua_pushnumber(state, info->damage);
		lua_setfield(state, -2, "damage");
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_VELOCITY)
	{
		push_vector3(state, &info->velocity);
		lua_setfield(state, -2, "velocity");
	}
}

static void push_object_bounds(lua_State* state, const PangeaScriptObjectBounds* bounds)
{
	lua_createtable(state, 0, 6);
	lua_pushnumber(state, bounds->left);
	lua_setfield(state, -2, "left");
	lua_pushnumber(state, bounds->right);
	lua_setfield(state, -2, "right");
	lua_pushnumber(state, bounds->front);
	lua_setfield(state, -2, "front");
	lua_pushnumber(state, bounds->back);
	lua_setfield(state, -2, "back");
	lua_pushnumber(state, bounds->top);
	lua_setfield(state, -2, "top");
	lua_pushnumber(state, bounds->bottom);
	lua_setfield(state, -2, "bottom");
}

static void push_object_params(lua_State* state, const PangeaScriptObjectParams* params)
{
	lua_createtable(state, params->count, 0);
	for (int i = 0; i < params->count; i++)
	{
		lua_pushinteger(state, params->values[i]);
		lua_rawseti(state, -2, i + 1);
	}
}

static bool read_object_params(lua_State* state, int index, PangeaScriptObjectParams* outParams)
{
	if (!outParams || !lua_istable(state, index))
		return false;

	*outParams = (PangeaScriptObjectParams){0};
	lua_Integer count = luaL_len(state, index);
	if (count < 0 || count > PANGEA_SCRIPT_MAX_OBJECT_PARAMS)
		return false;

	outParams->count = (int) count;
	for (int i = 0; i < outParams->count; i++)
	{
		lua_rawgeti(state, index, i + 1);
		if (!lua_isinteger(state, -1))
		{
			lua_pop(state, 1);
			return false;
		}

		outParams->values[i] = (int) lua_tointeger(state, -1);
		lua_pop(state, 1);
	}

	return true;
}

static void push_player_info(lua_State* state, const PangeaScriptPlayerInfo* info)
{
	lua_createtable(state, 0, 8);
	lua_pushinteger(state, info->playerNum);
	lua_setfield(state, -2, "playerNum");
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SCORE)
	{
		lua_pushinteger(state, info->score);
		lua_setfield(state, -2, "score");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
	{
		lua_pushnumber(state, info->health);
		lua_setfield(state, -2, "health");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
	{
		lua_pushinteger(state, info->lives);
		lua_setfield(state, -2, "lives");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_AMMO)
	{
		lua_pushinteger(state, info->ammo);
		lua_setfield(state, -2, "ammo");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_FUEL)
	{
		lua_pushnumber(state, info->fuel);
		lua_setfield(state, -2, "fuel");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SHIELD)
	{
		lua_pushnumber(state, info->shield);
		lua_setfield(state, -2, "shield");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_CURRENCY)
	{
		lua_pushinteger(state, info->currency);
		lua_setfield(state, -2, "currency");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE)
	{
		lua_pushinteger(state, info->inventoryType);
		lua_setfield(state, -2, "inventoryType");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY)
	{
		lua_pushinteger(state, info->inventoryQuantity);
		lua_setfield(state, -2, "inventoryQuantity");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_BOOST_TIMER)
	{
		lua_pushnumber(state, info->boostTimer);
		lua_setfield(state, -2, "boostTimer");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_TRACTION_TIMER)
	{
		lua_pushnumber(state, info->tractionTimer);
		lua_setfield(state, -2, "tractionTimer");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVISIBILITY_TIMER)
	{
		lua_pushnumber(state, info->invisibilityTimer);
		lua_setfield(state, -2, "invisibilityTimer");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HAZARD_TIMER)
	{
		lua_pushnumber(state, info->hazardTimer);
		lua_setfield(state, -2, "hazardTimer");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A)
	{
		lua_pushinteger(state, info->collectibleA);
		lua_setfield(state, -2, "collectibleA");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B)
	{
		lua_pushinteger(state, info->collectibleB);
		lua_setfield(state, -2, "collectibleB");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C)
	{
		lua_pushinteger(state, info->collectibleC);
		lua_setfield(state, -2, "collectibleC");
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_D)
	{
		lua_pushinteger(state, info->collectibleD);
		lua_setfield(state, -2, "collectibleD");
	}
}

static bool read_optional_number_field(lua_State* state, int index, const char* key, lua_Number* outValue)
{
	lua_getfield(state, index, key);
	if (lua_isnil(state, -1))
	{
		lua_pop(state, 1);
		return false;
	}
	if (!lua_isnumber(state, -1))
	{
		lua_pop(state, 1);
		return false;
	}

	*outValue = lua_tonumber(state, -1);
	lua_pop(state, 1);
	return true;
}

static bool read_optional_vector3_field(lua_State* state, int index, const char* key, PangeaScriptVector3* outVector)
{
	bool found;
	lua_getfield(state, index, key);
	found = !lua_isnil(state, -1);
	if (found && !read_vector3(state, lua_gettop(state), outVector))
		found = false;
	lua_pop(state, 1);
	return found;
}

static bool read_object_info(lua_State* state, int index, PangeaScriptObjectInfo* outInfo)
{
	lua_Number value = 0.0;
	if (!outInfo || !lua_istable(state, index))
		return false;

	*outInfo = (PangeaScriptObjectInfo){0};
	if (read_optional_number_field(state, index, "type", &value))
	{
		outInfo->type = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_TYPE;
	}
	if (read_optional_number_field(state, index, "kind", &value))
	{
		outInfo->kind = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_KIND;
	}
	if (read_optional_number_field(state, index, "mode", &value))
	{
		outInfo->mode = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_MODE;
	}
	if (read_optional_number_field(state, index, "statusBits", &value))
	{
		outInfo->statusBits = (unsigned int) value;
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_FLAGS;
	}
	if (read_optional_number_field(state, index, "cType", &value))
	{
		outInfo->cType = (unsigned int) value;
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_COLLISION;
	}
	if (read_optional_number_field(state, index, "cBits", &value))
	{
		outInfo->cBits = (unsigned int) value;
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_COLLISION;
	}
	if (read_optional_number_field(state, index, "health", &value))
	{
		outInfo->health = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_HEALTH;
	}
	if (read_optional_number_field(state, index, "damage", &value))
	{
		outInfo->damage = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_DAMAGE;
	}
	if (read_optional_vector3_field(state, index, "velocity", &outInfo->velocity))
		outInfo->validMask |= PANGEA_SCRIPT_OBJECT_INFO_VELOCITY;

	return outInfo->validMask != PANGEA_SCRIPT_OBJECT_INFO_NONE;
}

static bool read_object_bounds(lua_State* state, int index, PangeaScriptObjectBounds* outBounds)
{
	lua_Number value = 0.0;
	if (!outBounds || !lua_istable(state, index))
		return false;

	if (!read_number_field(state, index, "left", &value))
		return false;
	outBounds->left = (float) value;
	if (!read_number_field(state, index, "right", &value))
		return false;
	outBounds->right = (float) value;
	if (!read_number_field(state, index, "front", &value))
		return false;
	outBounds->front = (float) value;
	if (!read_number_field(state, index, "back", &value))
		return false;
	outBounds->back = (float) value;
	if (!read_number_field(state, index, "top", &value))
		return false;
	outBounds->top = (float) value;
	if (!read_number_field(state, index, "bottom", &value))
		return false;
	outBounds->bottom = (float) value;
	return true;
}

static bool read_player_info(lua_State* state, int index, PangeaScriptPlayerInfo* outInfo)
{
	lua_Number value = 0.0;
	if (!outInfo || !lua_istable(state, index))
		return false;

	*outInfo = (PangeaScriptPlayerInfo){0};
	if (read_optional_number_field(state, index, "score", &value))
	{
		outInfo->score = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_SCORE;
	}
	if (read_optional_number_field(state, index, "health", &value))
	{
		outInfo->health = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_HEALTH;
	}
	if (read_optional_number_field(state, index, "lives", &value))
	{
		outInfo->lives = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_LIVES;
	}
	if (read_optional_number_field(state, index, "ammo", &value))
	{
		outInfo->ammo = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_AMMO;
	}
	if (read_optional_number_field(state, index, "fuel", &value))
	{
		outInfo->fuel = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_FUEL;
	}
	if (read_optional_number_field(state, index, "shield", &value))
	{
		outInfo->shield = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_SHIELD;
	}
	if (read_optional_number_field(state, index, "currency", &value))
	{
		outInfo->currency = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_CURRENCY;
	}
	if (read_optional_number_field(state, index, "inventoryType", &value))
	{
		outInfo->inventoryType = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE;
	}
	if (read_optional_number_field(state, index, "inventoryQuantity", &value))
	{
		outInfo->inventoryQuantity = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY;
	}
	if (read_optional_number_field(state, index, "boostTimer", &value))
	{
		outInfo->boostTimer = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_BOOST_TIMER;
	}
	if (read_optional_number_field(state, index, "tractionTimer", &value))
	{
		outInfo->tractionTimer = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_TRACTION_TIMER;
	}
	if (read_optional_number_field(state, index, "invisibilityTimer", &value))
	{
		outInfo->invisibilityTimer = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_INVISIBILITY_TIMER;
	}
	if (read_optional_number_field(state, index, "hazardTimer", &value))
	{
		outInfo->hazardTimer = (float) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_HAZARD_TIMER;
	}
	if (read_optional_number_field(state, index, "collectibleA", &value))
	{
		outInfo->collectibleA = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A;
	}
	if (read_optional_number_field(state, index, "collectibleB", &value))
	{
		outInfo->collectibleB = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B;
	}
	if (read_optional_number_field(state, index, "collectibleC", &value))
	{
		outInfo->collectibleC = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C;
	}
	if (read_optional_number_field(state, index, "collectibleD", &value))
	{
		outInfo->collectibleD = (int) value;
		outInfo->validMask |= PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_D;
	}

	return outInfo->validMask != PANGEA_SCRIPT_PLAYER_INFO_NONE;
}

static void push_object_state_key(lua_State* state, PangeaScriptObjectHandle handle)
{
	lua_pushfstring(state, "%d:%d", handle.id, (int) handle.generation);
}

static int log_info(lua_State* state)
{
	PangeaScript_Log(PANGEA_LOG_INFO, "Lua", luaL_tolstring(state, 1, NULL));
	lua_pop(state, 1);
	return 0;
}

static int log_warn(lua_State* state)
{
	PangeaScript_Log(PANGEA_LOG_WARN, "Lua", luaL_tolstring(state, 1, NULL));
	lua_pop(state, 1);
	return 0;
}

static int log_error(lua_State* state)
{
	PangeaScript_Log(PANGEA_LOG_ERROR, "Lua", luaL_tolstring(state, 1, NULL));
	lua_pop(state, 1);
	return 0;
}

static int level_current(lua_State* state)
{
	lua_pushnil(state);
	return 1;
}

static int player_get(lua_State* state)
{
	int playerNum = (int) luaL_optinteger(state, 1, 0);
	PangeaScriptPlayerInfo info;
	if (!PangeaScript_GetPlayerInfo(playerNum, &info))
	{
		lua_pushnil(state);
		return 1;
	}

	push_player_info(state, &info);
	return 1;
}

static int player_set(lua_State* state)
{
	int playerNum = (int) luaL_optinteger(state, 1, 0);
	PangeaScriptPlayerInfo info;
	if (!read_player_info(state, 2, &info))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	info.playerNum = playerNum;
	lua_pushboolean(state, PangeaScript_SetPlayerInfo(playerNum, &info));
	return 1;
}

static void read_sound_request_options(lua_State* state, int index, PangeaScriptSoundRequest* request)
{
	lua_Number number = 0.0;
	if (!request || !lua_istable(state, index))
		return;

	if (read_optional_vector3_field(state, index, "position", &request->position))
		request->hasPosition = true;
	if (read_number_field(state, index, "volume", &number))
	{
		request->volume = (float) number;
		request->hasVolume = true;
	}
	if (read_number_field(state, index, "rate", &number))
	{
		request->rate = (float) number;
		request->hasRate = true;
	}
}

static int effects_play_sound(lua_State* state)
{
	if (!lua_isnumber(state, 1))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	PangeaScriptSoundRequest request = {
		.soundId = (int) lua_tonumber(state, 1),
	};
	read_sound_request_options(state, 2, &request);
	lua_pushboolean(state, PangeaScript_PlaySound(&request));
	return 1;
}

static void read_effect_request_options(lua_State* state, int index, PangeaScriptEffectRequest* request)
{
	lua_Number number = 0.0;
	if (!request || !lua_istable(state, index))
		return;

	if (read_optional_vector3_field(state, index, "position", &request->position))
		request->hasPosition = true;
	if (read_optional_vector3_field(state, index, "velocity", &request->velocity))
		request->hasVelocity = true;
	if (read_number_field(state, index, "scale", &number))
	{
		request->scale = (float) number;
		request->hasScale = true;
	}
	if (read_number_field(state, index, "quantity", &number))
	{
		request->quantity = (int) number;
		request->hasQuantity = true;
	}
}

static int effects_spawn(lua_State* state)
{
	if (!lua_isnumber(state, 1))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	PangeaScriptEffectRequest request = {
		.effectId = (int) lua_tonumber(state, 1),
	};
	read_effect_request_options(state, 2, &request);
	lua_pushboolean(state, PangeaScript_SpawnEffect(&request));
	return 1;
}

static void read_spawn_options(lua_State* state, int index, int* outSubtype, int* outAmount)
{
	*outSubtype = -1;
	*outAmount = -1;
	if (!lua_istable(state, index))
		return;

	lua_Number value = 0.0;
	if (read_number_field(state, index, "subtype", &value))
		*outSubtype = (int) value;
	if (read_number_field(state, index, "amount", &value))
		*outAmount = (int) value;
}

static int spawn_native(lua_State* state)
{
	const char* id = luaL_checkstring(state, 1);
	PangeaScriptVector3 position;
	if (!read_vector3(state, 2, &position))
	{
		lua_pushnil(state);
		return 1;
	}

	int subtype = -1;
	int amount = -1;
	read_spawn_options(state, 3, &subtype, &amount);

	PangeaScriptObjectHandle handle = {0, 0};
	PangeaScriptStatus status = PangeaScript_SpawnNative(id, position.x, position.y, position.z, subtype, amount, &handle);
	if (status == PANGEA_SCRIPT_OK && handle.id > 0)
	{
		push_object_handle(state, handle);
		return 1;
	}

	lua_pushnil(state);
	return 1;
}

static int spawn_scripted(lua_State* state)
{
	const char* id = luaL_checkstring(state, 1);
	PangeaScriptVector3 position;
	if (!read_vector3(state, 2, &position))
	{
		lua_pushnil(state);
		return 1;
	}

	PangeaScriptObjectHandle handle = {0, 0};
	PangeaScriptStatus status = PangeaScript_RegisterScriptedObject(id, position.x, position.y, position.z, &handle);
	if (status == PANGEA_SCRIPT_OK && handle.id > 0)
	{
		push_object_handle(state, handle);
		return 1;
	}

	lua_pushnil(state);
	return 1;
}

static int object_position(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptVector3 position;
	if (!read_object_handle(state, 1, &handle) || !PangeaScript_GetObjectPosition(handle, &position))
	{
		lua_pushnil(state);
		return 1;
	}

	push_vector3(state, &position);
	return 1;
}

static int object_info(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectInfo info;
	if (!read_object_handle(state, 1, &handle) || !PangeaScript_GetObjectInfo(handle, &info))
	{
		lua_pushnil(state);
		return 1;
	}

	push_object_info(state, &info);
	return 1;
}

static int object_bounds(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectBounds bounds;
	if (!read_object_handle(state, 1, &handle) || !PangeaScript_GetObjectBounds(handle, &bounds))
	{
		lua_pushnil(state);
		return 1;
	}

	push_object_bounds(state, &bounds);
	return 1;
}

static int object_params(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectParams params;
	if (!read_object_handle(state, 1, &handle) || !PangeaScript_GetObjectParams(handle, &params))
	{
		lua_pushnil(state);
		return 1;
	}

	push_object_params(state, &params);
	return 1;
}

static int object_exists(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	lua_pushboolean(state, read_object_handle(state, 1, &handle) && PangeaScript_ObjectExists(handle));
	return 1;
}

static int object_tags(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	if (!read_object_handle(state, 1, &handle))
	{
		lua_createtable(state, 0, 0);
		return 1;
	}

	int tagCount = PangeaScript_GetObjectTagCount(handle);
	lua_createtable(state, tagCount, 0);
	for (int i = 0; i < tagCount; i++)
	{
		const char* tag = PangeaScript_GetObjectTag(handle, i);
		if (tag)
		{
			lua_pushstring(state, tag);
			lua_rawseti(state, -2, i + 1);
		}
	}
	return 1;
}

static int object_has_tag(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	const char* tag = luaL_checkstring(state, 2);
	lua_pushboolean(state, read_object_handle(state, 1, &handle) && PangeaScript_ObjectHasTag(handle, tag));
	return 1;
}

static int object_add_tag(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	const char* tag = luaL_checkstring(state, 2);
	lua_pushboolean(state, read_object_handle(state, 1, &handle) && PangeaScript_AddObjectTag(handle, tag));
	return 1;
}

static int object_remove_tag(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	const char* tag = luaL_checkstring(state, 2);
	lua_pushboolean(state, read_object_handle(state, 1, &handle) && PangeaScript_RemoveObjectTag(handle, tag));
	return 1;
}

static int object_state(lua_State* state)
{
	PangeaScriptBackend* backend = get_backend(state);
	PangeaScriptObjectHandle handle;
	if (!backend || backend->objectStateRef == LUA_NOREF || !read_object_handle(state, 1, &handle) || !PangeaScript_ObjectExists(handle))
	{
		lua_pushnil(state);
		return 1;
	}

	lua_rawgeti(state, LUA_REGISTRYINDEX, backend->objectStateRef);
	push_object_state_key(state, handle);
	lua_gettable(state, -2);
	if (!lua_istable(state, -1))
	{
		lua_pop(state, 1);
		lua_createtable(state, 0, 0);
		push_object_state_key(state, handle);
		lua_pushvalue(state, -2);
		lua_settable(state, -4);
	}
	lua_remove(state, -2);
	return 1;
}

static int object_set_position(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptVector3 position;
	if (!read_object_handle(state, 1, &handle) || !read_vector3(state, 2, &position))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	lua_pushboolean(state, PangeaScript_SetObjectPosition(handle, &position));
	return 1;
}

static int object_set_velocity(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptVector3 velocity;
	if (!read_object_handle(state, 1, &handle) || !read_vector3(state, 2, &velocity))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	lua_pushboolean(state, PangeaScript_SetObjectVelocity(handle, &velocity));
	return 1;
}

static int object_set_info(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectInfo info;
	if (!read_object_handle(state, 1, &handle) || !read_object_info(state, 2, &info))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	lua_pushboolean(state, PangeaScript_SetObjectInfo(handle, &info));
	return 1;
}

static int object_set_bounds(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectBounds bounds;
	if (!read_object_handle(state, 1, &handle) || !read_object_bounds(state, 2, &bounds))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	lua_pushboolean(state, PangeaScript_SetObjectBounds(handle, &bounds));
	return 1;
}

static int object_set_params(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectParams params;
	if (!read_object_handle(state, 1, &handle) || !read_object_params(state, 2, &params))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	lua_pushboolean(state, PangeaScript_SetObjectParams(handle, &params));
	return 1;
}

static int object_delete(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	if (!read_object_handle(state, 1, &handle))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	lua_pushboolean(state, PangeaScript_DeleteObject(handle));
	return 1;
}

static int object_request_delete(lua_State* state)
{
	PangeaScriptObjectHandle handle;
	if (!read_object_handle(state, 1, &handle))
	{
		lua_pushboolean(state, 0);
		return 1;
	}

	lua_pushboolean(state, PangeaScript_RequestDeleteObject(handle));
	return 1;
}

static int api_capabilities(lua_State* state)
{
	lua_createtable(state, 0, 8);
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "objectPosition");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "objectMutation");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "objectTags");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "objectDynamicTags");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "objectState");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "objectInfo");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "objectBounds");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "objectParams");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "playerInfo");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "effects");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "spawnNative");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "spawnScripted");
	lua_pushboolean(state, 1);
	lua_setfield(state, -2, "levelSettings");
	return 1;
}

static void push_c_function(lua_State* state, PangeaScriptBackend* backend, lua_CFunction function)
{
	lua_pushlightuserdata(state, backend);
	lua_pushcclosure(state, function, 1);
}

static void set_c_function(lua_State* state, PangeaScriptBackend* backend, const char* name, lua_CFunction function)
{
	push_c_function(state, backend, function);
	lua_setfield(state, -2, name);
}

static void push_pangea_api(lua_State* state, PangeaScriptBackend* backend)
{
	lua_createtable(state, 0, 8);

	lua_createtable(state, 0, 2);
	lua_pushinteger(state, 1);
	lua_setfield(state, -2, "version");
	set_c_function(state, backend, "capabilities", api_capabilities);
	lua_setfield(state, -2, "api");

	lua_createtable(state, 0, 2);
	lua_pushstring(state, backend->gameInfo.gameId);
	lua_setfield(state, -2, "id");
	lua_pushstring(state, backend->gameInfo.gameName);
	lua_setfield(state, -2, "name");
	lua_setfield(state, -2, "game");

	lua_createtable(state, 0, 3);
	set_c_function(state, backend, "info", log_info);
	set_c_function(state, backend, "warn", log_warn);
	set_c_function(state, backend, "error", log_error);
	lua_setfield(state, -2, "log");

	lua_createtable(state, 0, 2);
	set_c_function(state, backend, "native", spawn_native);
	set_c_function(state, backend, "scripted", spawn_scripted);
	lua_setfield(state, -2, "spawn");

	lua_createtable(state, 0, 8);
	set_c_function(state, backend, "exists", object_exists);
	set_c_function(state, backend, "position", object_position);
	set_c_function(state, backend, "info", object_info);
	set_c_function(state, backend, "bounds", object_bounds);
	set_c_function(state, backend, "params", object_params);
	set_c_function(state, backend, "setPosition", object_set_position);
	set_c_function(state, backend, "setVelocity", object_set_velocity);
	set_c_function(state, backend, "setInfo", object_set_info);
	set_c_function(state, backend, "setBounds", object_set_bounds);
	set_c_function(state, backend, "setParams", object_set_params);
	set_c_function(state, backend, "tags", object_tags);
	set_c_function(state, backend, "hasTag", object_has_tag);
	set_c_function(state, backend, "addTag", object_add_tag);
	set_c_function(state, backend, "removeTag", object_remove_tag);
	set_c_function(state, backend, "state", object_state);
	set_c_function(state, backend, "delete", object_delete);
	set_c_function(state, backend, "requestDelete", object_request_delete);
	lua_setfield(state, -2, "object");

	lua_createtable(state, 0, 2);
	set_c_function(state, backend, "info", player_get);
	set_c_function(state, backend, "setInfo", player_set);
	lua_setfield(state, -2, "player");

	lua_createtable(state, 0, 2);
	set_c_function(state, backend, "playSound", effects_play_sound);
	set_c_function(state, backend, "spawn", effects_spawn);
	lua_setfield(state, -2, "effects");

	lua_createtable(state, 0, 3);
	lua_createtable(state, 0, 1);
	set_c_function(state, backend, "current", level_current);
	lua_setfield(state, -2, "level");
	lua_createtable(state, 0, 1);
	set_c_function(state, backend, "get", player_get);
	lua_setfield(state, -2, "player");
	lua_createtable(state, 0, 1);
	set_c_function(state, backend, "scripted", spawn_scripted);
	lua_setfield(state, -2, "spawn");
	lua_setfield(state, -2, "experimental");
}

static bool build_module_path(const char* moduleName, char* outPath, size_t outPathCapacity)
{
	if (!moduleName || !outPath || outPathCapacity == 0)
		return false;
	if (strstr(moduleName, "..") || strchr(moduleName, '\\') || moduleName[0] == '/' || strchr(moduleName, ':'))
		return false;

	const char* relative = moduleName;
	if (strncmp(relative, "./", 2) == 0)
		relative += 2;

	if (strncmp(relative, "Data/Scripts/", 13) == 0)
	{
		snprintf(outPath, outPathCapacity, "%s", relative);
	}
	else
	{
		snprintf(outPath, outPathCapacity, "Data/Scripts/src/%s", relative);
	}

	size_t length = strlen(outPath);
	if (length < 4 || strcmp(outPath + length - 4, ".lua") != 0)
	{
		if (length + 4 >= outPathCapacity)
			return false;
		strcat(outPath, ".lua");
	}

	return strncmp(outPath, "Data/Scripts/", 13) == 0;
}

static char* read_text_file(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return NULL;
	}
	long length = ftell(file);
	if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return NULL;
	}
	char* data = (char*) malloc((size_t) length + 1);
	if (!data)
	{
		fclose(file);
		return NULL;
	}
	size_t readLength = fread(data, 1, (size_t) length, file);
	fclose(file);
	data[readLength] = '\0';
	return data;
}

static int pangea_require(lua_State* state)
{
	PangeaScriptBackend* backend = get_backend(state);
	const char* moduleName = luaL_checkstring(state, 1);
	if (!backend)
		return luaL_error(state, "script backend is unavailable");

	if (strcmp(moduleName, "pangea") == 0)
	{
		lua_getglobal(state, "pangea");
		return 1;
	}

	char modulePath[PANGEA_LUA_PATH_CAPACITY];
	if (!build_module_path(moduleName, modulePath, sizeof(modulePath)))
		return luaL_error(state, "unsupported module path '%s'", moduleName);

	lua_getfield(state, LUA_REGISTRYINDEX, PANGEA_LUA_LOADED_KEY);
	lua_getfield(state, -1, modulePath);
	if (!lua_isnil(state, -1))
	{
		lua_remove(state, -2);
		return 1;
	}
	lua_pop(state, 1);

	char* source = read_text_file(modulePath);
	if (!source)
	{
		lua_pop(state, 1);
		return luaL_error(state, "module not found: %s", modulePath);
	}

	char chunkName[PANGEA_LUA_PATH_CAPACITY + 2];
	snprintf(chunkName, sizeof(chunkName), "@%s", modulePath);
	int loadStatus = luaL_loadbufferx(state, source, strlen(source), chunkName, "t");
	free(source);
	if (loadStatus != LUA_OK)
	{
		const char* message = lua_tostring(state, -1);
		return luaL_error(state, "%s", message ? message : "failed to load Lua module");
	}

	if (lua_pcall(state, 0, 1, 0) != LUA_OK)
	{
		const char* message = lua_tostring(state, -1);
		return luaL_error(state, "%s", message ? message : "failed to run Lua module");
	}

	if (lua_isnil(state, -1))
	{
		lua_pop(state, 1);
		lua_pushboolean(state, 1);
	}

	lua_pushvalue(state, -1);
	lua_setfield(state, -3, modulePath);
	lua_remove(state, -2);
	return 1;
}

static void install_global_require(lua_State* state, PangeaScriptBackend* backend)
{
	lua_newtable(state);
	lua_setfield(state, LUA_REGISTRYINDEX, PANGEA_LUA_LOADED_KEY);

	push_pangea_api(state, backend);
	lua_pushvalue(state, -1);
	lua_setglobal(state, "pangea");

	lua_getfield(state, LUA_REGISTRYINDEX, PANGEA_LUA_LOADED_KEY);
	lua_pushvalue(state, -2);
	lua_setfield(state, -2, "pangea");
	lua_pop(state, 2);

	push_c_function(state, backend, pangea_require);
	lua_setglobal(state, "require");
}

static void open_sandbox_libs(lua_State* state)
{
	luaL_requiref(state, "_G", luaopen_base, 1);
	lua_pop(state, 1);
	luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
	lua_pop(state, 1);
	luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
	lua_pop(state, 1);
	luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
	lua_pop(state, 1);
	luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
	lua_pop(state, 1);

	lua_pushnil(state);
	lua_setglobal(state, "dofile");
	lua_pushnil(state);
	lua_setglobal(state, "loadfile");
	lua_pushnil(state);
	lua_setglobal(state, "collectgarbage");
}

static const char* hook_name(PangeaScriptHook hook)
{
	switch (hook)
	{
		case PANGEA_SCRIPT_HOOK_GAME_START: return "onGameStart";
		case PANGEA_SCRIPT_HOOK_LEVEL_LOAD: return "onLevelLoad";
		case PANGEA_SCRIPT_HOOK_LEVEL_START: return "onLevelStart";
		case PANGEA_SCRIPT_HOOK_FRAME: return "onFrame";
		case PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE: return "onLevelComplete";
		case PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD: return "onLevelUnload";
		case PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN: return "onGameShutdown";
		case PANGEA_SCRIPT_HOOK_TERRAIN_ITEM: return "onTerrainItem";
		case PANGEA_SCRIPT_HOOK_SPLINE_ITEM: return "onSplineItem";
		case PANGEA_SCRIPT_HOOK_MAP_ITEM: return "onMapItem";
		case PANGEA_SCRIPT_HOOK_OBJECT_FRAME: return "onObjectFrame";
	}
	return "";
}

static void push_game_context(lua_State* state, const PangeaScriptGameInfo* gameInfo)
{
	lua_createtable(state, 0, 2);
	lua_pushstring(state, gameInfo->gameId);
	lua_setfield(state, -2, "gameId");
	lua_pushstring(state, gameInfo->gameName);
	lua_setfield(state, -2, "gameName");
}

static void push_level_context(PangeaScriptBackend* backend, const PangeaScriptLevelContext* context)
{
	lua_State* state = backend->state;
	push_game_context(state, &backend->gameInfo);
	lua_pushinteger(state, context->levelNum);
	lua_setfield(state, -2, "levelNum");
	if (context->levelName)
	{
		lua_pushstring(state, context->levelName);
		lua_setfield(state, -2, "levelName");
	}
}

static void push_frame_context(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context)
{
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	lua_State* state = backend->state;
	push_level_context(backend, &levelContext);
	lua_pushinteger(state, (lua_Integer) context->frameNum);
	lua_setfield(state, -2, "frameNum");
	lua_pushnumber(state, context->deltaSeconds);
	lua_setfield(state, -2, "deltaSeconds");
	lua_pushnumber(state, context->levelTimeSeconds);
	lua_setfield(state, -2, "levelTimeSeconds");
}

static void push_params_array(lua_State* state, const unsigned char* params, int paramCount)
{
	lua_createtable(state, paramCount, 0);
	for (int i = 0; i < paramCount; i++)
	{
		lua_pushinteger(state, params[i]);
		lua_rawseti(state, -2, i + 1);
	}
}

static void push_terrain_context(PangeaScriptBackend* backend, const PangeaScriptTerrainItemContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->itemType);
	lua_setfield(state, -2, "itemType");
	lua_pushinteger(state, context->remappedItemType);
	lua_setfield(state, -2, "remappedItemType");
	lua_pushinteger(state, context->playerNum);
	lua_setfield(state, -2, "playerNum");
	lua_pushboolean(state, context->networked);
	lua_setfield(state, -2, "networked");

	lua_createtable(state, 0, 3);
	lua_pushnumber(state, context->x);
	lua_setfield(state, -2, "x");
	lua_pushnumber(state, 0.0);
	lua_setfield(state, -2, "y");
	lua_pushnumber(state, context->z);
	lua_setfield(state, -2, "z");
	lua_setfield(state, -2, "position");

	lua_pushinteger(state, (lua_Integer) context->flags);
	lua_setfield(state, -2, "flags");
	push_params_array(state, context->params, context->paramCount);
	lua_setfield(state, -2, "params");
}

static void push_spline_context(PangeaScriptBackend* backend, const PangeaScriptSplineItemContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->itemType);
	lua_setfield(state, -2, "itemType");
	lua_pushinteger(state, context->splineNum);
	lua_setfield(state, -2, "splineNum");
	lua_pushnumber(state, context->placement);
	lua_setfield(state, -2, "placement");
	push_params_array(state, context->params, context->paramCount);
	lua_setfield(state, -2, "params");
}

static void push_map_item_context(PangeaScriptBackend* backend, const PangeaScriptMapItemContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->sceneNum);
	lua_setfield(state, -2, "sceneNum");
	lua_pushinteger(state, context->areaNum);
	lua_setfield(state, -2, "areaNum");
	lua_pushinteger(state, context->itemType);
	lua_setfield(state, -2, "itemType");

	lua_createtable(state, 0, 2);
	lua_pushnumber(state, context->x);
	lua_setfield(state, -2, "x");
	lua_pushnumber(state, context->y);
	lua_setfield(state, -2, "y");
	lua_setfield(state, -2, "position");

	push_params_array(state, context->params, context->paramCount);
	lua_setfield(state, -2, "params");
}

static void push_pickup_context(PangeaScriptBackend* backend, const PangeaScriptPickupContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->playerNum);
	lua_setfield(state, -2, "playerNum");
	if (context->pickupId)
	{
		lua_pushstring(state, context->pickupId);
		lua_setfield(state, -2, "pickupId");
	}
	lua_pushinteger(state, context->pickupType);
	lua_setfield(state, -2, "pickupType");
	lua_pushinteger(state, context->amount);
	lua_setfield(state, -2, "amount");

	push_object_handle(state, context->pickup);
	lua_setfield(state, -2, "pickup");
	push_object_handle(state, context->player);
	lua_setfield(state, -2, "player");
	push_vector3(state, &context->position);
	lua_setfield(state, -2, "position");
}

static void push_weapon_hit_context(PangeaScriptBackend* backend, const PangeaScriptWeaponHitContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->playerNum);
	lua_setfield(state, -2, "playerNum");
	if (context->weaponId)
	{
		lua_pushstring(state, context->weaponId);
		lua_setfield(state, -2, "weaponId");
	}
	lua_pushinteger(state, context->weaponType);
	lua_setfield(state, -2, "weaponType");
	lua_pushnumber(state, context->damage);
	lua_setfield(state, -2, "damage");
	lua_pushinteger(state, context->targetType);
	lua_setfield(state, -2, "targetType");
	lua_pushinteger(state, (lua_Integer) context->targetFlags);
	lua_setfield(state, -2, "targetFlags");

	push_object_handle(state, context->weapon);
	lua_setfield(state, -2, "weapon");
	push_object_handle(state, context->target);
	lua_setfield(state, -2, "target");
	push_vector3(state, &context->position);
	lua_setfield(state, -2, "position");
}

static void push_trigger_context(PangeaScriptBackend* backend, const PangeaScriptTriggerContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->playerNum);
	lua_setfield(state, -2, "playerNum");
	if (context->triggerId)
	{
		lua_pushstring(state, context->triggerId);
		lua_setfield(state, -2, "triggerId");
	}
	lua_pushinteger(state, context->triggerType);
	lua_setfield(state, -2, "triggerType");
	lua_pushinteger(state, (lua_Integer) context->sideBits);
	lua_setfield(state, -2, "sideBits");
	lua_pushinteger(state, context->otherType);
	lua_setfield(state, -2, "otherType");
	lua_pushinteger(state, (lua_Integer) context->otherFlags);
	lua_setfield(state, -2, "otherFlags");

	push_object_handle(state, context->self);
	lua_setfield(state, -2, "self");
	push_object_handle(state, context->other);
	lua_setfield(state, -2, "other");
	push_vector3(state, &context->position);
	lua_setfield(state, -2, "position");
}

static void push_object_collision_context(PangeaScriptBackend* backend, const PangeaScriptObjectCollisionContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->playerNum);
	lua_setfield(state, -2, "playerNum");
	if (context->collisionId)
	{
		lua_pushstring(state, context->collisionId);
		lua_setfield(state, -2, "collisionId");
	}
	lua_pushinteger(state, context->collisionType);
	lua_setfield(state, -2, "collisionType");
	lua_pushinteger(state, (lua_Integer) context->sideBits);
	lua_setfield(state, -2, "sideBits");
	lua_pushinteger(state, context->selfType);
	lua_setfield(state, -2, "selfType");
	lua_pushinteger(state, (lua_Integer) context->selfFlags);
	lua_setfield(state, -2, "selfFlags");
	lua_pushinteger(state, context->otherType);
	lua_setfield(state, -2, "otherType");
	lua_pushinteger(state, (lua_Integer) context->otherFlags);
	lua_setfield(state, -2, "otherFlags");
	lua_pushnumber(state, context->damage);
	lua_setfield(state, -2, "damage");

	push_object_handle(state, context->self);
	lua_setfield(state, -2, "self");
	push_object_handle(state, context->other);
	lua_setfield(state, -2, "other");
	push_vector3(state, &context->position);
	lua_setfield(state, -2, "position");
}

static void push_player_damage_context(PangeaScriptBackend* backend, const PangeaScriptPlayerDamageContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->playerNum);
	lua_setfield(state, -2, "playerNum");
	if (context->damageId)
	{
		lua_pushstring(state, context->damageId);
		lua_setfield(state, -2, "damageId");
	}
	lua_pushinteger(state, context->damageType);
	lua_setfield(state, -2, "damageType");
	lua_pushnumber(state, context->damage);
	lua_setfield(state, -2, "damage");

	push_object_handle(state, context->source);
	lua_setfield(state, -2, "source");
	push_object_handle(state, context->player);
	lua_setfield(state, -2, "player");
	push_vector3(state, &context->position);
	lua_setfield(state, -2, "position");
}

static void push_object_damage_context(PangeaScriptBackend* backend, const PangeaScriptObjectDamageContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, context->playerNum);
	lua_setfield(state, -2, "playerNum");
	if (context->damageId)
	{
		lua_pushstring(state, context->damageId);
		lua_setfield(state, -2, "damageId");
	}
	lua_pushinteger(state, context->damageType);
	lua_setfield(state, -2, "damageType");
	lua_pushnumber(state, context->damage);
	lua_setfield(state, -2, "damage");
	lua_pushinteger(state, context->targetType);
	lua_setfield(state, -2, "targetType");
	lua_pushinteger(state, (lua_Integer) context->targetFlags);
	lua_setfield(state, -2, "targetFlags");

	push_object_handle(state, context->source);
	lua_setfield(state, -2, "source");
	push_object_handle(state, context->target);
	lua_setfield(state, -2, "target");
	push_vector3(state, &context->position);
	lua_setfield(state, -2, "position");
}

static void push_object_delete_context(PangeaScriptBackend* backend, const PangeaScriptObjectDeleteContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	push_object_handle(state, context->object);
	lua_setfield(state, -2, "object");
	push_vector3(state, &context->position);
	lua_setfield(state, -2, "position");
	push_tags_array(state, context->tags, context->tagCount);
	lua_setfield(state, -2, "tags");
}

static void push_tags_array(lua_State* state, const char* const* tags, int tagCount)
{
	lua_createtable(state, tagCount, 0);
	for (int i = 0; i < tagCount; i++)
	{
		lua_pushstring(state, tags[i]);
		lua_rawseti(state, -2, i + 1);
	}
}

static void push_object_frame_context(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context)
{
	lua_State* state = backend->state;
	PangeaScriptLevelContext levelContext = { context->levelNum, NULL };
	push_level_context(backend, &levelContext);

	lua_pushinteger(state, (lua_Integer) context->frameNum);
	lua_setfield(state, -2, "frameNum");
	lua_pushnumber(state, context->deltaSeconds);
	lua_setfield(state, -2, "deltaSeconds");
	lua_pushnumber(state, context->levelTimeSeconds);
	lua_setfield(state, -2, "levelTimeSeconds");

	push_object_handle(state, context->object);
	lua_setfield(state, -2, "object");

	if (context->tagCount > 0 && context->tags[0])
	{
		lua_pushstring(state, context->tags[0]);
		lua_setfield(state, -2, "objectType");
	}

	push_vector3(state, &context->position);
	lua_setfield(state, -2, "position");

	push_tags_array(state, context->tags, context->tagCount);
	lua_setfield(state, -2, "tags");
}

static bool push_hook(PangeaScriptBackend* backend, const char* name)
{
	lua_State* state = backend->state;
	if (backend->entryRef != LUA_NOREF)
	{
		lua_rawgeti(state, LUA_REGISTRYINDEX, backend->entryRef);
		lua_getfield(state, -1, name);
		if (lua_isfunction(state, -1))
		{
			lua_remove(state, -2);
			return true;
		}
		lua_pop(state, 2);
	}

	lua_getglobal(state, name);
	if (lua_isfunction(state, -1))
		return true;

	lua_pop(state, 1);
	return false;
}

static PangeaScriptStatus call_function_on_top(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	lua_State* state = backend->state;
	if (lua_pcall(state, 1, 1, 0) != LUA_OK)
	{
		copy_error(error, errorCapacity, lua_tostring(state, -1));
		lua_pop(state, 1);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	return PANGEA_SCRIPT_OK;
}

static void apply_item_result(lua_State* state, int index, bool* handled, bool* markInUse)
{
	if (!lua_istable(state, index))
		return;

	index = lua_absindex(state, index);
	lua_getfield(state, index, "handled");
	if (!lua_isnil(state, -1))
		*handled = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "markInUse");
	if (!lua_isnil(state, -1))
		*markInUse = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);
}

static void apply_pickup_result(lua_State* state, int index, PangeaScriptPickupContext* context)
{
	if (!lua_istable(state, index) || !context)
		return;

	index = lua_absindex(state, index);
	lua_getfield(state, index, "handled");
	if (!lua_isnil(state, -1))
		context->handled = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "consumePickup");
	if (!lua_isnil(state, -1))
		context->consumePickup = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "scoreDelta");
	if (lua_isnumber(state, -1))
		context->scoreDelta = (int) lua_tointeger(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "healthDelta");
	if (lua_isnumber(state, -1))
		context->healthDelta = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);
}

static void apply_weapon_hit_result(lua_State* state, int index, PangeaScriptWeaponHitContext* context)
{
	if (!lua_istable(state, index) || !context)
		return;

	index = lua_absindex(state, index);
	lua_getfield(state, index, "handled");
	if (!lua_isnil(state, -1))
		context->handled = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "applyDamage");
	if (!lua_isnil(state, -1))
		context->applyDamage = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "destroyTarget");
	if (!lua_isnil(state, -1))
		context->destroyTarget = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "damage");
	if (lua_isnumber(state, -1))
		context->damage = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "scoreDelta");
	if (lua_isnumber(state, -1))
		context->scoreDelta = (int) lua_tointeger(state, -1);
	lua_pop(state, 1);
}

static void apply_trigger_result(lua_State* state, int index, PangeaScriptTriggerContext* context)
{
	if (!lua_istable(state, index) || !context)
		return;

	index = lua_absindex(state, index);
	lua_getfield(state, index, "handled");
	if (!lua_isnil(state, -1))
		context->handled = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "solid");
	if (!lua_isnil(state, -1))
		context->solid = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "deleteSelf");
	if (!lua_isnil(state, -1))
		context->deleteSelf = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "deleteOther");
	if (!lua_isnil(state, -1))
		context->deleteOther = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "damagePlayer");
	if (lua_isnumber(state, -1))
		context->damagePlayer = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "healthDelta");
	if (lua_isnumber(state, -1))
		context->healthDelta = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "scoreDelta");
	if (lua_isnumber(state, -1))
		context->scoreDelta = (int) lua_tointeger(state, -1);
	lua_pop(state, 1);
}

static void apply_object_collision_result(lua_State* state, int index, PangeaScriptObjectCollisionContext* context)
{
	if (!lua_istable(state, index) || !context)
		return;

	index = lua_absindex(state, index);
	lua_getfield(state, index, "handled");
	if (!lua_isnil(state, -1))
		context->handled = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "suppressNative");
	if (!lua_isnil(state, -1))
		context->suppressNative = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "deleteSelf");
	if (!lua_isnil(state, -1))
		context->deleteSelf = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "deleteOther");
	if (!lua_isnil(state, -1))
		context->deleteOther = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "applyDamage");
	if (!lua_isnil(state, -1))
		context->applyDamage = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "damage");
	if (lua_isnumber(state, -1))
		context->damage = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "scoreDelta");
	if (lua_isnumber(state, -1))
		context->scoreDelta = (int) lua_tointeger(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "healthDelta");
	if (lua_isnumber(state, -1))
		context->healthDelta = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);
}

static void apply_player_damage_result(lua_State* state, int index, PangeaScriptPlayerDamageContext* context)
{
	if (!lua_istable(state, index) || !context)
		return;

	index = lua_absindex(state, index);
	lua_getfield(state, index, "handled");
	if (!lua_isnil(state, -1))
		context->handled = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "applyDamage");
	if (!lua_isnil(state, -1))
		context->applyDamage = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "damage");
	if (lua_isnumber(state, -1))
		context->damage = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "healthDelta");
	if (lua_isnumber(state, -1))
		context->healthDelta = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "scoreDelta");
	if (lua_isnumber(state, -1))
		context->scoreDelta = (int) lua_tointeger(state, -1);
	lua_pop(state, 1);
}

static void apply_object_damage_result(lua_State* state, int index, PangeaScriptObjectDamageContext* context)
{
	if (!lua_istable(state, index) || !context)
		return;

	index = lua_absindex(state, index);
	lua_getfield(state, index, "handled");
	if (!lua_isnil(state, -1))
		context->handled = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "applyDamage");
	if (!lua_isnil(state, -1))
		context->applyDamage = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "destroyTarget");
	if (!lua_isnil(state, -1))
		context->destroyTarget = lua_toboolean(state, -1) != 0;
	lua_pop(state, 1);

	lua_getfield(state, index, "damage");
	if (lua_isnumber(state, -1))
		context->damage = (float) lua_tonumber(state, -1);
	lua_pop(state, 1);

	lua_getfield(state, index, "scoreDelta");
	if (lua_isnumber(state, -1))
		context->scoreDelta = (int) lua_tointeger(state, -1);
	lua_pop(state, 1);
}

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo)
{
	if (!gameInfo)
		return NULL;

	PangeaScriptBackend* backend = (PangeaScriptBackend*) calloc(1, sizeof(PangeaScriptBackend));
	if (!backend)
		return NULL;

	backend->state = luaL_newstate();
	if (!backend->state)
	{
		free(backend);
		return NULL;
	}

	backend->gameInfo = *gameInfo;
	backend->entryRef = LUA_NOREF;
	lua_newtable(backend->state);
	backend->objectStateRef = luaL_ref(backend->state, LUA_REGISTRYINDEX);
	open_sandbox_libs(backend->state);
	install_global_require(backend->state, backend);
	return backend;
}

void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend)
{
	if (!backend)
		return;
	if (backend->state)
	{
		if (backend->objectStateRef != LUA_NOREF)
			luaL_unref(backend->state, LUA_REGISTRYINDEX, backend->objectStateRef);
		lua_close(backend->state);
	}
	free(backend);
}

PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity)
{
	if (!backend || !backend->state || !source)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	lua_State* state = backend->state;
	if (backend->entryRef != LUA_NOREF)
	{
		luaL_unref(state, LUA_REGISTRYINDEX, backend->entryRef);
		backend->entryRef = LUA_NOREF;
	}

	if (luaL_loadbufferx(state, source, strlen(source), "@Data/Scripts/dist/main.lua", "t") != LUA_OK)
	{
		copy_error(error, errorCapacity, lua_tostring(state, -1));
		lua_pop(state, 1);
		return PANGEA_SCRIPT_PARSE_ERROR;
	}

	if (lua_pcall(state, 0, 1, 0) != LUA_OK)
	{
		copy_error(error, errorCapacity, lua_tostring(state, -1));
		lua_pop(state, 1);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	if (lua_istable(state, -1))
	{
		backend->entryRef = luaL_ref(state, LUA_REGISTRYINDEX);
	}
	else
	{
		lua_pop(state, 1);
	}

	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	return PangeaScriptBackend_CallNamedLevelHook(backend, hook_name(hook), context, error, errorCapacity);
}

PangeaScriptStatus PangeaScriptBackend_CallNamedLevelHook(PangeaScriptBackend* backend, const char* hookName, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	if (!backend || !hookName || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, hookName))
		return PANGEA_SCRIPT_OK;

	push_level_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	return PangeaScriptBackend_CallNamedFrameHook(backend, "onFrame", context, error, errorCapacity);
}

PangeaScriptStatus PangeaScriptBackend_CallNamedFrameHook(PangeaScriptBackend* backend, const char* hookName, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	if (!backend || !hookName || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, hookName))
		return PANGEA_SCRIPT_OK;

	push_frame_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onTerrainItem"))
		return PANGEA_SCRIPT_OK;

	push_terrain_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_item_result(backend->state, -1, &context->handled, &context->markInUse);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onSplineItem"))
		return PANGEA_SCRIPT_OK;

	push_spline_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_item_result(backend->state, -1, &context->handled, &context->markInUse);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onMapItem"))
		return PANGEA_SCRIPT_OK;

	push_map_item_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_item_result(backend->state, -1, &context->handled, &context->markInUse);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallPickupCollectedHook(PangeaScriptBackend* backend, PangeaScriptPickupContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onPickupCollected"))
		return PANGEA_SCRIPT_OK;

	push_pickup_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_pickup_result(backend->state, -1, context);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallWeaponHitHook(PangeaScriptBackend* backend, PangeaScriptWeaponHitContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onWeaponHit"))
		return PANGEA_SCRIPT_OK;

	push_weapon_hit_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_weapon_hit_result(backend->state, -1, context);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallTriggerEnterHook(PangeaScriptBackend* backend, PangeaScriptTriggerContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onTriggerEnter"))
		return PANGEA_SCRIPT_OK;

	push_trigger_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_trigger_result(backend->state, -1, context);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectCollisionHook(PangeaScriptBackend* backend, PangeaScriptObjectCollisionContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onObjectCollision"))
		return PANGEA_SCRIPT_OK;

	push_object_collision_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_object_collision_result(backend->state, -1, context);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallPlayerDamageHook(PangeaScriptBackend* backend, PangeaScriptPlayerDamageContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onPlayerDamage"))
		return PANGEA_SCRIPT_OK;

	push_player_damage_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_player_damage_result(backend->state, -1, context);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectDamageHook(PangeaScriptBackend* backend, PangeaScriptObjectDamageContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onObjectDamage"))
		return PANGEA_SCRIPT_OK;

	push_object_damage_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK)
		apply_object_damage_result(backend->state, -1, context);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectDeleteHook(PangeaScriptBackend* backend, const PangeaScriptObjectDeleteContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onObjectDelete"))
		return PANGEA_SCRIPT_OK;

	push_object_delete_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	lua_pop(backend->state, 1);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity)
{
	if (!backend || !context || !result)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!push_hook(backend, "onObjectFrame"))
		return PANGEA_SCRIPT_OK;

	push_object_frame_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status != PANGEA_SCRIPT_OK)
	{
		lua_pop(backend->state, 1);
		return status;
	}

	result->hasPositionOffset = false;
	result->positionOffset.x = 0.0f;
	result->positionOffset.y = 0.0f;
	result->positionOffset.z = 0.0f;

	lua_State* state = backend->state;
	if (lua_istable(state, -1))
	{
		lua_getfield(state, -1, "positionOffset");
		PangeaScriptVector3 offset;
		if (read_vector3(state, -1, &offset))
		{
			result->hasPositionOffset = true;
			result->positionOffset = offset;
		}
		lua_pop(state, 1);
	}

	lua_pop(state, 1);
	return PANGEA_SCRIPT_OK;
}

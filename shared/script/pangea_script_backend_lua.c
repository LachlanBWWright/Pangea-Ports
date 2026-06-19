#include "pangea_script_backend.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PANGEA_LUA_PATH_CAPACITY 260
#define PANGEA_LUA_MODULE_NAME_CAPACITY 192
#define PANGEA_LUA_MEMORY_LIMIT_BYTES (4 * 1024 * 1024)
#define PANGEA_LUA_LOAD_BUDGET 500000
#define PANGEA_LUA_FRAME_BUDGET 150000
#define PANGEA_LUA_ITEM_BUDGET 150000
#define PANGEA_LUA_OBJECT_BUDGET 100000
#define PANGEA_LUA_HOOK_INTERVAL 1000

typedef struct PangeaLuaAllocatorState
{
size_t limit;
size_t used;
bool exceeded;
} PangeaLuaAllocatorState;

struct PangeaScriptBackend
{
lua_State* state;
PangeaScriptGameInfo gameInfo;
PangeaLuaAllocatorState allocator;
int moduleRef;
int packageLoadedRef;
int packagePreloadRef;
int instructionBudget;
bool budgetExceeded;
char startupScriptPath[PANGEA_LUA_PATH_CAPACITY];
char scriptsRoot[PANGEA_LUA_PATH_CAPACITY];
char entryDir[PANGEA_LUA_PATH_CAPACITY];
};

static void copy_error(char* dest, int capacity, const char* message)
{
if (!dest || capacity <= 0)
return;
snprintf(dest, (size_t)capacity, "%s", message ? message : "Unknown Lua error");
}

static PangeaScriptBackend* get_backend_from_lua(lua_State* state)
{
return *((PangeaScriptBackend**)lua_getextraspace(state));
}

static void* pangea_lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
PangeaLuaAllocatorState* allocator = (PangeaLuaAllocatorState*) ud;
if (!allocator)
return NULL;

const size_t actualOldSize = ptr ? osize : 0;

if (nsize == 0)
{
if (ptr)
{
if (allocator->used >= actualOldSize)
allocator->used -= actualOldSize;
else
allocator->used = 0;
}
free(ptr);
return NULL;
}

size_t nextUsed = allocator->used;
if (nextUsed >= actualOldSize)
nextUsed -= actualOldSize;
else
nextUsed = 0;

if (nsize > allocator->limit || nextUsed > allocator->limit - nsize)
{
allocator->exceeded = true;
return NULL;
}

void* next = realloc(ptr, nsize);
if (!next)
return NULL;

allocator->used = nextUsed + nsize;
return next;
}

static bool read_text_file(const char* path, char** outBytes, size_t* outSize)
{
FILE* file = fopen(path, "rb");
if (!file)
return false;

if (fseek(file, 0, SEEK_END) != 0)
{
fclose(file);
return false;
}

long size = ftell(file);
if (size < 0)
{
fclose(file);
return false;
}

if (fseek(file, 0, SEEK_SET) != 0)
{
fclose(file);
return false;
}

char* bytes = (char*) malloc((size_t) size + 1);
if (!bytes)
{
fclose(file);
return false;
}

size_t readCount = fread(bytes, 1, (size_t) size, file);
fclose(file);
if (readCount != (size_t) size)
{
free(bytes);
return false;
}

bytes[size] = '\0';
*outBytes = bytes;
if (outSize)
*outSize = (size_t) size;
return true;
}

static void normalize_slashes(char* path)
{
for (char* cursor = path; cursor && *cursor; ++cursor)
{
if (*cursor == '\\')
*cursor = '/';
}
}

static void compute_runtime_roots(PangeaScriptBackend* backend, const char* scriptPath)
{
backend->startupScriptPath[0] = '\0';
backend->scriptsRoot[0] = '\0';
backend->entryDir[0] = '\0';
if (!scriptPath || !scriptPath[0])
return;

snprintf(backend->startupScriptPath, sizeof(backend->startupScriptPath), "%s", scriptPath);
normalize_slashes(backend->startupScriptPath);

const char* root = strstr(backend->startupScriptPath, "Data/Scripts/");
if (root == backend->startupScriptPath)
{
snprintf(backend->scriptsRoot, sizeof(backend->scriptsRoot), "%s", "Data/Scripts");
}
else
{
snprintf(backend->scriptsRoot, sizeof(backend->scriptsRoot), "%s", "Data/Scripts");
}

snprintf(backend->entryDir, sizeof(backend->entryDir), "%s", backend->startupScriptPath);
char* slash = strrchr(backend->entryDir, '/');
if (slash)
*slash = '\0';
else
snprintf(backend->entryDir, sizeof(backend->entryDir), "%s", backend->scriptsRoot);
}

static bool is_safe_module_name(const char* name)
{
if (!name || !name[0])
return false;

for (const char* cursor = name; *cursor; ++cursor)
{
const char c = *cursor;
const bool allowed =
(c >= 'a' && c <= 'z') ||
(c >= 'A' && c <= 'Z') ||
(c >= '0' && c <= '9') ||
c == '_' || c == '.' || c == '-';
if (!allowed)
return false;
}

return strstr(name, "..") == NULL;
}

static void module_name_to_path(const char* moduleName, char* outPath, size_t outCapacity)
{
size_t writeIndex = 0;
for (const char* cursor = moduleName; *cursor && writeIndex + 1 < outCapacity; ++cursor)
{
outPath[writeIndex++] = (*cursor == '.') ? '/' : *cursor;
}
outPath[writeIndex] = '\0';
}

static bool try_read_module(PangeaScriptBackend* backend, const char* moduleName, char* outResolvedPath, size_t outPathCapacity, char** outBytes, size_t* outSize)
{
char relativePath[PANGEA_LUA_MODULE_NAME_CAPACITY];
char candidate[PANGEA_LUA_PATH_CAPACITY];
const char* searchRoots[] =
{
backend->entryDir,
"Data/Scripts/dist",
"Data/Scripts/src",
"Data/Scripts",
};

module_name_to_path(moduleName, relativePath, sizeof(relativePath));

for (size_t i = 0; i < sizeof(searchRoots) / sizeof(searchRoots[0]); ++i)
{
if (!searchRoots[i] || !searchRoots[i][0])
continue;

snprintf(candidate, sizeof(candidate), "%s/%s.lua", searchRoots[i], relativePath);
normalize_slashes(candidate);
if (strncmp(candidate, "Data/Scripts/", 13) != 0 || strstr(candidate, "..") != NULL)
continue;
if (read_text_file(candidate, outBytes, outSize))
{
snprintf(outResolvedPath, outPathCapacity, "%s", candidate);
return true;
}
}

return false;
}

static int lua_traceback(lua_State* state)
{
const char* message = lua_tostring(state, 1);
if (!message)
message = "Lua error";
luaL_traceback(state, state, message, 1);
return 1;
}

static int lua_panic_handler(lua_State* state)
{
const char* message = lua_tostring(state, -1);
PangeaScript_Log(PANGEA_LOG_ERROR, "Lua", message ? message : "Lua panic");
return 0;
}

static void begin_instruction_budget(PangeaScriptBackend* backend, int budget);

static void instruction_budget_hook(lua_State* state, lua_Debug* debug)
{
(void) debug;
PangeaScriptBackend* backend = get_backend_from_lua(state);
if (!backend)
return;

backend->instructionBudget -= PANGEA_LUA_HOOK_INTERVAL;
if (backend->instructionBudget <= 0)
{
backend->budgetExceeded = true;
luaL_error(state, "Lua instruction budget exceeded");
}
}

static void begin_instruction_budget(PangeaScriptBackend* backend, int budget)
{
backend->budgetExceeded = false;
backend->instructionBudget = budget;
if (backend->state && budget > 0)
lua_sethook(backend->state, instruction_budget_hook, LUA_MASKCOUNT, PANGEA_LUA_HOOK_INTERVAL);
}

static void end_instruction_budget(PangeaScriptBackend* backend)
{
if (backend && backend->state)
lua_sethook(backend->state, NULL, 0, 0);
}

static int protected_call(PangeaScriptBackend* backend, int nargs, int nresults, int budget)
{
lua_State* state = backend->state;
const int errorIndex = lua_gettop(state) - nargs;
lua_pushcfunction(state, lua_traceback);
lua_insert(state, errorIndex);
begin_instruction_budget(backend, budget);
const int status = lua_pcall(state, nargs, nresults, errorIndex);
end_instruction_budget(backend);
lua_remove(state, errorIndex);
return status;
}

static void copy_lua_failure(PangeaScriptBackend* backend, int status, char* error, int errorCapacity)
{
if (backend->allocator.exceeded)
{
copy_error(error, errorCapacity, "Lua memory limit exceeded");
backend->allocator.exceeded = false;
return;
}
if (backend->budgetExceeded)
{
copy_error(error, errorCapacity, "Lua instruction budget exceeded");
return;
}
if (lua_isstring(backend->state, -1))
{
copy_error(error, errorCapacity, lua_tostring(backend->state, -1));
return;
}
copy_error(error, errorCapacity, status == LUA_ERRSYNTAX ? "Lua syntax error" : "Lua runtime error");
}

static PangeaScriptStatus status_from_lua_failure(PangeaScriptBackend* backend, int status)
{
if (backend->budgetExceeded)
return PANGEA_SCRIPT_BUDGET_EXCEEDED;
if (status == LUA_ERRSYNTAX)
return PANGEA_SCRIPT_PARSE_ERROR;
return PANGEA_SCRIPT_RUNTIME_ERROR;
}

static int readonly_newindex(lua_State* state)
{
(void) luaL_error(state, "attempt to modify read-only table");
return 0;
}

static void make_table_readonly(lua_State* state, int index)
{
if (index < 0)
index = lua_gettop(state) + index + 1;
lua_newtable(state);
lua_pushcfunction(state, readonly_newindex);
lua_setfield(state, -2, "__newindex");
lua_pushboolean(state, 0);
lua_setfield(state, -2, "__metatable");
lua_setmetatable(state, index);
}

static bool read_number_field(lua_State* state, int index, const char* key, double* outValue)
{
bool ok = false;
if (index < 0)
index = lua_gettop(state) + index + 1;
lua_getfield(state, index, key);
if (lua_isnumber(state, -1))
{
*outValue = lua_tonumber(state, -1);
ok = true;
}
lua_pop(state, 1);
return ok;
}

static bool read_bool_field(lua_State* state, int index, const char* key, bool* outValue, bool* present)
{
bool ok = false;
if (present)
*present = false;
if (index < 0)
index = lua_gettop(state) + index + 1;
lua_getfield(state, index, key);
if (lua_isboolean(state, -1))
{
if (present)
*present = true;
*outValue = lua_toboolean(state, -1) != 0;
ok = true;
}
else if (lua_isnil(state, -1))
{
ok = true;
}
lua_pop(state, 1);
return ok;
}

static bool read_object_handle(lua_State* state, int index, PangeaScriptObjectHandle* outHandle)
{
double id = 0.0;
double generation = 0.0;
if (!lua_istable(state, index) || !outHandle)
return false;
if (!read_number_field(state, index, "id", &id) || !read_number_field(state, index, "generation", &generation))
return false;
outHandle->id = (int) id;
outHandle->generation = (uint32_t) generation;
return true;
}

static bool read_vector3(lua_State* state, int index, PangeaScriptVector3* outVector)
{
double x = 0.0;
double y = 0.0;
double z = 0.0;
if (!lua_istable(state, index) || !outVector)
return false;
if (!read_number_field(state, index, "x", &x) || !read_number_field(state, index, "y", &y) || !read_number_field(state, index, "z", &z))
return false;
outVector->x = (float) x;
outVector->y = (float) y;
outVector->z = (float) z;
return true;
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

static void push_object_handle(lua_State* state, PangeaScriptObjectHandle handle)
{
lua_createtable(state, 0, 2);
lua_pushinteger(state, handle.id);
lua_setfield(state, -2, "id");
lua_pushinteger(state, (lua_Integer) handle.generation);
lua_setfield(state, -2, "generation");
}

static int l_log(lua_State* state)
{
int level = (int) lua_tointeger(state, lua_upvalueindex(1));
const char* message = luaL_tolstring(state, 1, NULL);
PangeaScript_Log((PangeaScriptLogLevel) level, "Lua", message);
lua_pop(state, 1);
return 0;
}

static int l_spawn_native(lua_State* state)
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
if (lua_istable(state, 3))
{
double value = 0.0;
if (read_number_field(state, 3, "subtype", &value))
subtype = (int) value;
if (read_number_field(state, 3, "amount", &value))
amount = (int) value;
}

PangeaScriptObjectHandle handle = {0};
if (PangeaScript_SpawnNative(id, position.x, position.y, position.z, subtype, amount, &handle) == PANGEA_SCRIPT_OK && handle.id > 0)
{
push_object_handle(state, handle);
return 1;
}

lua_pushnil(state);
return 1;
}

static int l_spawn_scripted(lua_State* state)
{
const char* id = luaL_checkstring(state, 1);
PangeaScriptVector3 position;
if (!read_vector3(state, 2, &position))
{
lua_pushnil(state);
return 1;
}

PangeaScriptObjectHandle handle = {0};
if (PangeaScript_RegisterScriptedObject(id, position.x, position.y, position.z, &handle) == PANGEA_SCRIPT_OK && handle.id > 0)
{
push_object_handle(state, handle);
return 1;
}

lua_pushnil(state);
return 1;
}

static int l_object_position(lua_State* state)
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

static int l_object_set_position(lua_State* state)
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

static int l_object_set_velocity(lua_State* state)
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

static int l_object_delete(lua_State* state)
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

static int l_return_nil(lua_State* state)
{
(void) state;
lua_pushnil(state);
return 1;
}

static int l_api_capabilities(lua_State* state)
{
lua_createtable(state, 0, 6);
lua_pushboolean(state, 1);
lua_setfield(state, -2, "objectPosition");
lua_pushboolean(state, 1);
lua_setfield(state, -2, "objectMutation");
lua_pushboolean(state, 1);
lua_setfield(state, -2, "spawnNative");
lua_pushboolean(state, 1);
lua_setfield(state, -2, "spawnScripted");
lua_pushboolean(state, 1);
lua_setfield(state, -2, "levelSettings");
lua_pushboolean(state, 0);
lua_setfield(state, -2, "networkedSimulation");
make_table_readonly(state, -1);
return 1;
}

static void push_log_function(lua_State* state, PangeaScriptLogLevel level)
{
lua_pushinteger(state, level);
lua_pushcclosure(state, l_log, 1);
}

static void install_pangea_module(PangeaScriptBackend* backend)
{
lua_State* state = backend->state;

lua_createtable(state, 0, 6);

lua_createtable(state, 0, 2);
lua_pushinteger(state, 1);
lua_setfield(state, -2, "version");
lua_pushcfunction(state, l_api_capabilities);
lua_setfield(state, -2, "capabilities");
make_table_readonly(state, -1);
lua_setfield(state, -2, "api");

lua_createtable(state, 0, 2);
lua_pushstring(state, backend->gameInfo.gameId);
lua_setfield(state, -2, "id");
lua_pushstring(state, backend->gameInfo.gameName);
lua_setfield(state, -2, "name");
make_table_readonly(state, -1);
lua_setfield(state, -2, "game");

lua_createtable(state, 0, 3);
push_log_function(state, PANGEA_LOG_INFO);
lua_setfield(state, -2, "info");
push_log_function(state, PANGEA_LOG_WARN);
lua_setfield(state, -2, "warn");
push_log_function(state, PANGEA_LOG_ERROR);
lua_setfield(state, -2, "error");
make_table_readonly(state, -1);
lua_setfield(state, -2, "log");

lua_createtable(state, 0, 2);
lua_pushcfunction(state, l_spawn_native);
lua_setfield(state, -2, "native");
lua_pushcfunction(state, l_spawn_scripted);
lua_setfield(state, -2, "scripted");
make_table_readonly(state, -1);
lua_setfield(state, -2, "spawn");

lua_createtable(state, 0, 4);
lua_pushcfunction(state, l_object_position);
lua_setfield(state, -2, "position");
lua_pushcfunction(state, l_object_set_position);
lua_setfield(state, -2, "setPosition");
lua_pushcfunction(state, l_object_set_velocity);
lua_setfield(state, -2, "setVelocity");
lua_pushcfunction(state, l_object_delete);
lua_setfield(state, -2, "delete");
make_table_readonly(state, -1);
lua_setfield(state, -2, "object");

lua_createtable(state, 0, 3);
lua_createtable(state, 0, 1);
lua_pushcfunction(state, l_return_nil);
lua_setfield(state, -2, "current");
make_table_readonly(state, -1);
lua_setfield(state, -2, "level");
lua_createtable(state, 0, 1);
lua_pushcfunction(state, l_return_nil);
lua_setfield(state, -2, "get");
make_table_readonly(state, -1);
lua_setfield(state, -2, "player");
lua_createtable(state, 0, 1);
lua_pushcfunction(state, l_spawn_scripted);
lua_setfield(state, -2, "scripted");
make_table_readonly(state, -1);
lua_setfield(state, -2, "spawn");
make_table_readonly(state, -1);
lua_setfield(state, -2, "experimental");

make_table_readonly(state, -1);
lua_pushvalue(state, -1);
lua_setglobal(state, "pangea");

lua_rawgeti(state, LUA_REGISTRYINDEX, backend->packageLoadedRef);
lua_pushvalue(state, -2);
lua_setfield(state, -2, "pangea");
lua_pop(state, 1);

lua_pop(state, 1);

lua_createtable(state, 0, 3);
push_log_function(state, PANGEA_LOG_INFO);
lua_setfield(state, -2, "log");
push_log_function(state, PANGEA_LOG_WARN);
lua_setfield(state, -2, "warn");
push_log_function(state, PANGEA_LOG_ERROR);
lua_setfield(state, -2, "error");
make_table_readonly(state, -1);
lua_setglobal(state, "console");
}

static int load_cached_module(lua_State* state, int ref, const char* key)
{
lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
lua_getfield(state, -1, key);
lua_remove(state, -2);
return !lua_isnil(state, -1);
}

static void store_cached_module(lua_State* state, int ref, const char* key)
{
lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
lua_pushvalue(state, -2);
lua_setfield(state, -2, key);
lua_pop(state, 1);
}

static int l_require(lua_State* state)
{
PangeaScriptBackend* backend = get_backend_from_lua(state);
const char* moduleName = luaL_checkstring(state, 1);
if (!backend)
return luaL_error(state, "Lua backend is unavailable");

if (load_cached_module(state, backend->packageLoadedRef, moduleName))
return 1;
lua_pop(state, 1);

if (!is_safe_module_name(moduleName))
return luaL_error(state, "invalid module name '%s'", moduleName);

char* bytes = NULL;
size_t size = 0;
char resolvedPath[PANGEA_LUA_PATH_CAPACITY];
resolvedPath[0] = '\0';
if (!try_read_module(backend, moduleName, resolvedPath, sizeof(resolvedPath), &bytes, &size))
return luaL_error(state, "module '%s' not found under Data/Scripts/", moduleName);

const int status = luaL_loadbufferx(state, bytes, size, resolvedPath, "t");
free(bytes);
if (status != LUA_OK)
return lua_error(state);
if (protected_call(backend, 0, 1, PANGEA_LUA_LOAD_BUDGET) != LUA_OK)
return lua_error(state);
if (lua_isnil(state, -1))
{
lua_pop(state, 1);
lua_pushboolean(state, 1);
}

store_cached_module(state, backend->packageLoadedRef, moduleName);
return 1;
}

static bool initialize_state(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
if (backend->state)
{
lua_close(backend->state);
backend->state = NULL;
}

backend->allocator.used = 0;
backend->allocator.exceeded = false;
backend->allocator.limit = PANGEA_LUA_MEMORY_LIMIT_BYTES;
backend->moduleRef = LUA_NOREF;
backend->packageLoadedRef = LUA_NOREF;
backend->packagePreloadRef = LUA_NOREF;
backend->budgetExceeded = false;
backend->instructionBudget = 0;

backend->state = lua_newstate(pangea_lua_alloc, &backend->allocator);
if (!backend->state)
{
copy_error(error, errorCapacity, "Failed to create Lua state");
return false;
}

*((PangeaScriptBackend**)lua_getextraspace(backend->state)) = backend;
lua_atpanic(backend->state, lua_panic_handler);

luaL_requiref(backend->state, LUA_GNAME, luaopen_base, 1);
lua_pop(backend->state, 1);
luaL_requiref(backend->state, LUA_COLIBNAME, luaopen_coroutine, 1);
lua_pop(backend->state, 1);
luaL_requiref(backend->state, LUA_TABLIBNAME, luaopen_table, 1);
lua_pop(backend->state, 1);
luaL_requiref(backend->state, LUA_STRLIBNAME, luaopen_string, 1);
lua_pop(backend->state, 1);
luaL_requiref(backend->state, LUA_MATHLIBNAME, luaopen_math, 1);
lua_pop(backend->state, 1);
luaL_requiref(backend->state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
lua_pop(backend->state, 1);

lua_pushnil(backend->state);
lua_setglobal(backend->state, "dofile");
lua_pushnil(backend->state);
lua_setglobal(backend->state, "loadfile");
lua_pushnil(backend->state);
lua_setglobal(backend->state, "package");
lua_pushnil(backend->state);
lua_setglobal(backend->state, "io");
lua_pushnil(backend->state);
lua_setglobal(backend->state, "os");
lua_pushnil(backend->state);
lua_setglobal(backend->state, "debug");

lua_getglobal(backend->state, "math");
if (lua_istable(backend->state, -1))
{
lua_pushnil(backend->state);
lua_setfield(backend->state, -2, "random");
lua_pushnil(backend->state);
lua_setfield(backend->state, -2, "randomseed");
}
lua_pop(backend->state, 1);

lua_newtable(backend->state);
backend->packageLoadedRef = luaL_ref(backend->state, LUA_REGISTRYINDEX);
lua_newtable(backend->state);
backend->packagePreloadRef = luaL_ref(backend->state, LUA_REGISTRYINDEX);

lua_pushcfunction(backend->state, l_require);
lua_setglobal(backend->state, "require");

install_pangea_module(backend);
return true;
}

static const char* level_hook_name(PangeaScriptHook hook)
{
switch (hook)
{
case PANGEA_SCRIPT_HOOK_GAME_START: return "onGameStart";
case PANGEA_SCRIPT_HOOK_LEVEL_LOAD: return "onLevelLoad";
case PANGEA_SCRIPT_HOOK_LEVEL_START: return "onLevelStart";
case PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE: return "onLevelComplete";
case PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD: return "onLevelUnload";
case PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN: return "onGameShutdown";
default: return NULL;
}
}

static bool push_module_callback(PangeaScriptBackend* backend, const char* callbackName)
{
lua_State* state = backend->state;
if (backend->moduleRef != LUA_NOREF)
{
lua_rawgeti(state, LUA_REGISTRYINDEX, backend->moduleRef);
lua_getfield(state, -1, callbackName);
lua_remove(state, -2);
if (lua_isfunction(state, -1))
return true;
lua_pop(state, 1);
}

lua_getglobal(state, callbackName);
if (lua_isfunction(state, -1))
return true;
lua_pop(state, 1);
return false;
}

static void push_params_array(lua_State* state, const unsigned char* params, int paramCount)
{
lua_createtable(state, paramCount, 0);
for (int i = 0; i < paramCount; ++i)
{
lua_pushinteger(state, params ? params[i] : 0);
lua_rawseti(state, -2, i + 1);
}
}

static void push_level_context(lua_State* state, const PangeaScriptLevelContext* context)
{
lua_createtable(state, 0, 2);
lua_pushinteger(state, context->levelNum);
lua_setfield(state, -2, "levelNum");
lua_pushstring(state, context->levelName ? context->levelName : "");
lua_setfield(state, -2, "levelName");
}

static void push_frame_context(lua_State* state, const PangeaScriptFrameContext* context)
{
lua_createtable(state, 0, 4);
lua_pushinteger(state, context->levelNum);
lua_setfield(state, -2, "levelNum");
lua_pushinteger(state, (lua_Integer) context->frameNum);
lua_setfield(state, -2, "frameNum");
lua_pushnumber(state, context->deltaSeconds);
lua_setfield(state, -2, "deltaSeconds");
lua_pushnumber(state, context->levelTimeSeconds);
lua_setfield(state, -2, "levelTimeSeconds");
}

static void push_terrain_item_context(lua_State* state, const PangeaScriptTerrainItemContext* context)
{
lua_createtable(state, 0, 10);
lua_pushinteger(state, context->levelNum);
lua_setfield(state, -2, "levelNum");
lua_pushinteger(state, context->itemType);
lua_setfield(state, -2, "itemType");
lua_pushinteger(state, context->remappedItemType);
lua_setfield(state, -2, "remappedItemType");
lua_pushinteger(state, context->playerNum);
lua_setfield(state, -2, "playerNum");
lua_pushboolean(state, context->networked);
lua_setfield(state, -2, "networked");
lua_pushnumber(state, context->x);
lua_setfield(state, -2, "x");
lua_pushnumber(state, context->z);
lua_setfield(state, -2, "z");
lua_pushinteger(state, (lua_Integer) context->flags);
lua_setfield(state, -2, "flags");
push_params_array(state, context->params, context->paramCount);
lua_setfield(state, -2, "params");
}

static void push_spline_item_context(lua_State* state, const PangeaScriptSplineItemContext* context)
{
lua_createtable(state, 0, 6);
lua_pushinteger(state, context->levelNum);
lua_setfield(state, -2, "levelNum");
lua_pushinteger(state, context->itemType);
lua_setfield(state, -2, "itemType");
lua_pushinteger(state, context->splineNum);
lua_setfield(state, -2, "splineNum");
lua_pushnumber(state, context->placement);
lua_setfield(state, -2, "placement");
push_params_array(state, context->params, context->paramCount);
lua_setfield(state, -2, "params");
}

static void push_map_item_context(lua_State* state, const PangeaScriptMapItemContext* context)
{
lua_createtable(state, 0, 7);
lua_pushinteger(state, context->levelNum);
lua_setfield(state, -2, "levelNum");
lua_pushinteger(state, context->sceneNum);
lua_setfield(state, -2, "sceneNum");
lua_pushinteger(state, context->areaNum);
lua_setfield(state, -2, "areaNum");
lua_pushinteger(state, context->itemType);
lua_setfield(state, -2, "itemType");
lua_pushnumber(state, context->x);
lua_setfield(state, -2, "x");
lua_pushnumber(state, context->y);
lua_setfield(state, -2, "y");
push_params_array(state, context->params, context->paramCount);
lua_setfield(state, -2, "params");
}

static void push_object_frame_context(lua_State* state, const PangeaScriptObjectFrameContext* context)
{
lua_createtable(state, 0, 7);
lua_pushinteger(state, context->levelNum);
lua_setfield(state, -2, "levelNum");
lua_pushinteger(state, (lua_Integer) context->frameNum);
lua_setfield(state, -2, "frameNum");
lua_pushnumber(state, context->deltaSeconds);
lua_setfield(state, -2, "deltaSeconds");
lua_pushnumber(state, context->levelTimeSeconds);
lua_setfield(state, -2, "levelTimeSeconds");
push_object_handle(state, context->object);
lua_setfield(state, -2, "object");
push_vector3(state, &context->position);
lua_setfield(state, -2, "position");
lua_createtable(state, context->tagCount, 0);
for (int i = 0; i < context->tagCount; ++i)
{
lua_pushstring(state, context->tags[i] ? context->tags[i] : "");
lua_rawseti(state, -2, i + 1);
}
lua_setfield(state, -2, "tags");
}

static bool parse_handled_result(lua_State* state, int index, bool* outHandled, bool* outMarkInUse)
{
bool handled = false;
bool markInUse = false;
bool present = false;
if (lua_isnil(state, index))
{
*outHandled = false;
*outMarkInUse = false;
return true;
}
if (!lua_istable(state, index))
return false;
if (!read_bool_field(state, index, "handled", &handled, &present) || !read_bool_field(state, index, "markInUse", &markInUse, NULL))
return false;
*outHandled = present ? handled : false;
*outMarkInUse = markInUse;
return true;
}

static bool parse_object_frame_result(lua_State* state, int index, PangeaScriptObjectFrameResult* outResult)
{
if (lua_isnil(state, index))
return true;
if (!lua_istable(state, index))
return false;

if (index < 0)
index = lua_gettop(state) + index + 1;
lua_getfield(state, index, "positionOffset");
if (lua_isnil(state, -1))
{
lua_pop(state, 1);
return true;
}
if (!read_vector3(state, -1, &outResult->positionOffset))
{
lua_pop(state, 1);
return false;
}
lua_pop(state, 1);
outResult->hasPositionOffset = true;
return true;
}

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo)
{
PangeaScriptBackend* backend = (PangeaScriptBackend*) calloc(1, sizeof(PangeaScriptBackend));
if (!backend)
return NULL;
backend->gameInfo = *gameInfo;
backend->moduleRef = LUA_NOREF;
backend->packageLoadedRef = LUA_NOREF;
backend->packagePreloadRef = LUA_NOREF;
backend->allocator.limit = PANGEA_LUA_MEMORY_LIMIT_BYTES;
return backend;
}

void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend)
{
if (!backend)
return;
if (backend->state)
lua_close(backend->state);
free(backend);
}

PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* scriptPath, const char* source, char* error, int errorCapacity)
{
if (!backend || !source)
{
copy_error(error, errorCapacity, "Lua backend received invalid arguments");
return PANGEA_SCRIPT_BAD_ARGUMENT;
}

compute_runtime_roots(backend, scriptPath);
if (!initialize_state(backend, error, errorCapacity))
return PANGEA_SCRIPT_RUNTIME_ERROR;

const int loadStatus = luaL_loadbufferx(backend->state, source, strlen(source), backend->startupScriptPath[0] ? backend->startupScriptPath : "main.lua", "t");
if (loadStatus != LUA_OK)
{
copy_lua_failure(backend, loadStatus, error, errorCapacity);
lua_settop(backend->state, 0);
return status_from_lua_failure(backend, loadStatus);
}

const int callStatus = protected_call(backend, 0, 1, PANGEA_LUA_LOAD_BUDGET);
if (callStatus != LUA_OK)
{
copy_lua_failure(backend, callStatus, error, errorCapacity);
lua_settop(backend->state, 0);
return status_from_lua_failure(backend, callStatus);
}

backend->moduleRef = LUA_NOREF;
if (lua_istable(backend->state, -1))
{
backend->moduleRef = luaL_ref(backend->state, LUA_REGISTRYINDEX);
}
else if (!lua_isnil(backend->state, -1))
{
copy_error(error, errorCapacity, "Lua startup script must return a table or nil");
lua_settop(backend->state, 0);
return PANGEA_SCRIPT_RUNTIME_ERROR;
}
else
{
lua_pop(backend->state, 1);
}

copy_error(error, errorCapacity, "");
lua_settop(backend->state, 0);
return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
const char* callbackName = level_hook_name(hook);
if (!backend || !backend->state || !callbackName || !context)
return PANGEA_SCRIPT_OK;
if (!push_module_callback(backend, callbackName))
return PANGEA_SCRIPT_OK;

push_level_context(backend->state, context);
const int status = protected_call(backend, 1, 0, PANGEA_LUA_ITEM_BUDGET);
if (status != LUA_OK)
{
copy_lua_failure(backend, status, error, errorCapacity);
lua_settop(backend->state, 0);
return status_from_lua_failure(backend, status);
}
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "");
return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
if (!backend || !backend->state || !context)
return PANGEA_SCRIPT_OK;
if (!push_module_callback(backend, "onFrame"))
return PANGEA_SCRIPT_OK;

push_frame_context(backend->state, context);
const int status = protected_call(backend, 1, 0, PANGEA_LUA_FRAME_BUDGET);
if (status != LUA_OK)
{
copy_lua_failure(backend, status, error, errorCapacity);
lua_settop(backend->state, 0);
return status_from_lua_failure(backend, status);
}
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "");
return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity)
{
if (!backend || !backend->state || !context)
return PANGEA_SCRIPT_OK;
if (!push_module_callback(backend, "onTerrainItem"))
return PANGEA_SCRIPT_OK;

push_terrain_item_context(backend->state, context);
const int status = protected_call(backend, 1, 1, PANGEA_LUA_ITEM_BUDGET);
if (status != LUA_OK)
{
copy_lua_failure(backend, status, error, errorCapacity);
lua_settop(backend->state, 0);
return status_from_lua_failure(backend, status);
}
if (!parse_handled_result(backend->state, -1, &context->handled, &context->markInUse))
{
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "onTerrainItem must return a table with boolean handled/markInUse fields");
return PANGEA_SCRIPT_RUNTIME_ERROR;
}
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "");
return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity)
{
if (!backend || !backend->state || !context)
return PANGEA_SCRIPT_OK;
if (!push_module_callback(backend, "onSplineItem"))
return PANGEA_SCRIPT_OK;

push_spline_item_context(backend->state, context);
const int status = protected_call(backend, 1, 1, PANGEA_LUA_ITEM_BUDGET);
if (status != LUA_OK)
{
copy_lua_failure(backend, status, error, errorCapacity);
lua_settop(backend->state, 0);
return status_from_lua_failure(backend, status);
}
if (!parse_handled_result(backend->state, -1, &context->handled, &context->markInUse))
{
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "onSplineItem must return a table with boolean handled/markInUse fields");
return PANGEA_SCRIPT_RUNTIME_ERROR;
}
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "");
return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity)
{
if (!backend || !backend->state || !context)
return PANGEA_SCRIPT_OK;
if (!push_module_callback(backend, "onMapItem"))
return PANGEA_SCRIPT_OK;

push_map_item_context(backend->state, context);
const int status = protected_call(backend, 1, 1, PANGEA_LUA_ITEM_BUDGET);
if (status != LUA_OK)
{
copy_lua_failure(backend, status, error, errorCapacity);
lua_settop(backend->state, 0);
return status_from_lua_failure(backend, status);
}
if (!parse_handled_result(backend->state, -1, &context->handled, &context->markInUse))
{
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "onMapItem must return a table with boolean handled/markInUse fields");
return PANGEA_SCRIPT_RUNTIME_ERROR;
}
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "");
return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity)
{
if (!backend || !backend->state || !context || !result)
return PANGEA_SCRIPT_OK;
if (!push_module_callback(backend, "onObjectFrame"))
return PANGEA_SCRIPT_OK;

*result = (PangeaScriptObjectFrameResult){0};
push_object_frame_context(backend->state, context);
const int status = protected_call(backend, 1, 1, PANGEA_LUA_OBJECT_BUDGET);
if (status != LUA_OK)
{
copy_lua_failure(backend, status, error, errorCapacity);
lua_settop(backend->state, 0);
return status_from_lua_failure(backend, status);
}
if (!parse_object_frame_result(backend->state, -1, result))
{
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "onObjectFrame must return nil or a table with a positionOffset vector");
return PANGEA_SCRIPT_RUNTIME_ERROR;
}
lua_settop(backend->state, 0);
copy_error(error, errorCapacity, "");
return PANGEA_SCRIPT_OK;
}

#include "pangea_script.h"
#include "pangea_script_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define PANGEA_SCRIPT_ERROR_CAPACITY 512
#define PANGEA_SCRIPT_PATH_CAPACITY 260
#define PANGEA_SCRIPT_CONFIG_CAPACITY 65536
#define PANGEA_SCRIPT_MAX_REMAPS 64
#define PANGEA_SCRIPT_MAX_LEVEL_SETTINGS 64
#define PANGEA_SCRIPT_MAX_ASSET_DEPENDENCIES 64
#define PANGEA_SCRIPT_MAX_NATIVE_ITEMS 128
#define PANGEA_SCRIPT_MAX_OBJECTS 2048
#define PANGEA_SCRIPT_MAX_OBJECT_TAGS 8
#define PANGEA_SCRIPT_MAX_SCRIPT_TAG_LENGTH 64
#define PANGEA_SCRIPT_MAX_PENDING_DELETES 256

#include "pangea_script_config.h"

typedef struct RegisteredObject
{
	bool active;
	uint32_t generation;
	void* nativeObject;
	const PangeaScriptObjectOps* ops;
	const char* tags[PANGEA_SCRIPT_MAX_OBJECT_TAGS];
	int tagCount;
	char scriptTags[PANGEA_SCRIPT_MAX_OBJECT_TAGS][PANGEA_SCRIPT_MAX_SCRIPT_TAG_LENGTH];
	int scriptTagCount;
	PangeaScriptCapabilityLevel capabilityLevel;
	PangeaScriptObjectParams scriptParams;
	bool hasScriptParams;
} RegisteredObject;

static PangeaScriptGameInfo gGameInfo;
static char gStartupScriptPath[PANGEA_SCRIPT_PATH_CAPACITY];
static char gConfigPath[PANGEA_SCRIPT_PATH_CAPACITY] = "Data/Scripts/config/levels.json";
static char gLastError[PANGEA_SCRIPT_ERROR_CAPACITY];
static int gErrorCount;
static PangeaScriptStatus gLastStatus = PANGEA_SCRIPT_NOT_ENABLED;
static bool gInitialized;
static bool gScriptLoaded;
static PangeaScriptBackend* gBackend;
static ItemRemap gItemRemaps[PANGEA_SCRIPT_MAX_REMAPS];
static int gItemRemapCount;
static LevelSetting gLevelSettings[PANGEA_SCRIPT_MAX_LEVEL_SETTINGS];
static int gLevelSettingCount;
static PangeaScriptAssetDependency gLevelAssetDependencies[PANGEA_SCRIPT_MAX_ASSET_DEPENDENCIES];
static int gLevelAssetDependencyCount;
static int gCurrentLevelNum = -1;
static PangeaScriptNativeItem gNativeItems[PANGEA_SCRIPT_MAX_NATIVE_ITEMS];
static int gNativeItemCount;
static RegisteredObject gRegisteredObjects[PANGEA_SCRIPT_MAX_OBJECTS];
static PangeaScriptObjectHandle gPendingDeletes[PANGEA_SCRIPT_MAX_PENDING_DELETES];
static int gPendingDeleteCount;
static int gBudgetExceededCount;
static int gHooksCalledCount;
static int gConsecutiveHookFailures;
static bool gScriptsDisabled;

#define PANGEA_SCRIPT_MAX_SCRIPTED_OBJECTS 256

typedef struct ScriptedObjectState
{
	PangeaScriptVector3 position;
	PangeaScriptVector3 velocity;
	char id[64];
} ScriptedObjectState;

static ScriptedObjectState gScriptedObjects[PANGEA_SCRIPT_MAX_SCRIPTED_OBJECTS];
static int gScriptedObjectCount;

static bool ScriptedGetPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !outPosition) return false;
	*outPosition = state->position;
	return true;
}

static bool ScriptedSetPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !position) return false;
	state->position = *position;
	return true;
}

static bool ScriptedSetVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !velocity) return false;
	state->velocity = *velocity;
	return true;
}

static bool ScriptedDeleteObject(void* nativeObject)
{
	(void) nativeObject;
	return true;
}

static bool ScriptedGetInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !outInfo)
		return false;

	*outInfo = (PangeaScriptObjectInfo)
	{
		.velocity = state->velocity,
		.validMask = PANGEA_SCRIPT_OBJECT_INFO_VELOCITY,
	};
	return true;
}

static bool ScriptedSetInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_VELOCITY)
		state->velocity = info->velocity;

	return true;
}

static bool ScriptedGetBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
{
	(void) nativeObject;
	if (!outBounds)
		return false;

	*outBounds = (PangeaScriptObjectBounds)
	{
		.left = -10.0f,
		.right = 10.0f,
		.front = -10.0f,
		.back = 10.0f,
		.top = 10.0f,
		.bottom = -10.0f,
	};
	return true;
}

static bool ScriptedSetBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
{
	(void) nativeObject;
	return bounds != NULL;
}

static bool ScriptedGetParams(void* nativeObject, PangeaScriptObjectParams* outParams)
{
	(void) nativeObject;
	if (!outParams)
		return false;

	*outParams = (PangeaScriptObjectParams){0};
	return true;
}

static bool ScriptedSetParams(void* nativeObject, const PangeaScriptObjectParams* params)
{
	(void) nativeObject;
	return params != NULL;
}

static const PangeaScriptObjectOps kScriptedOps = {
	.getPosition = ScriptedGetPosition,
	.setPosition = ScriptedSetPosition,
	.setVelocity = ScriptedSetVelocity,
	.deleteObject = ScriptedDeleteObject,
	.getInfo = ScriptedGetInfo,
	.setInfo = ScriptedSetInfo,
	.getBounds = ScriptedGetBounds,
	.setBounds = ScriptedSetBounds,
	.getParams = ScriptedGetParams,
	.setParams = ScriptedSetParams
};

static void reset_objects(void)
{
	memset(gRegisteredObjects, 0, sizeof(gRegisteredObjects));
	memset(gPendingDeletes, 0, sizeof(gPendingDeletes));
	gPendingDeleteCount = 0;
	gScriptedObjectCount = 0;
}

static void clear_level_settings(void)
{
	memset(gLevelSettings, 0, sizeof(gLevelSettings));
	gLevelSettingCount = 0;
	memset(gLevelAssetDependencies, 0, sizeof(gLevelAssetDependencies));
	gLevelAssetDependencyCount = 0;
}



static const LevelSetting* find_level_setting(const char* key)
{
	if (!key || !key[0])
		return NULL;

	for (int i = 0; i < gLevelSettingCount; i++)
	{
		if (strcmp(gLevelSettings[i].key, key) == 0)
			return &gLevelSettings[i];
	}

	return NULL;
}



static void clear_registered_object(RegisteredObject* object)
{
	if (!object)
		return;

	object->active = false;
	object->nativeObject = NULL;
	object->ops = NULL;
	object->tagCount = 0;
	object->scriptTagCount = 0;
	object->hasScriptParams = false;
	memset(&object->scriptParams, 0, sizeof(object->scriptParams));
	memset(object->tags, 0, sizeof(object->tags));
	memset(object->scriptTags, 0, sizeof(object->scriptTags));
	object->generation++;
	if (object->generation == 0)
		object->generation = 1;
}

static void set_error(PangeaScriptStatus status, const char* message);
static void notify_object_delete(PangeaScriptObjectHandle handle, RegisteredObject* object);

static RegisteredObject* resolve_object(PangeaScriptObjectHandle handle)
{
	if (handle.id <= 0 || handle.id > PANGEA_SCRIPT_MAX_OBJECTS)
		return NULL;

	RegisteredObject* object = &gRegisteredObjects[handle.id - 1];
	if (!object->active)
		return NULL;

	if (object->generation != handle.generation)
		return NULL;

	return object;
}

static bool pending_delete_contains(PangeaScriptObjectHandle handle)
{
	for (int i = 0; i < gPendingDeleteCount; i++)
	{
		if (gPendingDeletes[i].id == handle.id && gPendingDeletes[i].generation == handle.generation)
			return true;
	}

	return false;
}

static bool object_params_are_valid(const PangeaScriptObjectParams* params)
{
	return params && params->count >= 0 && params->count <= PANGEA_SCRIPT_MAX_OBJECT_PARAMS;
}

static void copy_object_params(PangeaScriptObjectParams* dest, const PangeaScriptObjectParams* source)
{
	if (!dest || !source)
		return;

	memset(dest, 0, sizeof(*dest));
	dest->count = source->count;
	for (int i = 0; i < source->count; i++)
		dest->values[i] = source->values[i];
}

static bool delete_object_now(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !object->ops || !object->ops->deleteObject)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks deletion/cleanup-safe capability level");
		return false;
	}

	bool deleted = object->ops->deleteObject(object->nativeObject);
	if (deleted && object->active && object->generation == handle.generation)
	{
		notify_object_delete(handle, object);
		clear_registered_object(object);
	}
	return deleted;
}

static void process_pending_deletes(void)
{
	PangeaScriptObjectHandle pending[PANGEA_SCRIPT_MAX_PENDING_DELETES];
	int count = gPendingDeleteCount;
	if (count <= 0)
		return;

	memcpy(pending, gPendingDeletes, sizeof(PangeaScriptObjectHandle) * (size_t) count);
	memset(gPendingDeletes, 0, sizeof(gPendingDeletes));
	gPendingDeleteCount = 0;

	for (int i = 0; i < count; i++)
		delete_object_now(pending[i]);
}

static void clear_pending_deletes(void)
{
	memset(gPendingDeletes, 0, sizeof(gPendingDeletes));
	gPendingDeleteCount = 0;
}

static void configure_registered_object(RegisteredObject* object, const PangeaScriptObjectRegistration* registration)
{
	object->active = true;
	object->nativeObject = registration->nativeObject;
	object->ops = registration->ops;
	object->tagCount = registration->tagCount;
	if (registration->capabilityLevel == PANGEA_SCRIPT_CAPABILITY_DEFAULT)
	{
		object->capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL;
	}
	else
	{
		object->capabilityLevel = registration->capabilityLevel;
	}
	memset(object->tags, 0, sizeof(object->tags));
	for (int i = 0; i < registration->tagCount; i++)
		object->tags[i] = registration->tags[i];
	if (object->generation == 0)
		object->generation = 1;
}

static void set_error(PangeaScriptStatus status, const char* message)
{
	gLastStatus = status;
	if (status != PANGEA_SCRIPT_OK && status != PANGEA_SCRIPT_FILE_NOT_FOUND)
	{
		gErrorCount++;
		gConsecutiveHookFailures++;
		if (status == PANGEA_SCRIPT_BUDGET_EXCEEDED)
		{
			gBudgetExceededCount++;
		}
		if (gConsecutiveHookFailures >= 5)
		{
			gScriptsDisabled = true;
			PangeaScript_Log(PANGEA_LOG_ERROR, "Native", "Disabling scripting host: exceeded maximum consecutive hook failures (5)");
		}
	}
	else
	{
		gConsecutiveHookFailures = 0;
	}

	if (!message)
	{
		gLastError[0] = '\0';
		return;
	}

	snprintf(gLastError, sizeof(gLastError), "%s", message);
}

static void set_backend_error(PangeaScriptStatus status, const char* message)
{
	if (message && message[0])
		set_error(status, message);
	else if (status == PANGEA_SCRIPT_RUNTIME_ERROR)
		set_error(status, "Lua scripting backend is unavailable");
	else
		set_error(status, "Script execution failed");
}

static bool copy_string(char* dest, size_t destSize, const char* source)
{
	if (!dest || destSize == 0 || !source || !source[0])
		return false;

	snprintf(dest, destSize, "%s", source);
	return true;
}

static char* read_text_file(const char* path, long* outSize)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return NULL;

	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return NULL;
	}

	long size = ftell(file);
	if (size < 0)
	{
		fclose(file);
		return NULL;
	}

	if (fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return NULL;
	}

	char* bytes = (char*) malloc((size_t)size + 1);
	if (!bytes)
	{
		fclose(file);
		return NULL;
	}

	size_t readCount = fread(bytes, 1, (size_t)size, file);
	fclose(file);
	if (readCount != (size_t)size)
	{
		free(bytes);
		return NULL;
	}

	bytes[size] = '\0';
	if (outSize)
		*outSize = size;
	return bytes;
}

// Custom JSON parser functions removed. Config parsing is handled in pangea_script_config.c.

PangeaScriptStatus PangeaScript_Init(const PangeaScriptGameInfo* gameInfo)
{
	if (!gameInfo || !gameInfo->gameId || !gameInfo->gameName)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_Init received incomplete game info");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	gGameInfo = *gameInfo;
	if (gBackend)
	{
		PangeaScriptBackend_Destroy(gBackend);
		gBackend = NULL;
	}
	gBackend = PangeaScriptBackend_Create(gameInfo);
	gInitialized = true;
	gScriptLoaded = false;
	gErrorCount = 0;
	gItemRemapCount = 0;
	clear_level_settings();
	gBudgetExceededCount = 0;
	gHooksCalledCount = 0;
	gConsecutiveHookFailures = 0;
	gScriptsDisabled = false;
	reset_objects();
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

void PangeaScript_Shutdown(void)
{
	gInitialized = false;
	gScriptLoaded = false;
	gItemRemapCount = 0;
	clear_level_settings();
	gBudgetExceededCount = 0;
	gHooksCalledCount = 0;
	gConsecutiveHookFailures = 0;
	gScriptsDisabled = false;
	reset_objects();
	if (gBackend)
	{
		PangeaScriptBackend_Destroy(gBackend);
		gBackend = NULL;
	}
}

bool PangeaScript_IsEnabled(void)
{
	return gInitialized;
}

bool PangeaScript_HasRunnableModule(void)
{
	return gScriptLoaded && gBackend != NULL;
}

PangeaScriptStatus PangeaScript_SetStartupScript(const char* path)
{
	if (!copy_string(gStartupScriptPath, sizeof(gStartupScriptPath), path))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Startup script path is empty");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	return PangeaScript_Reload();
}

PangeaScriptStatus PangeaScript_SetConfigPath(const char* path)
{
	if (!copy_string(gConfigPath, sizeof(gConfigPath), path))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Config path is empty");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScript_Reload(void)
{
	if (!gInitialized)
	{
		set_error(PANGEA_SCRIPT_NOT_ENABLED, "Scripting host is not initialized");
		return PANGEA_SCRIPT_NOT_ENABLED;
	}

	const char* scriptPath = gStartupScriptPath[0] ? gStartupScriptPath : "Data/Scripts/dist/main.lua";
	long scriptSize = 0;
	char* script = read_text_file(scriptPath, &scriptSize);
	if (!script)
	{
		gScriptLoaded = false;
		set_error(PANGEA_SCRIPT_FILE_NOT_FOUND, "Script file not found");
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	}

	gScriptLoaded = scriptSize > 0;
	if (!gScriptLoaded)
	{
		free(script);
		set_error(PANGEA_SCRIPT_PARSE_ERROR, "Script file is empty");
		return PANGEA_SCRIPT_PARSE_ERROR;
	}

	if (gBackend)
	{
		char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
		backendError[0] = '\0';
		PangeaScriptStatus status = PangeaScriptBackend_Load(gBackend, script, backendError, (int)sizeof(backendError));
		free(script);
		if (status != PANGEA_SCRIPT_OK)
		{
			gScriptLoaded = false;
			set_backend_error(status, backendError);
			return status;
		}

		set_error(PANGEA_SCRIPT_OK, "");
		return PANGEA_SCRIPT_OK;
	}

	free(script);
	set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Lua scripting backend is unavailable");
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

const char* PangeaScript_GetLastError(void)
{
	return gLastError;
}

int PangeaScript_GetErrorCount(void)
{
	return gErrorCount;
}

PangeaScriptStatus PangeaScript_GetLastStatus(void)
{
	return gLastStatus;
}

PangeaScriptStatus PangeaScript_LoadLevelConfig(int levelNum)
{
	gItemRemapCount = 0;
	clear_level_settings();
	reset_objects();
	gCurrentLevelNum = levelNum;

	long configSize = 0;
	char* config = read_text_file(gConfigPath, &configSize);
	if (!config)
	{
		set_error(PANGEA_SCRIPT_FILE_NOT_FOUND, "Script config file not found");
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	}

	if (configSize > PANGEA_SCRIPT_CONFIG_CAPACITY)
	{
		free(config);
		clear_level_settings();
		set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Script config file is too large");
		return PANGEA_SCRIPT_CONFIG_ERROR;
	}

	PangeaConfig parsedConfig;
	char errorMsg[512];
	errorMsg[0] = '\0';

	PangeaScriptStatus parseStatus = PangeaScript_ParseConfig(config, levelNum, &parsedConfig, errorMsg, sizeof(errorMsg));
	if (parseStatus != PANGEA_SCRIPT_OK)
	{
		free(config);
		clear_level_settings();
		set_error(parseStatus, errorMsg);
		return parseStatus;
	}

	bool found = parsedConfig.level.hasConfig;
	const char* scriptPath = parsedConfig.level.scriptPath;

	if (found)
	{
		gItemRemapCount = parsedConfig.level.itemRemapCount;
		for (int i = 0; i < gItemRemapCount; i++)
		{
			gItemRemaps[i] = parsedConfig.level.itemRemaps[i];
		}

		gLevelSettingCount = parsedConfig.level.levelSettingCount;
		for (int i = 0; i < gLevelSettingCount; i++)
		{
			gLevelSettings[i] = parsedConfig.level.levelSettings[i];
		}

		gLevelAssetDependencyCount = parsedConfig.level.assetDependencyCount;
		for (int i = 0; i < gLevelAssetDependencyCount; i++)
		{
			gLevelAssetDependencies[i] = parsedConfig.level.assetDependencies[i];
		}
	}

	if (found && scriptPath[0])
	{
		if (strncmp(scriptPath, "Data/Scripts/", 13) != 0)
		{
			free(config);
			clear_level_settings();
			set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Invalid script path: must be within Data/Scripts/");
			return PANGEA_SCRIPT_CONFIG_ERROR;
		}
		if (strstr(scriptPath, "..") != NULL || strchr(scriptPath, '\\') != NULL)
		{
			free(config);
			clear_level_settings();
			set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Invalid script path: path traversal detected");
			return PANGEA_SCRIPT_CONFIG_ERROR;
		}

		copy_string(gStartupScriptPath, sizeof(gStartupScriptPath), scriptPath);
		PangeaScriptStatus reloadStatus = PangeaScript_Reload();
		if (reloadStatus != PANGEA_SCRIPT_OK && reloadStatus != PANGEA_SCRIPT_FILE_NOT_FOUND)
		{
			free(config);
			clear_level_settings();
			return reloadStatus;
		}
	}

	free(config);
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

int PangeaScript_RemapTerrainItemType(int levelNum, int itemType)
{
	for (int i = 0; i < gItemRemapCount; i++)
	{
		if (gItemRemaps[i].levelNum == levelNum && gItemRemaps[i].fromType == itemType)
			return gItemRemaps[i].toType;
	}
	return itemType;
}

int PangeaScript_GetLevelAssetDependencyCount(void)
{
	return gLevelAssetDependencyCount;
}

bool PangeaScript_GetLevelAssetDependency(int index, PangeaScriptAssetDependency* outDependency)
{
	if (!outDependency || index < 0 || index >= gLevelAssetDependencyCount)
		return false;

	*outDependency = gLevelAssetDependencies[index];
	return true;
}

bool PangeaScript_GetLevelFloatSetting(const char* key, float* outValue)
{
	const LevelSetting* setting = find_level_setting(key);
	if (!setting || !outValue)
		return false;

	if (setting->type == LEVEL_SETTING_FLOAT)
	{
		*outValue = setting->floatValue;
		return true;
	}

	if (setting->type == LEVEL_SETTING_INT)
	{
		*outValue = (float)setting->intValue;
		return true;
	}

	return false;
}

bool PangeaScript_GetLevelIntSetting(const char* key, int* outValue)
{
	const LevelSetting* setting = find_level_setting(key);
	if (!setting || !outValue || setting->type != LEVEL_SETTING_INT)
		return false;

	*outValue = setting->intValue;
	return true;
}

bool PangeaScript_GetLevelBoolSetting(const char* key, bool* outValue)
{
	const LevelSetting* setting = find_level_setting(key);
	if (!setting || !outValue || setting->type != LEVEL_SETTING_BOOL)
		return false;

	*outValue = setting->boolValue;
	return true;
}

bool PangeaScript_GetLevelStringSetting(const char* key, char* outValue, int capacity)
{
	const LevelSetting* setting = find_level_setting(key);
	if (!setting || !outValue || capacity <= 0 || setting->type != LEVEL_SETTING_STRING)
		return false;

	snprintf(outValue, (size_t)capacity, "%s", setting->stringValue);
	return true;
}

PangeaScriptStatus PangeaScript_CallLevelHook(PangeaScriptHook hook, const PangeaScriptLevelContext* context)
{
	const char* hookName = NULL;
	switch (hook)
	{
		case PANGEA_SCRIPT_HOOK_GAME_START: hookName = "onGameStart"; break;
		case PANGEA_SCRIPT_HOOK_LEVEL_LOAD: hookName = "onLevelLoad"; break;
		case PANGEA_SCRIPT_HOOK_LEVEL_START: hookName = "onLevelStart"; break;
		case PANGEA_SCRIPT_HOOK_FRAME: hookName = "onFrame"; break;
		case PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE: hookName = "onLevelComplete"; break;
		case PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD: hookName = "onLevelUnload"; break;
		case PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN: hookName = "onGameShutdown"; break;
		case PANGEA_SCRIPT_HOOK_TERRAIN_ITEM: hookName = "onTerrainItem"; break;
		case PANGEA_SCRIPT_HOOK_SPLINE_ITEM: hookName = "onSplineItem"; break;
		case PANGEA_SCRIPT_HOOK_MAP_ITEM: hookName = "onMapItem"; break;
		case PANGEA_SCRIPT_HOOK_OBJECT_FRAME: hookName = "onObjectFrame"; break;
	}

	return PangeaScript_CallNamedLevelHook(hookName, context);
}

PangeaScriptStatus PangeaScript_CallNamedLevelHook(const char* hookName, const PangeaScriptLevelContext* context)
{
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!hookName || !hookName[0] || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallNamedLevelHook(gBackend, hookName, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* context)
{
	return PangeaScript_CallNamedFrameHook("onFrame", context);
}

PangeaScriptStatus PangeaScript_CallNamedFrameHook(const char* hookName, const PangeaScriptFrameContext* context)
{
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!hookName || !hookName[0] || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallNamedFrameHook(gBackend, hookName, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallTerrainItemHook(PangeaScriptTerrainItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallTerrainItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallSplineItemHook(PangeaScriptSplineItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallSplineItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallMapItemHook(PangeaScriptMapItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallMapItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallPickupCollectedHook(PangeaScriptPickupContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallPickupCollectedHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallWeaponHitHook(PangeaScriptWeaponHitContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallWeaponHitHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallTriggerEnterHook(PangeaScriptTriggerContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallTriggerEnterHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallObjectCollisionHook(PangeaScriptObjectCollisionContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallObjectCollisionHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallPlayerDamageHook(PangeaScriptPlayerDamageContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallPlayerDamageHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallObjectDamageHook(PangeaScriptObjectDamageContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	gCurrentLevelNum = context->levelNum;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallObjectDamageHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

PangeaScriptStatus PangeaScript_CallObjectDeleteHook(const PangeaScriptObjectDeleteContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallObjectDeleteHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

static void notify_object_delete(PangeaScriptObjectHandle handle, RegisteredObject* object)
{
	if (!object || !object->active || !gInitialized || gScriptsDisabled || !gScriptLoaded || !gBackend)
		return;

	PangeaScriptObjectDeleteContext context =
	{
		.levelNum = gCurrentLevelNum,
		.object = handle,
		.tags = object->tags,
		.tagCount = object->tagCount,
	};

	if (object->ops && object->ops->getPosition)
		(void) object->ops->getPosition(object->nativeObject, &context.position);

	(void) PangeaScript_CallObjectDeleteHook(&context);
}

PangeaScriptStatus PangeaScript_CallObjectFrame(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, PangeaScriptObjectFrameResult* outResult)
{
	if (!frameContext || !outResult)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame received an incomplete frame context or result pointer");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	outResult->hasPositionOffset = false;
	outResult->positionOffset.x = 0.0f;
	outResult->positionOffset.y = 0.0f;
	outResult->positionOffset.z = 0.0f;

	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	RegisteredObject* object = resolve_object(handle);
	if (!object)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame received an unknown or stale object handle");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	if (!object->ops || !object->ops->getPosition)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame received an object without position access");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	PangeaScriptVector3 position;
	if (!object->ops->getPosition(object->nativeObject, &position))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame could not read the object's current position");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	gCurrentLevelNum = frameContext->levelNum;

	const PangeaScriptObjectFrameContext context =
	{
		.levelNum = frameContext->levelNum,
		.frameNum = frameContext->frameNum,
		.deltaSeconds = frameContext->deltaSeconds,
		.levelTimeSeconds = frameContext->levelTimeSeconds,
		.object = handle,
		.position = position,
		.tags = object->tags,
		.tagCount = object->tagCount,
	};

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallObjectFrameHook(gBackend, &context, outResult, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
	{
		set_backend_error(status, backendError);
		clear_pending_deletes();
	}
	else
	{
		process_pending_deletes();
	}
	return status;
}

void PangeaScript_ResetObjects(void)
{
	reset_objects();
}

PangeaScriptStatus PangeaScript_RegisterObject(const PangeaScriptObjectRegistration* registration, PangeaScriptObjectHandle* outHandle)
{
	if (!registration || !registration->nativeObject || !registration->ops || !registration->ops->getPosition)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (registration->tagCount < 0 || registration->tagCount > PANGEA_SCRIPT_MAX_OBJECT_TAGS)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	int freeIndex = -1;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
	{
		RegisteredObject* object = &gRegisteredObjects[i];
		if (object->active)
		{
			if (object->nativeObject == registration->nativeObject)
			{
				configure_registered_object(object, registration);
				if (outHandle)
				{
					outHandle->id = i + 1;
					outHandle->generation = object->generation;
				}
				return PANGEA_SCRIPT_OK;
			}
			continue;
		}

		if (freeIndex < 0)
			freeIndex = i;
	}

	if (freeIndex < 0)
		return PANGEA_SCRIPT_BUDGET_EXCEEDED;

	RegisteredObject* object = &gRegisteredObjects[freeIndex];
	configure_registered_object(object, registration);
	if (outHandle)
	{
		outHandle->id = freeIndex + 1;
		outHandle->generation = object->generation;
	}
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScript_RegisterScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
	if (gScriptedObjectCount >= PANGEA_SCRIPT_MAX_SCRIPTED_OBJECTS)
		return PANGEA_SCRIPT_BUDGET_EXCEEDED;

	ScriptedObjectState* state = &gScriptedObjects[gScriptedObjectCount++];
	state->position.x = x;
	state->position.y = y;
	state->position.z = z;
	state->velocity.x = 0.0f;
	state->velocity.y = 0.0f;
	state->velocity.z = 0.0f;
	snprintf(state->id, sizeof(state->id), "%s", id ? id : "");
	const char* tags[2];
	int tagCount = 0;
	if (state->id[0])
		tags[tagCount++] = state->id;
	tags[tagCount++] = "scripted";

	PangeaScriptObjectRegistration reg = {
		.nativeObject = state,
		.ops = &kScriptedOps,
		.tags = tags,
		.tagCount = tagCount,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL
	};

	return PangeaScript_RegisterObject(&reg, outHandle);
}

bool PangeaScript_UnregisterObject(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return false;

	notify_object_delete(handle, object);
	clear_registered_object(object);
	return true;
}

bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle)
{
	return resolve_object(handle) != NULL;
}

bool PangeaScript_GetObjectPosition(PangeaScriptObjectHandle handle, PangeaScriptVector3* outPosition)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outPosition || !object->ops || !object->ops->getPosition)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks read capability level");
		return false;
	}

	return object->ops->getPosition(object->nativeObject, outPosition);
}

bool PangeaScript_SetObjectPosition(PangeaScriptObjectHandle handle, const PangeaScriptVector3* position)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !position || !object->ops || !object->ops->setPosition)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_BASE)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks transform/position capability level");
		return false;
	}

	return object->ops->setPosition(object->nativeObject, position);
}

bool PangeaScript_SetObjectVelocity(PangeaScriptObjectHandle handle, const PangeaScriptVector3* velocity)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !velocity || !object->ops || !object->ops->setVelocity)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks movable/velocity capability level");
		return false;
	}

	return object->ops->setVelocity(object->nativeObject, velocity);
}

bool PangeaScript_DeleteObject(PangeaScriptObjectHandle handle)
{
	return delete_object_now(handle);
}

bool PangeaScript_RequestDeleteObject(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !object->ops || !object->ops->deleteObject)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks deletion/cleanup-safe capability level");
		return false;
	}

	if (pending_delete_contains(handle))
		return true;

	if (gPendingDeleteCount >= PANGEA_SCRIPT_MAX_PENDING_DELETES)
	{
		set_error(PANGEA_SCRIPT_BUDGET_EXCEEDED, "Deferred object delete queue is full");
		return false;
	}

	gPendingDeletes[gPendingDeleteCount++] = handle;
	return true;
}

bool PangeaScript_GetObjectInfo(PangeaScriptObjectHandle handle, PangeaScriptObjectInfo* outInfo)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outInfo || !object->ops || !object->ops->getInfo)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks info read capability level");
		return false;
	}

	memset(outInfo, 0, sizeof(*outInfo));
	return object->ops->getInfo(object->nativeObject, outInfo);
}

bool PangeaScript_SetObjectInfo(PangeaScriptObjectHandle handle, const PangeaScriptObjectInfo* info)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !info || !object->ops || !object->ops->setInfo)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks info mutation capability level");
		return false;
	}

	return object->ops->setInfo(object->nativeObject, info);
}

bool PangeaScript_GetObjectBounds(PangeaScriptObjectHandle handle, PangeaScriptObjectBounds* outBounds)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outBounds || !object->ops || !object->ops->getBounds)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks bounds read capability level");
		return false;
	}

	memset(outBounds, 0, sizeof(*outBounds));
	return object->ops->getBounds(object->nativeObject, outBounds);
}

bool PangeaScript_SetObjectBounds(PangeaScriptObjectHandle handle, const PangeaScriptObjectBounds* bounds)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !bounds || !object->ops || !object->ops->setBounds)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks bounds mutation capability level");
		return false;
	}

	return object->ops->setBounds(object->nativeObject, bounds);
}

bool PangeaScript_GetObjectParams(PangeaScriptObjectHandle handle, PangeaScriptObjectParams* outParams)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outParams)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks params read capability level");
		return false;
	}

	memset(outParams, 0, sizeof(*outParams));
	if (object->ops && object->ops->getParams && object->ops->getParams(object->nativeObject, outParams) && outParams->count > 0)
		return true;

	if (object->hasScriptParams)
	{
		copy_object_params(outParams, &object->scriptParams);
		return true;
	}

	return true;
}

bool PangeaScript_SetObjectParams(PangeaScriptObjectHandle handle, const PangeaScriptObjectParams* params)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !object_params_are_valid(params))
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks params mutation capability level");
		return false;
	}

	copy_object_params(&object->scriptParams, params);
	object->hasScriptParams = true;

	if (object->ops && object->ops->setParams)
		object->ops->setParams(object->nativeObject, params);

	return true;
}

int PangeaScript_GetObjectTagCount(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
		return 0;

	return object->tagCount + object->scriptTagCount;
}

const char* PangeaScript_GetObjectTag(PangeaScriptObjectHandle handle, int index)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
		return NULL;
	if (index < 0 || index >= object->tagCount + object->scriptTagCount)
		return NULL;
	if (index >= object->tagCount)
		return object->scriptTags[index - object->tagCount];

	return object->tags[index];
}

bool PangeaScript_ObjectHasTag(PangeaScriptObjectHandle handle, const char* tag)
{
	if (!tag || !tag[0])
		return false;

	int tagCount = PangeaScript_GetObjectTagCount(handle);
	for (int i = 0; i < tagCount; i++)
	{
		const char* candidate = PangeaScript_GetObjectTag(handle, i);
		if (candidate && strcmp(candidate, tag) == 0)
			return true;
	}

	return false;
}

bool PangeaScript_AddObjectTag(PangeaScriptObjectHandle handle, const char* tag)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !tag || !tag[0] || object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
		return false;

	if (strlen(tag) >= PANGEA_SCRIPT_MAX_SCRIPT_TAG_LENGTH)
		return false;

	if (PangeaScript_ObjectHasTag(handle, tag))
		return true;

	if (object->scriptTagCount >= PANGEA_SCRIPT_MAX_OBJECT_TAGS)
		return false;

	snprintf(object->scriptTags[object->scriptTagCount], sizeof(object->scriptTags[object->scriptTagCount]), "%s", tag);
	object->scriptTagCount++;
	return true;
}

bool PangeaScript_RemoveObjectTag(PangeaScriptObjectHandle handle, const char* tag)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !tag || !tag[0] || object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
		return false;

	for (int i = 0; i < object->scriptTagCount; i++)
	{
		if (strcmp(object->scriptTags[i], tag) != 0)
			continue;

		for (int j = i + 1; j < object->scriptTagCount; j++)
			snprintf(object->scriptTags[j - 1], sizeof(object->scriptTags[j - 1]), "%s", object->scriptTags[j]);

		object->scriptTagCount--;
		object->scriptTags[object->scriptTagCount][0] = '\0';
		return true;
	}

	return false;
}

PangeaScriptStatus PangeaScript_RegisterNativeItems(const PangeaScriptNativeItem* items, int count)
{
	if (!items || count < 0 || count > PANGEA_SCRIPT_MAX_NATIVE_ITEMS)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	for (int i = 0; i < count; i++)
		gNativeItems[i] = items[i];

	gNativeItemCount = count;
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScript_SpawnNative(const char* id, float x, float y, float z, int subtype, int amount, PangeaScriptObjectHandle* outHandle)
{
	if (!id || !id[0])
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (gGameInfo.spawnNative)
	{
		return gGameInfo.spawnNative(id, x, y, z, subtype, amount, outHandle);
	}

	for (int i = 0; i < gNativeItemCount; i++)
	{
		if (gNativeItems[i].id && strcmp(gNativeItems[i].id, id) == 0)
			return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
}

bool PangeaScript_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (!outInfo || playerNum < 0 || !gGameInfo.getPlayerInfo)
		return false;

	return gGameInfo.getPlayerInfo(playerNum, outInfo);
}

bool PangeaScript_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (!info || playerNum < 0 || !gGameInfo.setPlayerInfo)
		return false;

	return gGameInfo.setPlayerInfo(playerNum, info);
}

bool PangeaScript_PlaySound(const PangeaScriptSoundRequest* request)
{
	if (!request || request->soundId < 0 || !gGameInfo.playSound)
		return false;

	return gGameInfo.playSound(request);
}

bool PangeaScript_SpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request || request->effectId < 0 || !gGameInfo.spawnEffect)
		return false;

	return gGameInfo.spawnEffect(request);
}

void PangeaScript_Log(PangeaScriptLogLevel level, const char* source, const char* message)
{
	const char* levelStr = "INFO";
	FILE* out = stdout;
	if (level == PANGEA_LOG_WARN)
	{
		levelStr = "WARNING";
		out = stderr;
	}
	else if (level == PANGEA_LOG_ERROR)
	{
		levelStr = "ERROR";
		out = stderr;
	}

	fprintf(out, "[PangeaScript %s] [%s] %s\n", levelStr, source ? source : "Native", message ? message : "");
	fflush(out);
}

void PangeaScript_GetStatusInfo(PangeaScriptStatusInfo* outInfo)
{
	if (!outInfo)
		return;

	outInfo->enabled = gInitialized;
	outInfo->configLoaded = gConfigPath[0] != '\0';
	outInfo->bundleLoaded = gScriptLoaded;
	snprintf(outInfo->activeScriptPath, sizeof(outInfo->activeScriptPath), "%s", gStartupScriptPath[0] ? gStartupScriptPath : "Data/Scripts/dist/main.lua");
	snprintf(outInfo->lastError, sizeof(outInfo->lastError), "%s", gLastError);
	outInfo->errorCount = gErrorCount;
	outInfo->budgetExceededCount = gBudgetExceededCount;
	outInfo->hooksCalledCount = gHooksCalledCount;
	outInfo->scriptsDisabled = gScriptsDisabled;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_GetStatusEnabled(void) { return gInitialized; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_GetStatusConfigLoaded(void) { return gConfigPath[0] != '\0'; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_GetStatusBundleLoaded(void) { return gScriptLoaded; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* PangeaScript_GetStatusActiveScriptPath(void) { return gStartupScriptPath[0] ? gStartupScriptPath : "Data/Scripts/dist/main.lua"; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* PangeaScript_GetStatusLastError(void) { return gLastError; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int PangeaScript_GetStatusErrorCount(void) { return gErrorCount; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int PangeaScript_GetStatusBudgetExceededCount(void) { return gBudgetExceededCount; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int PangeaScript_GetStatusHooksCalledCount(void) { return gHooksCalledCount; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_GetStatusScriptsDisabled(void) { return gScriptsDisabled; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
void* gPangeaScriptPreserveStatus[] = {
	(void*)PangeaScript_GetStatusEnabled,
	(void*)PangeaScript_GetStatusConfigLoaded,
	(void*)PangeaScript_GetStatusBundleLoaded,
	(void*)PangeaScript_GetStatusActiveScriptPath,
	(void*)PangeaScript_GetStatusLastError,
	(void*)PangeaScript_GetStatusErrorCount,
	(void*)PangeaScript_GetStatusBudgetExceededCount,
	(void*)PangeaScript_GetStatusHooksCalledCount,
	(void*)PangeaScript_GetStatusScriptsDisabled
};
#endif

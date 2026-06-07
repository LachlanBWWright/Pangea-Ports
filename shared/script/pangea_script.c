#include "pangea_script.h"
#include "pangea_script_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PANGEA_SCRIPT_ERROR_CAPACITY 512
#define PANGEA_SCRIPT_PATH_CAPACITY 260
#define PANGEA_SCRIPT_CONFIG_CAPACITY 65536
#define PANGEA_SCRIPT_MAX_REMAPS 64
#define PANGEA_SCRIPT_MAX_NATIVE_ITEMS 128
#define PANGEA_SCRIPT_MAX_OBJECTS 2048
#define PANGEA_SCRIPT_MAX_OBJECT_TAGS 8

typedef struct ItemRemap
{
	int levelNum;
	int fromType;
	int toType;
} ItemRemap;

typedef struct RegisteredObject
{
	bool active;
	uint32_t generation;
	void* nativeObject;
	const PangeaScriptObjectOps* ops;
	const char* tags[PANGEA_SCRIPT_MAX_OBJECT_TAGS];
	int tagCount;
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
static PangeaScriptNativeItem gNativeItems[PANGEA_SCRIPT_MAX_NATIVE_ITEMS];
static int gNativeItemCount;
static RegisteredObject gRegisteredObjects[PANGEA_SCRIPT_MAX_OBJECTS];

static void reset_objects(void)
{
	memset(gRegisteredObjects, 0, sizeof(gRegisteredObjects));
}

static void clear_registered_object(RegisteredObject* object)
{
	if (!object)
		return;

	object->active = false;
	object->nativeObject = NULL;
	object->ops = NULL;
	object->tagCount = 0;
	memset(object->tags, 0, sizeof(object->tags));
	object->generation++;
	if (object->generation == 0)
		object->generation = 1;
}

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

static void configure_registered_object(RegisteredObject* object, const PangeaScriptObjectRegistration* registration)
{
	object->active = true;
	object->nativeObject = registration->nativeObject;
	object->ops = registration->ops;
	object->tagCount = registration->tagCount;
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
		gErrorCount++;

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
		set_error(status, "JavaScript engine is not linked; script execution is unavailable");
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

static const char* find_level_block(const char* json, int levelNum)
{
	char key[32];
	snprintf(key, sizeof(key), "\"%d\"", levelNum);
	return strstr(json, key);
}

static bool parse_int_after_key(const char* start, const char* key, int* outValue)
{
	const char* keyPos = strstr(start, key);
	if (!keyPos)
		return false;

	const char* colon = strchr(keyPos, ':');
	if (!colon)
		return false;

	char* end = NULL;
	long value = strtol(colon + 1, &end, 10);
	if (end == colon + 1)
		return false;

	*outValue = (int)value;
	return true;
}

static bool parse_string_after_key(const char* start, const char* key, char* outValue, size_t outValueSize)
{
	const char* keyPos = strstr(start, key);
	if (!keyPos || !outValue || outValueSize == 0)
		return false;

	const char* colon = strchr(keyPos, ':');
	if (!colon)
		return false;

	const char* quote = strchr(colon + 1, '"');
	if (!quote)
		return false;

	quote++;
	const char* end = strchr(quote, '"');
	if (!end || end == quote)
		return false;

	size_t length = (size_t)(end - quote);
	if (length >= outValueSize)
		return false;

	memcpy(outValue, quote, length);
	outValue[length] = '\0';
	return true;
}

static void parse_item_remaps_for_level(const char* json, int levelNum)
{
	const char* level = find_level_block(json, levelNum);
	if (!level)
		return;

	const char* itemOverrides = strstr(level, "\"itemOverrides\"");
	if (!itemOverrides)
		return;

	const char* cursor = itemOverrides;
	while (gItemRemapCount < PANGEA_SCRIPT_MAX_REMAPS)
	{
		const char* fromKey = strstr(cursor, "\"from\"");
		if (!fromKey)
			break;

		const char* toKey = strstr(fromKey, "\"to\"");
		if (!toKey)
			break;

		int fromType = 0;
		int toType = 0;
		if (!parse_int_after_key(fromKey, "\"from\"", &fromType) ||
			!parse_int_after_key(toKey, "\"to\"", &toType))
		{
			cursor = toKey + 1;
			continue;
		}

		gItemRemaps[gItemRemapCount].levelNum = levelNum;
		gItemRemaps[gItemRemapCount].fromType = fromType;
		gItemRemaps[gItemRemapCount].toType = toType;
		gItemRemapCount++;
		cursor = toKey + 1;
	}
}

static bool parse_script_path_for_level(const char* json, int levelNum, char* outPath, size_t outPathSize)
{
	const char* level = find_level_block(json, levelNum);
	if (!level)
		return false;

	return parse_string_after_key(level, "\"script\"", outPath, outPathSize);
}

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
	reset_objects();
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

void PangeaScript_Shutdown(void)
{
	gInitialized = false;
	gScriptLoaded = false;
	gItemRemapCount = 0;
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

	const char* scriptPath = gStartupScriptPath[0] ? gStartupScriptPath : "Data/Scripts/dist/main.js";
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
	set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "JavaScript engine is not linked; script execution is unavailable");
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
		set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Script config file is too large");
		return PANGEA_SCRIPT_CONFIG_ERROR;
	}

	parse_item_remaps_for_level(config, levelNum);
	char scriptPath[PANGEA_SCRIPT_PATH_CAPACITY];
	if (parse_script_path_for_level(config, levelNum, scriptPath, sizeof(scriptPath)))
	{
		copy_string(gStartupScriptPath, sizeof(gStartupScriptPath), scriptPath);
		PangeaScriptStatus reloadStatus = PangeaScript_Reload();
		if (reloadStatus != PANGEA_SCRIPT_OK && reloadStatus != PANGEA_SCRIPT_FILE_NOT_FOUND)
		{
			free(config);
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

PangeaScriptStatus PangeaScript_CallLevelHook(PangeaScriptHook hook, const PangeaScriptLevelContext* context)
{
	(void) hook;
	(void) context;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallLevelHook(gBackend, hook, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* context)
{
	(void) context;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallFrameHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallTerrainItemHook(PangeaScriptTerrainItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallTerrainItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallSplineItemHook(PangeaScriptSplineItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallSplineItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallMapItemHook(PangeaScriptMapItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallMapItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
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
		set_backend_error(status, backendError);
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

bool PangeaScript_UnregisterObject(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return false;

	clear_registered_object(object);
	return true;
}

bool PangeaScript_GetObjectPosition(PangeaScriptObjectHandle handle, PangeaScriptVector3* outPosition)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outPosition || !object->ops || !object->ops->getPosition)
		return false;

	return object->ops->getPosition(object->nativeObject, outPosition);
}

bool PangeaScript_SetObjectPosition(PangeaScriptObjectHandle handle, const PangeaScriptVector3* position)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !position || !object->ops || !object->ops->setPosition)
		return false;

	return object->ops->setPosition(object->nativeObject, position);
}

bool PangeaScript_SetObjectVelocity(PangeaScriptObjectHandle handle, const PangeaScriptVector3* velocity)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !velocity || !object->ops || !object->ops->setVelocity)
		return false;

	return object->ops->setVelocity(object->nativeObject, velocity);
}

bool PangeaScript_DeleteObject(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !object->ops || !object->ops->deleteObject)
		return false;

	return object->ops->deleteObject(object->nativeObject);
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

PangeaScriptStatus PangeaScript_SpawnNative(const char* id, float x, float y, float z)
{
	(void) x;
	(void) y;
	(void) z;
	if (!id || !id[0])
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	for (int i = 0; i < gNativeItemCount; i++)
	{
		if (gNativeItems[i].id && strcmp(gNativeItems[i].id, id) == 0)
			return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
}

#include "pangea_script.h"
#include "pangea_script_contract.h"
#include "pangea_script_backend.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
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
#define PANGEA_SCRIPT_NATIVE_ID_CAPACITY 96
#define PANGEA_SCRIPT_NATIVE_CATEGORY_CAPACITY 32
#define PANGEA_SCRIPT_NATIVE_DEPENDENCY_CAPACITY 260
#define PANGEA_SCRIPT_MAX_CUSTOM_OBJECTS 64
#define PANGEA_SCRIPT_MAX_TERRAIN_REPLACEMENTS 128
#define PANGEA_SCRIPT_MAX_MAP_REPLACEMENTS 128
#define PANGEA_SCRIPT_MAX_OBJECTS 2048
#define PANGEA_SCRIPT_MAX_OBJECT_TAGS 8
#define PANGEA_SCRIPT_MAX_TRIGGER_CONTACTS 512
#define PANGEA_SCRIPT_OBJECT_TYPE_CAPACITY 96
#define PANGEA_SCRIPT_OBJECT_TAG_CAPACITY 64

#include "pangea_script_config.h"

typedef struct RegisteredObject
{
	bool active;
	bool enabled;
	bool destroying;
	uint32_t generation;
	void* nativeObject;
	const PangeaScriptObjectOps* ops;
	char objectTypeStorage[PANGEA_SCRIPT_OBJECT_TYPE_CAPACITY];
	char tagStorage[PANGEA_SCRIPT_MAX_OBJECT_TAGS][PANGEA_SCRIPT_OBJECT_TAG_CAPACITY];
	const char* objectType;
	const char* tags[PANGEA_SCRIPT_MAX_OBJECT_TAGS];
	int tagCount;
	PangeaScriptCapabilityLevel capabilityLevel;
	PangeaScriptObjectSource source;
	PangeaScriptObjectHandle owner;
	bool hasOwner;
} RegisteredObject;

static PangeaScriptGameInfo gGameInfo;
static char gStartupScriptPath[PANGEA_SCRIPT_PATH_CAPACITY];
static char gConfigPath[PANGEA_SCRIPT_PATH_CAPACITY] = "Data/Scripts/config/levels.json";
static char gLastError[PANGEA_SCRIPT_ERROR_CAPACITY];
static char gLastRuntimeError[PANGEA_SCRIPT_ERROR_CAPACITY];
static int gErrorCount;
static PangeaScriptStatus gLastStatus = PANGEA_SCRIPT_NOT_ENABLED;
static bool gInitialized;
static bool gScriptLoaded;
static uint32_t gRuntimeFingerprint;
static PangeaScriptBackend* gBackend;
static ItemRemap gItemRemaps[PANGEA_SCRIPT_MAX_REMAPS];
static int gItemRemapCount;
static LevelSetting gLevelSettings[PANGEA_SCRIPT_MAX_LEVEL_SETTINGS];
static int gLevelSettingCount;
static PangeaScriptAssetDependency gLevelAssetDependencies[PANGEA_SCRIPT_MAX_ASSET_DEPENDENCIES];
static int gLevelAssetDependencyCount;
static PangeaScriptNativeItem gNativeItems[PANGEA_SCRIPT_MAX_NATIVE_ITEMS];
typedef struct NativeItemStrings
{
	char id[PANGEA_SCRIPT_NATIVE_ID_CAPACITY];
	char category[PANGEA_SCRIPT_NATIVE_CATEGORY_CAPACITY];
	char dependencySummary[PANGEA_SCRIPT_NATIVE_DEPENDENCY_CAPACITY];
} NativeItemStrings;
static NativeItemStrings gNativeItemStrings[PANGEA_SCRIPT_MAX_NATIVE_ITEMS];
static NativeItemStrings gPendingNativeItemStrings[PANGEA_SCRIPT_MAX_NATIVE_ITEMS];
static int gNativeItemCount;
static PangeaScriptCustomObjectDefinition gCustomObjects[PANGEA_SCRIPT_MAX_CUSTOM_OBJECTS];
static int gCustomObjectCount;
static PangeaScriptTerrainReplacement gTerrainReplacements[PANGEA_SCRIPT_MAX_TERRAIN_REPLACEMENTS];
static int gTerrainReplacementCount;
static PangeaScriptMapReplacement gMapReplacements[PANGEA_SCRIPT_MAX_MAP_REPLACEMENTS];
static int gMapReplacementCount;
static PangeaScriptSplineReplacement gSplineReplacements[PANGEA_CONFIG_MAX_SPLINE_REPLACEMENTS];
static int gSplineReplacementCount;
static PangeaConfig gParsedConfig;
static RegisteredObject gRegisteredObjects[PANGEA_SCRIPT_MAX_OBJECTS];
static int gBudgetExceededCount;
static int gHooksCalledCount;
static int gConsecutiveHookFailures;
static bool gScriptsDisabled;
static bool gNetworkedMode;
static PangeaScriptFrameContext gCurrentCallbackFrame;
static bool gHasCurrentCallbackFrame;
static PangeaScriptFrameContext gLastFrameContext;
static bool gHasLastFrameContext;
static PangeaScriptObjectHandle gCurrentCallbackObject;
static bool gHasCurrentCallbackObject;
static PangeaScriptObjectFrameResult* gCurrentObjectFrameResult;
static PangeaScriptCommandTrace gCommandTrace = {
	.commandCount = 0,
	.hash = 0x811c9dc5u,
	.entryCount = 0,
	.overflow = false
};
static PangeaScriptCommandTraceEntry gCommandTraceEntries[PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY];
static PangeaScriptLifecycleTrace gLifecycleTrace = {
	.eventCount = 0,
	.entryCount = 0,
	.overflow = false
};
static PangeaScriptLifecycleTraceEntry gLifecycleTraceEntries[PANGEA_SCRIPT_LIFECYCLE_TRACE_CAPACITY];

typedef struct PangeaScriptDeferredDeletion
{
	bool active;
	int levelNum;
	PangeaScriptObjectHandle handle;
} PangeaScriptDeferredDeletion;

static PangeaScriptDeferredDeletion gDeferredDeletions[PANGEA_SCRIPT_DEFERRED_ACTION_CAPACITY];

typedef struct PangeaScriptTriggerContact
{
	bool active;
	PangeaScriptObjectHandle self;
	PangeaScriptObjectHandle other;
	int levelNum;
	unsigned int lastFrame;
	unsigned int sideBits;
} PangeaScriptTriggerContact;

static PangeaScriptTriggerContact gTriggerContacts[PANGEA_SCRIPT_MAX_TRIGGER_CONTACTS];

static bool copy_string(char* dest, size_t destSize, const char* source);
static PangeaScriptStatus call_object_event(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, const char* event, bool hasEventValue, int eventValue, PangeaScriptObjectHandle other, bool hasOther, unsigned int sideBits, PangeaScriptObjectFrameResult* outResult);

static uint32_t fingerprint_bytes(uint32_t hash, const unsigned char* bytes, size_t size)
{
	for (size_t index = 0; index < size; index++)
		hash = (hash ^ bytes[index]) * 0x01000193u;
	return hash;
}

static uint32_t fingerprint_u32(uint32_t hash, uint32_t value)
{
	for (int byte = 0; byte < 4; byte++)
	{
		hash = (hash ^ (unsigned char)(value & 0xffu)) * 0x01000193u;
		value >>= 8;
	}
	return hash;
}

static uint32_t compute_runtime_fingerprint(const char* gameId, const unsigned char* source, size_t sourceSize)
{
	uint32_t hash = 0x811c9dc5u;
	if (gameId)
		hash = fingerprint_bytes(hash, (const unsigned char*)gameId, strlen(gameId));
	hash = fingerprint_u32(hash, PANGEA_SCRIPT_CONTRACT_VERSION);
	hash = fingerprint_u32(hash, PANGEA_SCRIPT_API_VERSION);
	return fingerprint_bytes(hash, source, sourceSize);
}

static bool default_persistence_path(const char* key, char* outPath, size_t capacity)
{
	if (!key || !key[0] || !outPath || capacity == 0)
		return false;
	for (const char* cursor = key; *cursor; cursor++)
	{
		const unsigned char character = (unsigned char)*cursor;
		const bool valid = (character >= 'a' && character <= 'z') ||
			(character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9') || character == '_' || character == '-' || character == '.';
		if (!valid)
			return false;
	}
	const int written = snprintf(outPath, capacity,
#ifdef __EMSCRIPTEN__
		"Data/Scripts/persistence/.pangea-persistence-%s.bin",
#else
		"Data/Scripts/.pangea-persistence-%s.bin",
#endif
		key);
	return written > 0 && (size_t)written < capacity;
}

static PangeaScriptStatus default_load_persistent(const char* key, unsigned char* outData, int capacity, int* outSize)
{
	char path[PANGEA_SCRIPT_PATH_CAPACITY + 64];
	FILE* file;
	size_t size;
	if (!outData || capacity <= 0 || !outSize || !default_persistence_path(key, path, sizeof(path)))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	*outSize = 0;
	file = fopen(path, "rb");
	if (!file)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	size = fread(outData, 1, (size_t)capacity, file);
	if (ferror(file) || !feof(file))
	{
		fclose(file);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	fclose(file);
	*outSize = (int)size;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus default_save_persistent(const char* key, const unsigned char* data, int size)
{
	char path[PANGEA_SCRIPT_PATH_CAPACITY + 64];
	char temporaryPath[PANGEA_SCRIPT_PATH_CAPACITY + 68];
	FILE* file;
	size_t written;
	int closeStatus;
	if (!default_persistence_path(key, path, sizeof(path)) || size < 0)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
#ifdef __EMSCRIPTEN__
	EM_ASM({ FS.mkdirTree("Data/Scripts/persistence"); });
#endif
	if (size == 0)
		return remove(path) == 0 || errno == ENOENT ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!data)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	const int temporaryLength = snprintf(temporaryPath, sizeof(temporaryPath), "%s.tmp", path);
	if (temporaryLength <= 0 || (size_t)temporaryLength >= sizeof(temporaryPath))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	file = fopen(temporaryPath, "wb");
	if (!file)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	written = fwrite(data, 1, (size_t)size, file);
	closeStatus = fclose(file);
	if (written != (size_t)size || closeStatus != 0)
	{
		(void)remove(temporaryPath);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	if (rename(temporaryPath, path) != 0)
	{
		(void)remove(temporaryPath);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	return PANGEA_SCRIPT_OK;
}

static bool is_safe_script_path(const char* path)
{
	return path && path[0] != '/' && path[0] != '\\' && strchr(path, ':') == NULL &&
		strstr(path, "..") == NULL && strchr(path, '\\') == NULL;
}

static bool is_script_data_path(const char* path)
{
	static const char prefix[] = "Data/Scripts/";
	const size_t prefixLength = sizeof(prefix) - 1;

	return is_safe_script_path(path) && strncmp(path, prefix, prefixLength) == 0 &&
		path[prefixLength] != '\0';
}

static bool same_object_handle(PangeaScriptObjectHandle left, PangeaScriptObjectHandle right)
{
	return left.id == right.id && left.generation == right.generation;
}

static void clear_trigger_contacts_for_handle(PangeaScriptObjectHandle handle)
{
	for (int i = 0; i < PANGEA_SCRIPT_MAX_TRIGGER_CONTACTS; i++)
	{
		PangeaScriptTriggerContact* contact = &gTriggerContacts[i];
		if (contact->active && (same_object_handle(contact->self, handle) || same_object_handle(contact->other, handle)))
			contact->active = false;
	}
}

static bool record_trigger_contact(PangeaScriptObjectHandle self, PangeaScriptObjectHandle other, unsigned int sideBits, const PangeaScriptFrameContext* frameContext)
{
	PangeaScriptTriggerContact* freeContact = NULL;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_TRIGGER_CONTACTS; i++)
	{
		PangeaScriptTriggerContact* contact = &gTriggerContacts[i];
		if (!contact->active)
		{
			if (!freeContact) freeContact = contact;
			continue;
		}
		if (contact->levelNum != frameContext->levelNum ||
			(frameContext->frameNum < contact->lastFrame ||
			 frameContext->frameNum - contact->lastFrame > 1U))
		{
			contact->active = false;
			if (!freeContact) freeContact = contact;
			continue;
		}
		if (!same_object_handle(contact->self, self) || !same_object_handle(contact->other, other))
			continue;

		contact->levelNum = frameContext->levelNum;
		contact->lastFrame = frameContext->frameNum;
		contact->sideBits = sideBits;
		return true;
	}

	if (!freeContact)
		return false;
	freeContact->active = true;
	freeContact->self = self;
	freeContact->other = other;
	freeContact->levelNum = frameContext->levelNum;
	freeContact->lastFrame = frameContext->frameNum;
	freeContact->sideBits = sideBits;
	return false;
}

#define PANGEA_SCRIPT_COMMAND_DESCRIPTOR_ENTRY(id, capability, authority, phase, validation) \
	{id, capability, authority, phase, validation},
static const PangeaScriptCommandDescriptor gCommandDescriptors[] = {
	PANGEA_SCRIPT_COMMAND_DESCRIPTOR_LIST(PANGEA_SCRIPT_COMMAND_DESCRIPTOR_ENTRY)
};
#undef PANGEA_SCRIPT_COMMAND_DESCRIPTOR_ENTRY

#define PANGEA_SCRIPT_EVENT_DESCRIPTOR_ENTRY(id, phase, payload, result) \
	{id, phase, payload, result},
static const PangeaScriptEventDescriptor gEventDescriptors[] = {
	PANGEA_SCRIPT_EVENT_DESCRIPTOR_LIST(PANGEA_SCRIPT_EVENT_DESCRIPTOR_ENTRY)
};
#undef PANGEA_SCRIPT_EVENT_DESCRIPTOR_ENTRY

#define PANGEA_SCRIPT_OBJECT_EVENT_DESCRIPTOR_ENTRY(id, handler, phase, cleanup, statePolicy, invalidatesHandle) \
	{id, handler, phase, cleanup, statePolicy, invalidatesHandle},
static const PangeaScriptObjectEventDescriptor gObjectEventDescriptors[] = {
	PANGEA_SCRIPT_OBJECT_EVENT_DESCRIPTOR_LIST(PANGEA_SCRIPT_OBJECT_EVENT_DESCRIPTOR_ENTRY)
};
#undef PANGEA_SCRIPT_OBJECT_EVENT_DESCRIPTOR_ENTRY

#define PANGEA_SCRIPT_MAX_SCRIPTED_OBJECTS 256

typedef struct ScriptedObjectState
{
	bool active;
	PangeaScriptVector3 position;
	PangeaScriptVector3 velocity;
	PangeaScriptVector3 rotation;
	float scale;
	int animation;
	float animationSpeed;
	float animationBlendSeconds;
	bool collisionEnabled;
	char animationName[64];
	char id[64];
} ScriptedObjectState;

static ScriptedObjectState gScriptedObjects[PANGEA_SCRIPT_MAX_SCRIPTED_OBJECTS];
static int gScriptedObjectCount;

static void clear_registered_object(RegisteredObject* object);
static const PangeaScriptObjectOps kScriptedOps;

static void release_scripted_object_state(RegisteredObject* object)
{
	if (!object || object->ops != &kScriptedOps)
		return;

	for (int i = 0; i < PANGEA_SCRIPT_MAX_SCRIPTED_OBJECTS; i++)
	{
		if (object->nativeObject != &gScriptedObjects[i] || !gScriptedObjects[i].active)
			continue;
		gScriptedObjects[i].active = false;
		if (gScriptedObjectCount > 0)
			gScriptedObjectCount--;
		return;
	}
}

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

static bool ScriptedGetVelocity(void* nativeObject, PangeaScriptVector3* outVelocity)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !outVelocity) return false;
	*outVelocity = state->velocity;
	return true;
}

static bool ScriptedSetRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !rotation) return false;
	state->rotation = *rotation;
	return true;
}

static bool ScriptedGetRotation(void* nativeObject, PangeaScriptVector3* outRotation)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !outRotation) return false;
	*outRotation = state->rotation;
	return true;
}

static bool ScriptedSetScale(void* nativeObject, float scale)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !isfinite((double)scale) || scale <= 0.0f) return false;
	state->scale = scale;
	return true;
}

static bool ScriptedGetScale(void* nativeObject, float* outScale)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !outScale) return false;
	*outScale = state->scale;
	return true;
}

static bool ScriptedSetAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || animation < 0) return false;
	state->animation = animation;
	state->animationSpeed = speed;
	state->animationBlendSeconds = blendSeconds;
	state->animationName[0] = '\0';
	return true;
}

static bool ScriptedGetAnimation(void* nativeObject, int* outAnimation)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !outAnimation) return false;
	*outAnimation = state->animation;
	return true;
}

static bool ScriptedGetActive(void* nativeObject, bool* outActive)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !outActive) return false;
	*outActive = state->active;
	return true;
}

static bool ScriptedGetCollisionEnabled(void* nativeObject, bool* outEnabled)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !outEnabled) return false;
	*outEnabled = state->collisionEnabled;
	return true;
}

static bool ScriptedSetAnimationNamed(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state || !animation || !animation[0]) return false;
	state->animation = -1;
	state->animationSpeed = speed;
	state->animationBlendSeconds = blendSeconds;
	snprintf(state->animationName, sizeof(state->animationName), "%s", animation);
	return true;
}

static bool ScriptedSetCollisionEnabled(void* nativeObject, bool enabled)
{
	ScriptedObjectState* state = (ScriptedObjectState*) nativeObject;
	if (!state) return false;
	state->collisionEnabled = enabled;
	return true;
}

static void ScriptedAdvance(ScriptedObjectState* state, float deltaSeconds)
{
	if (!state || !isfinite((double)deltaSeconds) || deltaSeconds < 0.0f) return;
	state->position.x += state->velocity.x * deltaSeconds;
	state->position.y += state->velocity.y * deltaSeconds;
	state->position.z += state->velocity.z * deltaSeconds;
}

static bool ScriptedDeleteObject(void* nativeObject)
{
	(void) nativeObject;
	return true;
}

static const PangeaScriptObjectOps kScriptedOps = {
	.getPosition = ScriptedGetPosition,
	.getVelocity = ScriptedGetVelocity,
	.getRotation = ScriptedGetRotation,
	.getScale = ScriptedGetScale,
	.getAnimation = ScriptedGetAnimation,
	.getActive = ScriptedGetActive,
	.getCollisionEnabled = ScriptedGetCollisionEnabled,
	.setPosition = ScriptedSetPosition,
	.setVelocity = ScriptedSetVelocity,
	.setRotation = ScriptedSetRotation,
	.setScale = ScriptedSetScale,
	.setAnimation = ScriptedSetAnimation,
	.setAnimationNamed = ScriptedSetAnimationNamed,
	.setCollisionEnabled = ScriptedSetCollisionEnabled,
	.deleteObject = ScriptedDeleteObject
};

static void reset_objects(void)
{
	if (gBackend)
		PangeaScriptBackend_ResetObjectStates(gBackend);
	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
		clear_registered_object(&gRegisteredObjects[i]);
	for (int i = 0; i < PANGEA_SCRIPT_MAX_SCRIPTED_OBJECTS; i++)
		gScriptedObjects[i].active = false;
	gScriptedObjectCount = 0;
	memset(gTriggerContacts, 0, sizeof(gTriggerContacts));
	memset(gDeferredDeletions, 0, sizeof(gDeferredDeletions));
}

static void reset_scripted_objects(void)
{
	if (gBackend)
		PangeaScriptBackend_ResetObjectStates(gBackend);
	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
	{
		if (gRegisteredObjects[i].active && gRegisteredObjects[i].ops == &kScriptedOps)
			clear_registered_object(&gRegisteredObjects[i]);
	}
	memset(gTriggerContacts, 0, sizeof(gTriggerContacts));
	memset(gDeferredDeletions, 0, sizeof(gDeferredDeletions));
}

static void clear_level_settings(void)
{
	memset(gLevelSettings, 0, sizeof(gLevelSettings));
	gLevelSettingCount = 0;
	memset(gLevelAssetDependencies, 0, sizeof(gLevelAssetDependencies));
	gLevelAssetDependencyCount = 0;
	memset(gCustomObjects, 0, sizeof(gCustomObjects));
	gCustomObjectCount = 0;
	memset(gTerrainReplacements, 0, sizeof(gTerrainReplacements));
	gTerrainReplacementCount = 0;
	memset(gMapReplacements, 0, sizeof(gMapReplacements));
	gMapReplacementCount = 0;
	memset(gSplineReplacements, 0, sizeof(gSplineReplacements));
	gSplineReplacementCount = 0;
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
	object->enabled = false;
	object->destroying = false;
	object->nativeObject = NULL;
	object->ops = NULL;
	object->source = (PangeaScriptObjectSource){0};
	object->owner = (PangeaScriptObjectHandle){0};
	object->hasOwner = false;
	memset(object->objectTypeStorage, 0, sizeof(object->objectTypeStorage));
	memset(object->tagStorage, 0, sizeof(object->tagStorage));
	object->objectType = NULL;
	object->tagCount = 0;
	memset(object->tags, 0, sizeof(object->tags));
	object->generation++;
	if (object->generation == 0)
		object->generation = 1;
}

static RegisteredObject* resolve_registered_object(PangeaScriptObjectHandle handle)
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

static RegisteredObject* resolve_object(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_registered_object(handle);
	return object && object->enabled ? object : NULL;
}

static bool is_supported_object_event(const char* event)
{
	static const char* const events[] = {
		"spawn",
		"update",
		"triggerEnter",
		"triggerStay",
		"triggerExit",
		"animationEvent",
		"animationComplete",
		"activate",
		"deactivate",
		"streamIn",
		"streamOut",
		"checkpointReset",
		"destroy",
	};

	if (!event || !event[0])
		return false;
	for (int i = 0; i < (int)(sizeof(events) / sizeof(events[0])); i++)
		if (strcmp(event, events[i]) == 0)
			return true;
	return false;
}

static bool object_event_allows_position_fallback(const char* event)
{
	static const char* const lifecycleEvents[] = {
		"activate",
		"deactivate",
		"streamIn",
		"streamOut",
		"checkpointReset",
		"destroy",
	};

	if (!event)
		return false;
	for (int i = 0; i < (int)(sizeof(lifecycleEvents) / sizeof(lifecycleEvents[0])); i++)
		if (strcmp(event, lifecycleEvents[i]) == 0)
			return true;
	return false;
}

static void configure_registered_object(RegisteredObject* object, const PangeaScriptObjectRegistration* registration, bool preserveSource)
{
	object->active = true;
	object->enabled = true;
	object->nativeObject = registration->nativeObject;
	object->ops = registration->ops;
	if (!preserveSource)
		object->source = (PangeaScriptObjectSource){0};
	memset(object->objectTypeStorage, 0, sizeof(object->objectTypeStorage));
	if (registration->objectType && copy_string(object->objectTypeStorage, sizeof(object->objectTypeStorage), registration->objectType))
		object->objectType = object->objectTypeStorage;
	else
		object->objectType = NULL;
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
	memset(object->tagStorage, 0, sizeof(object->tagStorage));
	for (int i = 0; i < registration->tagCount; i++)
	{
		if (copy_string(object->tagStorage[i], sizeof(object->tagStorage[i]), registration->tags[i]))
			object->tags[i] = object->tagStorage[i];
	}
	if (object->generation == 0)
		object->generation = 1;
}

static void assign_spawned_object_owner(PangeaScriptObjectHandle handle)
{
	RegisteredObject* child;
	RegisteredObject* owner;

	if (!gHasCurrentCallbackObject)
		return;

	child = resolve_registered_object(handle);
	owner = resolve_registered_object(gCurrentCallbackObject);
	if (!child || !owner || child == owner)
		return;

	child->owner = gCurrentCallbackObject;
	child->hasOwner = true;
}

static void cleanup_owned_children(PangeaScriptObjectHandle ownerHandle)
{
	PangeaScriptObjectHandle children[PANGEA_SCRIPT_MAX_OBJECTS];
	PangeaScriptFrameContext frameContext = {0};
	int childCount = 0;

	if (gHasCurrentCallbackFrame)
		frameContext = gCurrentCallbackFrame;

	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
	{
		RegisteredObject* object = &gRegisteredObjects[i];
		if (!object->active || !object->hasOwner || object->owner.id != ownerHandle.id ||
			object->owner.generation != ownerHandle.generation)
			continue;

		children[childCount++] = (PangeaScriptObjectHandle){
			.id = i + 1,
			.generation = object->generation
		};
	}

	for (int i = 0; i < childCount; i++)
		(void)PangeaScript_ApplyObjectLifecycle(children[i], &frameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
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
		return;
	if (status != PANGEA_SCRIPT_OK && message[0])
		snprintf(gLastRuntimeError, sizeof(gLastRuntimeError), "%s", message);
	if (status == PANGEA_SCRIPT_OK && message[0] == '\0')
	{
		gLastError[0] = '\0';
		return;
	}

	snprintf(gLastError, sizeof(gLastError), "%s", message);
}

static void set_diagnostic_error(PangeaScriptStatus status, const char* message)
{
	gLastStatus = status;
	if (status != PANGEA_SCRIPT_OK && status != PANGEA_SCRIPT_FILE_NOT_FOUND)
		gErrorCount++;
	if (!message)
		return;
	if (status != PANGEA_SCRIPT_OK && message[0])
		snprintf(gLastRuntimeError, sizeof(gLastRuntimeError), "%s", message);
	if (status == PANGEA_SCRIPT_OK && message[0] == '\0')
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
		set_error(status, "Lua runtime could not be created; script execution is unavailable");
	else
		set_error(status, "Script execution failed");
}

static PangeaScriptStatus reject_networked_execution(void)
{
	if (!gNetworkedMode)
		return PANGEA_SCRIPT_OK;
	set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Networked scripting is disabled until authority and deterministic synchronization are available");
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

static void trace_hash_byte(unsigned char byte)
{
	gCommandTrace.hash = (gCommandTrace.hash ^ byte) * 0x01000193u;
}

static void trace_hash_u32(uint32_t value)
{
	for (int byte = 0; byte < 4; byte++)
	{
		trace_hash_byte((unsigned char)(value & 0xffu));
		value >>= 8;
	}
}

static const char* command_application_phase(const char* commandId)
{
	if (!commandId)
		return "unknown";
	for (size_t index = 0; index < sizeof(gCommandDescriptors) / sizeof(gCommandDescriptors[0]); index++)
	{
		if (strcmp(gCommandDescriptors[index].id, commandId) == 0)
			return gCommandDescriptors[index].applicationPhase;
	}
	return "unknown";
}

static const char* lifecycle_application_phase(const char* eventId)
{
	if (!eventId)
		return "unknown";
	if (strcmp(eventId, "levelLoad") == 0 || strcmp(eventId, "levelUnload") == 0)
		return "level";
	if (strcmp(eventId, "save") == 0 || strcmp(eventId, "load") == 0)
		return "persistence";
	if (strcmp(eventId, "terrainItem") == 0 || strcmp(eventId, "splineItem") == 0 || strcmp(eventId, "mapItem") == 0)
		return "constructor";
	if (strcmp(eventId, "trigger") == 0 || strcmp(eventId, "pickup") == 0 || strcmp(eventId, "weaponHit") == 0)
		return "interaction";
	if (strcmp(eventId, "damage") == 0 || strcmp(eventId, "damageApplied") == 0)
		return "damage";
	if (strcmp(eventId, "modeTransition") == 0)
		return "mode";
	if (strcmp(eventId, "gameStart") == 0 || strcmp(eventId, "levelStart") == 0 ||
		strcmp(eventId, "levelComplete") == 0 || strcmp(eventId, "gameShutdown") == 0 ||
		strcmp(eventId, "frame") == 0)
		return "callback";
	if (strncmp(eventId, "on", 2) == 0)
		return "callback";
	for (size_t index = 0; index < sizeof(gObjectEventDescriptors) / sizeof(gObjectEventDescriptors[0]); index++)
	{
		if (strcmp(gObjectEventDescriptors[index].id, eventId) == 0)
			return gObjectEventDescriptors[index].applicationPhase;
	}
	return "unknown";
}

static void record_command(const char* commandId, PangeaScriptObjectHandle handle, PangeaScriptStatus status)
{
	const char* applicationPhase = command_application_phase(commandId);
	uint32_t order = gCommandTrace.commandCount;
	if (gCommandTrace.commandCount == UINT32_MAX)
	{
		gCommandTrace.overflow = true;
		return;
	}

	gCommandTrace.commandCount++;
	if (gCommandTrace.entryCount < PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY)
	{
		PangeaScriptCommandTraceEntry* entry = &gCommandTraceEntries[gCommandTrace.entryCount];
		snprintf(entry->commandId, sizeof(entry->commandId), "%s", commandId ? commandId : "");
		snprintf(entry->applicationPhase, sizeof(entry->applicationPhase), "%s", applicationPhase);
		entry->order = order;
		entry->target = handle;
		entry->status = status;
		gCommandTrace.entryCount++;
	}
	else
	{
		gCommandTrace.overflow = true;
	}
	trace_hash_byte(0x01u);
	if (commandId)
	{
		for (const unsigned char* cursor = (const unsigned char*)commandId; *cursor; cursor++)
			trace_hash_byte(*cursor);
	}
	trace_hash_byte(0x00u);
	for (const unsigned char* cursor = (const unsigned char*)applicationPhase; *cursor; cursor++)
		trace_hash_byte(*cursor);
	trace_hash_byte(0x00u);
	trace_hash_u32(order);
	trace_hash_u32((uint32_t)handle.id);
	trace_hash_u32(handle.generation);
	trace_hash_u32((uint32_t)status);
}

void PangeaScript_RecordCommand(const char* commandId, PangeaScriptObjectHandle target, PangeaScriptStatus status)
{
	record_command(commandId, target, status);
}

void PangeaScript_RecordLifecycleEvent(const char* eventId, PangeaScriptObjectHandle target, PangeaScriptStatus status)
{
	const char* applicationPhase = lifecycle_application_phase(eventId);
	uint32_t order = gLifecycleTrace.eventCount;
	if (gLifecycleTrace.eventCount == UINT32_MAX)
	{
		gLifecycleTrace.overflow = true;
		return;
	}

	gLifecycleTrace.eventCount++;
	if (gLifecycleTrace.entryCount < PANGEA_SCRIPT_LIFECYCLE_TRACE_CAPACITY)
	{
		PangeaScriptLifecycleTraceEntry* entry = &gLifecycleTraceEntries[gLifecycleTrace.entryCount];
		snprintf(entry->eventId, sizeof(entry->eventId), "%s", eventId ? eventId : "");
		snprintf(entry->applicationPhase, sizeof(entry->applicationPhase), "%s", applicationPhase);
		entry->order = order;
		entry->target = target;
		entry->status = status;
		gLifecycleTrace.entryCount++;
	}
	else
	{
		gLifecycleTrace.overflow = true;
	}
}

static bool copy_string(char* dest, size_t destSize, const char* source)
{
	if (!dest || destSize == 0 || !source || !source[0])
		return false;

	int written = snprintf(dest, destSize, "%s", source);
	return written >= 0 && (size_t)written < destSize;
}

static char* read_text_file(const char* path, long* outSize)
{
	FILE* file = fopen(path, "rb");
	char rootedPath[PANGEA_SCRIPT_PATH_CAPACITY + 1];
	if (!file && path && path[0] != '/')
	{
		int written = snprintf(rootedPath, sizeof(rootedPath), "/%s", path);
		if (written > 0 && (size_t)written < sizeof(rootedPath))
			file = fopen(rootedPath, "rb");
	}
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
	PangeaScriptFrameContext shutdownFrame = {0};
	PangeaScriptGameInfo effectiveGameInfo;

	if (!gameInfo || !gameInfo->gameId || !gameInfo->gameId[0] || !gameInfo->gameName || !gameInfo->gameName[0])
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_Init received incomplete game info");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	effectiveGameInfo = *gameInfo;
	if (!effectiveGameInfo.loadPersistent)
		effectiveGameInfo.loadPersistent = default_load_persistent;
	if (!effectiveGameInfo.savePersistent)
		effectiveGameInfo.savePersistent = default_save_persistent;

	(void)PangeaScript_ApplyObjectLifecycleToAll(&shutdownFrame, PANGEA_SCRIPT_OBJECT_DESTROY);
	gGameInfo = effectiveGameInfo;
	gRuntimeFingerprint = compute_runtime_fingerprint(gGameInfo.gameId, NULL, 0);
	if (gBackend)
	{
		PangeaScriptBackend_Destroy(gBackend);
		gBackend = NULL;
	}
	gBackend = PangeaScriptBackend_Create(&effectiveGameInfo);
	if (!gBackend)
	{
		gInitialized = false;
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "PangeaScript_Init could not create the scripting backend");
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	gInitialized = true;
	gScriptLoaded = false;
	gErrorCount = 0;
	gLastError[0] = '\0';
	gLastRuntimeError[0] = '\0';
	gItemRemapCount = 0;
	memset(gNativeItems, 0, sizeof(gNativeItems));
	gNativeItemCount = 0;
	clear_level_settings();
	gBudgetExceededCount = 0;
	gHooksCalledCount = 0;
	gConsecutiveHookFailures = 0;
	gScriptsDisabled = false;
	gNetworkedMode = false;
	gCurrentCallbackFrame = (PangeaScriptFrameContext){0};
	gHasCurrentCallbackFrame = false;
	gLastFrameContext = (PangeaScriptFrameContext){0};
	gHasLastFrameContext = false;
	PangeaScript_ResetCommandTrace();
	PangeaScript_ResetLifecycleTrace();
	reset_objects();
	if (gStartupScriptPath[0])
	{
		PangeaScriptStatus startupStatus = PangeaScript_Reload();
		if (startupStatus != PANGEA_SCRIPT_OK && startupStatus != PANGEA_SCRIPT_FILE_NOT_FOUND)
			set_error(startupStatus, "Configured startup script could not be loaded during initialization");
	}
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

void PangeaScript_Shutdown(void)
{
	PangeaScriptFrameContext shutdownFrame = {0};
	(void)PangeaScript_ApplyObjectLifecycleToAll(&shutdownFrame, PANGEA_SCRIPT_OBJECT_DESTROY);
	gInitialized = false;
	gScriptLoaded = false;
	gItemRemapCount = 0;
	memset(gNativeItems, 0, sizeof(gNativeItems));
	gNativeItemCount = 0;
	clear_level_settings();
	gBudgetExceededCount = 0;
	gHooksCalledCount = 0;
	gConsecutiveHookFailures = 0;
	gScriptsDisabled = false;
	gNetworkedMode = false;
	gCurrentCallbackFrame = (PangeaScriptFrameContext){0};
	gHasCurrentCallbackFrame = false;
	gLastFrameContext = (PangeaScriptFrameContext){0};
	gHasLastFrameContext = false;
	PangeaScript_ResetCommandTrace();
	PangeaScript_ResetLifecycleTrace();
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
	return gScriptLoaded && gBackend != NULL && !gNetworkedMode;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void PangeaScript_SetNetworkedMode(bool networked)
{
	gNetworkedMode = networked;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_IsNetworkedMode(void)
{
	return gNetworkedMode;
}

PangeaScriptStatus PangeaScript_SetStartupScript(const char* path)
{
	if (!path || !path[0])
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Startup script path is empty");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!is_safe_script_path(path))
	{
		set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Invalid script path: must be relative and cannot contain traversal");
		return PANGEA_SCRIPT_CONFIG_ERROR;
	}
	if (!copy_string(gStartupScriptPath, sizeof(gStartupScriptPath), path))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Startup script path is empty");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!gInitialized)
		return PANGEA_SCRIPT_OK;

	return PangeaScript_Reload();
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
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
	gRuntimeFingerprint = compute_runtime_fingerprint(gGameInfo.gameId, (const unsigned char*)script, (size_t)scriptSize);

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

		gConsecutiveHookFailures = 0;
		gScriptsDisabled = false;
		gLastError[0] = '\0';
		gLastRuntimeError[0] = '\0';
		set_error(PANGEA_SCRIPT_OK, "");
		return PANGEA_SCRIPT_OK;
	}

	free(script);
	set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Lua runtime is not linked; script execution is unavailable");
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

static bool ensure_startup_script_loaded(void)
{
	if (gScriptLoaded || !gInitialized || !gStartupScriptPath[0])
		return gScriptLoaded;
	return PangeaScript_Reload() == PANGEA_SCRIPT_OK;
}

const char* PangeaScript_GetLastError(void)
{
	return gLastError;
}

void PangeaScript_ClearLastError(void)
{
	gLastStatus = PANGEA_SCRIPT_OK;
	gLastError[0] = '\0';
	gLastRuntimeError[0] = '\0';
}

int PangeaScript_GetErrorCount(void)
{
	return gErrorCount;
}

PangeaScriptStatus PangeaScript_GetLastStatus(void)
{
	return gLastStatus;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
PangeaScriptStatus PangeaScript_LoadLevelConfig(int levelNum)
{
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

	char errorMsg[512];
	errorMsg[0] = '\0';

	PangeaConfig* parsedConfig = (PangeaConfig*)calloc(1, sizeof(*parsedConfig));
	if (!parsedConfig)
	{
		free(config);
		set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Could not allocate level configuration");
		return PANGEA_SCRIPT_CONFIG_ERROR;
	}
	PangeaScriptStatus parseStatus = PangeaScript_ParseConfig(config, levelNum, parsedConfig, errorMsg, sizeof(errorMsg));
	if (parseStatus != PANGEA_SCRIPT_OK)
	{
		free(parsedConfig);
		free(config);
		set_error(parseStatus, errorMsg);
		return parseStatus;
	}

	bool found = parsedConfig->level.hasConfig;
	const char* scriptPath = parsedConfig->level.scriptPath;
	if (found && scriptPath[0] && !is_script_data_path(scriptPath))
	{
		free(parsedConfig);
		free(config);
		set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Invalid script path: path traversal detected");
		return PANGEA_SCRIPT_CONFIG_ERROR;
	}

	PangeaScriptFrameContext resetFrame = {
		.levelNum = levelNum,
		.frameNum = 0,
		.deltaSeconds = 0.0f,
		.levelTimeSeconds = 0.0f,
	};
	(void)PangeaScript_ApplyObjectLifecycleToAll(&resetFrame, PANGEA_SCRIPT_OBJECT_DESTROY);
	gItemRemapCount = 0;
	clear_level_settings();
	reset_scripted_objects();
	gParsedConfig = *parsedConfig;
	free(parsedConfig);
	scriptPath = gParsedConfig.level.scriptPath;

	if (found)
	{
		gItemRemapCount = gParsedConfig.level.itemRemapCount;
		for (int i = 0; i < gItemRemapCount; i++)
		{
			gItemRemaps[i] = gParsedConfig.level.itemRemaps[i];
		}

		gLevelSettingCount = gParsedConfig.level.levelSettingCount;
		for (int i = 0; i < gLevelSettingCount; i++)
		{
			gLevelSettings[i] = gParsedConfig.level.levelSettings[i];
		}

		gLevelAssetDependencyCount = gParsedConfig.level.assetDependencyCount;
		for (int i = 0; i < gLevelAssetDependencyCount; i++)
		{
			gLevelAssetDependencies[i] = gParsedConfig.level.assetDependencies[i];
		}

		gCustomObjectCount = gParsedConfig.level.customObjectCount;
		for (int i = 0; i < gCustomObjectCount; i++)
		{
			gCustomObjects[i] = gParsedConfig.level.customObjects[i];
		}
		gTerrainReplacementCount = gParsedConfig.level.terrainReplacementCount;
		for (int i = 0; i < gTerrainReplacementCount; i++)
			gTerrainReplacements[i] = gParsedConfig.level.terrainReplacements[i];
		gMapReplacementCount = gParsedConfig.level.mapReplacementCount;
		for (int i = 0; i < gMapReplacementCount; i++)
			gMapReplacements[i] = gParsedConfig.level.mapReplacements[i];
		gSplineReplacementCount = gParsedConfig.level.splineReplacementCount;
		for (int i = 0; i < gSplineReplacementCount; i++)
			gSplineReplacements[i] = gParsedConfig.level.splineReplacements[i];
	}

	if (found && scriptPath[0])
	{
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

int PangeaScript_GetCustomObjectDefinitionCount(void)
{
	return gCustomObjectCount;
}

const PangeaScriptCustomObjectDefinition* PangeaScript_GetCustomObjectDefinition(const char* id)
{
	if (!id || !id[0])
		return NULL;

	for (int i = 0; i < gCustomObjectCount; i++)
	{
		if (strcmp(gCustomObjects[i].id, id) == 0)
			return &gCustomObjects[i];
	}
	return NULL;
}

const PangeaScriptTerrainReplacement* PangeaScript_GetTerrainReplacement(int itemIndex, int nativeType, float x, float z)
{
	for (int i = 0; i < gTerrainReplacementCount; i++)
	{
		const PangeaScriptTerrainReplacement* replacement = &gTerrainReplacements[i];
		float dx = replacement->x - x;
		float dz = replacement->z - z;
		if (replacement->itemIndex == itemIndex && replacement->nativeType == nativeType &&
			dx > -0.5f && dx < 0.5f && dz > -0.5f && dz < 0.5f)
			return replacement;
	}
	return NULL;
}

const PangeaScriptMapReplacement* PangeaScript_GetMapReplacement(int itemIndex, int nativeType, float x, float y)
{
	for (int i = 0; i < gMapReplacementCount; i++)
	{
		const PangeaScriptMapReplacement* replacement = &gMapReplacements[i];
		float dx = replacement->x - x;
		float dy = replacement->y - y;
		if (replacement->itemIndex == itemIndex && replacement->nativeType == nativeType &&
			dx > -0.5f && dx < 0.5f && dy > -0.5f && dy < 0.5f)
			return replacement;
	}
	return NULL;
}

const PangeaScriptSplineReplacement* PangeaScript_GetSplineReplacement(int splineNum, int itemIndex, int nativeType, float placement)
{
	for (int i = 0; i < gSplineReplacementCount; i++)
	{
		const PangeaScriptSplineReplacement* replacement = &gSplineReplacements[i];
		float delta = replacement->placement - placement;
		if (replacement->splineNum == splineNum && replacement->itemIndex == itemIndex &&
			replacement->nativeType == nativeType && delta > -0.0001f && delta < 0.0001f)
			return replacement;
	}
	return NULL;
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

static const char* lifecycle_event_for_hook(PangeaScriptHook hook)
{
	switch (hook)
	{
		case PANGEA_SCRIPT_HOOK_GAME_START: return "gameStart";
		case PANGEA_SCRIPT_HOOK_LEVEL_LOAD: return "levelLoad";
		case PANGEA_SCRIPT_HOOK_LEVEL_START: return "levelStart";
		case PANGEA_SCRIPT_HOOK_FRAME: return "frame";
		case PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE: return "levelComplete";
		case PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD: return "levelUnload";
		case PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN: return "gameShutdown";
		case PANGEA_SCRIPT_HOOK_SAVE: return "save";
		case PANGEA_SCRIPT_HOOK_LOAD: return "load";
		default: return NULL;
	}
}

PangeaScriptStatus PangeaScript_CallLevelHook(PangeaScriptHook hook, const PangeaScriptLevelContext* context)
{
	PangeaScriptStatus status = PANGEA_SCRIPT_OK;
	const char* lifecycleEvent = lifecycle_event_for_hook(hook);

	if (!gInitialized)
	{
		set_error(PANGEA_SCRIPT_NOT_ENABLED, "Scripting host is not initialized");
		return PANGEA_SCRIPT_NOT_ENABLED;
	}
	if (hook == PANGEA_SCRIPT_HOOK_LEVEL_LOAD)
	{
		PangeaScriptFrameContext lifecycleContext = {
			.levelNum = context ? context->levelNum : 0,
		};
		if (gBackend && gScriptLoaded && !gScriptsDisabled && !gNetworkedMode)
			(void) PangeaScript_ApplyObjectLifecycleToAll(&lifecycleContext, PANGEA_SCRIPT_OBJECT_DESTROY);
		reset_objects();
	}
	status = reject_networked_execution();
	if (status != PANGEA_SCRIPT_OK)
		goto level_cleanup;
	if (gScriptsDisabled)
	{
		if (!gLastError[0])
			set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Scripting is disabled after repeated runtime failures");
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
		goto level_cleanup;
	}
	gHooksCalledCount++;
	if (!gScriptLoaded && !ensure_startup_script_loaded())
	{
		if (!gLastError[0])
			set_error(PANGEA_SCRIPT_FILE_NOT_FOUND, "No startup script is loaded");
		status = PANGEA_SCRIPT_FILE_NOT_FOUND;
		goto level_cleanup;
	}
	if (!gBackend)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Scripting backend is unavailable");
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
		goto level_cleanup;
	}

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	if (context)
	{
		gCurrentCallbackFrame.levelNum = context->levelNum;
		gCurrentCallbackFrame.frameNum = 0;
		gCurrentCallbackFrame.deltaSeconds = 0.0f;
		gCurrentCallbackFrame.levelTimeSeconds = 0.0f;
		gHasCurrentCallbackFrame = true;
	}
	status = PangeaScriptBackend_CallLevelHook(gBackend, hook, context, backendError, (int)sizeof(backendError));
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);

	level_cleanup:
	if (hook == PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD)
	{
		PangeaScriptFrameContext lifecycleContext = {
			.levelNum = context ? context->levelNum : 0,
		};
		if (gScriptLoaded && gBackend && !gScriptsDisabled && !gNetworkedMode)
		{
			PangeaScriptStatus cleanupStatus = PangeaScript_ApplyObjectLifecycleToAll(&lifecycleContext, PANGEA_SCRIPT_OBJECT_DESTROY);
			if (status == PANGEA_SCRIPT_OK && cleanupStatus != PANGEA_SCRIPT_OK)
				status = cleanupStatus;
		}
		reset_objects();
	}
	if (lifecycleEvent)
		PangeaScript_RecordLifecycleEvent(lifecycleEvent, (PangeaScriptObjectHandle){0}, status);
	return status;
}

PangeaScriptStatus PangeaScript_CallNativeSaveHook(int levelNum, int saveSlot, bool loading)
{
	char slotName[32];
	PangeaScriptLevelContext context;
	snprintf(slotName, sizeof(slotName), "slot-%d", saveSlot);
	context.levelNum = levelNum;
	context.levelName = slotName;
	return PangeaScript_CallLevelHook(loading ? PANGEA_SCRIPT_HOOK_LOAD : PANGEA_SCRIPT_HOOK_SAVE, &context);
}

PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* context)
{
	if (!gInitialized)
	{
		set_error(PANGEA_SCRIPT_NOT_ENABLED, "Scripting host is not initialized");
		return PANGEA_SCRIPT_NOT_ENABLED;
	}
	if (!context)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Frame hook context is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (reject_networked_execution() != PANGEA_SCRIPT_OK)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded && !ensure_startup_script_loaded())
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	if (gHasLastFrameContext)
	{
		const char* previousMode = gLastFrameContext.mode;
		const char* currentMode = context->mode;
		const bool modeChanged = (previousMode == NULL) != (currentMode == NULL) ||
			(previousMode && currentMode && strcmp(previousMode, currentMode) != 0);
		const bool modeStateChanged = gLastFrameContext.hasModeState != context->hasModeState ||
			(context->hasModeState && (gLastFrameContext.modePhase != context->modePhase ||
			gLastFrameContext.modeWave != context->modeWave ||
			gLastFrameContext.modeTimer != context->modeTimer));
		if (modeChanged || modeStateChanged)
			PangeaScript_RecordLifecycleEvent("modeTransition", (PangeaScriptObjectHandle){0}, PANGEA_SCRIPT_OK);
	}
	if (context)
	{
		gCurrentCallbackFrame = *context;
		gLastFrameContext = *context;
		gHasLastFrameContext = true;
		gHasCurrentCallbackFrame = true;
	}
	PangeaScriptStatus status = PangeaScriptBackend_CallFrameHook(gBackend, context, backendError, (int)sizeof(backendError));
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallTerrainItemHook(PangeaScriptTerrainItemContext* context)
{
	if (!context)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Terrain item hook context is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (!gGameInfo.capabilities.terrainItems)
	{
		set_error(PANGEA_SCRIPT_INCOMPATIBLE_ITEM, "Terrain item scripting is unavailable for this game");
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	if (reject_networked_execution() != PANGEA_SCRIPT_OK)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded && !ensure_startup_script_loaded())
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallTerrainItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	PangeaScript_RecordLifecycleEvent("terrainItem", (PangeaScriptObjectHandle){0}, status);
	return status;
}

PangeaScriptStatus PangeaScript_CallSplineItemHook(PangeaScriptSplineItemContext* context)
{
	if (!context)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Spline item hook context is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (!gGameInfo.capabilities.splineItems)
	{
		set_error(PANGEA_SCRIPT_INCOMPATIBLE_ITEM, "Spline item scripting is unavailable for this game");
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	if (reject_networked_execution() != PANGEA_SCRIPT_OK)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded && !ensure_startup_script_loaded())
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallSplineItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	PangeaScript_RecordLifecycleEvent("splineItem", (PangeaScriptObjectHandle){0}, status);
	return status;
}

PangeaScriptStatus PangeaScript_CallMapItemHook(PangeaScriptMapItemContext* context)
{
	if (!context)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Map item hook context is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (!gGameInfo.capabilities.mapItems)
	{
		set_error(PANGEA_SCRIPT_INCOMPATIBLE_ITEM, "Map item scripting is unavailable for this game");
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	if (reject_networked_execution() != PANGEA_SCRIPT_OK)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded && !ensure_startup_script_loaded())
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallMapItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	PangeaScript_RecordLifecycleEvent("mapItem", (PangeaScriptObjectHandle){0}, status);
	return status;
}

static PangeaScriptStatus call_object_event(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, const char* event, bool hasEventValue, int eventValue, PangeaScriptObjectHandle other, bool hasOther, unsigned int sideBits, PangeaScriptObjectFrameResult* outResult)
{
	if (!frameContext || !outResult || !is_supported_object_event(event))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectEvent received an incomplete context, result, or unsupported event");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	outResult->hasPositionOffset = false;
	outResult->positionOffset.x = 0.0f;
	outResult->positionOffset.y = 0.0f;
	outResult->positionOffset.z = 0.0f;

	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (reject_networked_execution() != PANGEA_SCRIPT_OK)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded && !ensure_startup_script_loaded())
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	RegisteredObject* object = resolve_registered_object(handle);
	if (!object)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame received an unknown or stale object handle");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!object->enabled && strcmp(event, "activate") != 0 && strcmp(event, "streamIn") != 0 &&
		strcmp(event, "deactivate") != 0 && strcmp(event, "streamOut") != 0 &&
		strcmp(event, "checkpointReset") != 0 && strcmp(event, "destroy") != 0)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectEvent received a deactivated object handle");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	if (!object->ops || !object->ops->getPosition)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectEvent received an object without position access");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	PangeaScriptVector3 position = {0};
	if (!object->ops->getPosition(object->nativeObject, &position) &&
		!object_event_allows_position_fallback(event))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectEvent could not read the object's current position");
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
		.objectType = object->objectType,
		.tags = object->tags,
		.tagCount = object->tagCount,
		.event = event,
		.hasEventValue = hasEventValue,
		.eventValue = eventValue,
		.other = other,
		.hasOther = hasOther,
		.sideBits = sideBits,
	};

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	const bool destroying = strcmp(event, "destroy") == 0;
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	const bool previousObjectValid = gHasCurrentCallbackObject;
	const PangeaScriptObjectHandle previousObject = gCurrentCallbackObject;
	PangeaScriptObjectFrameResult* const previousObjectFrameResult = gCurrentObjectFrameResult;
	const bool previousDestroying = object->destroying;
	gCurrentCallbackFrame = *frameContext;
	gHasCurrentCallbackFrame = true;
	gCurrentCallbackObject = handle;
	gHasCurrentCallbackObject = true;
	gCurrentObjectFrameResult = strcmp(event, "update") == 0 ? outResult : NULL;
	if (destroying)
		object->destroying = true;
	PangeaScriptStatus status = PangeaScriptBackend_CallObjectFrameHook(gBackend, &context, outResult, backendError, (int)sizeof(backendError));
	PangeaScript_RecordLifecycleEvent(event, handle, status);
	if (resolve_registered_object(handle) == object)
		object->destroying = previousDestroying;
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	gCurrentCallbackObject = previousObject;
	gHasCurrentCallbackObject = previousObjectValid;
	gCurrentObjectFrameResult = previousObjectFrameResult;
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallObjectFrame(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, PangeaScriptObjectFrameResult* outResult)
{
	PangeaScriptStatus status = call_object_event(handle, frameContext, "update", false, 0, (PangeaScriptObjectHandle){0}, false, 0, outResult);
	RegisteredObject* object = resolve_registered_object(handle);
	if (status == PANGEA_SCRIPT_OK && object && object->ops == &kScriptedOps)
		ScriptedAdvance((ScriptedObjectState*)object->nativeObject, frameContext ? frameContext->deltaSeconds : 0.0f);
	return status;
}

PangeaScriptStatus PangeaScript_CallObjectEvent(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, const char* event)
{
	PangeaScriptObjectFrameResult ignoredResult;
	if (!event || !event[0])
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object event name is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	return call_object_event(handle, frameContext, event, false, 0, (PangeaScriptObjectHandle){0}, false, 0, &ignoredResult);
}

PangeaScriptStatus PangeaScript_CallObjectEventWithValue(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, const char* event, int eventValue)
{
	PangeaScriptObjectFrameResult ignoredResult;
	if (!event || !event[0])
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object event name is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	return call_object_event(handle, frameContext, event, true, eventValue, (PangeaScriptObjectHandle){0}, false, 0, &ignoredResult);
}

static const char* lifecycle_event_name(PangeaScriptObjectLifecycle lifecycle)
{
	switch (lifecycle)
	{
		case PANGEA_SCRIPT_OBJECT_ACTIVATE: return "activate";
		case PANGEA_SCRIPT_OBJECT_DEACTIVATE: return "deactivate";
		case PANGEA_SCRIPT_OBJECT_STREAM_IN: return "streamIn";
		case PANGEA_SCRIPT_OBJECT_STREAM_OUT: return "streamOut";
		case PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET: return "checkpointReset";
		case PANGEA_SCRIPT_OBJECT_DESTROY: return "destroy";
		default: return NULL;
	}
}

PangeaScriptStatus PangeaScript_ApplyObjectLifecycle(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, PangeaScriptObjectLifecycle lifecycle)
{
	const char* event = lifecycle_event_name(lifecycle);
	if (!event || !frameContext)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object lifecycle transition is incomplete");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	if (lifecycle == PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET && gBackend)
	{
		/* Capture the first checkpoint baseline, then restore it on later resets. */
		if (!PangeaScriptBackend_RestoreObjectCheckpointState(gBackend, handle))
			PangeaScriptBackend_CaptureObjectCheckpointState(gBackend, handle);
	}

	PangeaScriptStatus status = PangeaScript_CallObjectEvent(handle, frameContext, event);
	RegisteredObject* object = resolve_registered_object(handle);
	const bool destructive = lifecycle == PANGEA_SCRIPT_OBJECT_STREAM_OUT || lifecycle == PANGEA_SCRIPT_OBJECT_DESTROY;
	if (status != PANGEA_SCRIPT_OK && !destructive)
	{
		if (lifecycle == PANGEA_SCRIPT_OBJECT_STREAM_IN && object)
		{
			if (!PangeaScript_DeleteObject(handle))
				(void)PangeaScript_UnregisterObject(handle);
		}
		return status;
	}

	if (!object && status == PANGEA_SCRIPT_OK)
		return PANGEA_SCRIPT_OK;
	if (!object)
		return status;
	if (lifecycle == PANGEA_SCRIPT_OBJECT_ACTIVATE || lifecycle == PANGEA_SCRIPT_OBJECT_STREAM_IN)
		object->enabled = true;
	else if (lifecycle == PANGEA_SCRIPT_OBJECT_DEACTIVATE)
		object->enabled = false;

	if (lifecycle == PANGEA_SCRIPT_OBJECT_DEACTIVATE)
	{
		if (gBackend)
			PangeaScriptBackend_ClearObjectState(gBackend, handle);
	}
	else if (lifecycle == PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET && gBackend)
	{
		/* Callbacks owned by the pre-reset engine state must not continue. */
		PangeaScriptBackend_ClearObjectResources(gBackend, handle);
	}

	if (destructive)
	{
		if (!PangeaScript_UnregisterObject(handle))
			return status == PANGEA_SCRIPT_OK ? PANGEA_SCRIPT_BAD_ARGUMENT : status;
	}

	return status;
}

PangeaScriptStatus PangeaScript_ApplyObjectLifecycleToAll(const PangeaScriptFrameContext* frameContext, PangeaScriptObjectLifecycle lifecycle)
{
	PangeaScriptObjectHandle handles[PANGEA_SCRIPT_MAX_OBJECTS];
	int count;
	PangeaScriptStatus firstFailure = PANGEA_SCRIPT_OK;
	const bool destructive = lifecycle == PANGEA_SCRIPT_OBJECT_STREAM_OUT || lifecycle == PANGEA_SCRIPT_OBJECT_DESTROY;

	if (!frameContext || !lifecycle_event_name(lifecycle))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object lifecycle broadcast is incomplete");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	count = PangeaScript_GetRegisteredObjectCount();
	if (count > PANGEA_SCRIPT_MAX_OBJECTS)
		count = PANGEA_SCRIPT_MAX_OBJECTS;
	for (int i = 0; i < count; i++)
	{
		if (!PangeaScript_GetRegisteredObjectHandle(i, &handles[i]))
		{
			set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Could not snapshot registered object handles for lifecycle broadcast");
			return PANGEA_SCRIPT_RUNTIME_ERROR;
		}
	}

	for (int i = 0; i < count; i++)
	{
		if (!PangeaScript_ObjectExists(handles[i]))
			continue;
		PangeaScriptStatus status = PangeaScript_ApplyObjectLifecycle(handles[i], frameContext, lifecycle);
		if (status != PANGEA_SCRIPT_OK && firstFailure == PANGEA_SCRIPT_OK)
			firstFailure = status;
		if (status != PANGEA_SCRIPT_OK && !destructive)
			return status;
	}

	if (firstFailure == PANGEA_SCRIPT_OK)
		set_error(PANGEA_SCRIPT_OK, "");
	return firstFailure;
}

static PangeaScriptStatus prepare_gameplay_hook(const void* context, const void* result)
{
	if (!context || !result)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Gameplay hook context and result are required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!gInitialized)
	{
		set_error(PANGEA_SCRIPT_NOT_ENABLED, "Scripting host is not initialized");
		return PANGEA_SCRIPT_NOT_ENABLED;
	}
	if (reject_networked_execution() != PANGEA_SCRIPT_OK)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (gScriptsDisabled || !gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!gScriptLoaded && !ensure_startup_script_loaded())
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	gHooksCalledCount++;
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScript_CallTriggerHook(const PangeaScriptTriggerContext* context, PangeaScriptTriggerResult* outResult)
{
	if (!outResult)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Trigger result is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	memset(outResult, 0, sizeof(*outResult));
	PangeaScriptStatus ready = prepare_gameplay_hook(context, outResult);
	if (ready != PANGEA_SCRIPT_OK) return ready;
	char error[PANGEA_SCRIPT_ERROR_CAPACITY] = {0};
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	if (context)
	{
		gCurrentCallbackFrame.levelNum = context->levelNum;
		gCurrentCallbackFrame.frameNum = 0;
		gCurrentCallbackFrame.deltaSeconds = 0.0f;
		gCurrentCallbackFrame.levelTimeSeconds = 0.0f;
		gHasCurrentCallbackFrame = true;
	}
	PangeaScriptStatus status = PangeaScriptBackend_CallTriggerHook(gBackend, context, outResult, error, sizeof(error));
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	if (status != PANGEA_SCRIPT_OK) set_backend_error(status, error);
	PangeaScript_RecordLifecycleEvent("trigger", context ? context->self : (PangeaScriptObjectHandle){0}, status);
	return status;
}

PangeaScriptStatus PangeaScript_CallPickupHook(const PangeaScriptPickupContext* context, PangeaScriptPickupResult* outResult)
{
	if (!outResult)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Pickup result is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!context)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Pickup context is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (context->pickup.id > 0 && !resolve_object(context->pickup))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Pickup handle is unknown or stale");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (context->player.id > 0 && !resolve_object(context->player))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Pickup player handle is unknown or stale");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	memset(outResult, 0, sizeof(*outResult));
	PangeaScriptStatus ready = prepare_gameplay_hook(context, outResult);
	if (ready != PANGEA_SCRIPT_OK) return ready;
	char error[PANGEA_SCRIPT_ERROR_CAPACITY] = {0};
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	if (context)
	{
		gCurrentCallbackFrame.levelNum = context->levelNum;
		gCurrentCallbackFrame.frameNum = 0;
		gCurrentCallbackFrame.deltaSeconds = 0.0f;
		gCurrentCallbackFrame.levelTimeSeconds = 0.0f;
		gHasCurrentCallbackFrame = true;
	}
	PangeaScriptStatus status = PangeaScriptBackend_CallPickupHook(gBackend, context, outResult, error, sizeof(error));
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	if (status != PANGEA_SCRIPT_OK) set_backend_error(status, error);
	PangeaScript_RecordLifecycleEvent("pickup", context->pickup, status);
	return status;
}

PangeaScriptStatus PangeaScript_CallWeaponHitHook(const PangeaScriptWeaponHitContext* context, PangeaScriptWeaponHitResult* outResult)
{
	if (!outResult)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Weapon-hit result is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!context)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Weapon-hit context is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (context->weapon.id > 0 && !resolve_object(context->weapon))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Weapon handle is unknown or stale");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (context->target.id > 0 && !resolve_object(context->target))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Weapon target handle is unknown or stale");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	memset(outResult, 0, sizeof(*outResult));
	PangeaScriptStatus ready = prepare_gameplay_hook(context, outResult);
	if (ready != PANGEA_SCRIPT_OK) return ready;
	char error[PANGEA_SCRIPT_ERROR_CAPACITY] = {0};
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	if (context)
	{
		gCurrentCallbackFrame.levelNum = context->levelNum;
		gCurrentCallbackFrame.frameNum = 0;
		gCurrentCallbackFrame.deltaSeconds = 0.0f;
		gCurrentCallbackFrame.levelTimeSeconds = 0.0f;
		gHasCurrentCallbackFrame = true;
	}
	PangeaScriptStatus status = PangeaScriptBackend_CallWeaponHitHook(gBackend, context, outResult, error, sizeof(error));
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	if (status != PANGEA_SCRIPT_OK) set_backend_error(status, error);
	PangeaScript_RecordLifecycleEvent("weaponHit", context->target, status);
	return status;
}

PangeaScriptStatus PangeaScript_CallDamageHook(const PangeaScriptDamageContext* context, PangeaScriptDamageResult* outResult)
{
	if (!outResult)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Damage result is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	memset(outResult, 0, sizeof(*outResult));
	if (context)
		outResult->damage = context->damage;
	PangeaScriptStatus ready = prepare_gameplay_hook(context, outResult);
	if (ready != PANGEA_SCRIPT_OK) return ready;
	char error[PANGEA_SCRIPT_ERROR_CAPACITY] = {0};
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	if (context)
	{
		gCurrentCallbackFrame.levelNum = context->levelNum;
		gCurrentCallbackFrame.frameNum = 0;
		gCurrentCallbackFrame.deltaSeconds = 0.0f;
		gCurrentCallbackFrame.levelTimeSeconds = 0.0f;
		gHasCurrentCallbackFrame = true;
	}
	PangeaScriptStatus status = PangeaScriptBackend_CallDamageHook(gBackend, context, outResult, error, sizeof(error));
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	if (status != PANGEA_SCRIPT_OK) set_backend_error(status, error);
	PangeaScript_RecordLifecycleEvent("damage", context ? context->target : (PangeaScriptObjectHandle){0}, status);
	return status;
}

PangeaScriptStatus PangeaScript_CallDamageAppliedHook(const PangeaScriptDamageContext* context)
{
	if (!context)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Damage-applied hook context is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	PangeaScriptStatus ready = prepare_gameplay_hook(context, context);
	if (ready != PANGEA_SCRIPT_OK) return ready;
	char error[PANGEA_SCRIPT_ERROR_CAPACITY] = {0};
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	gCurrentCallbackFrame.levelNum = context->levelNum;
	gCurrentCallbackFrame.frameNum = 0;
	gCurrentCallbackFrame.deltaSeconds = 0.0f;
	gCurrentCallbackFrame.levelTimeSeconds = 0.0f;
	gHasCurrentCallbackFrame = true;
	PangeaScriptStatus status = PangeaScriptBackend_CallDamageAppliedHook(gBackend, context, error, sizeof(error));
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	if (status != PANGEA_SCRIPT_OK) set_backend_error(status, error);
	PangeaScript_RecordLifecycleEvent("damageApplied", context->target, status);
	return status;
}

PangeaScriptStatus PangeaScript_CallPlayerEvent(const PangeaScriptPlayerEventContext* context, const char* event)
{
	if (!context || !event || !event[0])
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Player event context and event name are required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if ((strcmp(event, "onCheckpointReached") == 0 || strcmp(event, "onLapComplete") == 0 || strcmp(event, "onRaceFinish") == 0) && context->eventValue < 0)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Player race event value must be non-negative");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (strcmp(event, "onObjectiveComplete") == 0 && (context->eventValue < 0 || context->eventValue > 2))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Objective outcome must be 0 (win), 1 (loss), or 2 (draw)");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	PangeaScriptStatus ready = prepare_gameplay_hook(context, context);
	if (ready != PANGEA_SCRIPT_OK) return ready;
	char error[PANGEA_SCRIPT_ERROR_CAPACITY] = {0};
	const bool previousFrameValid = gHasCurrentCallbackFrame;
	const PangeaScriptFrameContext previousFrame = gCurrentCallbackFrame;
	gCurrentCallbackFrame.levelNum = context->levelNum;
	gCurrentCallbackFrame.frameNum = 0;
	gCurrentCallbackFrame.deltaSeconds = 0.0f;
	gCurrentCallbackFrame.levelTimeSeconds = 0.0f;
	gHasCurrentCallbackFrame = true;
	PangeaScriptStatus status = PangeaScriptBackend_CallPlayerEvent(gBackend, context, event, error, sizeof(error));
	gCurrentCallbackFrame = previousFrame;
	gHasCurrentCallbackFrame = previousFrameValid;
	if (status != PANGEA_SCRIPT_OK) set_backend_error(status, error);
	PangeaScript_RecordLifecycleEvent(event, context->player, status);
	return status;
}

void PangeaScript_ExpireTriggerContacts(const PangeaScriptFrameContext* frameContext)
{
	if (!frameContext) return;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_TRIGGER_CONTACTS; i++)
	{
		PangeaScriptTriggerContact* contact = &gTriggerContacts[i];
		if (!contact->active) continue;
		if (contact->levelNum == frameContext->levelNum &&
			frameContext->frameNum >= contact->lastFrame &&
			frameContext->frameNum - contact->lastFrame <= 1U)
			continue;
		PangeaScriptObjectFrameResult ignoredResult;
		PangeaScriptObjectHandle self = contact->self;
		PangeaScriptObjectHandle other = contact->other;
		unsigned int sideBits = contact->sideBits;
		contact->active = false;
		(void) call_object_event(self, frameContext, "triggerExit", false, 0, other, other.id > 0, sideBits, &ignoredResult);
	}
}

static PangeaScriptStatus queue_deferred_deletion(PangeaScriptObjectHandle handle, int levelNum)
{
	if (!resolve_object(handle))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Deferred deletion target is unknown or stale");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	for (int i = 0; i < PANGEA_SCRIPT_DEFERRED_ACTION_CAPACITY; i++)
	{
		if (gDeferredDeletions[i].active && same_object_handle(gDeferredDeletions[i].handle, handle))
			return PANGEA_SCRIPT_OK;
	}
	for (int i = 0; i < PANGEA_SCRIPT_DEFERRED_ACTION_CAPACITY; i++)
	{
		if (!gDeferredDeletions[i].active)
		{
			gDeferredDeletions[i] = (PangeaScriptDeferredDeletion){
				.active = true,
				.levelNum = levelNum,
				.handle = handle,
			};
			return PANGEA_SCRIPT_OK;
		}
	}
	set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Deferred action queue is full");
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

PangeaScriptStatus PangeaScript_ApplyDeferredActions(const PangeaScriptFrameContext* frameContext)
{
	PangeaScriptStatus firstFailure = PANGEA_SCRIPT_OK;
	if (!frameContext)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Deferred action frame context is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	for (int i = 0; i < PANGEA_SCRIPT_DEFERRED_ACTION_CAPACITY; i++)
	{
		PangeaScriptDeferredDeletion action = gDeferredDeletions[i];
		if (!action.active)
			continue;
		gDeferredDeletions[i].active = false;
		if (action.levelNum != frameContext->levelNum || !resolve_object(action.handle))
			continue;
		if (!PangeaScript_DeleteObject(action.handle) && firstFailure == PANGEA_SCRIPT_OK)
			firstFailure = PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	if (firstFailure != PANGEA_SCRIPT_OK)
		set_error(firstFailure, "A deferred object action could not be applied");
	else
		set_error(PANGEA_SCRIPT_OK, "");
	return firstFailure;
}

static bool call_object_trigger(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, unsigned int sideBits, bool defaultSolid, PangeaScriptObjectHandle other, int playerNum)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !frameContext || !object->ops || !object->ops->getPosition)
		return defaultSolid;
	PangeaScriptVector3 position;
	if (!object->ops->getPosition(object->nativeObject, &position))
		return defaultSolid;
	PangeaScriptTriggerContext context = {
		.levelNum = frameContext->levelNum,
		.playerNum = playerNum,
		.triggerId = object->objectType,
		.self = handle,
		.other = other,
		.position = position,
		.sideBits = sideBits,
	};
	PangeaScriptTriggerResult result;
	PangeaScriptStatus status = PangeaScript_CallTriggerHook(&context, &result);
	const char* objectEvent = record_trigger_contact(handle, other, sideBits, frameContext) ? "triggerStay" : "triggerEnter";
	PangeaScriptObjectFrameResult objectResult;
	(void) call_object_event(
		handle,
		frameContext,
		objectEvent,
		false,
		0,
		other,
		other.id > 0,
		sideBits,
		&objectResult);
	if (!resolve_object(handle))
		return status == PANGEA_SCRIPT_OK && result.hasSolid ? result.solid : defaultSolid;
	const PangeaScriptCustomObjectDefinition* definition = PangeaScript_GetCustomObjectDefinition(context.triggerId);
	if (definition && definition->collisionPreset == PANGEA_SCRIPT_COLLISION_PICKUP)
	{
		PangeaScriptPickupContext pickupContext = {
			.levelNum = frameContext->levelNum,
			.playerNum = playerNum,
			.pickupId = object->objectType,
			.pickup = handle,
			.player = other,
			.position = position,
		};
		PangeaScriptPickupResult pickupResult;
		if (PangeaScript_CallPickupHook(&pickupContext, &pickupResult) == PANGEA_SCRIPT_OK && pickupResult.hasConsumePickup && pickupResult.consumePickup)
			(void)queue_deferred_deletion(handle, frameContext->levelNum);
	}
	if (status != PANGEA_SCRIPT_OK)
		return defaultSolid;
	if (result.deleteSelf)
		PangeaScript_DeleteObject(handle);
	if (result.deleteOther && PangeaScript_ObjectExists(other))
		PangeaScript_DeleteObject(other);
	return result.hasSolid ? result.solid : defaultSolid;
}

bool PangeaScript_CallObjectTrigger(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, unsigned int sideBits, bool defaultSolid)
{
	return call_object_trigger(handle, frameContext, sideBits, defaultSolid, (PangeaScriptObjectHandle){0}, -1);
}

bool PangeaScript_CallObjectTriggerWithOther(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, unsigned int sideBits, bool defaultSolid, PangeaScriptObjectHandle other)
{
	return call_object_trigger(handle, frameContext, sideBits, defaultSolid, other, other.id > 0 ? 0 : -1);
}

bool PangeaScript_CallObjectTriggerWithOtherAndPlayer(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, unsigned int sideBits, bool defaultSolid, PangeaScriptObjectHandle other, int playerNum)
{
	if (playerNum < -1)
		return defaultSolid;
	return call_object_trigger(handle, frameContext, sideBits, defaultSolid, other, playerNum);
}

void PangeaScript_ResetObjects(void)
{
	reset_objects();
}

PangeaScriptStatus PangeaScript_RegisterObject(const PangeaScriptObjectRegistration* registration, PangeaScriptObjectHandle* outHandle)
{
	if (!registration || !registration->nativeObject || !registration->ops || !registration->ops->getPosition)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object registration is incomplete");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	if (registration->tagCount < 0 || registration->tagCount > PANGEA_SCRIPT_MAX_OBJECT_TAGS)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object registration has an invalid tag count");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (registration->tagCount > 0 && !registration->tags)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object registration has tags but no tag array");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	for (int i = 0; i < registration->tagCount; i++)
	{
		if (!registration->tags[i] || !registration->tags[i][0])
		{
			set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object registration contains an empty tag");
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		}
		if (strlen(registration->tags[i]) >= PANGEA_SCRIPT_OBJECT_TAG_CAPACITY)
		{
			set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object registration contains a tag that is too long");
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		}
	}
	if (registration->objectType && strlen(registration->objectType) >= PANGEA_SCRIPT_OBJECT_TYPE_CAPACITY)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object registration contains an object type that is too long");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	int freeIndex = -1;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
	{
		RegisteredObject* object = &gRegisteredObjects[i];
		if (object->active)
		{
			if (object->nativeObject == registration->nativeObject)
			{
				configure_registered_object(object, registration, true);
				if (outHandle)
				{
					outHandle->id = i + 1;
					outHandle->generation = object->generation;
				}
				set_error(PANGEA_SCRIPT_OK, "");
				return PANGEA_SCRIPT_OK;
			}
			continue;
		}

		if (freeIndex < 0)
			freeIndex = i;
	}

	if (freeIndex < 0)
	{
		set_error(PANGEA_SCRIPT_BUDGET_EXCEEDED, "Registered object capacity exceeded");
		return PANGEA_SCRIPT_BUDGET_EXCEEDED;
	}

	RegisteredObject* object = &gRegisteredObjects[freeIndex];
	configure_registered_object(object, registration, false);
	if (outHandle)
	{
		outHandle->id = freeIndex + 1;
		outHandle->generation = object->generation;
	}
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScript_RegisterScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
	if (!id || !id[0])
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Scripted object ID is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Scripted object position must contain finite coordinates");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (reject_networked_execution() != PANGEA_SCRIPT_OK)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (gGameInfo.spawnScripted)
	{
		PangeaScriptStatus status;
		PangeaScriptFrameContext spawnFrame = gHasCurrentCallbackFrame ? gCurrentCallbackFrame : (PangeaScriptFrameContext){0};
		PangeaScriptObjectHandle spawnedHandle = {0};
		PangeaScriptObjectHandle* callbackHandle = outHandle ? outHandle : &spawnedHandle;
		*callbackHandle = (PangeaScriptObjectHandle){0};
		status = gGameInfo.spawnScripted(id, x, y, z, callbackHandle);
		if (status == PANGEA_SCRIPT_OK && (callbackHandle->id <= 0 || callbackHandle->generation == 0))
		{
			set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Scripted spawn callback returned an invalid object handle");
			return PANGEA_SCRIPT_RUNTIME_ERROR;
		}
		if (status == PANGEA_SCRIPT_OK)
		{
			status = PangeaScript_CallObjectEvent(*callbackHandle, &spawnFrame, "spawn");
			if (status == PANGEA_SCRIPT_OK && !PangeaScript_ObjectExists(*callbackHandle))
				status = PANGEA_SCRIPT_RUNTIME_ERROR;
		}
		if (status != PANGEA_SCRIPT_OK)
		{
			if (PangeaScript_ObjectExists(*callbackHandle))
			{
				if (!PangeaScript_DeleteObject(*callbackHandle))
					(void)PangeaScript_UnregisterObject(*callbackHandle);
			}
			if (outHandle)
				*outHandle = (PangeaScriptObjectHandle){0};
		}
		if (status == PANGEA_SCRIPT_OK)
			assign_spawned_object_owner(*callbackHandle);
		return status;
	}

	int freeIndex = -1;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_SCRIPTED_OBJECTS; i++)
	{
		if (!gScriptedObjects[i].active)
		{
			freeIndex = i;
			break;
		}
	}
	if (freeIndex < 0)
	{
		set_error(PANGEA_SCRIPT_BUDGET_EXCEEDED, "Scripted object capacity exceeded");
		return PANGEA_SCRIPT_BUDGET_EXCEEDED;
	}

	ScriptedObjectState* state = &gScriptedObjects[freeIndex];
	state->active = true;
	gScriptedObjectCount++;
	state->position.x = x;
	state->position.y = y;
	state->position.z = z;
	state->velocity = (PangeaScriptVector3){0};
	state->rotation = (PangeaScriptVector3){0};
	state->scale = 1.0f;
	state->animation = -1;
	state->animationSpeed = 1.0f;
	state->animationBlendSeconds = 0.0f;
	state->collisionEnabled = true;
	state->animationName[0] = '\0';
	snprintf(state->id, sizeof(state->id), "%s", id ? id : "");

	PangeaScriptObjectRegistration reg = {
		.nativeObject = state,
		.ops = &kScriptedOps,
		.objectType = state->id,
		.tags = NULL,
		.tagCount = 0,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL
	};

	PangeaScriptObjectHandle spawnedHandle = {0};
	PangeaScriptObjectHandle* callbackHandle = outHandle ? outHandle : &spawnedHandle;
	PangeaScriptStatus status = PangeaScript_RegisterObject(&reg, callbackHandle);
	if (status != PANGEA_SCRIPT_OK)
		return status;
	if (gBackend && gScriptLoaded)
	{
		PangeaScriptFrameContext spawnFrame = gHasCurrentCallbackFrame ? gCurrentCallbackFrame : (PangeaScriptFrameContext){0};
		status = PangeaScript_CallObjectEvent(*callbackHandle, &spawnFrame, "spawn");
		if (status == PANGEA_SCRIPT_OK && !PangeaScript_ObjectExists(*callbackHandle))
			status = PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	if (status != PANGEA_SCRIPT_OK)
	{
		if (PangeaScript_ObjectExists(*callbackHandle))
		{
			if (!PangeaScript_DeleteObject(*callbackHandle))
				(void) PangeaScript_UnregisterObject(*callbackHandle);
		}
		if (outHandle)
			*outHandle = (PangeaScriptObjectHandle){0};
		return status;
	}
	assign_spawned_object_owner(*callbackHandle);
	return status;
}

bool PangeaScript_UnregisterObject(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_registered_object(handle);
	if (!object)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
		return false;
	}

	cleanup_owned_children(handle);
	if (gBackend)
		PangeaScriptBackend_ClearObjectState(gBackend, handle);
	clear_trigger_contacts_for_handle(handle);
	release_scripted_object_state(object);
	clear_registered_object(object);
	set_error(PANGEA_SCRIPT_OK, "");
	return true;
}

bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle)
{
	return resolve_registered_object(handle) != NULL;
}

int PangeaScript_GetScriptedObjectCount(void)
{
	return gScriptedObjectCount;
}

bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** outNativeObject)
{
	RegisteredObject* object;

	if (!outNativeObject)
		return false;

	object = resolve_registered_object(handle);
	if (!object || !object->nativeObject)
		return false;

	*outNativeObject = object->nativeObject;
	return true;
}

static bool valid_object_source(const PangeaScriptObjectSource* source)
{
	if (!source || source->kind < PANGEA_SCRIPT_SOURCE_TERRAIN || source->kind > PANGEA_SCRIPT_SOURCE_MAP)
		return false;
	if (!isfinite(source->x) || !isfinite(source->y) || !isfinite(source->z) || !isfinite(source->placement))
		return false;
	if (source->itemIndex < 0 || source->nativeType < 0)
		return false;
	if (source->kind == PANGEA_SCRIPT_SOURCE_SPLINE)
		return source->splineNum >= 0 && source->placement >= 0.0f && source->placement <= 1.0f;
	return source->splineNum == 0 && source->placement == 0.0f;
}

PangeaScriptStatus PangeaScript_AssociateObjectSource(PangeaScriptObjectHandle handle, const PangeaScriptObjectSource* source)
{
	RegisteredObject* object = resolve_registered_object(handle);
	if (!object || !source)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object source association received an unknown handle or incomplete source");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!valid_object_source(source))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Object source association contains an invalid source identity");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	object->source = *source;
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

bool PangeaScript_GetObjectSource(PangeaScriptObjectHandle handle, PangeaScriptObjectSource* outSource)
{
	RegisteredObject* object = resolve_registered_object(handle);
	if (!object || !outSource || object->source.kind == PANGEA_SCRIPT_SOURCE_NONE)
		return false;
	*outSource = object->source;
	return true;
}

static bool source_matches(const PangeaScriptObjectSource* expected, const PangeaScriptObjectSource* actual)
{
	if (!expected || !actual || expected->kind != actual->kind ||
		expected->itemIndex != actual->itemIndex || expected->nativeType != actual->nativeType)
		return false;
	if (expected->kind == PANGEA_SCRIPT_SOURCE_SPLINE &&
		(expected->splineNum != actual->splineNum || fabsf(expected->placement - actual->placement) > 0.0001f))
		return false;
	return fabsf(expected->x - actual->x) <= 0.5f &&
		fabsf(expected->y - actual->y) <= 0.5f &&
		fabsf(expected->z - actual->z) <= 0.5f;
}

bool PangeaScript_FindObjectBySource(const PangeaScriptObjectSource* source, PangeaScriptObjectHandle* outHandle)
{
	if (!outHandle || !valid_object_source(source))
		return false;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
	{
		RegisteredObject* object = &gRegisteredObjects[i];
		if (!object->active || !source_matches(source, &object->source))
			continue;
		*outHandle = (PangeaScriptObjectHandle){i + 1, object->generation};
		return true;
	}
	return false;
}

int PangeaScript_GetRegisteredObjectCount(void)
{
	int count = 0;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
		if (gRegisteredObjects[i].active) count++;
	return count;
}

bool PangeaScript_GetRegisteredObjectHandle(int index, PangeaScriptObjectHandle* outHandle)
{
	if (index < 0 || !outHandle) return false;
	int activeIndex = 0;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
	{
		if (!gRegisteredObjects[i].active) continue;
		if (activeIndex++ != index) continue;
		*outHandle = (PangeaScriptObjectHandle){i + 1, gRegisteredObjects[i].generation};
		return true;
	}
	return false;
}

int PangeaScript_GetObjectTagCount(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	return object ? object->tagCount : 0;
}

const char* PangeaScript_GetObjectTag(PangeaScriptObjectHandle handle, int index)
{
	RegisteredObject* object = resolve_object(handle);
	return object && index >= 0 && index < object->tagCount ? object->tags[index] : NULL;
}

static bool finish_object_command(
	const char* commandId,
	PangeaScriptObjectHandle handle,
	bool success,
	PangeaScriptStatus failureStatus,
	const char* failureMessage)
{
	PangeaScriptStatus status = success ? PANGEA_SCRIPT_OK : failureStatus;
	record_command(commandId, handle, status);
	if (success)
	{
		set_error(PANGEA_SCRIPT_OK, "");
		return true;
	}
	set_error(failureStatus, failureMessage);
	return false;
}

static bool is_finite_vector(const PangeaScriptVector3* vector)
{
	return vector && isfinite(vector->x) && isfinite(vector->y) && isfinite(vector->z);
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

bool PangeaScript_GetObjectVelocity(PangeaScriptObjectHandle handle, PangeaScriptVector3* outVelocity)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outVelocity || !object->ops || !object->ops->getVelocity)
		return false;
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks read capability level");
		return false;
	}
	return object->ops->getVelocity(object->nativeObject, outVelocity);
}

bool PangeaScript_GetObjectRotation(PangeaScriptObjectHandle handle, PangeaScriptVector3* outRotation)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outRotation || !object->ops || !object->ops->getRotation)
		return false;
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks read capability level");
		return false;
	}
	return object->ops->getRotation(object->nativeObject, outRotation);
}

bool PangeaScript_GetObjectScale(PangeaScriptObjectHandle handle, float* outScale)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outScale || !object->ops || !object->ops->getScale)
		return false;
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks read capability level");
		return false;
	}
	return object->ops->getScale(object->nativeObject, outScale);
}

bool PangeaScript_GetObjectAnimation(PangeaScriptObjectHandle handle, int* outAnimation)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outAnimation || !object->ops || !object->ops->getAnimation)
		return false;
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks read capability level");
		return false;
	}
	return object->ops->getAnimation(object->nativeObject, outAnimation);
}

bool PangeaScript_GetObjectActive(PangeaScriptObjectHandle handle, bool* outActive)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outActive || !object->ops || !object->ops->getActive)
		return false;
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks read capability level");
		return false;
	}
	return object->ops->getActive(object->nativeObject, outActive);
}

bool PangeaScript_GetObjectCollisionEnabled(PangeaScriptObjectHandle handle, bool* outEnabled)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outEnabled || !object->ops || !object->ops->getCollisionEnabled)
		return false;
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks read capability level");
		return false;
	}
	return object->ops->getCollisionEnabled(object->nativeObject, outEnabled);
}

bool PangeaScript_SetObjectPosition(PangeaScriptObjectHandle handle, const PangeaScriptVector3* position)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.setPosition", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (!is_finite_vector(position))
		return finish_object_command("pangea.object.setPosition", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object position must contain finite coordinates");
	if (!object->ops || !object->ops->setPosition)
		return finish_object_command("pangea.object.setPosition", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object does not support position commands");

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_BASE)
		return finish_object_command("pangea.object.setPosition", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks transform/position capability level");

	return finish_object_command("pangea.object.setPosition", handle, object->ops->setPosition(object->nativeObject, position), PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the position command");
}

bool PangeaScript_SetObjectPositionOffset(PangeaScriptObjectHandle handle, const PangeaScriptVector3* offset)
{
	if (!gCurrentObjectFrameResult || !gHasCurrentCallbackObject ||
		handle.id != gCurrentCallbackObject.id || handle.generation != gCurrentCallbackObject.generation)
		return finish_object_command("pangea.object.setPositionOffset", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object position offset commands are only valid for the current object during onObjectFrame");
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.setPositionOffset", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (!is_finite_vector(offset))
		return finish_object_command("pangea.object.setPositionOffset", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object position offset must contain finite coordinates");
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_BASE)
		return finish_object_command("pangea.object.setPositionOffset", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks transform/position capability level");
	gCurrentObjectFrameResult->hasPositionOffset = true;
	gCurrentObjectFrameResult->positionOffset = *offset;
	return finish_object_command("pangea.object.setPositionOffset", handle, true, PANGEA_SCRIPT_RUNTIME_ERROR, "");
}

bool PangeaScript_SetObjectVelocity(PangeaScriptObjectHandle handle, const PangeaScriptVector3* velocity)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.setVelocity", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (!is_finite_vector(velocity))
		return finish_object_command("pangea.object.setVelocity", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object velocity must contain finite coordinates");
	if (!object->ops || !object->ops->setVelocity)
		return finish_object_command("pangea.object.setVelocity", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object does not support velocity commands");

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
		return finish_object_command("pangea.object.setVelocity", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks movable/velocity capability level");

	return finish_object_command("pangea.object.setVelocity", handle, object->ops->setVelocity(object->nativeObject, velocity), PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the velocity command");
}

bool PangeaScript_SetObjectRotation(PangeaScriptObjectHandle handle, const PangeaScriptVector3* rotation)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.setRotation", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (!is_finite_vector(rotation))
		return finish_object_command("pangea.object.setRotation", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object rotation must contain finite coordinates");
	if (!object->ops || !object->ops->setRotation)
		return finish_object_command("pangea.object.setRotation", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object does not support rotation commands");
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_BASE)
		return finish_object_command("pangea.object.setRotation", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks transform/rotation capability level");
	return finish_object_command("pangea.object.setRotation", handle, object->ops->setRotation(object->nativeObject, rotation), PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the rotation command");
}

bool PangeaScript_SetObjectScale(PangeaScriptObjectHandle handle, float scale)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.setScale", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (!isfinite(scale) || scale < 0.000001f || scale > 100.0f)
		return finish_object_command("pangea.object.setScale", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object scale must be finite and between 0.000001 and 100");
	if (!object->ops || !object->ops->setScale)
		return finish_object_command("pangea.object.setScale", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object does not support scale commands");
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_BASE)
		return finish_object_command("pangea.object.setScale", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks transform/scale capability level");
	return finish_object_command("pangea.object.setScale", handle, object->ops->setScale(object->nativeObject, scale), PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the scale command");
}

bool PangeaScript_SetObjectAnimation(PangeaScriptObjectHandle handle, int animation, float speed, float blendSeconds)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.setAnimation", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (animation < 0 || !isfinite(speed) || speed < 0.0f || !isfinite(blendSeconds) || blendSeconds < 0.0f)
		return finish_object_command("pangea.object.setAnimation", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Animation must be non-negative and finite");
	if (!object->ops || !object->ops->setAnimation)
		return finish_object_command("pangea.object.setAnimation", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object does not support numeric animation commands");
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
		return finish_object_command("pangea.object.setAnimation", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks animation capability level");
	return finish_object_command("pangea.object.setAnimation", handle, object->ops->setAnimation(object->nativeObject, animation, speed, blendSeconds), PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the animation command");
}

bool PangeaScript_SetObjectAnimationNamed(PangeaScriptObjectHandle handle, const char* animation, float speed, float blendSeconds)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.setAnimation", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (!animation || !animation[0] || !isfinite(speed) || speed < 0.0f || !isfinite(blendSeconds) || blendSeconds < 0.0f)
		return finish_object_command("pangea.object.setAnimation", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Animation name and values must be valid and finite");
	if (!object->ops || !object->ops->setAnimationNamed)
		return finish_object_command("pangea.object.setAnimation", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object does not support named animation commands");
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
		return finish_object_command("pangea.object.setAnimation", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks animation capability level");
	return finish_object_command("pangea.object.setAnimation", handle, object->ops->setAnimationNamed(object->nativeObject, animation, speed, blendSeconds), PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the named animation command");
}

bool PangeaScript_SetObjectCollisionEnabled(PangeaScriptObjectHandle handle, bool enabled)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.setCollisionEnabled", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (!object->ops || !object->ops->setCollisionEnabled)
		return finish_object_command("pangea.object.setCollisionEnabled", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object does not support collision commands");
	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_BASE)
		return finish_object_command("pangea.object.setCollisionEnabled", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks collision capability level");
	return finish_object_command("pangea.object.setCollisionEnabled", handle, object->ops->setCollisionEnabled(object->nativeObject, enabled), PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the collision command");
}

bool PangeaScript_SetObjectActive(PangeaScriptObjectHandle handle, bool active)
{
	RegisteredObject* object = resolve_registered_object(handle);
	bool nativeApplied = false;
	if (!object)
		return finish_object_command("pangea.object.setActive", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (object->enabled == active)
		return finish_object_command("pangea.object.setActive", handle, true, PANGEA_SCRIPT_RUNTIME_ERROR, "");
	if (object->ops && object->ops->setActive)
	{
		if (!object->ops->setActive(object->nativeObject, active))
			return finish_object_command("pangea.object.setActive", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the active-state command");
		nativeApplied = true;
	}
	if (gBackend && gScriptLoaded && gHasCurrentCallbackFrame)
	{
		PangeaScriptStatus status = PangeaScript_ApplyObjectLifecycle(
			handle,
			&gCurrentCallbackFrame,
			active ? PANGEA_SCRIPT_OBJECT_ACTIVATE : PANGEA_SCRIPT_OBJECT_DEACTIVATE);
		if (status != PANGEA_SCRIPT_OK)
		{
			if (nativeApplied && object->ops && object->ops->setActive)
				(void)object->ops->setActive(object->nativeObject, !active);
			char lifecycleError[PANGEA_SCRIPT_ERROR_CAPACITY];
			snprintf(lifecycleError, sizeof(lifecycleError), "%s", PangeaScript_GetLastError());
			return finish_object_command("pangea.object.setActive", handle, false, status, lifecycleError);
		}
		return finish_object_command("pangea.object.setActive", handle, true, PANGEA_SCRIPT_RUNTIME_ERROR, "");
	}
	object->enabled = active;
	if (!active && gBackend)
		PangeaScriptBackend_ClearObjectState(gBackend, handle);
	return finish_object_command("pangea.object.setActive", handle, true, PANGEA_SCRIPT_RUNTIME_ERROR, "");
}

bool PangeaScript_DeleteObject(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return finish_object_command("pangea.object.delete", handle, false, PANGEA_SCRIPT_BAD_ARGUMENT, "Object handle is unknown or stale");
	if (!object->ops || !object->ops->deleteObject)
		return finish_object_command("pangea.object.delete", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Object does not support deletion commands");

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
		return finish_object_command("pangea.object.delete", handle, false, PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks deletion/cleanup-safe capability level");

	if (!object->destroying && gBackend && gScriptLoaded)
	{
		const PangeaScriptFrameContext destroyFrame = gHasCurrentCallbackFrame
			? gCurrentCallbackFrame
			: (gHasLastFrameContext ? gLastFrameContext : (PangeaScriptFrameContext){0});
		(void) PangeaScript_CallObjectEvent(handle, &destroyFrame, "destroy");
		object = resolve_object(handle);
		if (!object)
		{
			set_error(PANGEA_SCRIPT_OK, "");
			return true;
		}
	}

	bool deleted = object->ops->deleteObject(object->nativeObject);
	if (deleted && resolve_object(handle) == object)
	{
		cleanup_owned_children(handle);
		release_scripted_object_state(object);
		if (gBackend)
			PangeaScriptBackend_ClearObjectState(gBackend, handle);
		clear_registered_object(object);
	}
	return finish_object_command("pangea.object.delete", handle, deleted, PANGEA_SCRIPT_RUNTIME_ERROR, "Native object rejected the deletion command");
}

PangeaScriptStatus PangeaScript_RegisterNativeItems(const PangeaScriptNativeItem* items, int count)
{
	if (count < 0 || count > PANGEA_SCRIPT_MAX_NATIVE_ITEMS || (count > 0 && !items))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Native item registration is incomplete");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	for (int i = 0; i < count; i++)
	{
		if (!items[i].id || !items[i].id[0] || !items[i].category || !items[i].category[0] ||
			!items[i].dependencySummary || !items[i].dependencySummary[0] || items[i].nativeType < 0)
		{
			set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Native item registration requires an ID, category, dependency summary, and non-negative type");
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		}
		for (int previous = 0; previous < i; previous++)
		{
			if (strcmp(items[previous].id, items[i].id) == 0)
			{
				set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Native item registration contains duplicate IDs");
				return PANGEA_SCRIPT_CONFIG_ERROR;
			}
		}
		if (!copy_string(gPendingNativeItemStrings[i].id, sizeof(gPendingNativeItemStrings[i].id), items[i].id) ||
			!copy_string(gPendingNativeItemStrings[i].category, sizeof(gPendingNativeItemStrings[i].category), items[i].category) ||
			!copy_string(gPendingNativeItemStrings[i].dependencySummary, sizeof(gPendingNativeItemStrings[i].dependencySummary), items[i].dependencySummary))
		{
			set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Native item registration contains a string that is too long");
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		}
	}

	memset(gNativeItems, 0, sizeof(gNativeItems));
	memset(gNativeItemStrings, 0, sizeof(gNativeItemStrings));
	for (int i = 0; i < count; i++)
	{
		gNativeItemStrings[i] = gPendingNativeItemStrings[i];
		gNativeItems[i] = (PangeaScriptNativeItem){
			.id = gNativeItemStrings[i].id,
			.nativeType = items[i].nativeType,
			.category = gNativeItemStrings[i].category,
			.dependencySummary = gNativeItemStrings[i].dependencySummary,
		};
	}
	gNativeItemCount = count;
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

int PangeaScript_GetNativeItemCount(void)
{
	return gNativeItemCount;
}

const PangeaScriptNativeItem* PangeaScript_GetNativeItem(int index)
{
	if (index < 0 || index >= gNativeItemCount)
		return NULL;
	return &gNativeItems[index];
}

int PangeaScript_GetCommandDescriptorCount(void)
{
	return (int)(sizeof(gCommandDescriptors) / sizeof(gCommandDescriptors[0]));
}

const PangeaScriptCommandDescriptor* PangeaScript_GetCommandDescriptor(int index)
{
	if (index < 0 || index >= PangeaScript_GetCommandDescriptorCount())
		return NULL;
	return &gCommandDescriptors[index];
}

int PangeaScript_GetEventDescriptorCount(void)
{
	return (int)(sizeof(gEventDescriptors) / sizeof(gEventDescriptors[0]));
}

const PangeaScriptEventDescriptor* PangeaScript_GetEventDescriptor(int index)
{
	if (index < 0 || index >= PangeaScript_GetEventDescriptorCount())
		return NULL;
	return &gEventDescriptors[index];
}

int PangeaScript_GetObjectEventDescriptorCount(void)
{
	return (int)(sizeof(gObjectEventDescriptors) / sizeof(gObjectEventDescriptors[0]));
}

const PangeaScriptObjectEventDescriptor* PangeaScript_GetObjectEventDescriptor(int index)
{
	if (index < 0 || index >= PangeaScript_GetObjectEventDescriptorCount())
		return NULL;
	return &gObjectEventDescriptors[index];
}

void PangeaScript_ResetCommandTrace(void)
{
	gCommandTrace.commandCount = 0;
	gCommandTrace.hash = 0x811c9dc5u;
	gCommandTrace.entryCount = 0;
	gCommandTrace.overflow = false;
	memset(gCommandTraceEntries, 0, sizeof(gCommandTraceEntries));
}

void PangeaScript_ResetLifecycleTrace(void)
{
	gLifecycleTrace.eventCount = 0;
	gLifecycleTrace.entryCount = 0;
	gLifecycleTrace.overflow = false;
	memset(gLifecycleTraceEntries, 0, sizeof(gLifecycleTraceEntries));
	gLastFrameContext = (PangeaScriptFrameContext){0};
	gHasLastFrameContext = false;
}

void PangeaScript_GetLifecycleTrace(PangeaScriptLifecycleTrace* outTrace)
{
	if (!outTrace)
		return;
	*outTrace = gLifecycleTrace;
}

bool PangeaScript_GetLifecycleTraceEntry(int index, PangeaScriptLifecycleTraceEntry* outEntry)
{
	if (!outEntry || index < 0 || (uint32_t)index >= gLifecycleTrace.entryCount)
		return false;
	*outEntry = gLifecycleTraceEntries[index];
	return true;
}

PangeaScriptStatus PangeaScript_CompareLifecycleTrace(
	const PangeaScriptLifecycleTrace* expectedTrace,
	const PangeaScriptLifecycleTraceEntry* expectedEntries,
	uint32_t expectedEntryCount,
	PangeaScriptLifecycleTraceComparison* outComparison)
{
	uint32_t sharedEntryCount;
	if (!expectedTrace || !outComparison || expectedEntryCount > PANGEA_SCRIPT_LIFECYCLE_TRACE_CAPACITY ||
		expectedTrace->entryCount != expectedEntryCount || (expectedEntryCount > 0 && !expectedEntries))
	{
		set_diagnostic_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Lifecycle trace comparison received invalid bounded entries");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	memset(outComparison, 0, sizeof(*outComparison));
	outComparison->matches = true;
	outComparison->firstMismatchIndex = UINT32_MAX;
	outComparison->expectedEventCount = expectedTrace->eventCount;
	outComparison->actualEventCount = gLifecycleTrace.eventCount;
	sharedEntryCount = expectedEntryCount < gLifecycleTrace.entryCount ? expectedEntryCount : gLifecycleTrace.entryCount;
	if (expectedTrace->eventCount != gLifecycleTrace.eventCount ||
		expectedTrace->entryCount != gLifecycleTrace.entryCount ||
		expectedTrace->overflow != gLifecycleTrace.overflow)
		outComparison->matches = false;

	for (uint32_t index = 0; index < sharedEntryCount; index++)
	{
		const PangeaScriptLifecycleTraceEntry* expected = &expectedEntries[index];
		const PangeaScriptLifecycleTraceEntry* actual = &gLifecycleTraceEntries[index];
		if (strcmp(expected->eventId, actual->eventId) == 0 &&
			strcmp(expected->applicationPhase, actual->applicationPhase) == 0 &&
			expected->order == actual->order && expected->target.id == actual->target.id &&
			expected->target.generation == actual->target.generation && expected->status == actual->status)
			continue;
		if (outComparison->firstMismatchIndex == UINT32_MAX)
		{
			outComparison->firstMismatchIndex = index;
			snprintf(outComparison->expectedEventId, sizeof(outComparison->expectedEventId), "%s", expected->eventId);
			snprintf(outComparison->actualEventId, sizeof(outComparison->actualEventId), "%s", actual->eventId);
			outComparison->expectedTarget = expected->target;
			outComparison->actualTarget = actual->target;
			outComparison->expectedStatus = expected->status;
			outComparison->actualStatus = actual->status;
		}
		outComparison->matches = false;
	}
	if (!outComparison->matches && outComparison->firstMismatchIndex == UINT32_MAX)
		outComparison->firstMismatchIndex = sharedEntryCount;
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

static uint32_t normalized_trace_target(
	PangeaScriptObjectHandle target,
	PangeaScriptObjectHandle* targets,
	uint32_t* targetCount)
{
	if (target.id <= 0 || target.generation == 0)
		return 0;
	for (uint32_t index = 0; index < *targetCount; index++)
	{
		if (targets[index].id == target.id && targets[index].generation == target.generation)
			return index + 1;
	}
	if (*targetCount >= PANGEA_SCRIPT_LIFECYCLE_TRACE_CAPACITY)
		return UINT32_MAX;
	targets[*targetCount] = target;
	(*targetCount)++;
	return *targetCount;
}

PangeaScriptStatus PangeaScript_CompareNormalizedLifecycleTrace(
	const PangeaScriptLifecycleTrace* expectedTrace,
	const PangeaScriptLifecycleTraceEntry* expectedEntries,
	uint32_t expectedEntryCount,
	PangeaScriptLifecycleTraceComparison* outComparison)
{
	PangeaScriptObjectHandle expectedTargets[PANGEA_SCRIPT_LIFECYCLE_TRACE_CAPACITY];
	PangeaScriptObjectHandle actualTargets[PANGEA_SCRIPT_LIFECYCLE_TRACE_CAPACITY];
	uint32_t expectedTargetCount = 0;
	uint32_t actualTargetCount = 0;
	uint32_t sharedEntryCount;
	if (!expectedTrace || !outComparison || expectedEntryCount > PANGEA_SCRIPT_LIFECYCLE_TRACE_CAPACITY ||
		expectedTrace->entryCount != expectedEntryCount || (expectedEntryCount > 0 && !expectedEntries))
	{
		set_diagnostic_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Normalized lifecycle trace comparison received invalid bounded entries");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	memset(outComparison, 0, sizeof(*outComparison));
	outComparison->matches = true;
	outComparison->firstMismatchIndex = UINT32_MAX;
	outComparison->expectedEventCount = expectedTrace->eventCount;
	outComparison->actualEventCount = gLifecycleTrace.eventCount;
	sharedEntryCount = expectedEntryCount < gLifecycleTrace.entryCount ? expectedEntryCount : gLifecycleTrace.entryCount;
	if (expectedTrace->eventCount != gLifecycleTrace.eventCount ||
		expectedTrace->entryCount != gLifecycleTrace.entryCount ||
		expectedTrace->overflow != gLifecycleTrace.overflow)
		outComparison->matches = false;

	for (uint32_t index = 0; index < sharedEntryCount; index++)
	{
		const PangeaScriptLifecycleTraceEntry* expected = &expectedEntries[index];
		const PangeaScriptLifecycleTraceEntry* actual = &gLifecycleTraceEntries[index];
		uint32_t expectedTarget = normalized_trace_target(expected->target, expectedTargets, &expectedTargetCount);
		uint32_t actualTarget = normalized_trace_target(actual->target, actualTargets, &actualTargetCount);
		if (strcmp(expected->eventId, actual->eventId) == 0 &&
			strcmp(expected->applicationPhase, actual->applicationPhase) == 0 &&
			expectedTarget == actualTarget && expected->status == actual->status)
			continue;
		if (outComparison->firstMismatchIndex == UINT32_MAX)
		{
			outComparison->firstMismatchIndex = index;
			snprintf(outComparison->expectedEventId, sizeof(outComparison->expectedEventId), "%s", expected->eventId);
			snprintf(outComparison->actualEventId, sizeof(outComparison->actualEventId), "%s", actual->eventId);
			outComparison->expectedTarget = expected->target;
			outComparison->actualTarget = actual->target;
			outComparison->expectedStatus = expected->status;
			outComparison->actualStatus = actual->status;
		}
		outComparison->matches = false;
	}
	if (!outComparison->matches && outComparison->firstMismatchIndex == UINT32_MAX)
		outComparison->firstMismatchIndex = sharedEntryCount;
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

void PangeaScript_GetCommandTrace(PangeaScriptCommandTrace* outTrace)
{
	if (!outTrace)
		return;
	*outTrace = gCommandTrace;
}

bool PangeaScript_GetCommandTraceEntry(int index, PangeaScriptCommandTraceEntry* outEntry)
{
	if (!outEntry || index < 0 || (uint32_t)index >= gCommandTrace.entryCount)
		return false;
	*outEntry = gCommandTraceEntries[index];
	return true;
}

PangeaScriptStatus PangeaScript_CompareCommandTrace(
	const PangeaScriptCommandTrace* expectedTrace,
	const PangeaScriptCommandTraceEntry* expectedEntries,
	uint32_t expectedEntryCount,
	PangeaScriptCommandTraceComparison* outComparison)
{
	uint32_t sharedEntryCount;

	if (!expectedTrace || !outComparison || expectedEntryCount > PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY ||
		expectedTrace->entryCount != expectedEntryCount || (expectedEntryCount > 0 && !expectedEntries))
	{
		set_diagnostic_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Command trace comparison received invalid bounded entries");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	memset(outComparison, 0, sizeof(*outComparison));
	outComparison->matches = true;
	outComparison->firstMismatchIndex = UINT32_MAX;
	outComparison->expectedCommandCount = expectedTrace->commandCount;
	outComparison->actualCommandCount = gCommandTrace.commandCount;
	outComparison->expectedHash = expectedTrace->hash;
	outComparison->actualHash = gCommandTrace.hash;
	sharedEntryCount = expectedEntryCount < gCommandTrace.entryCount ? expectedEntryCount : gCommandTrace.entryCount;

	if (expectedTrace->commandCount != gCommandTrace.commandCount ||
		expectedTrace->hash != gCommandTrace.hash ||
		expectedTrace->entryCount != gCommandTrace.entryCount ||
		expectedTrace->overflow != gCommandTrace.overflow)
	{
		outComparison->matches = false;
	}

	for (uint32_t index = 0; index < sharedEntryCount; index++)
	{
		const PangeaScriptCommandTraceEntry* expected = &expectedEntries[index];
		const PangeaScriptCommandTraceEntry* actual = &gCommandTraceEntries[index];
		if (strcmp(expected->commandId, actual->commandId) == 0 &&
			strcmp(expected->applicationPhase, actual->applicationPhase) == 0 &&
			expected->order == actual->order &&
			expected->target.id == actual->target.id &&
			expected->target.generation == actual->target.generation &&
			expected->status == actual->status)
			continue;
		if (outComparison->firstMismatchIndex == UINT32_MAX)
		{
			outComparison->firstMismatchIndex = index;
			snprintf(outComparison->expectedCommandId, sizeof(outComparison->expectedCommandId), "%s", expected->commandId);
			snprintf(outComparison->actualCommandId, sizeof(outComparison->actualCommandId), "%s", actual->commandId);
			outComparison->expectedTarget = expected->target;
			outComparison->actualTarget = actual->target;
			outComparison->expectedStatus = expected->status;
			outComparison->actualStatus = actual->status;
		}
		outComparison->matches = false;
	}

	if (!outComparison->matches && outComparison->firstMismatchIndex == UINT32_MAX && expectedEntryCount != gCommandTrace.entryCount)
		outComparison->firstMismatchIndex = sharedEntryCount;
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

int PangeaScript_ResolveNativeItemType(const char* id)
{
	char* end = NULL;
	long type;
	if (!id || !id[0])
		return -1;
	errno = 0;
	type = strtol(id, &end, 10);
	if (errno == 0 && end && end != id && *end == '\0' && type >= 0 && type <= INT_MAX)
		return (int) type;
	for (int i = 0; i < gNativeItemCount; i++)
		if (gNativeItems[i].id && strcmp(gNativeItems[i].id, id) == 0)
			return gNativeItems[i].nativeType;
	return -1;
}

PangeaScriptStatus PangeaScript_SpawnNative(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	if (!id || !id[0])
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Native item ID is required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Native item position must contain finite coordinates");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (!params)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Native item parameters are required");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	for (int i = 0; i < 4; i++)
	{
		if (params[i] < 0 || params[i] > 255)
		{
			set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Native item parameters must be in the byte range 0-255");
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		}
	}

	if (gGameInfo.spawnNative)
	{
		if (outHandle)
			*outHandle = (PangeaScriptObjectHandle){0};
		PangeaScriptStatus status = gGameInfo.spawnNative(id, x, y, z, params, outHandle);
		if (status == PANGEA_SCRIPT_OK)
			set_error(status, "");
		else if (status == PANGEA_SCRIPT_INCOMPATIBLE_ITEM)
		{
			char message[256];
			snprintf(message, sizeof(message), "Native item '%s' has no initializer or its required level assets are unavailable", id);
			set_error(status, message);
		}
		else if (status == PANGEA_SCRIPT_BAD_ARGUMENT)
			set_error(status, "Native item ID, position, or parameters are invalid");
		else
			set_error(status, "Native item initializer failed or synthetic item capacity is exhausted");
		return status;
	}

	for (int i = 0; i < gNativeItemCount; i++)
	{
		if (gNativeItems[i].id && strcmp(gNativeItems[i].id, id) == 0)
		{
			set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "The game adapter did not provide a native spawn callback");
			return PANGEA_SCRIPT_RUNTIME_ERROR;
		}
	}

	set_error(PANGEA_SCRIPT_INCOMPATIBLE_ITEM, "The native item ID is not registered for this game");
	return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
EMSCRIPTEN_KEEPALIVE
void PangeaScript_LogJS(int level, const char* message)
{
	PangeaScript_Log((PangeaScriptLogLevel)level, "Web", message);
}
#endif

void PangeaScript_GetStatusInfo(PangeaScriptStatusInfo* outInfo)
{
	if (!outInfo)
		return;

	outInfo->enabled = gInitialized;
	outInfo->configLoaded = gConfigPath[0] != '\0';
	outInfo->bundleLoaded = gScriptLoaded;
	snprintf(outInfo->activeScriptPath, sizeof(outInfo->activeScriptPath), "%s", gStartupScriptPath[0] ? gStartupScriptPath : "Data/Scripts/dist/main.lua");
	snprintf(outInfo->lastError, sizeof(outInfo->lastError), "%s", gLastError[0] ? gLastError : gLastRuntimeError);
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
const char* PangeaScript_GetStatusLastError(void) { return gLastError[0] ? gLastError : gLastRuntimeError; }

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
#endif
uint32_t PangeaScript_GetRuntimeFingerprint(void) { return gRuntimeFingerprint; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
bool PangeaScript_GetObjectPositionJS(int id, uint32_t generation, float* outX, float* outY, float* outZ)
{
	PangeaScriptObjectHandle handle = { id, generation };
	PangeaScriptVector3 pos;
	if (PangeaScript_GetObjectPosition(handle, &pos))
	{
		*outX = pos.x;
		*outY = pos.y;
		*outZ = pos.z;
		return true;
	}
	return false;
}

EMSCRIPTEN_KEEPALIVE
bool PangeaScript_SetObjectPositionJS(int id, uint32_t generation, float x, float y, float z)
{
	PangeaScriptObjectHandle handle = { id, generation };
	PangeaScriptVector3 pos = { x, y, z };
	return PangeaScript_SetObjectPosition(handle, &pos);
}

EMSCRIPTEN_KEEPALIVE
bool PangeaScript_SetObjectVelocityJS(int id, uint32_t generation, float x, float y, float z)
{
	PangeaScriptObjectHandle handle = { id, generation };
	PangeaScriptVector3 vel = { x, y, z };
	return PangeaScript_SetObjectVelocity(handle, &vel);
}

EMSCRIPTEN_KEEPALIVE
bool PangeaScript_SetObjectRotationJS(int id, uint32_t generation, float x, float y, float z)
{
	PangeaScriptObjectHandle handle = { id, generation };
	PangeaScriptVector3 rotation = { x, y, z };
	return PangeaScript_SetObjectRotation(handle, &rotation);
}

EMSCRIPTEN_KEEPALIVE
bool PangeaScript_SetObjectScaleJS(int id, uint32_t generation, float scale)
{
	PangeaScriptObjectHandle handle = { id, generation };
	return PangeaScript_SetObjectScale(handle, scale);
}

EMSCRIPTEN_KEEPALIVE
bool PangeaScript_SetObjectAnimationJS(int id, uint32_t generation, int animation, float speed, float blendSeconds)
{
	PangeaScriptObjectHandle handle = { id, generation };
	return PangeaScript_SetObjectAnimation(handle, animation, speed, blendSeconds);
}

EMSCRIPTEN_KEEPALIVE
bool PangeaScript_SetObjectAnimationNamedJS(int id, uint32_t generation, const char* animation, float speed, float blendSeconds)
{
	PangeaScriptObjectHandle handle = { id, generation };
	return PangeaScript_SetObjectAnimationNamed(handle, animation, speed, blendSeconds);
}

EMSCRIPTEN_KEEPALIVE
bool PangeaScript_DeleteObjectJS(int id, uint32_t generation)
{
	PangeaScriptObjectHandle handle = { id, generation };
	return PangeaScript_DeleteObject(handle);
}

EMSCRIPTEN_KEEPALIVE
int PangeaScript_SpawnNativeJS(const char* id, float x, float y, float z, int param0, int param1, int param2, int param3, int* outId, uint32_t* outGen)
{
	PangeaScriptObjectHandle handle = {0, 0};
	const int params[4] = {param0, param1, param2, param3};
	PangeaScriptStatus status = PangeaScript_SpawnNative(id, x, y, z, params, &handle);
	if (status == PANGEA_SCRIPT_OK)
	{
		*outId = handle.id;
		*outGen = handle.generation;
	}
	return (int) status;
}

EMSCRIPTEN_KEEPALIVE
int PangeaScript_RegisterScriptedObjectJS(const char* id, float x, float y, float z, int* outId, uint32_t* outGen)
{
	PangeaScriptObjectHandle handle = {0, 0};
	PangeaScriptStatus status = PangeaScript_RegisterScriptedObject(id, x, y, z, &handle);
	if (status == PANGEA_SCRIPT_OK)
	{
		*outId = handle.id;
		*outGen = handle.generation;
	}
	return (int) status;
}

EMSCRIPTEN_KEEPALIVE
int PangeaScript_ProbeScriptedObjectJS(const char* id, float x, float y, float z)
{
	PangeaScriptObjectHandle handle = {0, 0};
	PangeaScriptStatus status = PangeaScript_RegisterScriptedObject(id, x, y, z, &handle);
	if (status == PANGEA_SCRIPT_OK && !PangeaScript_DeleteObject(handle))
		return (int)PANGEA_SCRIPT_RUNTIME_ERROR;
	return (int)status;
}

EMSCRIPTEN_KEEPALIVE
int PangeaScript_ProbeNativeObjectJS(const char* id, float x, float y, float z)
{
	PangeaScriptObjectHandle handle = {0, 0};
	const int params[4] = {0, 0, 0, 0};
	PangeaScriptStatus status = PangeaScript_SpawnNative(id, x, y, z, params, &handle);
	if (status != PANGEA_SCRIPT_OK)
		return (int)status;
	if (handle.id <= 0 || handle.generation == 0 || !PangeaScript_DeleteObject(handle))
		return (int)PANGEA_SCRIPT_RUNTIME_ERROR;
	return (int)status;
}

EMSCRIPTEN_KEEPALIVE
int PangeaScript_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z)
{
	return PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z) != NULL
		? (int)PANGEA_SCRIPT_OK
		: (int)PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
}

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
	(void*)PangeaScript_GetStatusScriptsDisabled,
	(void*)PangeaScript_GetRuntimeFingerprint,
	(void*)PangeaScript_LogJS,
	(void*)PangeaScript_GetObjectPositionJS,
	(void*)PangeaScript_SetObjectPositionJS,
	(void*)PangeaScript_SetObjectVelocityJS,
	(void*)PangeaScript_SetObjectRotationJS,
	(void*)PangeaScript_SetObjectScaleJS,
	(void*)PangeaScript_SetObjectAnimationJS,
	(void*)PangeaScript_SetObjectAnimationNamedJS,
	(void*)PangeaScript_DeleteObjectJS,
	(void*)PangeaScript_SpawnNativeJS,
	(void*)PangeaScript_RegisterScriptedObjectJS,
	(void*)PangeaScript_ProbeScriptedObjectJS,
	(void*)PangeaScript_ProbeNativeObjectJS,
	(void*)PangeaScript_ProbeTerrainReplacementJS
};
#endif

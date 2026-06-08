#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PangeaScriptStatus
{
	PANGEA_SCRIPT_OK = 0,
	PANGEA_SCRIPT_NOT_ENABLED,
	PANGEA_SCRIPT_FILE_NOT_FOUND,
	PANGEA_SCRIPT_PARSE_ERROR,
	PANGEA_SCRIPT_RUNTIME_ERROR,
	PANGEA_SCRIPT_BAD_ARGUMENT,
	PANGEA_SCRIPT_BUDGET_EXCEEDED,
	PANGEA_SCRIPT_INCOMPATIBLE_ITEM,
	PANGEA_SCRIPT_CONFIG_ERROR,
} PangeaScriptStatus;

typedef enum PangeaScriptHook
{
	PANGEA_SCRIPT_HOOK_GAME_START = 0,
	PANGEA_SCRIPT_HOOK_LEVEL_LOAD,
	PANGEA_SCRIPT_HOOK_LEVEL_START,
	PANGEA_SCRIPT_HOOK_FRAME,
	PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE,
	PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD,
	PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN,
	PANGEA_SCRIPT_HOOK_TERRAIN_ITEM,
	PANGEA_SCRIPT_HOOK_SPLINE_ITEM,
	PANGEA_SCRIPT_HOOK_MAP_ITEM,
	PANGEA_SCRIPT_HOOK_OBJECT_FRAME,
} PangeaScriptHook;

typedef struct PangeaScriptVector3
{
	float x;
	float y;
	float z;
} PangeaScriptVector3;

typedef struct PangeaScriptObjectHandle
{
	int id;
	uint32_t generation;
} PangeaScriptObjectHandle;

typedef struct PangeaScriptGameInfo
{
	const char* gameId;
	const char* gameName;
} PangeaScriptGameInfo;

typedef struct PangeaScriptLevelContext
{
	int levelNum;
	const char* levelName;
} PangeaScriptLevelContext;

typedef struct PangeaScriptFrameContext
{
	int levelNum;
	unsigned int frameNum;
	float deltaSeconds;
	float levelTimeSeconds;
} PangeaScriptFrameContext;

typedef struct PangeaScriptTerrainItemContext
{
	int levelNum;
	int itemType;
	int remappedItemType;
	int playerNum;
	bool networked;
	float x;
	float z;
	unsigned int flags;
	const unsigned char* params;
	int paramCount;
	bool handled;
	bool markInUse;
} PangeaScriptTerrainItemContext;

typedef struct PangeaScriptSplineItemContext
{
	int levelNum;
	int itemType;
	int splineNum;
	float placement;
	const unsigned char* params;
	int paramCount;
	bool handled;
	bool markInUse;
} PangeaScriptSplineItemContext;

typedef struct PangeaScriptMapItemContext
{
	int levelNum;
	int sceneNum;
	int areaNum;
	int itemType;
	float x;
	float y;
	const unsigned char* params;
	int paramCount;
	bool handled;
	bool markInUse;
} PangeaScriptMapItemContext;

typedef struct PangeaScriptObjectOps
{
	bool (*getPosition)(void* nativeObject, PangeaScriptVector3* outPosition);
	bool (*setPosition)(void* nativeObject, const PangeaScriptVector3* position);
	bool (*setVelocity)(void* nativeObject, const PangeaScriptVector3* velocity);
	bool (*deleteObject)(void* nativeObject);
} PangeaScriptObjectOps;

typedef enum PangeaScriptCapabilityLevel
{
	PANGEA_SCRIPT_CAPABILITY_DEFAULT = 0,
	PANGEA_SCRIPT_CAPABILITY_UNSUPPORTED,
	PANGEA_SCRIPT_CAPABILITY_READ_ONLY,
	PANGEA_SCRIPT_CAPABILITY_BASE,
	PANGEA_SCRIPT_CAPABILITY_FULL
} PangeaScriptCapabilityLevel;

typedef struct PangeaScriptObjectRegistration
{
	void* nativeObject;
	const PangeaScriptObjectOps* ops;
	const char* const* tags;
	int tagCount;
	PangeaScriptCapabilityLevel capabilityLevel;
} PangeaScriptObjectRegistration;

typedef struct PangeaScriptObjectFrameContext
{
	int levelNum;
	unsigned int frameNum;
	float deltaSeconds;
	float levelTimeSeconds;
	PangeaScriptObjectHandle object;
	PangeaScriptVector3 position;
	const char* const* tags;
	int tagCount;
} PangeaScriptObjectFrameContext;

typedef struct PangeaScriptObjectFrameResult
{
	bool hasPositionOffset;
	PangeaScriptVector3 positionOffset;
} PangeaScriptObjectFrameResult;

typedef struct PangeaScriptNativeItem
{
	const char* id;
	int nativeType;
	const char* category;
	const char* dependencySummary;
} PangeaScriptNativeItem;

PangeaScriptStatus PangeaScript_Init(const PangeaScriptGameInfo* gameInfo);
void PangeaScript_Shutdown(void);

bool PangeaScript_IsEnabled(void);
bool PangeaScript_HasRunnableModule(void);

PangeaScriptStatus PangeaScript_SetStartupScript(const char* path);
PangeaScriptStatus PangeaScript_SetConfigPath(const char* path);
PangeaScriptStatus PangeaScript_Reload(void);

const char* PangeaScript_GetLastError(void);
int PangeaScript_GetErrorCount(void);
PangeaScriptStatus PangeaScript_GetLastStatus(void);

PangeaScriptStatus PangeaScript_LoadLevelConfig(int levelNum);
int PangeaScript_RemapTerrainItemType(int levelNum, int itemType);

PangeaScriptStatus PangeaScript_CallLevelHook(PangeaScriptHook hook, const PangeaScriptLevelContext* context);
PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* context);
PangeaScriptStatus PangeaScript_CallTerrainItemHook(PangeaScriptTerrainItemContext* context);
PangeaScriptStatus PangeaScript_CallSplineItemHook(PangeaScriptSplineItemContext* context);
PangeaScriptStatus PangeaScript_CallMapItemHook(PangeaScriptMapItemContext* context);
PangeaScriptStatus PangeaScript_CallObjectFrame(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, PangeaScriptObjectFrameResult* outResult);

void PangeaScript_ResetObjects(void);
PangeaScriptStatus PangeaScript_RegisterObject(const PangeaScriptObjectRegistration* registration, PangeaScriptObjectHandle* outHandle);
bool PangeaScript_UnregisterObject(PangeaScriptObjectHandle handle);
bool PangeaScript_GetObjectPosition(PangeaScriptObjectHandle handle, PangeaScriptVector3* outPosition);
bool PangeaScript_SetObjectPosition(PangeaScriptObjectHandle handle, const PangeaScriptVector3* position);
bool PangeaScript_SetObjectVelocity(PangeaScriptObjectHandle handle, const PangeaScriptVector3* velocity);
bool PangeaScript_DeleteObject(PangeaScriptObjectHandle handle);

PangeaScriptStatus PangeaScript_RegisterNativeItems(const PangeaScriptNativeItem* items, int count);
PangeaScriptStatus PangeaScript_SpawnNative(const char* id, float x, float y, float z);

typedef enum PangeaScriptLogLevel
{
	PANGEA_LOG_INFO = 0,
	PANGEA_LOG_WARN,
	PANGEA_LOG_ERROR
} PangeaScriptLogLevel;

typedef struct PangeaScriptStatusInfo
{
	bool enabled;
	bool configLoaded;
	bool bundleLoaded;
	char activeScriptPath[256];
	char lastError[256];
	int errorCount;
	int budgetExceededCount;
	int hooksCalledCount;
	bool scriptsDisabled;
} PangeaScriptStatusInfo;

void PangeaScript_Log(PangeaScriptLogLevel level, const char* source, const char* message);
void PangeaScript_GetStatusInfo(PangeaScriptStatusInfo* outInfo);

bool PangeaScript_GetStatusEnabled(void);
bool PangeaScript_GetStatusConfigLoaded(void);
bool PangeaScript_GetStatusBundleLoaded(void);
const char* PangeaScript_GetStatusActiveScriptPath(void);
const char* PangeaScript_GetStatusLastError(void);
int PangeaScript_GetStatusErrorCount(void);
int PangeaScript_GetStatusBudgetExceededCount(void);
int PangeaScript_GetStatusHooksCalledCount(void);
bool PangeaScript_GetStatusScriptsDisabled(void);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>

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
} PangeaScriptHook;

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

PangeaScriptStatus PangeaScript_RegisterNativeItems(const PangeaScriptNativeItem* items, int count);
PangeaScriptStatus PangeaScript_SpawnNative(const char* id, float x, float y, float z);

#ifdef __cplusplus
}
#endif

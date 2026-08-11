#pragma once

#include "pangea_script.h"

#define PANGEA_CONFIG_MAX_REMAPS 64
#define PANGEA_CONFIG_MAX_LEVEL_SETTINGS 64
#define PANGEA_CONFIG_MAX_ASSET_DEPENDENCIES 64
#define PANGEA_CONFIG_MAX_CUSTOM_OBJECTS 64
#define PANGEA_CONFIG_MAX_TERRAIN_REPLACEMENTS 128
#define PANGEA_CONFIG_MAX_SPLINE_REPLACEMENTS 128

typedef enum LevelSettingType
{
	LEVEL_SETTING_FLOAT,
	LEVEL_SETTING_INT,
	LEVEL_SETTING_BOOL,
	LEVEL_SETTING_STRING
} LevelSettingType;

typedef struct ItemRemap
{
	int levelNum;
	int fromType;
	int toType;
} ItemRemap;

typedef struct LevelSetting
{
	char key[64];
	LevelSettingType type;
	union
	{
		float floatValue;
		int intValue;
		bool boolValue;
		char stringValue[256];
	};
} LevelSetting;

typedef struct PangeaConfigLevel
{
	bool hasConfig;
	char scriptPath[260];
	ItemRemap itemRemaps[PANGEA_CONFIG_MAX_REMAPS];
	int itemRemapCount;
	LevelSetting levelSettings[PANGEA_CONFIG_MAX_LEVEL_SETTINGS];
	int levelSettingCount;
	PangeaScriptAssetDependency assetDependencies[PANGEA_CONFIG_MAX_ASSET_DEPENDENCIES];
	int assetDependencyCount;
	PangeaScriptCustomObjectDefinition customObjects[PANGEA_CONFIG_MAX_CUSTOM_OBJECTS];
	int customObjectCount;
	PangeaScriptTerrainReplacement terrainReplacements[PANGEA_CONFIG_MAX_TERRAIN_REPLACEMENTS];
	int terrainReplacementCount;
	PangeaScriptSplineReplacement splineReplacements[PANGEA_CONFIG_MAX_SPLINE_REPLACEMENTS];
	int splineReplacementCount;
} PangeaConfigLevel;

typedef struct PangeaConfig
{
	int version;
	PangeaConfigLevel level;
} PangeaConfig;

PangeaScriptStatus PangeaScript_ParseConfig(const char* json, int targetLevelNum, PangeaConfig* outConfig, char* errorMsg, int errorCapacity);

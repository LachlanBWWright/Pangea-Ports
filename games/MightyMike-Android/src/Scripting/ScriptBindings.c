#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"

#include <SDL3/SDL.h>

static int GetAreaLevelNum(int sceneNum, int areaNum)
{
	return (sceneNum * 3) + areaNum;
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "mightymike.bunny",
		.nativeType = 3,
		.category = "pickup",
		.dependencySummary = "bunny objective state, playfield, and object manager",
	},
	{
		.id = "mightymike.healthPow",
		.nativeType = 15,
		.category = "pickup",
		.dependencySummary = "health pickup assets, player state, and object manager",
	},
	{
		.id = "mightymike.key",
		.nativeType = 19,
		.category = "pickup",
		.dependencySummary = "key pickup assets, inventory state, and object manager",
	},
};

static void LogScriptStatus(const char* action, PangeaScriptStatus status)
{
	static Boolean runtimeUnavailableLogged = false;

	if (status == PANGEA_SCRIPT_OK || status == PANGEA_SCRIPT_FILE_NOT_FOUND || status == PANGEA_SCRIPT_NOT_ENABLED)
		return;

	if (status == PANGEA_SCRIPT_RUNTIME_ERROR && runtimeUnavailableLogged)
		return;

	if (status == PANGEA_SCRIPT_RUNTIME_ERROR)
		runtimeUnavailableLogged = true;

	SDL_Log("Mighty Mike scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void MikeScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "MightyMike-Android",
		.gameName = "Mighty Mike",
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
}

void MikeScript_Shutdown(void)
{
	PangeaScript_Shutdown();
}

void MikeScript_LoadAreaConfig(int sceneNum, int areaNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(GetAreaLevelNum(sceneNum, areaNum));
	LogScriptStatus("area config load", status);
}

static void CallAreaHook(PangeaScriptHook hook, int sceneNum, int areaNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = GetAreaLevelNum(sceneNum, areaNum),
		.levelName = NULL,
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

void MikeScript_OnAreaLoad(int sceneNum, int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, sceneNum, areaNum, "onAreaLoad");
}

void MikeScript_OnAreaStart(int sceneNum, int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_START, sceneNum, areaNum, "onAreaStart");
}

void MikeScript_OnAreaFrame(int sceneNum, int areaNum, unsigned int frameNum, float deltaSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = GetAreaLevelNum(sceneNum, areaNum),
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = 0.0f,
	};

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onAreaFrame", status);
}

void MikeScript_OnAreaComplete(int sceneNum, int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, sceneNum, areaNum, "onAreaComplete");
}

void MikeScript_OnAreaUnload(int sceneNum, int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, sceneNum, areaNum, "onAreaUnload");
}

int MikeScript_RemapMapItemType(int sceneNum, int areaNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(GetAreaLevelNum(sceneNum, areaNum), itemType);
}

Boolean MikeScript_OnMapItem(ObjectEntryType* itemPtr, int sceneNum, int areaNum, int itemType)
{
	const unsigned char params[] =
	{
		itemPtr->parm[0],
		itemPtr->parm[1],
		itemPtr->parm[2],
		itemPtr->parm[3],
	};

	PangeaScriptMapItemContext context =
	{
		.levelNum = GetAreaLevelNum(sceneNum, areaNum),
		.sceneNum = sceneNum,
		.areaNum = areaNum,
		.itemType = itemType,
		.x = (float)itemPtr->x,
		.y = (float)itemPtr->y,
		.params = params,
		.paramCount = (int)(sizeof(params) / sizeof(params[0])),
		.handled = false,
		.markInUse = false,
	};

	PangeaScriptStatus status = PangeaScript_CallMapItemHook(&context);
	LogScriptStatus("onMapItem", status);
	return context.handled && context.markInUse;
}

#endif

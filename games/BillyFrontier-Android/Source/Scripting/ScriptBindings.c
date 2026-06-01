#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "billy.peso",
		.nativeType = 36,
		.category = "pickup",
		.dependencySummary = "peso pickup assets, score, and terrain systems",
	},
	{
		.id = "billy.freeLifePow",
		.nativeType = 32,
		.category = "pickup",
		.dependencySummary = "free-life powerup assets, player state, and terrain systems",
	},
	{
		.id = "billy.boost",
		.nativeType = 21,
		.category = "pickup",
		.dependencySummary = "stampede boost assets, stampede speed state, and terrain systems",
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

	SDL_Log("Billy Frontier scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void BillyScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "BillyFrontier-Android",
		.gameName = "Billy Frontier",
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
}

void BillyScript_Shutdown(void)
{
	PangeaScript_Shutdown();
}

void BillyScript_LoadAreaConfig(int areaNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(areaNum);
	LogScriptStatus("area config load", status);
}

static void CallAreaHook(PangeaScriptHook hook, int areaNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = areaNum,
		.levelName = NULL,
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

void BillyScript_OnAreaLoad(int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, areaNum, "onAreaLoad");
}

void BillyScript_OnAreaStart(int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_START, areaNum, "onAreaStart");
}

void BillyScript_OnAreaFrame(int areaNum, unsigned int frameNum, float deltaSeconds, float areaTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = areaNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = areaTimeSeconds,
	};

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onAreaFrame", status);
}

void BillyScript_OnAreaComplete(int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, areaNum, "onAreaComplete");
}

void BillyScript_OnAreaUnload(int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, areaNum, "onAreaUnload");
}

int BillyScript_RemapTerrainItemType(int areaNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(areaNum, itemType);
}

Boolean BillyScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int areaNum, int originalType, int remappedType, float x, float z)
{
	const unsigned char params[] =
	{
		itemPtr->parm[0],
		itemPtr->parm[1],
		itemPtr->parm[2],
		itemPtr->parm[3],
	};

	PangeaScriptTerrainItemContext context =
	{
		.levelNum = areaNum,
		.itemType = originalType,
		.remappedItemType = remappedType,
		.x = x,
		.z = z,
		.flags = itemPtr->flags,
		.params = params,
		.paramCount = (int)(sizeof(params) / sizeof(params[0])),
		.handled = false,
		.markInUse = false,
	};

	PangeaScriptStatus status = PangeaScript_CallTerrainItemHook(&context);
	LogScriptStatus("onTerrainItem", status);
	return context.handled && context.markInUse;
}

Boolean BillyScript_OnSplineItem(SplineItemType* itemPtr, int areaNum, int splineNum)
{
	const unsigned char params[] =
	{
		itemPtr->parm[0],
		itemPtr->parm[1],
		itemPtr->parm[2],
		itemPtr->parm[3],
	};

	PangeaScriptSplineItemContext context =
	{
		.levelNum = areaNum,
		.itemType = itemPtr->type,
		.splineNum = splineNum,
		.placement = itemPtr->placement,
		.params = params,
		.paramCount = (int)(sizeof(params) / sizeof(params[0])),
		.handled = false,
		.markInUse = false,
	};

	PangeaScriptStatus status = PangeaScript_CallSplineItemHook(&context);
	LogScriptStatus("onSplineItem", status);
	return context.handled && context.markInUse;
}

#endif

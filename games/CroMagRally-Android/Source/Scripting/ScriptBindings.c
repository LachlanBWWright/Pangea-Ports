#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "cromag.pow",
		.nativeType = 5,
		.category = "pickup",
		.dependencySummary = "powerup trigger assets, player inventory, and terrain systems",
	},
	{
		.id = "cromag.token",
		.nativeType = 11,
		.category = "pickup",
		.dependencySummary = "token scoring state, infobar, and terrain systems",
	},
	{
		.id = "cromag.stickyTiresPow",
		.nativeType = 12,
		.category = "pickup",
		.dependencySummary = "sticky tire pickup assets, vehicle physics state, and terrain systems",
	},
	{
		.id = "cromag.suspensionPow",
		.nativeType = 13,
		.category = "pickup",
		.dependencySummary = "suspension pickup assets, vehicle physics state, and terrain systems",
	},
	{
		.id = "cromag.invisibilityPow",
		.nativeType = 29,
		.category = "pickup",
		.dependencySummary = "invisibility pickup assets, player visibility state, and terrain systems",
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

	SDL_Log("Cro-Mag Rally scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void CroMagScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "CroMagRally-Android",
		.gameName = "Cro-Mag Rally",
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
}

void CroMagScript_Shutdown(void)
{
	PangeaScript_Shutdown();
}

void CroMagScript_LoadTrackConfig(int trackNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(trackNum);
	LogScriptStatus("track config load", status);
}

static void CallRaceHook(PangeaScriptHook hook, int trackNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = trackNum,
		.levelName = NULL,
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

void CroMagScript_OnRaceLoad(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, trackNum, "onRaceLoad");
}

void CroMagScript_OnRaceStart(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_START, trackNum, "onRaceStart");
}

void CroMagScript_OnRaceFrame(int trackNum, unsigned int frameNum, float deltaSeconds, float raceTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = trackNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = raceTimeSeconds,
	};

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onRaceFrame", status);
}

void CroMagScript_OnRaceComplete(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, trackNum, "onRaceComplete");
}

void CroMagScript_OnRaceUnload(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, trackNum, "onRaceUnload");
}

int CroMagScript_RemapTerrainItemType(int trackNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(trackNum, itemType);
}

Boolean CroMagScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int trackNum, int playerNum, int originalType, int remappedType, float x, float z)
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
		.levelNum = trackNum,
		.itemType = originalType,
		.remappedItemType = remappedType,
		.playerNum = playerNum,
		.networked = false,
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

#endif

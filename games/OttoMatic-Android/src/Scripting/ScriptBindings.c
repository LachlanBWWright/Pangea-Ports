#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "ottomatic.human",
		.nativeType = 4,
		.category = "npc",
		.dependencySummary = "human rescue state, skeletons, terrain, and player systems",
	},
	{
		.id = "ottomatic.powerupPod",
		.nativeType = 6,
		.category = "pickup",
		.dependencySummary = "powerup pod assets, effects, and terrain systems",
	},
	{
		.id = "ottomatic.checkpoint",
		.nativeType = 27,
		.category = "trigger",
		.dependencySummary = "checkpoint state and terrain systems",
	},
	{
		.id = "ottomatic.teleporter",
		.nativeType = 57,
		.category = "trigger",
		.dependencySummary = "teleporter state, terrain, and level transition systems",
	},
};

static PangeaScriptFrameContext gCurrentFrameContext;
static ObjNode* gCurrentScriptObject;
static Boolean gCurrentScriptObjectUsesGlobals;

static const char* const kHumanFarmerTags[] =
{
	"ottomatic.human",
	"ottomatic.human.farmer",
};

static const char* const kHumanBeewomanTags[] =
{
	"ottomatic.human",
	"ottomatic.human.beewoman",
};

static const char* const kHumanScientistTags[] =
{
	"ottomatic.human",
	"ottomatic.human.scientist",
};

static const char* const kHumanSkirtladyTags[] =
{
	"ottomatic.human",
	"ottomatic.human.skirtlady",
};

static const char* const* GetHumanTags(int humanType, int* outTagCount)
{
	if (!outTagCount)
		return NULL;

	*outTagCount = 2;
	switch (humanType)
	{
		case HUMAN_TYPE_FARMER:
			return kHumanFarmerTags;

		case HUMAN_TYPE_BEEWOMAN:
			return kHumanBeewomanTags;

		case HUMAN_TYPE_SCIENTIST:
			return kHumanScientistTags;

		case HUMAN_TYPE_SKIRTLADY:
		default:
			return kHumanSkirtladyTags;
	}
}

static bool OttoHumanGetPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* human = (ObjNode*) nativeObject;
	if (!human || !outPosition)
		return false;

	if (human == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		outPosition->x = gCoord.x;
		outPosition->y = gCoord.y;
		outPosition->z = gCoord.z;
		return true;
	}

	outPosition->x = human->Coord.x;
	outPosition->y = human->Coord.y;
	outPosition->z = human->Coord.z;
	return true;
}

static bool OttoHumanSetPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* human = (ObjNode*) nativeObject;
	if (!human || !position)
		return false;

	human->Coord.x = position->x;
	human->Coord.y = position->y;
	human->Coord.z = position->z;

	if (human == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		gCoord.x = position->x;
		gCoord.y = position->y;
		gCoord.z = position->z;
	}

	return true;
}

static bool OttoHumanSetVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* human = (ObjNode*) nativeObject;
	if (!human || !velocity)
		return false;

	human->Delta.x = velocity->x;
	human->Delta.y = velocity->y;
	human->Delta.z = velocity->z;

	if (human == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		gDelta.x = velocity->x;
		gDelta.y = velocity->y;
		gDelta.z = velocity->z;
	}

	return true;
}

static bool OttoHumanDelete(void* nativeObject)
{
	ObjNode* human = (ObjNode*) nativeObject;
	if (!human || human == gCurrentScriptObject)
		return false;

	OttoScript_UnregisterHuman(human);
	DeleteObject(human);
	return true;
}

static const PangeaScriptObjectOps kOttoHumanObjectOps =
{
	.getPosition = OttoHumanGetPosition,
	.setPosition = OttoHumanSetPosition,
	.setVelocity = OttoHumanSetVelocity,
	.deleteObject = OttoHumanDelete,
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

	SDL_Log("Otto Matic scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void OttoScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "OttoMatic-Android",
		.gameName = "Otto Matic",
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);
	gCurrentFrameContext = (PangeaScriptFrameContext){0};
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
}

void OttoScript_Shutdown(void)
{
	gCurrentFrameContext = (PangeaScriptFrameContext){0};
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;
	PangeaScript_Shutdown();
}

void OttoScript_LoadLevelConfig(int levelNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(levelNum);
	LogScriptStatus("level config load", status);
}

static void CallLevelHook(PangeaScriptHook hook, int levelNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = levelNum,
		.levelName = NULL,
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

void OttoScript_OnLevelLoad(int levelNum)
{
	PangeaScript_ResetObjects();
	gCurrentFrameContext = (PangeaScriptFrameContext)
	{
		.levelNum = levelNum,
	};
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, levelNum, "onLevelLoad");
}

void OttoScript_OnLevelStart(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, levelNum, "onLevelStart");
}

void OttoScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	gCurrentFrameContext = context;

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onFrame", status);
}

void OttoScript_OnLevelComplete(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, levelNum, "onLevelComplete");
}

void OttoScript_OnLevelUnload(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, levelNum, "onLevelUnload");
	PangeaScript_ResetObjects();
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;
}

int OttoScript_RemapTerrainItemType(int levelNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(levelNum, itemType);
}

Boolean OttoScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z)
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
		.levelNum = levelNum,
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

Boolean OttoScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum)
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
		.levelNum = levelNum,
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

void OttoScript_RegisterHuman(ObjNode* human)
{
	int tagCount = 0;
	const char* const* tags;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptObjectRegistration registration;
	PangeaScriptStatus status;

	if (!human)
		return;

	tags = GetHumanTags(human->HumanType, &tagCount);
	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = human,
		.ops = &kOttoHumanObjectOps,
		.tags = tags,
		.tagCount = tagCount,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	if (status == PANGEA_SCRIPT_OK)
	{
		human->ScriptObjectID = handle.id;
		human->ScriptObjectGeneration = handle.generation;
	}
	else
	{
		human->ScriptObjectID = 0;
		human->ScriptObjectGeneration = 0;
	}

	LogScriptStatus("human registration", status);
}

void OttoScript_UnregisterHuman(ObjNode* human)
{
	PangeaScriptObjectHandle handle;

	if (!human || human->ScriptObjectID <= 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = human->ScriptObjectID,
		.generation = human->ScriptObjectGeneration,
	};

	(void) PangeaScript_UnregisterObject(handle);
	human->ScriptObjectID = 0;
	human->ScriptObjectGeneration = 0;
	human->ScriptVisualOffset.x = 0.0f;
	human->ScriptVisualOffset.y = 0.0f;
	human->ScriptVisualOffset.z = 0.0f;

	if (gCurrentScriptObject == human)
	{
		gCurrentScriptObject = NULL;
		gCurrentScriptObjectUsesGlobals = false;
	}
}

void OttoScript_RunHumanObjectFrame(ObjNode* human, Boolean usesGlobals)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectFrameResult result = {0};
	PangeaScriptStatus status;

	if (!human)
		return;

	human->ScriptVisualOffset.x = 0.0f;
	human->ScriptVisualOffset.y = 0.0f;
	human->ScriptVisualOffset.z = 0.0f;

	if (human->ScriptObjectID <= 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = human->ScriptObjectID,
		.generation = human->ScriptObjectGeneration,
	};

	gCurrentScriptObject = human;
	gCurrentScriptObjectUsesGlobals = usesGlobals;
	status = PangeaScript_CallObjectFrame(handle, &gCurrentFrameContext, &result);
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;

	if (status == PANGEA_SCRIPT_OK && result.hasPositionOffset)
	{
		human->ScriptVisualOffset.x = result.positionOffset.x;
		human->ScriptVisualOffset.y = result.positionOffset.y;
		human->ScriptVisualOffset.z = result.positionOffset.z;
	}

	LogScriptStatus("onObjectFrame", status);
}

void OttoScript_ApplyHumanVisualOffset(ObjNode* human)
{
	OGLPoint3D baseCoord;

	if (!human)
		return;

	if (human->ScriptVisualOffset.x == 0.0f &&
		human->ScriptVisualOffset.y == 0.0f &&
		human->ScriptVisualOffset.z == 0.0f)
	{
		return;
	}

	baseCoord = human->Coord;
	human->Coord.x += human->ScriptVisualOffset.x;
	human->Coord.y += human->ScriptVisualOffset.y;
	human->Coord.z += human->ScriptVisualOffset.z;
	UpdateObjectTransforms(human);
	human->Coord = baseCoord;
}

#endif

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include <stdio.h>
#include <string.h>

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

static bool OttoObjectGetPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outPosition)
		return false;

	if (node == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		outPosition->x = gCoord.x;
		outPosition->y = gCoord.y;
		outPosition->z = gCoord.z;
		return true;
	}

	outPosition->x = node->Coord.x;
	outPosition->y = node->Coord.y;
	outPosition->z = node->Coord.z;
	return true;
}

static bool OttoObjectSetPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !position)
		return false;

	node->Coord.x = position->x;
	node->Coord.y = position->y;
	node->Coord.z = position->z;

	if (node == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		gCoord.x = position->x;
		gCoord.y = position->y;
		gCoord.z = position->z;
	}

	return true;
}

static bool OttoObjectSetVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !velocity)
		return false;

	node->Delta.x = velocity->x;
	node->Delta.y = velocity->y;
	node->Delta.z = velocity->z;

	if (node == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		gDelta.x = velocity->x;
		gDelta.y = velocity->y;
		gDelta.z = velocity->z;
	}

	return true;
}

static bool OttoObjectDelete(void* nativeObject)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || node == gCurrentScriptObject)
		return false;

	OttoScript_UnregisterObjectNode(node);
	DeleteObject(node);
	return true;
}

static const PangeaScriptObjectOps kOttoObjectNodeOps =
{
	.getPosition = OttoObjectGetPosition,
	.setPosition = OttoObjectSetPosition,
	.setVelocity = OttoObjectSetVelocity,
	.deleteObject = OttoObjectDelete,
};

static void LogScriptStatus(const char* action, PangeaScriptStatus status)
{
	static PangeaScriptStatus lastStatus = PANGEA_SCRIPT_OK;
	static char lastAction[64];
	static char lastError[256];
	const char* error;

	if (status == PANGEA_SCRIPT_OK || status == PANGEA_SCRIPT_FILE_NOT_FOUND || status == PANGEA_SCRIPT_NOT_ENABLED)
		return;

	error = PangeaScript_GetLastError();
	if (!error)
		error = "";

	if (status == lastStatus && strcmp(action, lastAction) == 0 && strcmp(error, lastError) == 0)
		return;

	lastStatus = status;
	snprintf(lastAction, sizeof(lastAction), "%s", action);
	snprintf(lastError, sizeof(lastError), "%s", error);

	SDL_Log("Otto Matic scripting %s failed: %s", action, error);
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

void OttoScript_RegisterObjectNode(ObjNode* theNode, PangeaScriptCapabilityLevel capabilityLevel, const char* const* tags, int tagCount)
{
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptObjectRegistration registration;
	PangeaScriptStatus status;

	if (!theNode)
		return;

	if (theNode->ScriptObjectID > 0)
		return;

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = theNode,
		.ops = &kOttoObjectNodeOps,
		.tags = tags,
		.tagCount = tagCount,
		.capabilityLevel = capabilityLevel,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	if (status == PANGEA_SCRIPT_OK)
	{
		theNode->ScriptObjectID = handle.id;
		theNode->ScriptObjectGeneration = handle.generation;
	}
	else
	{
		theNode->ScriptObjectID = 0;
		theNode->ScriptObjectGeneration = 0;
	}

	LogScriptStatus("object registration", status);
}

void OttoScript_UnregisterObjectNode(ObjNode* theNode)
{
	PangeaScriptObjectHandle handle;

	if (!theNode || theNode->ScriptObjectID <= 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = theNode->ScriptObjectID,
		.generation = theNode->ScriptObjectGeneration,
	};

	(void) PangeaScript_UnregisterObject(handle);
	theNode->ScriptObjectID = 0;
	theNode->ScriptObjectGeneration = 0;
	theNode->ScriptVisualOffset.x = 0.0f;
	theNode->ScriptVisualOffset.y = 0.0f;
	theNode->ScriptVisualOffset.z = 0.0f;

	if (gCurrentScriptObject == theNode)
	{
		gCurrentScriptObject = NULL;
		gCurrentScriptObjectUsesGlobals = false;
	}
}

void OttoScript_RunObjectFrame(ObjNode* theNode, Boolean usesGlobals)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectFrameResult result = {0};
	PangeaScriptStatus status;

	if (!theNode)
		return;

	theNode->ScriptVisualOffset.x = 0.0f;
	theNode->ScriptVisualOffset.y = 0.0f;
	theNode->ScriptVisualOffset.z = 0.0f;

	if (theNode->ScriptObjectID <= 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = theNode->ScriptObjectID,
		.generation = theNode->ScriptObjectGeneration,
	};

	gCurrentScriptObject = theNode;
	gCurrentScriptObjectUsesGlobals = usesGlobals;
	status = PangeaScript_CallObjectFrame(handle, &gCurrentFrameContext, &result);

	const char* error = PangeaScript_GetLastError();
	if (status == PANGEA_SCRIPT_BAD_ARGUMENT && error && strstr(error, "stale object handle"))
	{
		theNode->ScriptObjectID = 0;
		theNode->ScriptObjectGeneration = 0;
		if (theNode->Slot == HUMAN_SLOT)
		{
			OttoScript_RegisterHuman(theNode);
		}

		if (theNode->ScriptObjectID > 0)
		{
			handle = (PangeaScriptObjectHandle)
			{
				.id = theNode->ScriptObjectID,
				.generation = theNode->ScriptObjectGeneration,
			};
			status = PangeaScript_CallObjectFrame(handle, &gCurrentFrameContext, &result);
		}
	}

	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;

	if (status == PANGEA_SCRIPT_OK && result.hasPositionOffset)
	{
		theNode->ScriptVisualOffset.x = result.positionOffset.x;
		theNode->ScriptVisualOffset.y = result.positionOffset.y;
		theNode->ScriptVisualOffset.z = result.positionOffset.z;
	}

	LogScriptStatus("onObjectFrame", status);
}

void OttoScript_ApplyObjectVisualOffset(ObjNode* theNode)
{
	OGLPoint3D baseCoord;

	if (!theNode)
		return;

	if (theNode->ScriptVisualOffset.x == 0.0f &&
		theNode->ScriptVisualOffset.y == 0.0f &&
		theNode->ScriptVisualOffset.z == 0.0f)
	{
		return;
	}

	baseCoord = theNode->Coord;
	theNode->Coord.x += theNode->ScriptVisualOffset.x;
	theNode->Coord.y += theNode->ScriptVisualOffset.y;
	theNode->Coord.z += theNode->ScriptVisualOffset.z;
	UpdateObjectTransforms(theNode);
	theNode->Coord = baseCoord;
}

void OttoScript_RegisterHuman(ObjNode* human)
{
	int tagCount = 0;
	const char* const* tags = GetHumanTags(human->HumanType, &tagCount);
	OttoScript_RegisterObjectNode(human, PANGEA_SCRIPT_CAPABILITY_FULL, tags, tagCount);
}

void OttoScript_UnregisterHuman(ObjNode* human)
{
	OttoScript_UnregisterObjectNode(human);
}

void OttoScript_RunHumanObjectFrame(ObjNode* human, Boolean usesGlobals)
{
	OttoScript_RunObjectFrame(human, usesGlobals);
}

void OttoScript_ApplyHumanVisualOffset(ObjNode* human)
{
	OttoScript_ApplyObjectVisualOffset(human);
}

#endif

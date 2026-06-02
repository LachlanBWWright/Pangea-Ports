#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "structs.h"

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;
static const char* const kBillyPlayerTags[] = { "billy.player" };

static bool BillyScript_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = obj->Coord.x;
	outPosition->y = obj->Coord.y;
	outPosition->z = obj->Coord.z;
	return true;
}

static bool BillyScript_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	return true;
}

static bool BillyScript_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	return true;
}

static bool BillyScript_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	BillyScript_UnregisterPlayerObject(obj);
	DeleteObject(obj);
	return true;
}

static const PangeaScriptObjectOps kBillyPlayerObjectOps =
{
	.getPosition = BillyScript_GetObjectPosition,
	.setPosition = BillyScript_SetObjectPosition,
	.setVelocity = BillyScript_SetObjectVelocity,
	.deleteObject = BillyScript_DeleteObject,
};

void BillyScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void BillyScript_ResetObjectRegistry(void)
{
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	PangeaScript_ResetObjects();
}

void BillyScript_RegisterPlayerObject(ObjNode* playerObj)
{
	PangeaScriptObjectRegistration registration;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;

	if (!playerObj)
		return;

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = playerObj,
		.ops = &kBillyPlayerObjectOps,
		.tags = kBillyPlayerTags,
		.tagCount = 1,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	playerObj->ScriptVisualOffset = (OGLVector3D){0};
	if (status == PANGEA_SCRIPT_OK)
	{
		playerObj->ScriptObjectID = handle.id;
		playerObj->ScriptObjectGeneration = handle.generation;
	}
	else
	{
		playerObj->ScriptObjectID = 0;
		playerObj->ScriptObjectGeneration = 0;
	}
	LogScriptStatus("player registration", status);
}

void BillyScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	PangeaScriptObjectHandle handle;

	if (!playerObj || playerObj->ScriptObjectID == 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = (int) playerObj->ScriptObjectID,
		.generation = playerObj->ScriptObjectGeneration,
	};
	(void) PangeaScript_UnregisterObject(handle);
	playerObj->ScriptObjectID = 0;
	playerObj->ScriptObjectGeneration = 0;
	playerObj->ScriptVisualOffset = (OGLVector3D){0};
}

void BillyScript_ApplyObjectScripting(ObjNode* obj)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectFrameResult result = {0};
	PangeaScriptStatus status;
	OGLPoint3D baseCoord;

	if (!obj || obj->ScriptObjectID == 0 || obj->CType == INVALID_NODE_FLAG)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = (int) obj->ScriptObjectID,
		.generation = obj->ScriptObjectGeneration,
	};

	status = PangeaScript_CallObjectFrame(handle, &gScriptFrameContext, &result);
	LogScriptStatus("onObjectFrame", status);
	if (status != PANGEA_SCRIPT_OK || obj->CType == INVALID_NODE_FLAG)
	{
		obj->ScriptVisualOffset = (OGLVector3D){0};
		return;
	}

	UpdateObjectTransforms(obj);
	CalcObjectBoxFromNode(obj);
	obj->ScriptVisualOffset = (OGLVector3D){0};
	if (!result.hasPositionOffset)
		return;

	obj->ScriptVisualOffset.x = result.positionOffset.x;
	obj->ScriptVisualOffset.y = result.positionOffset.y;
	obj->ScriptVisualOffset.z = result.positionOffset.z;
	baseCoord = obj->Coord;
	obj->Coord.x += result.positionOffset.x;
	obj->Coord.y += result.positionOffset.y;
	obj->Coord.z += result.positionOffset.z;
	UpdateObjectTransforms(obj);
	obj->Coord = baseCoord;
}

void BillyScript_RunObjectFrame(ObjNode* obj)
{
	BillyScript_ApplyObjectScripting(obj);
}

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
	BillyScript_ResetObjectRegistry();
}

void BillyScript_Shutdown(void)
{
	BillyScript_ResetObjectRegistry();
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
	PangeaScript_ResetObjects();
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
	BillyScript_CacheFrameContext(&context);

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
	PangeaScript_ResetObjects();
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

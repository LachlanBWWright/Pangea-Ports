#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"

#include "externs.h"
#include "myglobals.h"
#include "object.h"

#include <SDL3/SDL.h>

static PangeaScriptFrameContext gScriptFrameContext;
static const char* const kMikePlayerTags[] = { "mightymike.player" };

static int32_t MikeScript_FloatToFixed(float value)
{
	return (int32_t) SDL_roundf(value * 65536.0f);
}

static float MikeScript_FixedToFloat(int32_t value)
{
	return (float) value / 65536.0f;
}

static void MikeScript_SyncPlayerGlobals(ObjNode* obj)
{
	if (obj != gMyNodePtr)
		return;

	gMyX = obj->X.Int;
	gMyY = obj->Y.Int;
	gMyDX = obj->DX;
	gMyDY = obj->DY;
	gMySumDX = obj->DX;
	gMySumDY = obj->DY;
}

static bool MikeScript_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = MikeScript_FixedToFloat(obj->X.L);
	outPosition->y = MikeScript_FixedToFloat(obj->Y.L);
	outPosition->z = 0.0f;
	return true;
}

static bool MikeScript_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->X.L = MikeScript_FloatToFixed(position->x);
	obj->Y.L = MikeScript_FloatToFixed(position->y);
	obj->OldX = obj->X;
	obj->OldY = obj->Y;
	CalcObjectBox2(obj);
	MikeScript_SyncPlayerGlobals(obj);
	return true;
}

static bool MikeScript_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->DX = MikeScript_FloatToFixed(velocity->x);
	obj->DY = MikeScript_FloatToFixed(velocity->y);
	MikeScript_SyncPlayerGlobals(obj);
	return true;
}

static bool MikeScript_DeletePlayerObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	MikeScript_UnregisterPlayerObject(obj);
	DeleteObject(obj);
	return true;
}

static const PangeaScriptObjectOps kMikePlayerObjectOps =
{
	.getPosition = MikeScript_GetObjectPosition,
	.setPosition = MikeScript_SetObjectPosition,
	.setVelocity = MikeScript_SetObjectVelocity,
	.deleteObject = MikeScript_DeletePlayerObject,
};

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

void MikeScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void MikeScript_ResetObjectRegistry(void)
{
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	PangeaScript_ResetObjects();
}

void MikeScript_RegisterPlayerObject(ObjNode* playerObj)
{
	PangeaScriptObjectRegistration registration;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;

	if (!playerObj)
		return;

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = playerObj,
		.ops = &kMikePlayerObjectOps,
		.tags = kMikePlayerTags,
		.tagCount = 1,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	playerObj->ScriptVisualOffsetX = 0;
	playerObj->ScriptVisualOffsetY = 0;
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

void MikeScript_UnregisterPlayerObject(ObjNode* playerObj)
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
	playerObj->ScriptVisualOffsetX = 0;
	playerObj->ScriptVisualOffsetY = 0;
}

void MikeScript_RunObjectFrame(ObjNode* obj)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectFrameResult result = {0};
	PangeaScriptStatus status;

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
		obj->ScriptVisualOffsetX = 0;
		obj->ScriptVisualOffsetY = 0;
		return;
	}

	CalcObjectBox2(obj);
	MikeScript_SyncPlayerGlobals(obj);
	obj->ScriptVisualOffsetX = 0;
	obj->ScriptVisualOffsetY = 0;
	if (!result.hasPositionOffset)
		return;

	obj->ScriptVisualOffsetX = MikeScript_FloatToFixed(result.positionOffset.x);
	obj->ScriptVisualOffsetY = MikeScript_FloatToFixed(result.positionOffset.y);
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
	MikeScript_ResetObjectRegistry();
}

void MikeScript_Shutdown(void)
{
	MikeScript_ResetObjectRegistry();
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
	PangeaScript_ResetObjects();
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
	MikeScript_CacheFrameContext(&context);

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
	PangeaScript_ResetObjects();
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

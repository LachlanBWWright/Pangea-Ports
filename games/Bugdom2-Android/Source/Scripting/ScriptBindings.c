#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;
static const char* const kBugdom2PlayerTags[] = { "bugdom2.player" };

static bool Bugdom2Script_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = obj->Coord.x;
	outPosition->y = obj->Coord.y;
	outPosition->z = obj->Coord.z;
	return true;
}

static bool Bugdom2Script_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	return true;
}

static bool Bugdom2Script_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	return true;
}

static bool Bugdom2Script_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	Bugdom2Script_UnregisterObject(obj);
	DeleteObject(obj);
	return true;
}

static const PangeaScriptObjectOps kBugdom2PlayerObjectOps =
{
	.getPosition = Bugdom2Script_GetObjectPosition,
	.setPosition = Bugdom2Script_SetObjectPosition,
	.setVelocity = Bugdom2Script_SetObjectVelocity,
	.deleteObject = Bugdom2Script_DeleteObject,
};

void Bugdom2Script_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void Bugdom2Script_ResetObjectRegistry(void)
{
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	PangeaScript_ResetObjects();
}

void Bugdom2Script_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
{
	PangeaScriptObjectRegistration registration;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	const char* tags[4];
	int tagCount = 0;

	if (!obj)
		return;

	if (obj->ScriptObjectID > 0)
		return;

	if (nativeId)
	{
		tags[tagCount++] = nativeId;
	}
	if (category)
	{
		tags[tagCount++] = category;
	}

	Boolean isCollectible = false;
	if (nativeId && (strcmp(nativeId, "bugdom2.dcell") == 0 ||
	                 strcmp(nativeId, "bugdom2.gliderPart") == 0 ||
	                 strcmp(nativeId, "bugdom2.hobobag") == 0 ||
	                 strcmp(nativeId, "bugdom2.acorn") == 0))
	{
		isCollectible = true;
	}
	else if (obj->Kind == PICKUP_KIND_POW)
	{
		if (obj->Special[0] == POW_KIND_GREENCLOVER ||
		    obj->Special[0] == POW_KIND_BLUECLOVER ||
		    obj->Special[0] == POW_KIND_GOLDCLOVER ||
		    obj->Special[0] == POW_KIND_REDKEY ||
		    obj->Special[0] == POW_KIND_GREENKEY ||
		    obj->Special[0] == POW_KIND_BLUEKEY)
		{
			isCollectible = true;
		}
	}

	if (isCollectible)
	{
		tags[tagCount++] = "bugdom2.collectible";
	}

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = obj,
		.ops = &kBugdom2PlayerObjectOps,
		.tags = tags,
		.tagCount = tagCount,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	obj->ScriptVisualOffset = (OGLVector3D){0};
	if (status == PANGEA_SCRIPT_OK)
	{
		obj->ScriptObjectID = handle.id;
		obj->ScriptObjectGeneration = (int) handle.generation;
	}
	else
	{
		obj->ScriptObjectID = 0;
		obj->ScriptObjectGeneration = 0;
	}
	LogScriptStatus("object registration", status);
}

void Bugdom2Script_UnregisterObject(ObjNode* obj)
{
	PangeaScriptObjectHandle handle;

	if (!obj || obj->ScriptObjectID == 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = obj->ScriptObjectID,
		.generation = (uint32_t) obj->ScriptObjectGeneration,
	};
	(void) PangeaScript_UnregisterObject(handle);
	obj->ScriptObjectID = 0;
	obj->ScriptObjectGeneration = 0;
	obj->ScriptVisualOffset = (OGLVector3D){0};
}

void Bugdom2Script_RegisterPlayerObject(ObjNode* playerObj)
{
	Bugdom2Script_RegisterObject(playerObj, "bugdom2.player", "player");
}

void Bugdom2Script_UnregisterPlayerObject(ObjNode* playerObj)
{
	Bugdom2Script_UnregisterObject(playerObj);
}

void Bugdom2Script_ApplyObjectScripting(ObjNode* obj)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectFrameResult result = {0};
	PangeaScriptStatus status;
	OGLPoint3D baseCoord;

	if (!obj || obj->ScriptObjectID == 0 || obj->CType == INVALID_NODE_FLAG)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = obj->ScriptObjectID,
		.generation = (uint32_t) obj->ScriptObjectGeneration,
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

void Bugdom2Script_RunObjectFrame(ObjNode* obj)
{
	Bugdom2Script_ApplyObjectScripting(obj);
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "bugdom2.powerup",
		.nativeType = 35,
		.category = "powerup",
		.dependencySummary = "global powerup models and terrain/collision systems",
	},
	{
		.id = "bugdom2.dcell",
		.nativeType = 49,
		.category = "pickup",
		.dependencySummary = "global pickup models and terrain/collision systems",
	},
	{
		.id = "bugdom2.gliderPart",
		.nativeType = 85,
		.category = "pickup",
		.dependencySummary = "level-specific glider part models and terrain/collision systems",
	},
};

typedef struct Bugdom2NamedAsset
{
	const char* id;
	int nativeId;
} Bugdom2NamedAsset;

static const Bugdom2NamedAsset kSkeletonDependencies[] =
{
	{ "skipExplore", SKELETON_TYPE_SKIP_EXPLORE },
	{ "skipTunnel", SKELETON_TYPE_SKIP_TUNNEL },
	{ "skipTitle", SKELETON_TYPE_SKIP_TITLE },
	{ "snail", SKELETON_TYPE_SNAIL },
	{ "gnome", SKELETON_TYPE_GNOME },
	{ "houseFly", SKELETON_TYPE_HOUSEFLY },
	{ "evilPlant", SKELETON_TYPE_EVILPLANT },
	{ "chipmunk", SKELETON_TYPE_CHIPMUNK },
	{ "snakeHead", SKELETON_TYPE_SNAKEHEAD },
	{ "buddyBug", SKELETON_TYPE_BUDDYBUG },
	{ "checkpoint", SKELETON_TYPE_CHECKPOINT },
	{ "flea", SKELETON_TYPE_FLEA },
	{ "tick", SKELETON_TYPE_TICK },
	{ "mouseTrap", SKELETON_TYPE_MOUSETRAP },
	{ "mouse", SKELETON_TYPE_MOUSE },
	{ "toySoldier", SKELETON_TYPE_TOYSOLDIER },
	{ "otto", SKELETON_TYPE_OTTO },
	{ "bumbleBee", SKELETON_TYPE_BUMBLEBEE },
	{ "hoboBag", SKELETON_TYPE_HOBOBAG },
	{ "dragonfly", SKELETON_TYPE_DRAGONFLY },
	{ "frog", SKELETON_TYPE_FROG },
	{ "moth", SKELETON_TYPE_MOTH },
	{ "computerBug", SKELETON_TYPE_COMPUTERBUG },
	{ "roach", SKELETON_TYPE_ROACH },
	{ "ant", SKELETON_TYPE_ANT },
	{ "fish", SKELETON_TYPE_FISH },
};

static int FindNamedAsset(const Bugdom2NamedAsset* assets, int count, const char* id)
{
	if (!assets || !id)
		return -1;

	for (int i = 0; i < count; i++)
	{
		if (strcmp(assets[i].id, id) == 0)
			return assets[i].nativeId;
	}

	return -1;
}

static void LogScriptStatus(const char* action, PangeaScriptStatus status)
{
	static Boolean runtimeUnavailableLogged = false;

	if (status == PANGEA_SCRIPT_OK || status == PANGEA_SCRIPT_FILE_NOT_FOUND || status == PANGEA_SCRIPT_NOT_ENABLED)
		return;

	if (status == PANGEA_SCRIPT_RUNTIME_ERROR && runtimeUnavailableLogged)
		return;

	if (status == PANGEA_SCRIPT_RUNTIME_ERROR)
		runtimeUnavailableLogged = true;

	SDL_Log("Bugdom2 scripting %s failed: %s", action, PangeaScript_GetLastError());
}

static PangeaScriptStatus Bugdom2Script_SpawnNativeCallback(const char* id, float x, float y, float z, int subtype, int amount, PangeaScriptObjectHandle* outHandle)
{
	(void) amount;
	if (strcmp(id, "bugdom2.dcell") == 0)
	{
		if (gBG3DContainerList[MODEL_GROUP_LEVELSPECIFIC] == nil)
		{
			return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		}

		ObjNode* newObj;
		gNewObjectDefinition.genre = 0;
		gNewObjectDefinition.group = MODEL_GROUP_LEVELSPECIFIC;
		gNewObjectDefinition.type = PLAYROOM_ObjType_DCell;
		gNewObjectDefinition.scale = 1.0;
		gNewObjectDefinition.coord.x = x;
		gNewObjectDefinition.coord.z = z;
		gNewObjectDefinition.coord.y = y;
		gNewObjectDefinition.flags = gAutoFadeStatusBits;
		gNewObjectDefinition.slot = 358;
		gNewObjectDefinition.moveCall = MoveStaticObject;
		gNewObjectDefinition.rot = RandomFloat() * PI2;
		newObj = MakeNewDisplayGroupObject(&gNewObjectDefinition);
		if (!newObj)
			return PANGEA_SCRIPT_RUNTIME_ERROR;

		newObj->TerrainItemPtr = NULL;
		newObj->CType = CTYPE_MISC | CTYPE_BLOCKCAMERA | CTYPE_BLOCKSHADOW;
		newObj->CBits = CBITS_ALLSOLID;
		CreateCollisionBoxFromBoundingBox(newObj, 1, 1);

		Bugdom2Script_RegisterObject(newObj, "bugdom2.dcell", "pickup");

		if (outHandle)
		{
			outHandle->id = newObj->ScriptObjectID;
			outHandle->generation = (uint32_t) newObj->ScriptObjectGeneration;
		}
		return PANGEA_SCRIPT_OK;
	}

	if (strcmp(id, "bugdom2.powerup") == 0)
	{
		int powKind = subtype >= 0 ? subtype : 0;
		OGLPoint3D where = { x, y, z };
		ObjNode* pow = MakePOW(powKind, &where);
		if (!pow)
			return PANGEA_SCRIPT_RUNTIME_ERROR;

		pow->TerrainItemPtr = NULL;
		Bugdom2Script_RegisterObject(pow, "bugdom2.powerup", "powerup");

		if (outHandle)
		{
			outHandle->id = pow->ScriptObjectID;
			outHandle->generation = (uint32_t) pow->ScriptObjectGeneration;
		}
		return PANGEA_SCRIPT_OK;
	}

	return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
}

void Bugdom2Script_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "Bugdom2-Android",
		.gameName = "Bugdom 2",
		.spawnNative = Bugdom2Script_SpawnNativeCallback,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	Bugdom2Script_ResetObjectRegistry();
}

void Bugdom2Script_Shutdown(void)
{
	Bugdom2Script_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void Bugdom2Script_LoadLevelConfig(int levelNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(levelNum);
	LogScriptStatus("level config load", status);
}

void Bugdom2Script_LoadLevelAssetDependencies(int levelNum)
{
	int dependencyCount = PangeaScript_GetLevelAssetDependencyCount();

	for (int i = 0; i < dependencyCount; i++)
	{
		PangeaScriptAssetDependency dependency;
		if (!PangeaScript_GetLevelAssetDependency(i, &dependency))
			continue;

		if (strcmp(dependency.kind, "skeleton") == 0)
		{
			int skeletonType = FindNamedAsset(kSkeletonDependencies, (int)(sizeof(kSkeletonDependencies) / sizeof(kSkeletonDependencies[0])), dependency.id);
			if (skeletonType < 0)
			{
				SDL_Log("Bugdom2 scripting asset dependency ignored: unknown skeleton '%s' for level %d", dependency.id, levelNum);
				continue;
			}
			LoadASkeleton((Byte)skeletonType);
			continue;
		}

		if (strcmp(dependency.kind, "modelGroup") == 0 && strcmp(dependency.id, "foliage") == 0)
		{
			if (gBG3DContainerList[MODEL_GROUP_FOLIAGE] == nil)
				LoadFoliage();
			continue;
		}

		if (strcmp(dependency.kind, "modelGroup") == 0 && strcmp(dependency.id, "global") == 0)
		{
			continue;
		}

		SDL_Log("Bugdom2 scripting asset dependency ignored: unsupported %s '%s' for level %d", dependency.kind, dependency.id, levelNum);
	}
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

void Bugdom2Script_OnLevelLoad(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, levelNum, "onLevelLoad");
}

void Bugdom2Script_OnLevelStart(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, levelNum, "onLevelStart");
}

void Bugdom2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	Bugdom2Script_CacheFrameContext(&context);

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onFrame", status);
}

void Bugdom2Script_OnLevelComplete(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, levelNum, "onLevelComplete");
}

void Bugdom2Script_OnLevelUnload(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, levelNum, "onLevelUnload");
	PangeaScript_ResetObjects();
}

int Bugdom2Script_RemapTerrainItemType(int levelNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(levelNum, itemType);
}

Boolean Bugdom2Script_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z)
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

Boolean Bugdom2Script_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum)
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

#endif

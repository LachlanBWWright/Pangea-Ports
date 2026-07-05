#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;

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

static bool Bugdom2Script_GetObjectInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outInfo || obj->CType == INVALID_NODE_FLAG)
		return false;

	*outInfo = (PangeaScriptObjectInfo)
	{
		.type = obj->Type,
		.kind = obj->Kind,
		.mode = obj->Mode,
		.statusBits = obj->StatusBits,
		.cType = obj->CType,
		.cBits = obj->CBits,
		.health = obj->Health,
		.damage = obj->Damage,
		.velocity = { obj->Delta.x, obj->Delta.y, obj->Delta.z },
		.validMask = PANGEA_SCRIPT_OBJECT_INFO_TYPE
			| PANGEA_SCRIPT_OBJECT_INFO_KIND
			| PANGEA_SCRIPT_OBJECT_INFO_MODE
			| PANGEA_SCRIPT_OBJECT_INFO_FLAGS
			| PANGEA_SCRIPT_OBJECT_INFO_COLLISION
			| PANGEA_SCRIPT_OBJECT_INFO_HEALTH
			| PANGEA_SCRIPT_OBJECT_INFO_DAMAGE
			| PANGEA_SCRIPT_OBJECT_INFO_VELOCITY,
	};
	return true;
}

static bool Bugdom2Script_SetObjectInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !info || obj->CType == INVALID_NODE_FLAG)
		return false;

	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_TYPE)
		obj->Type = info->type;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_KIND)
		obj->Kind = info->kind;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_MODE)
		obj->Mode = info->mode;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_FLAGS)
		obj->StatusBits = info->statusBits;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_COLLISION)
	{
		obj->CType = info->cType;
		obj->CBits = info->cBits;
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_HEALTH)
		obj->Health = info->health;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_DAMAGE)
		obj->Damage = info->damage;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_VELOCITY)
	{
		obj->Delta.x = info->velocity.x;
		obj->Delta.y = info->velocity.y;
		obj->Delta.z = info->velocity.z;
	}

	return true;
}

static bool Bugdom2Script_GetObjectBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outBounds || obj->CType == INVALID_NODE_FLAG)
		return false;

	*outBounds = (PangeaScriptObjectBounds)
	{
		.left = obj->LeftOff,
		.right = obj->RightOff,
		.front = obj->FrontOff,
		.back = obj->BackOff,
		.top = obj->TopOff,
		.bottom = obj->BottomOff,
	};
	return true;
}

static bool Bugdom2Script_SetObjectBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !bounds || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->LeftOff = bounds->left;
	obj->RightOff = bounds->right;
	obj->FrontOff = bounds->front;
	obj->BackOff = bounds->back;
	obj->TopOff = bounds->top;
	obj->BottomOff = bounds->bottom;
	CalcObjectBoxFromNode(obj);
	return true;
}

static bool Bugdom2Script_GetObjectParams(void* nativeObject, PangeaScriptObjectParams* outParams)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outParams || obj->CType == INVALID_NODE_FLAG)
		return false;

	*outParams = (PangeaScriptObjectParams){0};
	const Byte* params = NULL;
	if (obj->TerrainItemPtr)
		params = obj->TerrainItemPtr->parm;
	else if (obj->SplineItemPtr)
		params = obj->SplineItemPtr->parm;

	if (!params)
		return true;

	outParams->count = 4;
	for (int i = 0; i < outParams->count; i++)
		outParams->values[i] = params[i];
	return true;
}

static bool Bugdom2Script_SetObjectParams(void* nativeObject, const PangeaScriptObjectParams* params)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !params || obj->CType == INVALID_NODE_FLAG || params->count < 0 || params->count > 4)
		return false;

	Byte* itemParams = NULL;
	if (obj->TerrainItemPtr)
		itemParams = obj->TerrainItemPtr->parm;
	else if (obj->SplineItemPtr)
		itemParams = obj->SplineItemPtr->parm;

	if (!itemParams)
		return false;

	for (int i = 0; i < params->count; i++)
		itemParams[i] = (Byte) params->values[i];
	return true;
}

static const PangeaScriptObjectOps kBugdom2PlayerObjectOps =
{
	.getPosition = Bugdom2Script_GetObjectPosition,
	.setPosition = Bugdom2Script_SetObjectPosition,
	.setVelocity = Bugdom2Script_SetObjectVelocity,
	.deleteObject = Bugdom2Script_DeleteObject,
	.getInfo = Bugdom2Script_GetObjectInfo,
	.setInfo = Bugdom2Script_SetObjectInfo,
	.getBounds = Bugdom2Script_GetObjectBounds,
	.setBounds = Bugdom2Script_SetObjectBounds,
	.getParams = Bugdom2Script_GetObjectParams,
	.setParams = Bugdom2Script_SetObjectParams,
};

static int Bugdom2Script_GetKeyMapMask(void)
{
	int mask = 0;

	for (int i = 0; i < NUM_KEY_TYPES; i++)
	{
		if (gPlayerInfo.hasKey[i])
			mask |= 1 << i;
	}

	if (gPlayerInfo.hasMap)
		mask |= 1 << NUM_KEY_TYPES;

	return mask;
}

static void Bugdom2Script_SetKeyMapMask(int mask)
{
	for (int i = 0; i < NUM_KEY_TYPES; i++)
		gPlayerInfo.hasKey[i] = (mask & (1 << i)) != 0;

	gPlayerInfo.hasMap = (mask & (1 << NUM_KEY_TYPES)) != 0;
}

static short Bugdom2Script_ClampShortCounter(int value)
{
	if (value < 0)
		return 0;
	if (value > 32767)
		return 32767;
	return (short) value;
}

static Byte Bugdom2Script_ClampByteCounter(int value)
{
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return (Byte) value;
}

static float Bugdom2Script_ClampUnit(float value)
{
	if (value < 0.0f)
		return 0.0f;
	if (value > 1.0f)
		return 1.0f;
	return value;
}

static bool Bugdom2Script_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum != 0 || !outInfo)
		return false;

	*outInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = playerNum,
		.score = (int) gScore,
		.health = gPlayerInfo.health,
		.lives = gPlayerInfo.lives,
		.fuel = gPlayerInfo.glidePower,
		.shield = gPlayerInfo.shieldTimer,
		.currency = Bugdom2Script_GetKeyMapMask(),
		.boostTimer = gPlayerInfo.rammingTimer,
		.collectibleA = gPlayerInfo.numGreenClovers,
		.collectibleB = gPlayerInfo.numBlueClovers,
		.collectibleC = gPlayerInfo.numGoldClovers,
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_SCORE
			| PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_LIVES
			| PANGEA_SCRIPT_PLAYER_INFO_FUEL
			| PANGEA_SCRIPT_PLAYER_INFO_SHIELD
			| PANGEA_SCRIPT_PLAYER_INFO_CURRENCY
			| PANGEA_SCRIPT_PLAYER_INFO_BOOST_TIMER
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C,
	};
	return true;
}

static bool Bugdom2Script_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum != 0 || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SCORE)
		gScore = info->score < 0 ? 0 : (uint32_t) info->score;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
		gPlayerInfo.health = info->health;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
		gPlayerInfo.lives = Bugdom2Script_ClampByteCounter(info->lives);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_FUEL)
		gPlayerInfo.glidePower = Bugdom2Script_ClampUnit(info->fuel);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SHIELD)
		gPlayerInfo.shieldTimer = info->shield < 0.0f ? 0.0f : info->shield;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_CURRENCY)
		Bugdom2Script_SetKeyMapMask(info->currency);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_BOOST_TIMER)
		gPlayerInfo.rammingTimer = info->boostTimer < 0.0f ? 0.0f : info->boostTimer;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A)
		gPlayerInfo.numGreenClovers = Bugdom2Script_ClampShortCounter(info->collectibleA);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B)
		gPlayerInfo.numBlueClovers = Bugdom2Script_ClampShortCounter(info->collectibleB);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C)
		gPlayerInfo.numGoldClovers = Bugdom2Script_ClampShortCounter(info->collectibleC);
	return true;
}

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
	{
		.id = "bugdom2.trigger",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "collision callback dispatch, player state, and object manager",
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

static bool Bugdom2Script_PlaySound(const PangeaScriptSoundRequest* request)
{
	if (!request)
		return false;

	if (request->hasPosition)
	{
		OGLPoint3D where = { request->position.x, request->position.y, request->position.z };
		PlayEffect3D(request->soundId, &where);
		return true;
	}

	PlayEffect(request->soundId);
	return true;
}

static bool Bugdom2Script_SpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request || !request->hasPosition)
		return false;

	OGLPoint3D position = { request->position.x, request->position.y, request->position.z };
	float scale = request->hasScale ? request->scale : 1.0f;

	switch (request->effectId)
	{
		case 0:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_WhiteSpark4, 100, 1.0f);
			return true;

		case 1:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_BlueSpark, 100, 1.0f);
			return true;

		case 2:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_YellowGlint, 100, 1.0f);
			return true;

		case 3:
			MakeSplash(position.x, position.y, position.z, scale);
			return true;

		case 4:
			MakeFireExplosion(&position);
			return true;

		case 5:
			MakePuff(&position, 14.0f * scale, PARTICLE_SObjType_BlackSmoke, GL_SRC_ALPHA, GL_ONE, 0.5f);
			return true;

		case 6:
			MakeConfettiExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_WhiteSpark4, 100);
			return true;

		default:
			return false;
	}
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
		.getPlayerInfo = Bugdom2Script_GetPlayerInfo,
		.setPlayerInfo = Bugdom2Script_SetPlayerInfo,
		.playSound = Bugdom2Script_PlaySound,
		.spawnEffect = Bugdom2Script_SpawnEffect,
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

static void Bugdom2Script_DisableTriggerObject(ObjNode* obj)
{
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return;

	Bugdom2Script_UnregisterObject(obj);
	obj->CType = 0;
	obj->CBits = 0;
	obj->StatusBits |= STATUS_BIT_HIDDEN;
	if (obj->ShadowNode)
		obj->ShadowNode->StatusBits |= STATUS_BIT_HIDDEN;
}

static void Bugdom2Script_ApplyHealthDelta(float healthDelta)
{
	if (healthDelta > 0.0f)
	{
		gPlayerInfo.health += healthDelta;
		if (gPlayerInfo.health > 1.0f)
			gPlayerInfo.health = 1.0f;
		return;
	}

	if (healthDelta < 0.0f)
	{
		PlayerLoseHealth(-healthDelta, PLAYER_DEATH_TYPE_EXPLODE);
	}
}

Boolean Bugdom2Script_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid)
{
	PangeaScriptTriggerContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};

	if (!triggerObj || triggerObj->CType == INVALID_NODE_FLAG)
		return false;

	if (triggerObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(triggerObj, triggerId, "trigger");

	if (otherObj && otherObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(otherObj, "bugdom2.player", "player");

	(void) Bugdom2Script_GetObjectPosition(triggerObj, &position);
	context = (PangeaScriptTriggerContext)
	{
		.levelNum = gLevelNum,
		.playerNum = 0,
		.triggerId = triggerId,
		.triggerType = triggerType,
		.self = {
			.id = triggerObj->ScriptObjectID,
			.generation = (uint32_t) triggerObj->ScriptObjectGeneration,
		},
		.other = {
			.id = otherObj ? otherObj->ScriptObjectID : 0,
			.generation = otherObj ? (uint32_t) otherObj->ScriptObjectGeneration : 0,
		},
		.position = position,
		.sideBits = sideBits,
		.otherType = otherObj ? otherObj->Type : 0,
		.otherFlags = otherObj ? otherObj->CType : 0,
		.solid = true,
	};

	status = PangeaScript_CallTriggerEnterHook(&context);
	LogScriptStatus("onTriggerEnter", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		gScore += context.scoreDelta;

	if (context.damagePlayer > 0.0f)
		PlayerLoseHealth(context.damagePlayer, PLAYER_DEATH_TYPE_EXPLODE);

	if (context.healthDelta != 0.0f)
		Bugdom2Script_ApplyHealthDelta(context.healthDelta);

	if (context.deleteOther && otherObj && otherObj != gPlayerInfo.objNode)
	{
		Bugdom2Script_UnregisterObject(otherObj);
		DeleteObject(otherObj);
	}

	if (context.deleteSelf)
		Bugdom2Script_DisableTriggerObject(triggerObj);

	if (outSolid)
		*outSolid = context.solid;
	return true;
}

Boolean Bugdom2Script_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits)
{
	PangeaScriptObjectCollisionContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};

	if (!selfObj || !otherObj || selfObj->CType == INVALID_NODE_FLAG || otherObj->CType == INVALID_NODE_FLAG)
		return false;

	if (!PangeaScript_HasRunnableModule())
		return false;

	if (selfObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(selfObj, selfObj == gPlayerInfo.objNode ? "bugdom2.player" : "bugdom2.object", selfObj == gPlayerInfo.objNode ? "player" : "object");
	if (otherObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(otherObj, "bugdom2.object", "object");

	(void) Bugdom2Script_GetObjectPosition(otherObj, &position);
	context = (PangeaScriptObjectCollisionContext)
	{
		.levelNum = gLevelNum,
		.playerNum = selfObj == gPlayerInfo.objNode ? 0 : -1,
		.collisionId = collisionId ? collisionId : "object.contact",
		.collisionType = collisionType,
		.self = {
			.id = selfObj->ScriptObjectID,
			.generation = (uint32_t) selfObj->ScriptObjectGeneration,
		},
		.other = {
			.id = otherObj->ScriptObjectID,
			.generation = (uint32_t) otherObj->ScriptObjectGeneration,
		},
		.position = position,
		.sideBits = sideBits,
		.selfType = selfObj->Type,
		.selfFlags = selfObj->CType,
		.otherType = otherObj->Type,
		.otherFlags = otherObj->CType,
		.damage = otherObj->Damage,
	};

	status = PangeaScript_CallObjectCollisionHook(&context);
	LogScriptStatus("onObjectCollision", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		gScore += context.scoreDelta;

	if (context.applyDamage && context.damage != 0.0f)
	{
		if (selfObj == gPlayerInfo.objNode)
			PlayerLoseHealth(context.damage, PLAYER_DEATH_TYPE_EXPLODE);
		else
			selfObj->Health -= context.damage;
	}

	if (context.healthDelta != 0.0f && selfObj == gPlayerInfo.objNode)
		Bugdom2Script_ApplyHealthDelta(context.healthDelta);

	if (context.deleteOther && otherObj != gPlayerInfo.objNode && otherObj->CType != INVALID_NODE_FLAG)
	{
		Bugdom2Script_UnregisterObject(otherObj);
		DeleteObject(otherObj);
	}

	if (context.deleteSelf && selfObj != gPlayerInfo.objNode && selfObj->CType != INVALID_NODE_FLAG)
	{
		Bugdom2Script_UnregisterObject(selfObj);
		DeleteObject(selfObj);
	}

	return context.suppressNative;
}

Boolean Bugdom2Script_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount)
{
	PangeaScriptPickupContext context;
	PangeaScriptVector3 position = {0};
	PangeaScriptStatus status;

	if (!pickupObj || pickupObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (pickupObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(pickupObj, pickupId, "pickup");

	if (playerObj && playerObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(playerObj, "bugdom2.player", "player");

	(void) Bugdom2Script_GetObjectPosition(pickupObj, &position);
	context = (PangeaScriptPickupContext)
	{
		.levelNum = gLevelNum,
		.playerNum = 0,
		.pickupId = pickupId,
		.pickupType = pickupType,
		.amount = amount,
		.pickup = {
			.id = pickupObj->ScriptObjectID,
			.generation = (uint32_t) pickupObj->ScriptObjectGeneration,
		},
		.player = {
			.id = playerObj ? playerObj->ScriptObjectID : 0,
			.generation = playerObj ? (uint32_t) playerObj->ScriptObjectGeneration : 0,
		},
		.position = position,
		.handled = false,
		.consumePickup = false,
		.scoreDelta = 0,
		.healthDelta = 0.0f,
	};

	status = PangeaScript_CallPickupCollectedHook(&context);
	LogScriptStatus("onPickupCollected", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		gScore += context.scoreDelta;

	if (context.healthDelta != 0.0f)
		Bugdom2Script_ApplyHealthDelta(context.healthDelta);

	return context.consumePickup;
}

Boolean Bugdom2Script_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage)
{
	PangeaScriptWeaponHitContext context;
	PangeaScriptVector3 position = {0};
	PangeaScriptStatus status;

	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (weaponObj && weaponObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(weaponObj, weaponId, "weapon");

	if (targetObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(targetObj, "bugdom2.enemy", "enemy");

	(void) Bugdom2Script_GetObjectPosition(targetObj, &position);
	context = (PangeaScriptWeaponHitContext)
	{
		.levelNum = gLevelNum,
		.playerNum = 0,
		.weaponId = weaponId,
		.weaponType = weaponType,
		.damage = *ioDamage,
		.weapon = {
			.id = weaponObj ? weaponObj->ScriptObjectID : 0,
			.generation = weaponObj ? (uint32_t) weaponObj->ScriptObjectGeneration : 0,
		},
		.target = {
			.id = targetObj->ScriptObjectID,
			.generation = (uint32_t) targetObj->ScriptObjectGeneration,
		},
		.position = position,
		.targetType = targetObj->Type,
		.targetFlags = targetObj->CType,
		.handled = false,
		.applyDamage = false,
		.destroyTarget = false,
		.scoreDelta = 0,
	};

	status = PangeaScript_CallWeaponHitHook(&context);
	LogScriptStatus("onWeaponHit", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		gScore += context.scoreDelta;

	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		Bugdom2Script_UnregisterObject(targetObj);
		DeleteObject(targetObj);
		return true;
	}

	if (context.applyDamage)
	{
		if (context.damage < 0.0f)
			context.damage = 0.0f;

		*ioDamage = context.damage;
		return false;
	}

	return true;
}

Boolean Bugdom2Script_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage)
{
	PangeaScriptObjectDamageContext context;
	PangeaScriptVector3 position = {0};
	PangeaScriptStatus status;

	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (sourceObj && sourceObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(sourceObj, damageId ? damageId : "bugdom2.objectDamage", "damageSource");

	if (targetObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(targetObj, "bugdom2.damageTarget", "damageTarget");

	(void) Bugdom2Script_GetObjectPosition(targetObj, &position);
	context = (PangeaScriptObjectDamageContext)
	{
		.levelNum = gLevelNum,
		.playerNum = 0,
		.damageId = damageId ? damageId : "bugdom2.objectDamage",
		.damageType = damageType,
		.damage = *ioDamage,
		.source = {
			.id = sourceObj ? sourceObj->ScriptObjectID : 0,
			.generation = sourceObj ? (uint32_t) sourceObj->ScriptObjectGeneration : 0,
		},
		.target = {
			.id = targetObj->ScriptObjectID,
			.generation = (uint32_t) targetObj->ScriptObjectGeneration,
		},
		.position = position,
		.targetType = targetObj->Type,
		.targetFlags = targetObj->CType,
		.handled = false,
		.applyDamage = false,
		.destroyTarget = false,
		.scoreDelta = 0,
	};

	status = PangeaScript_CallObjectDamageHook(&context);
	LogScriptStatus("onObjectDamage", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		gScore += context.scoreDelta;

	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		Bugdom2Script_UnregisterObject(targetObj);
		DeleteObject(targetObj);
		return true;
	}

	if (context.applyDamage)
	{
		if (context.damage < 0.0f)
			context.damage = 0.0f;

		*ioDamage = context.damage;
		return false;
	}

	return true;
}

Boolean Bugdom2Script_OnPlayerDamage(float* ioDamage, Byte deathType, ObjNode* sourceObj)
{
	ObjNode* playerObj = gPlayerInfo.objNode;
	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	PangeaScriptPlayerDamageContext context;
	PangeaScriptStatus status;

	if (!ioDamage || !PangeaScript_HasRunnableModule())
		return false;

	if (playerObj && playerObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterPlayerObject(playerObj);
	if (sourceObj && sourceObj->ScriptObjectID == 0)
		Bugdom2Script_RegisterObject(sourceObj, "bugdom2.damageSource", "damage");

	if (sourceObj && sourceObj->ScriptObjectID > 0)
	{
		sourceHandle.id = sourceObj->ScriptObjectID;
		sourceHandle.generation = (uint32_t) sourceObj->ScriptObjectGeneration;
	}
	if (playerObj && playerObj->ScriptObjectID > 0)
	{
		playerHandle.id = playerObj->ScriptObjectID;
		playerHandle.generation = (uint32_t) playerObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptPlayerDamageContext)
	{
		.levelNum = gLevelNum,
		.playerNum = 0,
		.damageId = "bugdom2.playerDamage",
		.damageType = deathType,
		.damage = *ioDamage,
		.source = sourceHandle,
		.player = playerHandle,
		.position = playerObj ? (PangeaScriptVector3){ playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z } : (PangeaScriptVector3){0},
		.handled = false,
		.applyDamage = true,
		.healthDelta = 0.0f,
		.scoreDelta = 0,
	};

	status = PangeaScript_CallPlayerDamageHook(&context);
	LogScriptStatus("onPlayerDamage", status);
	if (status != PANGEA_SCRIPT_OK)
		return false;

	if (context.scoreDelta != 0)
		gScore += context.scoreDelta;

	if (context.healthDelta > 0.0f)
		Bugdom2Script_ApplyHealthDelta(context.healthDelta);
	else if (context.healthDelta < 0.0f)
		context.damage += -context.healthDelta;

	if (context.damage < 0.0f)
		context.damage = 0.0f;
	*ioDamage = context.damage;

	return context.handled && !context.applyDamage;
}

#endif

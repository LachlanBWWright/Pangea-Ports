#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "player.h"
#include "structs.h"

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;

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

	BillyScript_UnregisterObject(obj);
	DeleteObject(obj);
	return true;
}

static bool BillyScript_GetObjectInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
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

static bool BillyScript_SetObjectInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
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

static bool BillyScript_GetObjectBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
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

static bool BillyScript_SetObjectBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
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

static bool BillyScript_GetObjectParams(void* nativeObject, PangeaScriptObjectParams* outParams)
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

static bool BillyScript_SetObjectParams(void* nativeObject, const PangeaScriptObjectParams* params)
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

static const PangeaScriptObjectOps kBillyPlayerObjectOps =
{
	.getPosition = BillyScript_GetObjectPosition,
	.setPosition = BillyScript_SetObjectPosition,
	.setVelocity = BillyScript_SetObjectVelocity,
	.deleteObject = BillyScript_DeleteObject,
	.getInfo = BillyScript_GetObjectInfo,
	.setInfo = BillyScript_SetObjectInfo,
	.getBounds = BillyScript_GetObjectBounds,
	.setBounds = BillyScript_SetObjectBounds,
	.getParams = BillyScript_GetObjectParams,
	.setParams = BillyScript_SetObjectParams,
};

static int BillyScript_ClampCounter(int value, int maxValue)
{
	if (value < 0)
		return 0;
	if (value > maxValue)
		return maxValue;
	return value;
}

static float BillyScript_GetPlayerHealth(void)
{
	if (!gPlayerInfo.objNode || gPlayerInfo.objNode->CType == INVALID_NODE_FLAG)
		return 0.0f;

	return gPlayerInfo.objNode->Health;
}

static void BillyScript_SetPlayerHealth(float health)
{
	if (!gPlayerInfo.objNode || gPlayerInfo.objNode->CType == INVALID_NODE_FLAG)
		return;

	if (health < 0.0f)
		health = 0.0f;
	if (health > 1.0f)
		health = 1.0f;
	gPlayerInfo.objNode->Health = health;
}

static float BillyScript_ClampShield(float shield)
{
	if (shield < 0.0f)
		return 0.0f;
	if (shield > MAX_SHIELD)
		return MAX_SHIELD;
	return shield;
}

static bool BillyScript_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum != 0 || !outInfo)
		return false;

	*outInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = playerNum,
		.score = (int) gScore,
		.health = BillyScript_GetPlayerHealth(),
		.lives = gPlayerInfo.lives,
		.ammo = gPlayerInfo.ammoCount,
		.shield = gPlayerInfo.shieldPower,
		.currency = gPlayerInfo.pesos,
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_SCORE
			| PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_LIVES
			| PANGEA_SCRIPT_PLAYER_INFO_AMMO
			| PANGEA_SCRIPT_PLAYER_INFO_SHIELD
			| PANGEA_SCRIPT_PLAYER_INFO_CURRENCY,
	};
	return true;
}

static bool BillyScript_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum != 0 || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SCORE)
		gScore = info->score < 0 ? 0 : (uint32_t) info->score;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
		BillyScript_SetPlayerHealth(info->health);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
		gPlayerInfo.lives = (SInt8) BillyScript_ClampCounter(info->lives, 99);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_AMMO)
		gPlayerInfo.ammoCount = BillyScript_ClampCounter(info->ammo, 999);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SHIELD)
		gPlayerInfo.shieldPower = BillyScript_ClampShield(info->shield);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_CURRENCY)
		gPlayerInfo.pesos = BillyScript_ClampCounter(info->currency, 9999);
	return true;
}

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

void BillyScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
{
	PangeaScriptObjectRegistration registration;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	const char* tags[2];
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

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = obj,
		.ops = &kBillyPlayerObjectOps,
		.tags = tags,
		.tagCount = tagCount,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	obj->ScriptVisualOffset = (OGLVector3D){0};
	if (status == PANGEA_SCRIPT_OK)
	{
		obj->ScriptObjectID = handle.id;
		obj->ScriptObjectGeneration = handle.generation;
	}
	else
	{
		obj->ScriptObjectID = 0;
		obj->ScriptObjectGeneration = 0;
	}
	LogScriptStatus("object registration", status);
}

void BillyScript_UnregisterObject(ObjNode* obj)
{
	PangeaScriptObjectHandle handle;

	if (!obj || obj->ScriptObjectID == 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = (int) obj->ScriptObjectID,
		.generation = obj->ScriptObjectGeneration,
	};
	(void) PangeaScript_UnregisterObject(handle);
	obj->ScriptObjectID = 0;
	obj->ScriptObjectGeneration = 0;
	obj->ScriptVisualOffset = (OGLVector3D){0};
}

void BillyScript_RegisterPlayerObject(ObjNode* playerObj)
{
	BillyScript_RegisterObject(playerObj, "billy.player", "player");
}

void BillyScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	BillyScript_UnregisterObject(playerObj);
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
	{
		.id = "billy.trigger",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "collision callback dispatch, player state, and object manager",
	},
	{
		.id = "billy.explosiveItem",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "enemy-only trigger collision, explosion effects, and object manager",
	},
};

static bool BillyScript_PlaySound(const PangeaScriptSoundRequest* request)
{
	if (!request)
		return false;

	if (request->hasPosition)
	{
		const OGLPoint3D where = { request->position.x, request->position.y, request->position.z };
		PlayEffect3D((short) request->soundId, &where);
		return true;
	}

	PlayEffect((short) request->soundId);
	return true;
}

static bool BillyScript_SpawnEffect(const PangeaScriptEffectRequest* request)
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
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_YellowGlint, 100, 1.0f);
			return true;

		case 2:
			MakePuff(&position, 10.0f * scale, PARTICLE_SObjType_GreySmoke, GL_SRC_ALPHA, GL_ONE, 1.0f);
			return true;

		case 3:
			MakeSplash(position.x, position.y, position.z, scale);
			return true;

		case 4:
			MakeFireExplosion(&position);
			return true;

		default:
			return false;
	}
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

	SDL_Log("Billy Frontier scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void BillyScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "BillyFrontier-Android",
		.gameName = "Billy Frontier",
		.getPlayerInfo = BillyScript_GetPlayerInfo,
		.setPlayerInfo = BillyScript_SetPlayerInfo,
		.playSound = BillyScript_PlaySound,
		.spawnEffect = BillyScript_SpawnEffect,
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

static void CallAreaHook(int areaNum, const char* hookName)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = areaNum,
		.levelName = NULL,
	};

	PangeaScriptStatus status = PangeaScript_CallNamedLevelHook(hookName, &context);
	LogScriptStatus(hookName, status);
}

void BillyScript_OnAreaLoad(int areaNum)
{
	CallAreaHook(areaNum, "onAreaLoad");
}

void BillyScript_OnAreaStart(int areaNum)
{
	CallAreaHook(areaNum, "onAreaStart");
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

	PangeaScriptStatus status = PangeaScript_CallNamedFrameHook("onAreaFrame", &context);
	LogScriptStatus("onAreaFrame", status);
}

void BillyScript_OnAreaComplete(int areaNum)
{
	CallAreaHook(areaNum, "onAreaComplete");
}

void BillyScript_OnAreaUnload(int areaNum)
{
	CallAreaHook(areaNum, "onAreaUnload");
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

static void BillyScript_DisableTriggerObject(ObjNode* obj)
{
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return;

	BillyScript_UnregisterObject(obj);
	obj->CType = 0;
	obj->CBits = 0;
	obj->StatusBits |= STATUS_BIT_HIDDEN;
	if (obj->ShadowNode)
		obj->ShadowNode->StatusBits |= STATUS_BIT_HIDDEN;
}

static void BillyScript_ApplyHealthDelta(float healthDelta)
{
	ObjNode* player = gPlayerInfo.objNode;
	if (!player || player->CType == INVALID_NODE_FLAG)
		return;

	if (healthDelta > 0.0f)
	{
		player->Health += healthDelta;
		if (player->Health > 1.0f)
			player->Health = 1.0f;
		return;
	}

	if (healthDelta < 0.0f)
	{
		player->Health += healthDelta;
		if (player->Health <= 0.0f)
			KillPlayer(PLAYER_DEATH_TYPE_TRAMPLED);
	}
}

Boolean BillyScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid)
{
	PangeaScriptTriggerContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};

	if (!triggerObj || triggerObj->CType == INVALID_NODE_FLAG)
		return false;

	if (triggerObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(triggerObj, triggerId, "trigger");

	if (otherObj && otherObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(otherObj, "billy.player", "player");

	(void) BillyScript_GetObjectPosition(triggerObj, &position);
	context = (PangeaScriptTriggerContext)
	{
		.levelNum = gCurrentArea,
		.playerNum = 0,
		.triggerId = triggerId,
		.triggerType = triggerType,
		.self = {
			.id = (int) triggerObj->ScriptObjectID,
			.generation = triggerObj->ScriptObjectGeneration,
		},
		.other = {
			.id = otherObj ? (int) otherObj->ScriptObjectID : 0,
			.generation = otherObj ? otherObj->ScriptObjectGeneration : 0,
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

	if (context.scoreDelta > 0)
	{
		gScore += (uint32_t) context.scoreDelta;
	}
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreDelta = (uint32_t) -context.scoreDelta;
		gScore = scoreDelta > gScore ? 0 : gScore - scoreDelta;
	}

	if (context.damagePlayer > 0.0f)
		BillyScript_ApplyHealthDelta(-context.damagePlayer);

	if (context.healthDelta != 0.0f)
		BillyScript_ApplyHealthDelta(context.healthDelta);

	if (context.deleteOther && otherObj && otherObj != gPlayerInfo.objNode)
		BillyScript_DisableTriggerObject(otherObj);

	if (context.deleteSelf)
		BillyScript_DisableTriggerObject(triggerObj);

	if (outSolid)
		*outSolid = context.solid;
	return true;
}

Boolean BillyScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits)
{
	PangeaScriptObjectCollisionContext context;
	PangeaScriptVector3 position = {0};
	PangeaScriptStatus status;

	if (!selfObj || !otherObj || selfObj->CType == INVALID_NODE_FLAG || otherObj->CType == INVALID_NODE_FLAG)
		return false;

	if (!PangeaScript_HasRunnableModule())
		return false;

	if (selfObj->ScriptObjectID == 0)
	{
		if (selfObj == gPlayerInfo.objNode)
			BillyScript_RegisterObject(selfObj, "billy.player", "player");
		else
			BillyScript_RegisterObject(selfObj, "billy.object", "object");
	}
	if (otherObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(otherObj, "billy.object", "object");

	(void) BillyScript_GetObjectPosition(otherObj, &position);
	context = (PangeaScriptObjectCollisionContext)
	{
		.levelNum = gCurrentArea,
		.playerNum = selfObj == gPlayerInfo.objNode ? 0 : -1,
		.collisionId = collisionId ? collisionId : "object.contact",
		.collisionType = collisionType,
		.self = {
			.id = (int) selfObj->ScriptObjectID,
			.generation = selfObj->ScriptObjectGeneration,
		},
		.other = {
			.id = (int) otherObj->ScriptObjectID,
			.generation = otherObj->ScriptObjectGeneration,
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

	if (context.scoreDelta > 0)
	{
		gScore += (uint32_t) context.scoreDelta;
	}
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreDelta = (uint32_t) -context.scoreDelta;
		gScore = scoreDelta > gScore ? 0 : gScore - scoreDelta;
	}

	if (context.applyDamage && context.damage != 0.0f)
	{
		if (selfObj == gPlayerInfo.objNode)
			BillyScript_ApplyHealthDelta(-context.damage);
		else
			selfObj->Health -= context.damage;
	}

	if (context.healthDelta != 0.0f && selfObj == gPlayerInfo.objNode)
		BillyScript_ApplyHealthDelta(context.healthDelta);

	if (context.deleteOther && otherObj != gPlayerInfo.objNode && otherObj->CType != INVALID_NODE_FLAG)
		BillyScript_DeleteObject(otherObj);

	if (context.deleteSelf && selfObj != gPlayerInfo.objNode && selfObj->CType != INVALID_NODE_FLAG)
		BillyScript_DeleteObject(selfObj);

	return context.suppressNative || context.deleteSelf || context.deleteOther;
}

Boolean BillyScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount)
{
	PangeaScriptObjectHandle pickupHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	PangeaScriptPickupContext context;
	PangeaScriptStatus status;

	if (!pickupObj || pickupObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (pickupObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(pickupObj, pickupId ? pickupId : "billy.pickup", "pickup");

	if (playerObj && playerObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(playerObj, "billy.player", "player");

	if (pickupObj->ScriptObjectID > 0)
	{
		pickupHandle.id = (int) pickupObj->ScriptObjectID;
		pickupHandle.generation = pickupObj->ScriptObjectGeneration;
	}
	if (playerObj && playerObj->ScriptObjectID > 0)
	{
		playerHandle.id = (int) playerObj->ScriptObjectID;
		playerHandle.generation = playerObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptPickupContext)
	{
		.levelNum = gCurrentArea,
		.playerNum = 0,
		.pickupId = pickupId ? pickupId : "billy.pickup",
		.pickupType = pickupType,
		.amount = amount,
		.pickup = pickupHandle,
		.player = playerHandle,
		.position = { pickupObj->Coord.x, pickupObj->Coord.y, pickupObj->Coord.z },
		.handled = false,
		.consumePickup = false,
	};

	status = PangeaScript_CallPickupCollectedHook(&context);
	LogScriptStatus("onPickupCollected", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta > 0)
	{
		gScore += (uint32_t) context.scoreDelta;
	}
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreDelta = (uint32_t) -context.scoreDelta;
		gScore = scoreDelta > gScore ? 0 : gScore - scoreDelta;
	}

	if (context.healthDelta != 0.0f)
		BillyScript_ApplyHealthDelta(context.healthDelta);

	return context.consumePickup;
}

Boolean BillyScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage)
{
	PangeaScriptObjectHandle weaponHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
	PangeaScriptWeaponHitContext context;
	PangeaScriptStatus status;

	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (weaponObj && weaponObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(weaponObj, weaponId ? weaponId : "billy.weaponHit", "weapon");

	if (targetObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(targetObj, "billy.enemy", "enemy");

	if (weaponObj && weaponObj->ScriptObjectID > 0)
	{
		weaponHandle.id = (int) weaponObj->ScriptObjectID;
		weaponHandle.generation = weaponObj->ScriptObjectGeneration;
	}
	if (targetObj->ScriptObjectID > 0)
	{
		targetHandle.id = (int) targetObj->ScriptObjectID;
		targetHandle.generation = targetObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptWeaponHitContext)
	{
		.levelNum = gCurrentArea,
		.playerNum = 0,
		.weaponId = weaponId ? weaponId : "billy.weaponHit",
		.weaponType = weaponType,
		.damage = *ioDamage,
		.weapon = weaponHandle,
		.target = targetHandle,
		.position = { targetObj->Coord.x, targetObj->Coord.y, targetObj->Coord.z },
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

	if (context.scoreDelta > 0)
	{
		gScore += (uint32_t) context.scoreDelta;
	}
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreDelta = (uint32_t) -context.scoreDelta;
		gScore = scoreDelta > gScore ? 0 : gScore - scoreDelta;
	}

	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		BillyScript_DeleteObject(targetObj);
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

Boolean BillyScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage)
{
	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
	PangeaScriptObjectDamageContext context;
	PangeaScriptStatus status;

	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (sourceObj && sourceObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(sourceObj, damageId ? damageId : "billy.objectDamage", "damageSource");

	if (targetObj->ScriptObjectID == 0)
		BillyScript_RegisterObject(targetObj, "billy.damageTarget", "damageTarget");

	if (sourceObj && sourceObj->ScriptObjectID > 0)
	{
		sourceHandle.id = (int) sourceObj->ScriptObjectID;
		sourceHandle.generation = sourceObj->ScriptObjectGeneration;
	}
	if (targetObj->ScriptObjectID > 0)
	{
		targetHandle.id = (int) targetObj->ScriptObjectID;
		targetHandle.generation = targetObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptObjectDamageContext)
	{
		.levelNum = gCurrentArea,
		.playerNum = 0,
		.damageId = damageId ? damageId : "billy.objectDamage",
		.damageType = damageType,
		.damage = *ioDamage,
		.source = sourceHandle,
		.target = targetHandle,
		.position = { targetObj->Coord.x, targetObj->Coord.y, targetObj->Coord.z },
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

	if (context.scoreDelta > 0)
	{
		gScore += (uint32_t) context.scoreDelta;
	}
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreDelta = (uint32_t) -context.scoreDelta;
		gScore = scoreDelta > gScore ? 0 : gScore - scoreDelta;
	}

	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		BillyScript_DeleteObject(targetObj);
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

Boolean BillyScript_OnPlayerDamage(ObjNode* sourceObj, float* ioDamage, const char* damageId, int damageType)
{
	ObjNode* playerObj = gPlayerInfo.objNode;
	PangeaScriptPlayerDamageContext context;
	PangeaScriptVector3 position = {0};
	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	PangeaScriptStatus status;

	if (!ioDamage || !PangeaScript_HasRunnableModule())
		return false;

	if (playerObj && playerObj->CType != INVALID_NODE_FLAG)
	{
		if (playerObj->ScriptObjectID == 0)
			BillyScript_RegisterObject(playerObj, "billy.player", "player");

		playerHandle = (PangeaScriptObjectHandle)
		{
			.id = (int) playerObj->ScriptObjectID,
			.generation = playerObj->ScriptObjectGeneration,
		};
		(void) BillyScript_GetObjectPosition(playerObj, &position);
	}

	if (sourceObj && sourceObj->CType != INVALID_NODE_FLAG)
	{
		if (sourceObj->ScriptObjectID == 0)
			BillyScript_RegisterObject(sourceObj, "billy.damageSource", "damage");

		sourceHandle = (PangeaScriptObjectHandle)
		{
			.id = (int) sourceObj->ScriptObjectID,
			.generation = sourceObj->ScriptObjectGeneration,
		};
	}

	context = (PangeaScriptPlayerDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.damageId = damageId ? damageId : "billy.playerDamage",
		.damageType = damageType,
		.damage = *ioDamage,
		.source = sourceHandle,
		.player = playerHandle,
		.position = position,
		.handled = false,
		.applyDamage = true,
	};

	status = PangeaScript_CallPlayerDamageHook(&context);
	LogScriptStatus("onPlayerDamage", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta > 0)
	{
		gScore += (uint32_t) context.scoreDelta;
	}
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreDelta = (uint32_t) -context.scoreDelta;
		gScore = scoreDelta > gScore ? 0 : gScore - scoreDelta;
	}

	if (context.healthDelta > 0.0f)
		BillyScript_ApplyHealthDelta(context.healthDelta);
	else if (context.healthDelta < 0.0f)
		context.damage += -context.healthDelta;

	if (context.damage < 0.0f)
		context.damage = 0.0f;
	*ioDamage = context.damage;

	return context.handled && !context.applyDamage;
}

#endif

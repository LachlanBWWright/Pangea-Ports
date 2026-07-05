#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "structs.h"

static void LogScriptStatus(const char* action, PangeaScriptStatus status);
static void BugdomScript_UpdateObjectCollisionBox(ObjNode* obj);

static PangeaScriptFrameContext gScriptFrameContext;

static bool BugdomScript_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = obj->Coord.x;
	outPosition->y = obj->Coord.y;
	outPosition->z = obj->Coord.z;
	return true;
}

static bool BugdomScript_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	return true;
}

static bool BugdomScript_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	return true;
}

static bool BugdomScript_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	BugdomScript_UnregisterObject(obj);
	DeleteObject(obj);
	return true;
}

static bool BugdomScript_GetObjectInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
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

static bool BugdomScript_SetObjectInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
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

static bool BugdomScript_GetObjectBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
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

static bool BugdomScript_SetObjectBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !bounds || obj->CType == INVALID_NODE_FLAG || !obj->CollisionBoxes)
		return false;

	obj->LeftOff = (short) bounds->left;
	obj->RightOff = (short) bounds->right;
	obj->FrontOff = (short) bounds->front;
	obj->BackOff = (short) bounds->back;
	obj->TopOff = (short) bounds->top;
	obj->BottomOff = (short) bounds->bottom;
	CalcObjectBoxFromNode(obj);
	return true;
}

static bool BugdomScript_GetObjectParams(void* nativeObject, PangeaScriptObjectParams* outParams)
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

static bool BugdomScript_SetObjectParams(void* nativeObject, const PangeaScriptObjectParams* params)
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

static const PangeaScriptObjectOps kBugdomPlayerObjectOps =
{
	.getPosition = BugdomScript_GetObjectPosition,
	.setPosition = BugdomScript_SetObjectPosition,
	.setVelocity = BugdomScript_SetObjectVelocity,
	.deleteObject = BugdomScript_DeleteObject,
	.getInfo = BugdomScript_GetObjectInfo,
	.setInfo = BugdomScript_SetObjectInfo,
	.getBounds = BugdomScript_GetObjectBounds,
	.setBounds = BugdomScript_SetObjectBounds,
	.getParams = BugdomScript_GetObjectParams,
	.setParams = BugdomScript_SetObjectParams,
};

enum
{
	BUGDOM_SCRIPT_MAX_KEY_TYPES = 5,
};

static int BugdomScript_GetKeyMask(void)
{
	int mask = 0;

	for (int i = 0; i < BUGDOM_SCRIPT_MAX_KEY_TYPES; i++)
	{
		if (DoWeHaveTheKey(i))
			mask |= 1 << i;
	}

	return mask;
}

static void BugdomScript_SetKeyMask(int mask)
{
	for (int i = 0; i < BUGDOM_SCRIPT_MAX_KEY_TYPES; i++)
	{
		Boolean hasKey = DoWeHaveTheKey(i);

		if ((mask & (1 << i)) && !hasKey)
			GetKey(i);
		else if (!(mask & (1 << i)) && hasKey)
			UseKey(i);
	}
}

static short BugdomScript_ClampShortCounter(int value)
{
	if (value < 0)
		return 0;
	if (value > 32767)
		return 32767;
	return (short) value;
}

static bool BugdomScript_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum != 0 || !outInfo)
		return false;

	*outInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = playerNum,
		.score = (int) gScore,
		.health = gMyHealth,
		.lives = gNumLives,
		.currency = gMoney,
		.inventoryType = BugdomScript_GetKeyMask(),
		.collectibleA = gNumGreenClovers,
		.collectibleB = gNumBlueClovers,
		.collectibleC = gNumGoldClovers,
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_SCORE
			| PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_LIVES
			| PANGEA_SCRIPT_PLAYER_INFO_CURRENCY
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C,
	};
	return true;
}

static bool BugdomScript_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum != 0 || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SCORE)
		gScore = info->score < 0 ? 0 : (uint32_t) info->score;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
		gMyHealth = info->health;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
		gNumLives = BugdomScript_ClampShortCounter(info->lives);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_CURRENCY)
		gMoney = BugdomScript_ClampShortCounter(info->currency);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE)
		BugdomScript_SetKeyMask(info->inventoryType);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A)
		gNumGreenClovers = BugdomScript_ClampShortCounter(info->collectibleA);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B)
		gNumBlueClovers = BugdomScript_ClampShortCounter(info->collectibleB);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C)
		gNumGoldClovers = BugdomScript_ClampShortCounter(info->collectibleC);
	return true;
}

void BugdomScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void BugdomScript_ResetObjectRegistry(void)
{
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	PangeaScript_ResetObjects();
}

void BugdomScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
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
		.ops = &kBugdomPlayerObjectOps,
		.tags = tags,
		.tagCount = tagCount,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	obj->ScriptVisualOffset = (TQ3Vector3D){0};
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

void BugdomScript_UnregisterObject(ObjNode* obj)
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
	obj->ScriptVisualOffset = (TQ3Vector3D){0};
}

void BugdomScript_RegisterPlayerObject(ObjNode* playerObj)
{
	BugdomScript_RegisterObject(playerObj, "bugdom.player", "player");
}

void BugdomScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	BugdomScript_UnregisterObject(playerObj);
}

Boolean BugdomScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid)
{
	PangeaScriptObjectHandle triggerHandle = {0};
	PangeaScriptObjectHandle otherHandle = {0};
	PangeaScriptTriggerContext context;

	if (!triggerObj || triggerObj->CType == INVALID_NODE_FLAG)
		return false;

	if (triggerObj->ScriptObjectID > 0)
	{
		triggerHandle.id = triggerObj->ScriptObjectID;
		triggerHandle.generation = (uint32_t) triggerObj->ScriptObjectGeneration;
	}
	if (otherObj && otherObj->ScriptObjectID > 0)
	{
		otherHandle.id = otherObj->ScriptObjectID;
		otherHandle.generation = (uint32_t) otherObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptTriggerContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.triggerId = triggerId,
		.triggerType = triggerType,
		.self = triggerHandle,
		.other = otherHandle,
		.position = { triggerObj->Coord.x, triggerObj->Coord.y, triggerObj->Coord.z },
		.sideBits = sideBits,
		.otherType = otherObj ? otherObj->Kind : 0,
		.otherFlags = otherObj ? otherObj->CType : 0,
		.handled = false,
		.solid = outSolid ? *outSolid : false,
		.deleteSelf = false,
		.deleteOther = false,
		.damagePlayer = 0.0f,
		.healthDelta = 0.0f,
		.scoreDelta = 0,
	};

	PangeaScriptStatus status = PangeaScript_CallTriggerEnterHook(&context);
	LogScriptStatus("onTriggerEnter", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta > 0)
		gScore += (uint32_t) context.scoreDelta;
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreLoss = (uint32_t) -context.scoreDelta;
		gScore = scoreLoss > gScore ? 0 : gScore - scoreLoss;
	}
	if (context.damagePlayer != 0.0f)
		LoseHealth(context.damagePlayer);
	if (context.healthDelta > 0.0f)
		GetHealth(context.healthDelta);
	else if (context.healthDelta < 0.0f)
		LoseHealth(-context.healthDelta);
	if (context.deleteOther && otherObj && otherObj != gPlayerObj && otherObj->CType != INVALID_NODE_FLAG)
	{
		BugdomScript_UnregisterObject(otherObj);
		DeleteObject(otherObj);
	}
	if (context.deleteSelf && triggerObj->CType != INVALID_NODE_FLAG)
	{
		triggerObj->TerrainItemPtr = nil;
		triggerObj->CType = 0;
		triggerObj->CBits = 0;
		triggerObj->StatusBits |= STATUS_BIT_HIDDEN;
		if (triggerObj->ShadowNode)
			triggerObj->ShadowNode->StatusBits |= STATUS_BIT_HIDDEN;
		BugdomScript_UnregisterObject(triggerObj);
	}
	if (outSolid)
		*outSolid = context.solid;

	return true;
}

Boolean BugdomScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits)
{
	PangeaScriptObjectHandle selfHandle = {0};
	PangeaScriptObjectHandle otherHandle = {0};

	if (!selfObj || !otherObj || selfObj->CType == INVALID_NODE_FLAG || otherObj->CType == INVALID_NODE_FLAG)
		return false;

	if (!PangeaScript_HasRunnableModule())
		return false;

	if (selfObj->ScriptObjectID > 0)
	{
		selfHandle.id = selfObj->ScriptObjectID;
		selfHandle.generation = (uint32_t) selfObj->ScriptObjectGeneration;
	}
	if (otherObj->ScriptObjectID > 0)
	{
		otherHandle.id = otherObj->ScriptObjectID;
		otherHandle.generation = (uint32_t) otherObj->ScriptObjectGeneration;
	}

	PangeaScriptObjectCollisionContext context =
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.collisionId = collisionId,
		.collisionType = collisionType,
		.self = selfHandle,
		.other = otherHandle,
		.position = { otherObj->Coord.x, otherObj->Coord.y, otherObj->Coord.z },
		.sideBits = sideBits,
		.selfType = selfObj->Kind,
		.selfFlags = selfObj->CType,
		.otherType = otherObj->Kind,
		.otherFlags = otherObj->CType,
		.damage = otherObj->Damage,
		.handled = false,
		.suppressNative = false,
		.deleteSelf = false,
		.deleteOther = false,
		.applyDamage = false,
		.scoreDelta = 0,
		.healthDelta = 0.0f,
	};

	PangeaScriptStatus status = PangeaScript_CallObjectCollisionHook(&context);
	LogScriptStatus("onObjectCollision", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta > 0)
		gScore += (uint32_t) context.scoreDelta;
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreLoss = (uint32_t) -context.scoreDelta;
		gScore = scoreLoss > gScore ? 0 : gScore - scoreLoss;
	}
	if (context.applyDamage && context.damage != 0.0f)
		PlayerGotHurt(otherObj, context.damage, false, true, true, INVINCIBILITY_DURATION);
	if (context.healthDelta > 0.0f)
		GetHealth(context.healthDelta);
	else if (context.healthDelta < 0.0f)
		LoseHealth(-context.healthDelta);
	if (context.deleteOther && otherObj->CType != INVALID_NODE_FLAG)
	{
		BugdomScript_UnregisterObject(otherObj);
		DeleteObject(otherObj);
	}
	if (context.deleteSelf && selfObj != gPlayerObj && selfObj->CType != INVALID_NODE_FLAG)
	{
		BugdomScript_UnregisterObject(selfObj);
		DeleteObject(selfObj);
	}

	return context.suppressNative;
}

void BugdomScript_ApplyObjectScripting(ObjNode* obj)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectFrameResult result = {0};
	PangeaScriptStatus status;
	TQ3Point3D baseCoord;

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
		obj->ScriptVisualOffset = (TQ3Vector3D){0};
		return;
	}

	UpdateObjectTransforms(obj);
	BugdomScript_UpdateObjectCollisionBox(obj);
	obj->ScriptVisualOffset = (TQ3Vector3D){0};
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

void BugdomScript_RunObjectFrame(ObjNode* obj)
{
	BugdomScript_ApplyObjectScripting(obj);
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "bugdom.nut",
		.nativeType = 2,
		.category = "pickup",
		.dependencySummary = "nut pickup assets, terrain, and player systems",
	},
	{
		.id = "bugdom.clover",
		.nativeType = 5,
		.category = "pickup",
		.dependencySummary = "clover pickup assets and terrain systems",
	},
	{
		.id = "bugdom.checkpoint",
		.nativeType = 32,
		.category = "trigger",
		.dependencySummary = "checkpoint state and terrain systems",
	},
	{
		.id = "bugdom.powerup",
		.nativeType = 2,
		.category = "powerup",
		.dependencySummary = "nut-generated powerup assets and inventory systems",
	},
};

static bool BugdomScript_PlaySound(const PangeaScriptSoundRequest* request)
{
	if (!request)
		return false;

	if (request->hasPosition)
	{
		TQ3Point3D where = { request->position.x, request->position.y, request->position.z };
		PlayEffect3D(request->soundId, &where);
		return true;
	}

	PlayEffect(request->soundId);
	return true;
}

static bool BugdomScript_SpawnParticleBurst(
	const PangeaScriptEffectRequest* request,
	Byte texture,
	float gravity,
	float baseScale,
	int count)
{
	TQ3Point3D position = { request->position.x, request->position.y, request->position.z };
	TQ3Vector3D baseVelocity = request->hasVelocity
		? (TQ3Vector3D){ request->velocity.x, request->velocity.y, request->velocity.z }
		: (TQ3Vector3D){0, 0, 0};
	float scale = request->hasScale ? request->scale : 1.0f;
	int32_t group = NewParticleGroup(
		PARTICLE_TYPE_FALLINGSPARKS,
		PARTICLE_FLAGS_BOUNCE,
		gravity,
		0,
		baseScale * scale,
		0,
		0.8f,
		texture);

	if (group == -1)
		return false;

	for (int i = 0; i < count; i++)
	{
		TQ3Point3D particlePosition =
		{
			position.x + (RandomFloat() - 0.5f) * 60.0f * scale,
			position.y + RandomFloat() * 40.0f * scale,
			position.z + (RandomFloat() - 0.5f) * 60.0f * scale,
		};
		TQ3Vector3D particleVelocity =
		{
			baseVelocity.x + (RandomFloat() - 0.5f) * 600.0f * scale,
			baseVelocity.y + RandomFloat() * 500.0f * scale,
			baseVelocity.z + (RandomFloat() - 0.5f) * 600.0f * scale,
		};
		AddParticleToGroup(group, &particlePosition, &particleVelocity, RandomFloat() + scale, FULL_ALPHA);
	}

	return true;
}

static bool BugdomScript_SpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request || !request->hasPosition)
		return false;

	TQ3Point3D position = { request->position.x, request->position.y, request->position.z };
	float scale = request->hasScale ? request->scale : 1.0f;

	switch (request->effectId)
	{
		case 0:
			return BugdomScript_SpawnParticleBurst(request, PARTICLE_TEXTURE_WHITE, 800.0f, 25.0f, 24);

		case 1:
			return BugdomScript_SpawnParticleBurst(request, PARTICLE_TEXTURE_FIRE, 300.0f, 30.0f, 32);

		case 2:
			return BugdomScript_SpawnParticleBurst(request, PARTICLE_TEXTURE_BLUEFIRE, 500.0f, 25.0f, 24);

		case 3:
			MakeSplash(position.x, position.y, position.z, 0.3f * scale, 1.0f);
			return true;

		case 4:
			MakeShockwave(&position, 300.0f * scale);
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

	SDL_Log("Bugdom scripting %s failed: %s", action, PangeaScript_GetLastError());
}

static void BugdomScript_UpdateObjectCollisionBox(ObjNode* obj)
{
	if (!obj || !obj->CollisionBoxes || obj->NumCollisionBoxes != 1)
		return;

	CalcObjectBoxFromNode(obj);
}

void BugdomScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "Bugdom-android",
		.gameName = "Bugdom",
		.getPlayerInfo = BugdomScript_GetPlayerInfo,
		.setPlayerInfo = BugdomScript_SetPlayerInfo,
		.playSound = BugdomScript_PlaySound,
		.spawnEffect = BugdomScript_SpawnEffect,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	BugdomScript_ResetObjectRegistry();
}

void BugdomScript_Shutdown(void)
{
	BugdomScript_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void BugdomScript_LoadLevelConfig(int levelNum)
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

void BugdomScript_OnLevelLoad(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, levelNum, "onLevelLoad");
}

void BugdomScript_OnLevelStart(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, levelNum, "onLevelStart");
}

void BugdomScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	BugdomScript_CacheFrameContext(&context);

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onFrame", status);
}

void BugdomScript_OnLevelComplete(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, levelNum, "onLevelComplete");
}

void BugdomScript_OnLevelUnload(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, levelNum, "onLevelUnload");
	PangeaScript_ResetObjects();
}

int BugdomScript_RemapTerrainItemType(int levelNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(levelNum, itemType);
}

Boolean BugdomScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z)
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

Boolean BugdomScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum)
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

Boolean BugdomScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount)
{
	PangeaScriptObjectHandle pickupHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	PangeaScriptPickupContext context;
	PangeaScriptStatus status;

	if (!pickupObj || pickupObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (pickupObj->ScriptObjectID == 0)
		BugdomScript_RegisterObject(pickupObj, pickupId ? pickupId : "bugdom.pickup", "pickup");

	if (playerObj && playerObj->ScriptObjectID == 0)
		BugdomScript_RegisterPlayerObject(playerObj);

	if (pickupObj->ScriptObjectID > 0)
	{
		pickupHandle.id = pickupObj->ScriptObjectID;
		pickupHandle.generation = (uint32_t) pickupObj->ScriptObjectGeneration;
	}
	if (playerObj && playerObj->ScriptObjectID > 0)
	{
		playerHandle.id = playerObj->ScriptObjectID;
		playerHandle.generation = (uint32_t) playerObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptPickupContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.pickupId = pickupId ? pickupId : "bugdom.pickup",
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
		gScore += (uint32_t) context.scoreDelta;
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreLoss = (uint32_t) -context.scoreDelta;
		gScore = scoreLoss > gScore ? 0 : gScore - scoreLoss;
	}

	if (context.healthDelta > 0.0f)
		GetHealth(context.healthDelta);
	else if (context.healthDelta < 0.0f)
		LoseHealth(-context.healthDelta);

	return context.consumePickup;
}

Boolean BugdomScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage)
{
	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (weaponObj && weaponObj->CType != INVALID_NODE_FLAG)
		BugdomScript_RegisterObject(weaponObj, weaponId ? weaponId : "bugdom.weaponHit", "weapon");
	BugdomScript_RegisterObject(targetObj, "bugdom.enemy", "enemy");

	PangeaScriptObjectHandle weaponHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
	if (weaponObj && weaponObj->ScriptObjectID > 0)
	{
		weaponHandle.id = weaponObj->ScriptObjectID;
		weaponHandle.generation = (uint32_t) weaponObj->ScriptObjectGeneration;
	}
	if (targetObj->ScriptObjectID > 0)
	{
		targetHandle.id = targetObj->ScriptObjectID;
		targetHandle.generation = (uint32_t) targetObj->ScriptObjectGeneration;
	}

	PangeaScriptWeaponHitContext context =
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.weaponId = weaponId ? weaponId : "bugdom.weaponHit",
		.weaponType = weaponType,
		.weapon = weaponHandle,
		.target = targetHandle,
		.position = { targetObj->Coord.x, targetObj->Coord.y, targetObj->Coord.z },
		.targetType = targetObj->Kind,
		.targetFlags = targetObj->CType,
		.damage = *ioDamage,
		.handled = false,
		.applyDamage = false,
		.destroyTarget = false,
		.scoreDelta = 0,
	};

	PangeaScriptStatus status = PangeaScript_CallWeaponHitHook(&context);
	LogScriptStatus("onWeaponHit", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta > 0)
		gScore += (uint32_t) context.scoreDelta;
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreLoss = (uint32_t) -context.scoreDelta;
		gScore = scoreLoss > gScore ? 0 : gScore - scoreLoss;
	}
	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		BugdomScript_DeleteObject(targetObj);
		return true;
	}
	if (context.applyDamage)
	{
		*ioDamage = context.damage < 0.0f ? 0.0f : context.damage;
		return false;
	}

	return true;
}

Boolean BugdomScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage)
{
	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (sourceObj && sourceObj->CType != INVALID_NODE_FLAG)
		BugdomScript_RegisterObject(sourceObj, damageId ? damageId : "bugdom.objectDamage", "damageSource");
	BugdomScript_RegisterObject(targetObj, "bugdom.damageTarget", "damageTarget");

	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
	if (sourceObj && sourceObj->ScriptObjectID > 0)
	{
		sourceHandle.id = sourceObj->ScriptObjectID;
		sourceHandle.generation = (uint32_t) sourceObj->ScriptObjectGeneration;
	}
	if (targetObj->ScriptObjectID > 0)
	{
		targetHandle.id = targetObj->ScriptObjectID;
		targetHandle.generation = (uint32_t) targetObj->ScriptObjectGeneration;
	}

	PangeaScriptObjectDamageContext context =
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.damageId = damageId ? damageId : "bugdom.objectDamage",
		.damageType = damageType,
		.damage = *ioDamage,
		.source = sourceHandle,
		.target = targetHandle,
		.position = { targetObj->Coord.x, targetObj->Coord.y, targetObj->Coord.z },
		.targetType = targetObj->Kind,
		.targetFlags = targetObj->CType,
		.handled = false,
		.applyDamage = false,
		.destroyTarget = false,
		.scoreDelta = 0,
	};

	PangeaScriptStatus status = PangeaScript_CallObjectDamageHook(&context);
	LogScriptStatus("onObjectDamage", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta > 0)
		gScore += (uint32_t) context.scoreDelta;
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreLoss = (uint32_t) -context.scoreDelta;
		gScore = scoreLoss > gScore ? 0 : gScore - scoreLoss;
	}
	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		BugdomScript_DeleteObject(targetObj);
		return true;
	}
	if (context.applyDamage)
	{
		*ioDamage = context.damage < 0.0f ? 0.0f : context.damage;
		return false;
	}

	return true;
}

Boolean BugdomScript_OnPlayerDamage(ObjNode* sourceObj, float* ioDamage)
{
	ObjNode* playerObj = gPlayerObj;
	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};

	if (!ioDamage || !PangeaScript_HasRunnableModule())
		return false;

	if (playerObj && playerObj->ScriptObjectID == 0)
		BugdomScript_RegisterPlayerObject(playerObj);
	if (sourceObj && sourceObj->ScriptObjectID == 0)
		BugdomScript_RegisterObject(sourceObj, "bugdom.damageSource", "damage");

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

	PangeaScriptPlayerDamageContext context =
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.damageId = "bugdom.playerDamage",
		.damageType = 0,
		.damage = *ioDamage,
		.source = sourceHandle,
		.player = playerHandle,
		.position = playerObj ? (PangeaScriptVector3){ playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z } : (PangeaScriptVector3){0},
		.handled = false,
		.applyDamage = true,
		.healthDelta = 0.0f,
		.scoreDelta = 0,
	};

	PangeaScriptStatus status = PangeaScript_CallPlayerDamageHook(&context);
	LogScriptStatus("onPlayerDamage", status);
	if (status != PANGEA_SCRIPT_OK)
		return false;

	if (context.scoreDelta > 0)
		gScore += (uint32_t) context.scoreDelta;
	else if (context.scoreDelta < 0)
	{
		uint32_t scoreLoss = (uint32_t) -context.scoreDelta;
		gScore = scoreLoss > gScore ? 0 : gScore - scoreLoss;
	}

	if (context.healthDelta > 0.0f)
		GetHealth(context.healthDelta);
	else if (context.healthDelta < 0.0f)
		context.damage += -context.healthDelta;

	if (context.damage < 0.0f)
		context.damage = 0.0f;
	*ioDamage = context.damage;

	return context.handled && !context.applyDamage;
}

#endif

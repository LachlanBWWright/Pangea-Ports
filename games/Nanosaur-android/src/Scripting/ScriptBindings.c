#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "structs.h"

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;

static bool NanosaurScript_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = obj->Coord.x;
	outPosition->y = obj->Coord.y;
	outPosition->z = obj->Coord.z;
	return true;
}

static bool NanosaurScript_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	return true;
}

static bool NanosaurScript_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	return true;
}

static bool NanosaurScript_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	NanosaurScript_UnregisterObject(obj);
	DeleteObject(obj);
	return true;
}

static bool NanosaurScript_GetObjectInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outInfo || obj->CType == INVALID_NODE_FLAG)
		return false;

	*outInfo = (PangeaScriptObjectInfo)
	{
		.type = obj->Type,
		.kind = obj->Kind,
		.statusBits = obj->StatusBits,
		.cType = obj->CType,
		.cBits = obj->CBits,
		.health = obj->Health,
		.damage = obj->Damage,
		.velocity = { obj->Delta.x, obj->Delta.y, obj->Delta.z },
		.validMask = PANGEA_SCRIPT_OBJECT_INFO_TYPE
			| PANGEA_SCRIPT_OBJECT_INFO_KIND
			| PANGEA_SCRIPT_OBJECT_INFO_FLAGS
			| PANGEA_SCRIPT_OBJECT_INFO_COLLISION
			| PANGEA_SCRIPT_OBJECT_INFO_HEALTH
			| PANGEA_SCRIPT_OBJECT_INFO_DAMAGE
			| PANGEA_SCRIPT_OBJECT_INFO_VELOCITY,
	};
	return true;
}

static bool NanosaurScript_SetObjectInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !info || obj->CType == INVALID_NODE_FLAG)
		return false;

	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_TYPE)
		obj->Type = info->type;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_KIND)
		obj->Kind = info->kind;
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

static bool NanosaurScript_GetObjectBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
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

static bool NanosaurScript_SetObjectBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !bounds || obj->CType == INVALID_NODE_FLAG)
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

static bool NanosaurScript_GetObjectParams(void* nativeObject, PangeaScriptObjectParams* outParams)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outParams || obj->CType == INVALID_NODE_FLAG)
		return false;

	*outParams = (PangeaScriptObjectParams){0};
	if (!obj->TerrainItemPtr)
		return true;

	outParams->count = 4;
	for (int i = 0; i < outParams->count; i++)
		outParams->values[i] = obj->TerrainItemPtr->parm[i];
	return true;
}

static bool NanosaurScript_SetObjectParams(void* nativeObject, const PangeaScriptObjectParams* params)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !params || obj->CType == INVALID_NODE_FLAG || params->count < 0 || params->count > 4 || !obj->TerrainItemPtr)
		return false;

	for (int i = 0; i < params->count; i++)
		obj->TerrainItemPtr->parm[i] = (Byte) params->values[i];
	return true;
}

static const PangeaScriptObjectOps kNanosaurPlayerObjectOps =
{
	.getPosition = NanosaurScript_GetObjectPosition,
	.setPosition = NanosaurScript_SetObjectPosition,
	.setVelocity = NanosaurScript_SetObjectVelocity,
	.deleteObject = NanosaurScript_DeleteObject,
	.getInfo = NanosaurScript_GetObjectInfo,
	.setInfo = NanosaurScript_SetObjectInfo,
	.getBounds = NanosaurScript_GetObjectBounds,
	.setBounds = NanosaurScript_SetObjectBounds,
	.getParams = NanosaurScript_GetObjectParams,
	.setParams = NanosaurScript_SetObjectParams,
};

static int NanosaurScript_ClampCounter(int value, int maxValue)
{
	if (value < 0)
		return 0;
	if (value > maxValue)
		return maxValue;
	return value;
}

static float NanosaurScript_ClampFuel(float value)
{
	if (value < 0.0f)
		return 0.0f;
	if (value > MAX_FUEL_CAPACITY)
		return MAX_FUEL_CAPACITY;
	return value;
}

static int NanosaurScript_GetRecoveredEggMask(void)
{
	int mask = 0;

	for (int i = 0; i < NUM_EGG_SPECIES; i++)
	{
		if (gRecoveredEggs[i])
			mask |= 1 << i;
	}

	return mask;
}

static void NanosaurScript_SetRecoveredEggMask(int mask)
{
	for (int i = 0; i < NUM_EGG_SPECIES; i++)
		gRecoveredEggs[i] = (mask & (1 << i)) != 0;
}

static void NanosaurScript_SetWeaponInventory(int attackMode, int quantity)
{
	if (attackMode < 0 || attackMode >= NUM_ATTACK_MODES)
		return;

	gWeaponInventory[attackMode] = (short) NanosaurScript_ClampCounter(quantity, 999);
	gPossibleAttackModes[attackMode] = gWeaponInventory[attackMode] > 0;
	if (attackMode == ATTACK_MODE_BLASTER)
		gPossibleAttackModes[attackMode] = true;
}

static bool NanosaurScript_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum != 0 || !outInfo)
		return false;

	*outInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = playerNum,
		.score = (int) gScore,
		.health = gMyHealth,
		.lives = gNumLives,
		.fuel = gFuel,
		.ammo = gWeaponInventory[ATTACK_MODE_BLASTER],
		.currency = NanosaurScript_GetRecoveredEggMask(),
		.inventoryType = gCurrentAttackMode,
		.inventoryQuantity = gWeaponInventory[gCurrentAttackMode],
		.collectibleA = gWeaponInventory[ATTACK_MODE_HEATSEEK],
		.collectibleB = gWeaponInventory[ATTACK_MODE_SONICSCREAM],
		.collectibleC = gWeaponInventory[ATTACK_MODE_TRIBLAST],
		.collectibleD = gWeaponInventory[ATTACK_MODE_NUKE],
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_SCORE
			| PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_LIVES
			| PANGEA_SCRIPT_PLAYER_INFO_AMMO
			| PANGEA_SCRIPT_PLAYER_INFO_FUEL
			| PANGEA_SCRIPT_PLAYER_INFO_CURRENCY
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C
			| PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_D,
	};
	return true;
}

static bool NanosaurScript_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum != 0 || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SCORE)
		gScore = info->score < 0 ? 0 : (uint32_t) info->score;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
		gMyHealth = info->health;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
		gNumLives = (short) NanosaurScript_ClampCounter(info->lives, 99);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_FUEL)
		gFuel = NanosaurScript_ClampFuel(info->fuel);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_AMMO)
		NanosaurScript_SetWeaponInventory(ATTACK_MODE_BLASTER, info->ammo);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_CURRENCY)
		NanosaurScript_SetRecoveredEggMask(info->currency);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE)
	{
		if (info->inventoryType >= 0 && info->inventoryType < NUM_ATTACK_MODES)
			gCurrentAttackMode = (Byte) info->inventoryType;
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY)
		NanosaurScript_SetWeaponInventory(gCurrentAttackMode, info->inventoryQuantity);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A)
		NanosaurScript_SetWeaponInventory(ATTACK_MODE_HEATSEEK, info->collectibleA);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B)
		NanosaurScript_SetWeaponInventory(ATTACK_MODE_SONICSCREAM, info->collectibleB);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C)
		NanosaurScript_SetWeaponInventory(ATTACK_MODE_TRIBLAST, info->collectibleC);
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_D)
		NanosaurScript_SetWeaponInventory(ATTACK_MODE_NUKE, info->collectibleD);
	return true;
}

void NanosaurScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void NanosaurScript_ResetObjectRegistry(void)
{
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	PangeaScript_ResetObjects();
}

void NanosaurScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
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
		.ops = &kNanosaurPlayerObjectOps,
		.tags = tags,
		.tagCount = tagCount,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	obj->ScriptVisualOffset = (TQ3Vector3D){0};
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

void NanosaurScript_UnregisterObject(ObjNode* obj)
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
	obj->ScriptVisualOffset = (TQ3Vector3D){0};
}

void NanosaurScript_RegisterPlayerObject(ObjNode* playerObj)
{
	NanosaurScript_RegisterObject(playerObj, "nanosaur.player", "player");
}

void NanosaurScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	NanosaurScript_UnregisterObject(playerObj);
}

Boolean NanosaurScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount)
{
	PangeaScriptObjectHandle pickupHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	PangeaScriptPickupContext context;

	if (!pickupObj)
		return false;

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
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.pickupId = pickupId,
		.pickupType = pickupType,
		.amount = amount,
		.pickup = pickupHandle,
		.player = playerHandle,
		.position = { pickupObj->Coord.x, pickupObj->Coord.y, pickupObj->Coord.z },
		.handled = false,
		.consumePickup = false,
		.scoreDelta = 0,
		.healthDelta = 0.0f,
	};

	PangeaScriptStatus status = PangeaScript_CallPickupCollectedHook(&context);
	LogScriptStatus("onPickupCollected", status);
	if (status != PANGEA_SCRIPT_OK)
		return false;

	if (context.scoreDelta != 0)
		AddToScore(context.scoreDelta);
	if (context.healthDelta != 0.0f)
		GetHealth(context.healthDelta);
	if (context.consumePickup && pickupObj->CType != INVALID_NODE_FLAG)
	{
		pickupObj->TerrainItemPtr = nil;
		NanosaurScript_UnregisterObject(pickupObj);
		DeleteObject(pickupObj);
	}

	return context.handled || context.consumePickup;
}

Boolean NanosaurScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float damage)
{
	PangeaScriptObjectHandle weaponHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
	PangeaScriptWeaponHitContext context;

	if (!weaponObj || !targetObj || targetObj->CType == INVALID_NODE_FLAG)
		return false;

	if (weaponObj->ScriptObjectID > 0)
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
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.weaponId = weaponId,
		.weaponType = weaponType,
		.damage = damage,
		.weapon = weaponHandle,
		.target = targetHandle,
		.position = { targetObj->Coord.x, targetObj->Coord.y, targetObj->Coord.z },
		.targetType = targetObj->Kind,
		.targetFlags = targetObj->CType,
		.handled = false,
		.applyDamage = false,
		.destroyTarget = false,
		.scoreDelta = 0,
	};

	PangeaScriptStatus status = PangeaScript_CallWeaponHitHook(&context);
	LogScriptStatus("onWeaponHit", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		AddToScore(context.scoreDelta);
	if (context.applyDamage && targetObj->CType != INVALID_NODE_FLAG && (targetObj->CType & CTYPE_ENEMY))
		EnemyGotHurt(targetObj, weaponObj, context.damage);
	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		if (targetObj->CType & CTYPE_CRYSTAL)
		{
			ExplodeCrystal(targetObj);
		}
		else
		{
			NanosaurScript_UnregisterObject(targetObj);
			DeleteObject(targetObj);
		}
	}

	return true;
}

Boolean NanosaurScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage)
{
	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
	PangeaScriptObjectDamageContext context;

	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (sourceObj && sourceObj->ScriptObjectID == 0)
		NanosaurScript_RegisterObject(sourceObj, damageId ? damageId : "nanosaur.objectDamage", "damageSource");

	if (targetObj->ScriptObjectID == 0)
		NanosaurScript_RegisterObject(targetObj, "nanosaur.damageTarget", "damageTarget");

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
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.damageId = damageId ? damageId : "nanosaur.objectDamage",
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

	if (context.scoreDelta != 0)
		AddToScore(context.scoreDelta);
	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		if (targetObj->CType & CTYPE_CRYSTAL)
			ExplodeCrystal(targetObj);
		else
		{
			NanosaurScript_UnregisterObject(targetObj);
			DeleteObject(targetObj);
		}
		return true;
	}
	if (context.applyDamage)
	{
		*ioDamage = context.damage < 0.0f ? 0.0f : context.damage;
		return false;
	}

	return true;
}

Boolean NanosaurScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid)
{
	PangeaScriptObjectHandle triggerHandle = {0};
	PangeaScriptObjectHandle otherHandle = {0};
	PangeaScriptTriggerContext context;

	if (!triggerObj || triggerObj->CType == INVALID_NODE_FLAG)
		return false;

	if (triggerObj->ScriptObjectID > 0)
	{
		triggerHandle.id = (int) triggerObj->ScriptObjectID;
		triggerHandle.generation = triggerObj->ScriptObjectGeneration;
	}
	if (otherObj && otherObj->ScriptObjectID > 0)
	{
		otherHandle.id = (int) otherObj->ScriptObjectID;
		otherHandle.generation = otherObj->ScriptObjectGeneration;
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

	if (context.scoreDelta != 0)
		AddToScore(context.scoreDelta);
	if (context.damagePlayer != 0.0f)
		GetHealth(-context.damagePlayer);
	if (context.healthDelta != 0.0f)
		GetHealth(context.healthDelta);
	if (context.deleteOther && otherObj && otherObj != gPlayerObj && otherObj->CType != INVALID_NODE_FLAG)
	{
		NanosaurScript_UnregisterObject(otherObj);
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
		NanosaurScript_UnregisterObject(triggerObj);
	}
	if (outSolid)
		*outSolid = context.solid;

	return true;
}

Boolean NanosaurScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits)
{
	PangeaScriptObjectCollisionContext context;
	PangeaScriptObjectHandle selfHandle = {0};
	PangeaScriptObjectHandle otherHandle = {0};
	PangeaScriptVector3 position = {0};

	if (!selfObj || !otherObj || selfObj->CType == INVALID_NODE_FLAG || otherObj->CType == INVALID_NODE_FLAG)
		return false;

	if (!PangeaScript_HasRunnableModule())
		return false;

	if (selfObj->ScriptObjectID == 0)
	{
		if (selfObj == gPlayerObj)
			NanosaurScript_RegisterObject(selfObj, "nanosaur.player", "player");
		else
			NanosaurScript_RegisterObject(selfObj, "nanosaur.object", "object");
	}
	if (otherObj->ScriptObjectID == 0)
		NanosaurScript_RegisterObject(otherObj, "nanosaur.object", "object");

	if (selfObj->ScriptObjectID > 0)
	{
		selfHandle.id = (int) selfObj->ScriptObjectID;
		selfHandle.generation = selfObj->ScriptObjectGeneration;
	}
	if (otherObj->ScriptObjectID > 0)
	{
		otherHandle.id = (int) otherObj->ScriptObjectID;
		otherHandle.generation = otherObj->ScriptObjectGeneration;
	}
	NanosaurScript_GetObjectPosition(otherObj, &position);

	context = (PangeaScriptObjectCollisionContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = selfObj == gPlayerObj ? 0 : -1,
		.collisionId = collisionId ? collisionId : "object.contact",
		.collisionType = collisionType,
		.self = selfHandle,
		.other = otherHandle,
		.position = position,
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
		.healthDelta = 0.0f,
		.scoreDelta = 0,
	};

	PangeaScriptStatus status = PangeaScript_CallObjectCollisionHook(&context);
	LogScriptStatus("onObjectCollision", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		AddToScore(context.scoreDelta);
	if (context.applyDamage && context.damage != 0.0f)
	{
		if (selfObj == gPlayerObj)
			PlayerGotHurt(selfObj, context.damage, true, false);
		else
			selfObj->Health -= context.damage;
	}
	if (context.healthDelta != 0.0f && selfObj == gPlayerObj)
		GetHealth(context.healthDelta);
	if (context.deleteOther && otherObj != gPlayerObj && otherObj->CType != INVALID_NODE_FLAG)
	{
		NanosaurScript_UnregisterObject(otherObj);
		DeleteObject(otherObj);
	}
	if (context.deleteSelf && selfObj != gPlayerObj && selfObj->CType != INVALID_NODE_FLAG)
	{
		NanosaurScript_UnregisterObject(selfObj);
		DeleteObject(selfObj);
	}

	return context.suppressNative || context.deleteSelf || context.deleteOther;
}

void NanosaurScript_ApplyObjectScripting(ObjNode* obj)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectFrameResult result = {0};
	PangeaScriptStatus status;
	TQ3Point3D baseCoord;

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
		obj->ScriptVisualOffset = (TQ3Vector3D){0};
		return;
	}

	UpdateObjectTransforms(obj);
	CalcObjectBoxFromNode(obj);
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

void NanosaurScript_RunObjectFrame(ObjNode* obj)
{
	NanosaurScript_ApplyObjectScripting(obj);
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "nanosaur.powerup",
		.nativeType = 1,
		.category = "powerup",
		.dependencySummary = "powerup assets, terrain, and player systems",
	},
	{
		.id = "nanosaur.egg",
		.nativeType = 5,
		.category = "pickup",
		.dependencySummary = "egg pickup assets and inventory systems",
	},
	{
		.id = "nanosaur.crystal",
		.nativeType = 15,
		.category = "pickup",
		.dependencySummary = "crystal pickup assets and terrain systems",
	},
	{
		.id = "nanosaur.stepstone",
		.nativeType = 17,
		.category = "trigger",
		.dependencySummary = "step stone terrain, collision, and movement systems",
	},
};

static bool NanosaurScript_PlaySound(const PangeaScriptSoundRequest* request)
{
	if (!request)
		return false;

	PlayEffect((short) request->soundId);
	return true;
}

static bool NanosaurScript_SpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request || !request->hasPosition)
		return false;

	TQ3Point3D position = { request->position.x, request->position.y, request->position.z };
	float scale = request->hasScale ? request->scale : 1.0f;

	switch (request->effectId)
	{
		case 0:
			return MakeDustPuff(position.x, position.y, position.z, scale) != NULL;

		case 1:
			return MakeSmokePuff(position.x, position.y, position.z, scale) != NULL;

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

	SDL_Log("Nanosaur scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void NanosaurScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "Nanosaur-android",
		.gameName = "Nanosaur",
		.getPlayerInfo = NanosaurScript_GetPlayerInfo,
		.setPlayerInfo = NanosaurScript_SetPlayerInfo,
		.playSound = NanosaurScript_PlaySound,
		.spawnEffect = NanosaurScript_SpawnEffect,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	NanosaurScript_ResetObjectRegistry();
}

void NanosaurScript_Shutdown(void)
{
	NanosaurScript_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void NanosaurScript_LoadLevelConfig(int levelNum)
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

void NanosaurScript_OnLevelLoad(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, levelNum, "onLevelLoad");
}

void NanosaurScript_OnLevelStart(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, levelNum, "onLevelStart");
}

void NanosaurScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	NanosaurScript_CacheFrameContext(&context);

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onFrame", status);
}

void NanosaurScript_OnLevelComplete(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, levelNum, "onLevelComplete");
}

void NanosaurScript_OnLevelUnload(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, levelNum, "onLevelUnload");
	PangeaScript_ResetObjects();
}

int NanosaurScript_RemapTerrainItemType(int levelNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(levelNum, itemType);
}

Boolean NanosaurScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z)
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

Boolean NanosaurScript_OnPlayerDamage(ObjNode* playerObj, float* ioDamage)
{
	PangeaScriptObjectHandle playerHandle = {0};

	if (!ioDamage || !PangeaScript_HasRunnableModule())
		return false;

	if (playerObj && playerObj->ScriptObjectID == 0)
		NanosaurScript_RegisterPlayerObject(playerObj);
	if (playerObj && playerObj->ScriptObjectID > 0)
	{
		playerHandle.id = playerObj->ScriptObjectID;
		playerHandle.generation = (uint32_t) playerObj->ScriptObjectGeneration;
	}

	PangeaScriptPlayerDamageContext context =
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.damageId = "nanosaur.playerDamage",
		.damageType = 0,
		.damage = *ioDamage,
		.source = {0},
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

	if (context.scoreDelta != 0)
		AddToScore(context.scoreDelta);

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

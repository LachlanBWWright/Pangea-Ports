#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "player.h"
#include "structs.h"

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;

static bool Nanosaur2Script_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = obj->Coord.x;
	outPosition->y = obj->Coord.y;
	outPosition->z = obj->Coord.z;
	return true;
}

static bool Nanosaur2Script_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	return true;
}

static bool Nanosaur2Script_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	return true;
}

static bool Nanosaur2Script_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	Nanosaur2Script_UnregisterObject(obj);
	DeleteObject(obj);
	return true;
}

static bool Nanosaur2Script_GetObjectInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
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

static bool Nanosaur2Script_SetObjectInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
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

static bool Nanosaur2Script_GetObjectBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
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

static bool Nanosaur2Script_SetObjectBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
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

static bool Nanosaur2Script_GetObjectParams(void* nativeObject, PangeaScriptObjectParams* outParams)
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

static bool Nanosaur2Script_SetObjectParams(void* nativeObject, const PangeaScriptObjectParams* params)
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

static const PangeaScriptObjectOps kNanosaur2PlayerObjectOps =
{
	.getPosition = Nanosaur2Script_GetObjectPosition,
	.setPosition = Nanosaur2Script_SetObjectPosition,
	.setVelocity = Nanosaur2Script_SetObjectVelocity,
	.deleteObject = Nanosaur2Script_DeleteObject,
	.getInfo = Nanosaur2Script_GetObjectInfo,
	.setInfo = Nanosaur2Script_SetObjectInfo,
	.getBounds = Nanosaur2Script_GetObjectBounds,
	.setBounds = Nanosaur2Script_SetObjectBounds,
	.getParams = Nanosaur2Script_GetObjectParams,
	.setParams = Nanosaur2Script_SetObjectParams,
};

static bool Nanosaur2Script_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS || !outInfo)
		return false;

	PlayerInfoType* player = &gPlayerInfo[playerNum];
	int currentWeapon = player->currentWeapon;
	int currentAmmo = 0;
	if (currentWeapon >= 0 && currentWeapon < NUM_WEAPON_TYPES)
		currentAmmo = player->weaponQuantity[currentWeapon];

	*outInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = playerNum,
		.health = player->health,
		.lives = player->numFreeLives,
		.fuel = player->jetpackFuel,
		.shield = player->shieldPower,
		.ammo = currentAmmo,
		.inventoryType = currentWeapon,
		.inventoryQuantity = currentAmmo,
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_LIVES
			| PANGEA_SCRIPT_PLAYER_INFO_FUEL
			| PANGEA_SCRIPT_PLAYER_INFO_SHIELD
			| PANGEA_SCRIPT_PLAYER_INFO_AMMO
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY,
	};
	return true;
}

static bool Nanosaur2Script_SetWeaponInventory(PlayerInfoType* player, int weaponType, int quantity)
{
	if (!player || weaponType < WEAPON_TYPE_NONE || weaponType >= NUM_WEAPON_TYPES || quantity < 0)
		return false;

	if (weaponType == WEAPON_TYPE_NONE)
	{
		player->currentWeapon = WEAPON_TYPE_NONE;
		return true;
	}

	if (quantity > 999)
		quantity = 999;

	player->weaponQuantity[weaponType] = (short) quantity;
	if (weaponType == WEAPON_TYPE_SONICSCREAM && quantity == 0)
		player->weaponQuantity[weaponType] = 999;

	if (player->weaponQuantity[weaponType] > 0)
		player->currentWeapon = (short) weaponType;
	else if (player->currentWeapon == weaponType)
		player->currentWeapon = WEAPON_TYPE_SONICSCREAM;

	return true;
}

static bool Nanosaur2Script_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS || !info)
		return false;

	PlayerInfoType* player = &gPlayerInfo[playerNum];
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
		player->health = info->health;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
		player->numFreeLives = (short) info->lives;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_FUEL)
		player->jetpackFuel = info->fuel;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SHIELD)
		player->shieldPower = info->shield < 0.0f ? 0.0f : info->shield;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_AMMO)
	{
		if (!Nanosaur2Script_SetWeaponInventory(player, player->currentWeapon, info->ammo))
			return false;
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE)
	{
		int quantity = info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY
			? info->inventoryQuantity
			: 0;
		if (info->inventoryType >= 0 && info->inventoryType < NUM_WEAPON_TYPES)
			quantity = player->weaponQuantity[info->inventoryType];
		if (quantity <= 0)
			quantity = 1;
		if (!Nanosaur2Script_SetWeaponInventory(player, info->inventoryType, quantity))
			return false;
	}
	else if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY)
	{
		if (!Nanosaur2Script_SetWeaponInventory(player, player->currentWeapon, info->inventoryQuantity))
			return false;
	}
	return true;
}

void Nanosaur2Script_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void Nanosaur2Script_ResetObjectRegistry(void)
{
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	PangeaScript_ResetObjects();
}

void Nanosaur2Script_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
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
		.ops = &kNanosaur2PlayerObjectOps,
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

void Nanosaur2Script_UnregisterObject(ObjNode* obj)
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

void Nanosaur2Script_RegisterPlayerObject(ObjNode* playerObj)
{
	Nanosaur2Script_RegisterObject(playerObj, "nanosaur2.player", "player");
}

void Nanosaur2Script_UnregisterPlayerObject(ObjNode* playerObj)
{
	Nanosaur2Script_UnregisterObject(playerObj);
}

void Nanosaur2Script_ApplyObjectScripting(ObjNode* obj)
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

void Nanosaur2Script_RunObjectFrame(ObjNode* obj)
{
	Nanosaur2Script_ApplyObjectScripting(obj);
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "nanosaur2.egg",
		.nativeType = 3,
		.category = "pickup",
		.dependencySummary = "egg objective state, terrain, and player systems",
	},
	{
		.id = "nanosaur2.weaponPow",
		.nativeType = 6,
		.category = "pickup",
		.dependencySummary = "weapon pickup assets, player weapon state, and terrain systems",
	},
	{
		.id = "nanosaur2.healthPow",
		.nativeType = 21,
		.category = "pickup",
		.dependencySummary = "health pickup assets, player state, and terrain systems",
	},
	{
		.id = "nanosaur2.fuelPow",
		.nativeType = 22,
		.category = "pickup",
		.dependencySummary = "jetpack fuel state, pickup assets, and player systems",
	},
	{
		.id = "nanosaur2.shieldPow",
		.nativeType = 23,
		.category = "pickup",
		.dependencySummary = "shield state, shield object lifecycle, pickup assets, and player systems",
	},
	{
		.id = "nanosaur2.freeLifePow",
		.nativeType = 24,
		.category = "pickup",
		.dependencySummary = "extra-life counters, pickup assets, and player systems",
	},
	{
		.id = "nanosaur2.powerup",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "pickup trigger callbacks, collision state, and player inventory systems",
	},
	{
		.id = "nanosaur2.trigger",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "native trigger callback fallback and collision state",
	},
	{
		.id = "nanosaur2.mine",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "mine trigger collision, explosion effects, player health, and object manager",
	},
	{
		.id = "nanosaur2.electrode",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "electrode trigger collision, player health, and electric effects",
	},
	{
		.id = "nanosaur2.forestDoorKey",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "forest door key pickup state, door state, effects, and object manager",
	},
	{
		.id = "nanosaur2.smackable",
		.nativeType = 0,
		.category = "trigger",
		.dependencySummary = "smackable prop trigger collision, effects, and object manager",
	},
};

static int Nanosaur2Script_GetPlayerNum(ObjNode* obj)
{
	if (!obj || obj->PlayerNum >= MAX_PLAYERS)
		return -1;

	return obj->PlayerNum;
}

static void Nanosaur2Script_DisableTriggerObject(ObjNode* obj)
{
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return;

	Nanosaur2Script_UnregisterObject(obj);
	obj->CType = 0;
	obj->CBits = 0;
	obj->StatusBits |= STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION;
	if (obj->ShadowNode)
		obj->ShadowNode->StatusBits |= STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION;
}

static void Nanosaur2Script_ApplyHealthDelta(int playerNum, float healthDelta, const OGLPoint3D* where)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return;

	if (healthDelta < 0.0f)
	{
		(void) PlayerLoseHealth((short) playerNum, -healthDelta, PLAYER_DEATH_TYPE_DEATHDIVE, (OGLPoint3D*) where, true);
		return;
	}

	gPlayerInfo[playerNum].health += healthDelta;
	if (gPlayerInfo[playerNum].health > 1.0f)
		gPlayerInfo[playerNum].health = 1.0f;
	if (gPlayerInfo[playerNum].objNode)
		gPlayerInfo[playerNum].objNode->Health = gPlayerInfo[playerNum].health;
}

static bool Nanosaur2Script_PlaySound(const PangeaScriptSoundRequest* request)
{
	if (!request)
		return false;

	if (request->hasPosition)
	{
		OGLPoint3D where = { request->position.x, request->position.y, request->position.z };
		PlayEffect3D((short) request->soundId, &where);
		return true;
	}

	PlayEffect((short) request->soundId);
	return true;
}

static bool Nanosaur2Script_SpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request || !request->hasPosition)
		return false;

	OGLPoint3D position = { request->position.x, request->position.y, request->position.z };
	float scale = request->hasScale ? request->scale : 1.0f;

	switch (request->effectId)
	{
		case 0:
			MakeSparkExplosion(&position, 300.0f, scale, PARTICLE_SObjType_WhiteSpark4, 100, 1.0f);
			return true;

		case 1:
			MakeSparkExplosion(&position, 300.0f, scale, PARTICLE_SObjType_RedSpark, 100, 1.0f);
			return true;

		case 2:
			MakeSparkExplosion(&position, 300.0f, scale, PARTICLE_SObjType_BlueSpark, 100, 1.0f);
			return true;

		case 3:
			MakePuff(3, &position, 10.0f * scale, PARTICLE_SObjType_GreySmoke, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, 1.0f);
			return true;

		case 4:
			MakeSplash(&position, scale);
			return true;

		case 5:
			MakeFireExplosion(&position);
			return true;

		case 6:
			MakeConfettiExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_Confetti_NanoFlesh, 100);
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

	SDL_Log("Nanosaur 2 scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void Nanosaur2Script_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "Nanosaur2-Android",
		.gameName = "Nanosaur 2",
		.getPlayerInfo = Nanosaur2Script_GetPlayerInfo,
		.setPlayerInfo = Nanosaur2Script_SetPlayerInfo,
		.playSound = Nanosaur2Script_PlaySound,
		.spawnEffect = Nanosaur2Script_SpawnEffect,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	Nanosaur2Script_ResetObjectRegistry();
}

void Nanosaur2Script_Shutdown(void)
{
	Nanosaur2Script_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void Nanosaur2Script_LoadLevelConfig(int levelNum)
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

void Nanosaur2Script_OnLevelLoad(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, levelNum, "onLevelLoad");
}

void Nanosaur2Script_OnLevelStart(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, levelNum, "onLevelStart");
}

void Nanosaur2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	Nanosaur2Script_CacheFrameContext(&context);

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onFrame", status);
}

void Nanosaur2Script_OnLevelComplete(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, levelNum, "onLevelComplete");
}

void Nanosaur2Script_OnLevelUnload(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, levelNum, "onLevelUnload");
	PangeaScript_ResetObjects();
}

int Nanosaur2Script_RemapTerrainItemType(int levelNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(levelNum, itemType);
}

Boolean Nanosaur2Script_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z)
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

Boolean Nanosaur2Script_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum)
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

Boolean Nanosaur2Script_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid)
{
	PangeaScriptObjectHandle selfHandle = {0};
	PangeaScriptObjectHandle otherHandle = {0};
	int playerNum = Nanosaur2Script_GetPlayerNum(otherObj);
	PangeaScriptTriggerContext context;
	PangeaScriptStatus status;

	if (!triggerObj || !otherObj || triggerObj->CType == INVALID_NODE_FLAG || otherObj->CType == INVALID_NODE_FLAG)
		return false;

	if (triggerObj->ScriptObjectID == 0)
		Nanosaur2Script_RegisterObject(triggerObj, triggerId ? triggerId : "nanosaur2.trigger", "trigger");
	if (otherObj->ScriptObjectID == 0)
		Nanosaur2Script_RegisterObject(otherObj, playerNum >= 0 ? "nanosaur2.player" : "nanosaur2.object", playerNum >= 0 ? "player" : "object");

	if (triggerObj->ScriptObjectID > 0)
	{
		selfHandle.id = (int) triggerObj->ScriptObjectID;
		selfHandle.generation = triggerObj->ScriptObjectGeneration;
	}
	if (otherObj->ScriptObjectID > 0)
	{
		otherHandle.id = (int) otherObj->ScriptObjectID;
		otherHandle.generation = otherObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptTriggerContext)
	{
		.levelNum = gLevelNum,
		.playerNum = playerNum,
		.triggerId = triggerId ? triggerId : "nanosaur2.trigger",
		.triggerType = triggerType,
		.self = selfHandle,
		.other = otherHandle,
		.position = { triggerObj->Coord.x, triggerObj->Coord.y, triggerObj->Coord.z },
		.sideBits = sideBits,
		.otherType = otherObj->Type,
		.otherFlags = otherObj->CType,
		.handled = false,
		.solid = true,
		.deleteSelf = false,
		.deleteOther = false,
		.damagePlayer = 0.0f,
		.healthDelta = 0.0f,
		.scoreDelta = 0,
	};

	status = PangeaScript_CallTriggerEnterHook(&context);
	LogScriptStatus("onTriggerEnter", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.damagePlayer > 0.0f && playerNum >= 0)
		(void) PlayerLoseHealth((short) playerNum, context.damagePlayer, PLAYER_DEATH_TYPE_DEATHDIVE, &triggerObj->Coord, true);
	if (context.healthDelta != 0.0f)
		Nanosaur2Script_ApplyHealthDelta(playerNum, context.healthDelta, &triggerObj->Coord);
	if (context.scoreDelta > 0 && playerNum >= 0)
		gPlayerInfo[playerNum].numFreeLives += context.scoreDelta;
	if (context.scoreDelta < 0 && playerNum >= 0)
	{
		int livesDelta = -context.scoreDelta;
		gPlayerInfo[playerNum].numFreeLives = livesDelta > gPlayerInfo[playerNum].numFreeLives ? 0 : gPlayerInfo[playerNum].numFreeLives - livesDelta;
	}
	if (context.deleteOther && playerNum < 0)
		Nanosaur2Script_DisableTriggerObject(otherObj);
	if (context.deleteSelf)
		Nanosaur2Script_DisableTriggerObject(triggerObj);
	if (outSolid)
		*outSolid = context.solid;

	return true;
}

Boolean Nanosaur2Script_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits)
{
	PangeaScriptObjectHandle selfHandle = {0};
	PangeaScriptObjectHandle otherHandle = {0};
	int playerNum = Nanosaur2Script_GetPlayerNum(selfObj);
	PangeaScriptObjectCollisionContext context;
	PangeaScriptStatus status;

	if (!selfObj || !otherObj || selfObj->CType == INVALID_NODE_FLAG || otherObj->CType == INVALID_NODE_FLAG)
		return false;

	if (!PangeaScript_HasRunnableModule())
		return false;

	if (selfObj->ScriptObjectID == 0)
		Nanosaur2Script_RegisterObject(selfObj, playerNum >= 0 ? "nanosaur2.player" : "nanosaur2.object", playerNum >= 0 ? "player" : "object");
	if (otherObj->ScriptObjectID == 0)
		Nanosaur2Script_RegisterObject(otherObj, "nanosaur2.object", "object");

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

	context = (PangeaScriptObjectCollisionContext)
	{
		.levelNum = gLevelNum,
		.playerNum = playerNum,
		.collisionId = collisionId ? collisionId : "object.contact",
		.collisionType = collisionType,
		.self = selfHandle,
		.other = otherHandle,
		.position = { otherObj->Coord.x, otherObj->Coord.y, otherObj->Coord.z },
		.sideBits = sideBits,
		.selfType = selfObj->Type,
		.selfFlags = selfObj->CType,
		.otherType = otherObj->Type,
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

	status = PangeaScript_CallObjectCollisionHook(&context);
	LogScriptStatus("onObjectCollision", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.applyDamage && context.damage != 0.0f)
	{
		if (playerNum >= 0)
			(void) PlayerLoseHealth((short) playerNum, context.damage, PLAYER_DEATH_TYPE_DEATHDIVE, &otherObj->Coord, true);
		else
			selfObj->Health -= context.damage;
	}
	if (context.healthDelta != 0.0f)
		Nanosaur2Script_ApplyHealthDelta(playerNum, context.healthDelta, &otherObj->Coord);
	if (context.scoreDelta > 0 && playerNum >= 0)
		gPlayerInfo[playerNum].numFreeLives += context.scoreDelta;
	if (context.scoreDelta < 0 && playerNum >= 0)
	{
		int livesDelta = -context.scoreDelta;
		gPlayerInfo[playerNum].numFreeLives = livesDelta > gPlayerInfo[playerNum].numFreeLives ? 0 : gPlayerInfo[playerNum].numFreeLives - livesDelta;
	}
	if (context.deleteOther && otherObj->CType != INVALID_NODE_FLAG)
	{
		Nanosaur2Script_UnregisterObject(otherObj);
		DeleteObject(otherObj);
	}
	if (context.deleteSelf && playerNum < 0 && selfObj->CType != INVALID_NODE_FLAG)
	{
		Nanosaur2Script_UnregisterObject(selfObj);
		DeleteObject(selfObj);
	}

	return context.suppressNative;
}

Boolean Nanosaur2Script_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount)
{
	PangeaScriptObjectHandle pickupHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	PangeaScriptPickupContext context;
	PangeaScriptStatus status;
	int playerNum = Nanosaur2Script_GetPlayerNum(playerObj);

	if (!pickupObj || pickupObj->CType == INVALID_NODE_FLAG || playerNum < 0 || playerNum >= MAX_PLAYERS || !PangeaScript_HasRunnableModule())
		return false;

	if (pickupObj->ScriptObjectID == 0)
		Nanosaur2Script_RegisterObject(pickupObj, pickupId ? pickupId : "nanosaur2.pickup", "pickup");

	if (playerObj && playerObj->ScriptObjectID == 0)
		Nanosaur2Script_RegisterPlayerObject(playerObj);

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
		.levelNum = gLevelNum,
		.playerNum = playerNum,
		.pickupId = pickupId ? pickupId : "nanosaur2.pickup",
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

	if (context.healthDelta != 0.0f)
		Nanosaur2Script_ApplyHealthDelta(playerNum, context.healthDelta, &pickupObj->Coord);
	if (context.scoreDelta > 0)
		gPlayerInfo[playerNum].numFreeLives += context.scoreDelta;
	if (context.scoreDelta < 0)
	{
		int livesDelta = -context.scoreDelta;
		gPlayerInfo[playerNum].numFreeLives = livesDelta > gPlayerInfo[playerNum].numFreeLives ? 0 : gPlayerInfo[playerNum].numFreeLives - livesDelta;
	}

	return context.consumePickup;
}

Boolean Nanosaur2Script_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage)
{
	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (weaponObj && weaponObj->CType != INVALID_NODE_FLAG)
		Nanosaur2Script_RegisterObject(weaponObj, weaponId ? weaponId : "nanosaur2.weaponHit", "weapon");
	Nanosaur2Script_RegisterObject(targetObj, "nanosaur2.enemy", "enemy");

	PangeaScriptObjectHandle weaponHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
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

	PangeaScriptWeaponHitContext context =
	{
		.levelNum = gLevelNum,
		.playerNum = 0,
		.weaponId = weaponId ? weaponId : "nanosaur2.weaponHit",
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
		gPlayerInfo[0].numFreeLives += context.scoreDelta;
	if (context.scoreDelta < 0)
	{
		int livesDelta = -context.scoreDelta;
		gPlayerInfo[0].numFreeLives = livesDelta > gPlayerInfo[0].numFreeLives ? 0 : gPlayerInfo[0].numFreeLives - livesDelta;
	}
	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		Nanosaur2Script_DeleteObject(targetObj);
		return true;
	}
	if (context.applyDamage)
	{
		*ioDamage = context.damage < 0.0f ? 0.0f : context.damage;
		return false;
	}

	return true;
}

Boolean Nanosaur2Script_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage)
{
	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (sourceObj && sourceObj->CType != INVALID_NODE_FLAG)
		Nanosaur2Script_RegisterObject(sourceObj, damageId ? damageId : "nanosaur2.objectDamage", "damageSource");
	Nanosaur2Script_RegisterObject(targetObj, "nanosaur2.damageTarget", "damageTarget");

	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
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

	PangeaScriptObjectDamageContext context =
	{
		.levelNum = gLevelNum,
		.playerNum = 0,
		.damageId = damageId ? damageId : "nanosaur2.objectDamage",
		.damageType = damageType,
		.source = sourceHandle,
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

	PangeaScriptStatus status = PangeaScript_CallObjectDamageHook(&context);
	LogScriptStatus("onObjectDamage", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta > 0)
		gPlayerInfo[0].numFreeLives += context.scoreDelta;
	if (context.scoreDelta < 0)
	{
		int livesDelta = -context.scoreDelta;
		gPlayerInfo[0].numFreeLives = livesDelta > gPlayerInfo[0].numFreeLives ? 0 : gPlayerInfo[0].numFreeLives - livesDelta;
	}
	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		Nanosaur2Script_DeleteObject(targetObj);
		return true;
	}
	if (context.applyDamage)
	{
		*ioDamage = context.damage < 0.0f ? 0.0f : context.damage;
		return false;
	}

	return true;
}

Boolean Nanosaur2Script_OnPlayerDamage(short playerNum, float* ioDamage, Byte deathType, OGLPoint3D* where)
{
	if (!ioDamage || playerNum < 0 || playerNum >= MAX_PLAYERS || !PangeaScript_HasRunnableModule())
		return false;

	ObjNode* playerObj = gPlayerInfo[playerNum].objNode;
	if (playerObj && playerObj->ScriptObjectID == 0)
		Nanosaur2Script_RegisterPlayerObject(playerObj);

	PangeaScriptObjectHandle playerHandle = {0};
	if (playerObj && playerObj->ScriptObjectID > 0)
	{
		playerHandle.id = (int) playerObj->ScriptObjectID;
		playerHandle.generation = playerObj->ScriptObjectGeneration;
	}

	PangeaScriptVector3 position = {0};
	if (where)
		position = (PangeaScriptVector3){ where->x, where->y, where->z };
	else if (playerObj)
		position = (PangeaScriptVector3){ playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z };

	PangeaScriptPlayerDamageContext context =
	{
		.levelNum = gLevelNum,
		.playerNum = playerNum,
		.damageId = "nanosaur2.playerDamage",
		.damageType = deathType,
		.damage = *ioDamage,
		.source = {0},
		.player = playerHandle,
		.position = position,
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
		gPlayerInfo[playerNum].numFreeLives += context.scoreDelta;
	if (context.scoreDelta < 0)
	{
		int livesDelta = -context.scoreDelta;
		gPlayerInfo[playerNum].numFreeLives = livesDelta > gPlayerInfo[playerNum].numFreeLives ? 0 : gPlayerInfo[playerNum].numFreeLives - livesDelta;
	}

	if (context.healthDelta > 0.0f)
		Nanosaur2Script_ApplyHealthDelta(playerNum, context.healthDelta, where);
	else if (context.healthDelta < 0.0f)
		context.damage += -context.healthDelta;

	if (context.damage < 0.0f)
		context.damage = 0.0f;
	*ioDamage = context.damage;

	return context.handled && !context.applyDamage;
}

#endif

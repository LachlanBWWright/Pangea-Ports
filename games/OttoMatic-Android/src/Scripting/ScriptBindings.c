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
	{
		.id = "ottomatic.bumperBubble",
		.nativeType = 30,
		.category = "trigger",
		.dependencySummary = "bumper bubble collision and movement systems",
	},
	{
		.id = "ottomatic.fallingSlimePlatform",
		.nativeType = 40,
		.category = "platform",
		.dependencySummary = "slime platform collision, movement, and terrain systems",
	},
	{
		.id = "ottomatic.spinningPlatform",
		.nativeType = 55,
		.category = "platform",
		.dependencySummary = "spinning platform collision and movement systems",
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

static bool OttoObjectGetInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outInfo || node->CType == INVALID_NODE_FLAG)
		return false;

	*outInfo = (PangeaScriptObjectInfo)
	{
		.type = node->Type,
		.kind = node->Kind,
		.mode = node->Mode,
		.statusBits = node->StatusBits,
		.cType = node->CType,
		.cBits = node->CBits,
		.health = node->Health,
		.damage = node->Damage,
		.velocity = { node->Delta.x, node->Delta.y, node->Delta.z },
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

static bool OttoObjectSetInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !info || node->CType == INVALID_NODE_FLAG)
		return false;

	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_TYPE)
		node->Type = info->type;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_KIND)
		node->Kind = info->kind;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_MODE)
		node->Mode = info->mode;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_FLAGS)
		node->StatusBits = info->statusBits;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_COLLISION)
	{
		node->CType = info->cType;
		node->CBits = info->cBits;
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_HEALTH)
		node->Health = info->health;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_DAMAGE)
		node->Damage = info->damage;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_VELOCITY)
	{
		node->Delta.x = info->velocity.x;
		node->Delta.y = info->velocity.y;
		node->Delta.z = info->velocity.z;
	}

	return true;
}

static bool OttoObjectGetBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outBounds || node->CType == INVALID_NODE_FLAG)
		return false;

	*outBounds = (PangeaScriptObjectBounds)
	{
		.left = node->LeftOff,
		.right = node->RightOff,
		.front = node->FrontOff,
		.back = node->BackOff,
		.top = node->TopOff,
		.bottom = node->BottomOff,
	};
	return true;
}

static bool OttoObjectSetBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !bounds || node->CType == INVALID_NODE_FLAG)
		return false;

	node->LeftOff = (int) bounds->left;
	node->RightOff = (int) bounds->right;
	node->FrontOff = (int) bounds->front;
	node->BackOff = (int) bounds->back;
	node->TopOff = (int) bounds->top;
	node->BottomOff = (int) bounds->bottom;
	CalcObjectBoxFromNode(node);
	return true;
}

static bool OttoObjectGetParams(void* nativeObject, PangeaScriptObjectParams* outParams)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outParams || node->CType == INVALID_NODE_FLAG)
		return false;

	*outParams = (PangeaScriptObjectParams){0};
	const Byte* params = NULL;
	if (node->TerrainItemPtr)
		params = node->TerrainItemPtr->parm;
	else if (node->SplineItemPtr)
		params = node->SplineItemPtr->parm;

	if (!params)
		return true;

	outParams->count = 4;
	for (int i = 0; i < outParams->count; i++)
		outParams->values[i] = params[i];
	return true;
}

static bool OttoObjectSetParams(void* nativeObject, const PangeaScriptObjectParams* params)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !params || node->CType == INVALID_NODE_FLAG || params->count < 0 || params->count > 4)
		return false;

	Byte* itemParams = NULL;
	if (node->TerrainItemPtr)
		itemParams = node->TerrainItemPtr->parm;
	else if (node->SplineItemPtr)
		itemParams = node->SplineItemPtr->parm;

	if (!itemParams)
		return false;

	for (int i = 0; i < params->count; i++)
		itemParams[i] = (Byte) params->values[i];
	return true;
}

static const PangeaScriptObjectOps kOttoObjectNodeOps =
{
	.getPosition = OttoObjectGetPosition,
	.setPosition = OttoObjectSetPosition,
	.setVelocity = OttoObjectSetVelocity,
	.deleteObject = OttoObjectDelete,
	.getInfo = OttoObjectGetInfo,
	.setInfo = OttoObjectSetInfo,
	.getBounds = OttoObjectGetBounds,
	.setBounds = OttoObjectSetBounds,
	.getParams = OttoObjectGetParams,
	.setParams = OttoObjectSetParams,
};

static int OttoScript_GetCurrentWeaponQuantity(void)
{
	int index = FindWeaponInventoryIndex(gPlayerInfo.currentWeaponType);
	if (index == NO_INVENTORY_HERE)
		return 0;

	return gPlayerInfo.weaponInventory[index].quantity;
}

static bool OttoScript_SetWeaponInventory(int weaponType, int quantity)
{
	if (weaponType < 0 || weaponType >= NUM_WEAPON_TYPES || quantity < 0)
		return false;

	if (weaponType == WEAPON_TYPE_FIST)
	{
		gPlayerInfo.weaponInventory[0].type = WEAPON_TYPE_FIST;
		gPlayerInfo.weaponInventory[0].quantity = 99;
		gPlayerInfo.currentWeaponType = WEAPON_TYPE_FIST;
		return true;
	}

	int index = FindWeaponInventoryIndex((short) weaponType);
	if (quantity == 0)
	{
		if (index != NO_INVENTORY_HERE)
		{
			gPlayerInfo.weaponInventory[index].type = NO_INVENTORY_HERE;
			gPlayerInfo.weaponInventory[index].quantity = 0;
		}
		if (gPlayerInfo.currentWeaponType == weaponType)
			gPlayerInfo.currentWeaponType = WEAPON_TYPE_FIST;
		return true;
	}

	if (index == NO_INVENTORY_HERE)
	{
		IncWeaponQuantity((short) weaponType, (short) quantity);
	}
	else
	{
		if (quantity > 99)
			quantity = 99;
		gPlayerInfo.weaponInventory[index].quantity = (short) quantity;
	}

	gPlayerInfo.currentWeaponType = (short) weaponType;
	return true;
}

static bool OttoScript_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum != 0 || !outInfo)
		return false;

	*outInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = playerNum,
		.score = (int) gScore,
		.health = gPlayerInfo.health,
		.lives = gPlayerInfo.lives,
		.fuel = gPlayerInfo.fuel,
		.inventoryType = gPlayerInfo.currentWeaponType,
		.inventoryQuantity = OttoScript_GetCurrentWeaponQuantity(),
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_SCORE
			| PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_LIVES
			| PANGEA_SCRIPT_PLAYER_INFO_FUEL
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY,
	};
	return true;
}

static bool OttoScript_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum != 0 || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SCORE)
		gScore = info->score < 0 ? 0 : (uint32_t) info->score;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
		gPlayerInfo.health = info->health;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
		gPlayerInfo.lives = (Byte) info->lives;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_FUEL)
		gPlayerInfo.fuel = info->fuel;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE)
	{
		int quantity = info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY
			? info->inventoryQuantity
			: OttoScript_GetCurrentWeaponQuantity();
		if (quantity <= 0)
			quantity = 1;
		if (!OttoScript_SetWeaponInventory(info->inventoryType, quantity))
			return false;
	}
	else if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY)
	{
		if (!OttoScript_SetWeaponInventory(gPlayerInfo.currentWeaponType, info->inventoryQuantity))
			return false;
	}
	return true;
}

static bool OttoScript_PlaySound(const PangeaScriptSoundRequest* request)
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

static bool OttoScript_SpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request || !request->hasPosition)
		return false;

	OGLPoint3D position = { request->position.x, request->position.y, request->position.z };
	OGLVector3D velocity = request->hasVelocity
		? (OGLVector3D){ request->velocity.x, request->velocity.y, request->velocity.z }
		: (OGLVector3D){0, 0, 0};
	float scale = request->hasScale ? request->scale : 1.0f;

	switch (request->effectId)
	{
		case 0:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_WhiteSpark4, 100);
			return true;

		case 1:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_BlueSpark, 100);
			return true;

		case 2:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f, scale, PARTICLE_SObjType_RedSpark, 100);
			return true;

		case 3:
			MakePuff(&position, 20.0f * scale, PARTICLE_SObjType_GreySmoke, GL_SRC_ALPHA, GL_ONE, 1.0f);
			return true;

		case 4:
			MakeSplash(position.x, position.y, position.z);
			return true;

		case 5:
			MakeFireExplosion(position.x, position.z, &velocity);
			return true;

		default:
			return false;
	}
}

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
		.getPlayerInfo = OttoScript_GetPlayerInfo,
		.setPlayerInfo = OttoScript_SetPlayerInfo,
		.playSound = OttoScript_PlaySound,
		.spawnEffect = OttoScript_SpawnEffect,
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

void OttoScript_RegisterTaggedObjectNode(ObjNode* theNode, const char* nativeId, const char* category)
{
	const char* tags[2];
	int tagCount = 0;

	if (nativeId)
	{
		tags[tagCount++] = nativeId;
	}
	if (category)
	{
		tags[tagCount++] = category;
	}

	OttoScript_RegisterObjectNode(theNode, PANGEA_SCRIPT_CAPABILITY_FULL, tags, tagCount);
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

Boolean OttoScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid)
{
	PangeaScriptObjectHandle triggerHandle = {0};
	PangeaScriptObjectHandle otherHandle = {0};
	PangeaScriptTriggerContext context;

	if (!triggerObj || triggerObj->CType == INVALID_NODE_FLAG)
		return false;

	if (triggerObj->ScriptObjectID > 0)
	{
		triggerHandle.id = triggerObj->ScriptObjectID;
		triggerHandle.generation = triggerObj->ScriptObjectGeneration;
	}
	if (otherObj && otherObj->ScriptObjectID > 0)
	{
		otherHandle.id = otherObj->ScriptObjectID;
		otherHandle.generation = otherObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptTriggerContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
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
		PlayerLoseHealth(context.damagePlayer, PLAYER_DEATH_TYPE_EXPLODE);
	if (context.healthDelta > 0.0f)
	{
		gPlayerInfo.health += context.healthDelta;
		if (gPlayerInfo.health > 1.0f)
			gPlayerInfo.health = 1.0f;
	}
	else if (context.healthDelta < 0.0f)
	{
		PlayerLoseHealth(-context.healthDelta, PLAYER_DEATH_TYPE_EXPLODE);
	}
	if (context.deleteOther && otherObj && otherObj != gPlayerInfo.objNode && otherObj->CType != INVALID_NODE_FLAG)
	{
		OttoScript_UnregisterObjectNode(otherObj);
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
		OttoScript_UnregisterObjectNode(triggerObj);
	}
	if (outSolid)
		*outSolid = context.solid;

	return true;
}

Boolean OttoScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits)
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
		selfHandle.generation = selfObj->ScriptObjectGeneration;
	}
	if (otherObj->ScriptObjectID > 0)
	{
		otherHandle.id = otherObj->ScriptObjectID;
		otherHandle.generation = otherObj->ScriptObjectGeneration;
	}

	PangeaScriptObjectCollisionContext context =
	{
		.levelNum = gCurrentFrameContext.levelNum,
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
	{
		if (selfObj == gPlayerInfo.objNode)
			PlayerLoseHealth(context.damage, PLAYER_DEATH_TYPE_EXPLODE);
		else
			selfObj->Health -= context.damage;
	}
	if (context.healthDelta > 0.0f && selfObj == gPlayerInfo.objNode)
	{
		gPlayerInfo.health += context.healthDelta;
		if (gPlayerInfo.health > 1.0f)
			gPlayerInfo.health = 1.0f;
	}
	else if (context.healthDelta < 0.0f && selfObj == gPlayerInfo.objNode)
	{
		PlayerLoseHealth(-context.healthDelta, PLAYER_DEATH_TYPE_EXPLODE);
	}
	if (context.deleteOther && otherObj != gPlayerInfo.objNode && otherObj->CType != INVALID_NODE_FLAG)
	{
		OttoScript_UnregisterObjectNode(otherObj);
		DeleteObject(otherObj);
	}
	if (context.deleteSelf && selfObj != gPlayerInfo.objNode && selfObj->CType != INVALID_NODE_FLAG)
	{
		OttoScript_UnregisterObjectNode(selfObj);
		DeleteObject(selfObj);
	}

	return context.suppressNative;
}

Boolean OttoScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount)
{
	PangeaScriptObjectHandle pickupHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	PangeaScriptPickupContext context;
	PangeaScriptStatus status;

	if (!pickupObj || pickupObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (pickupObj->ScriptObjectID <= 0)
		OttoScript_RegisterTaggedObjectNode(pickupObj, pickupId ? pickupId : "ottomatic.pickup", "pickup");

	if (playerObj && playerObj->ScriptObjectID <= 0)
		OttoScript_RegisterTaggedObjectNode(playerObj, "ottomatic.player", "player");

	if (pickupObj->ScriptObjectID > 0)
	{
		pickupHandle.id = pickupObj->ScriptObjectID;
		pickupHandle.generation = pickupObj->ScriptObjectGeneration;
	}
	if (playerObj && playerObj->ScriptObjectID > 0)
	{
		playerHandle.id = playerObj->ScriptObjectID;
		playerHandle.generation = playerObj->ScriptObjectGeneration;
	}

	context = (PangeaScriptPickupContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.pickupId = pickupId ? pickupId : "ottomatic.pickup",
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
	{
		gPlayerInfo.health += context.healthDelta;
		if (gPlayerInfo.health > 1.0f)
			gPlayerInfo.health = 1.0f;
	}
	else if (context.healthDelta < 0.0f)
	{
		PlayerLoseHealth(-context.healthDelta, PLAYER_DEATH_TYPE_EXPLODE);
	}

	return context.consumePickup;
}

Boolean OttoScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage)
{
	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (weaponObj && weaponObj->CType != INVALID_NODE_FLAG)
		OttoScript_RegisterTaggedObjectNode(weaponObj, weaponId ? weaponId : "ottomatic.weaponHit", "weapon");
	OttoScript_RegisterTaggedObjectNode(targetObj, "ottomatic.enemy", "enemy");

	PangeaScriptObjectHandle weaponHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
	if (weaponObj && weaponObj->ScriptObjectID > 0)
	{
		weaponHandle.id = weaponObj->ScriptObjectID;
		weaponHandle.generation = weaponObj->ScriptObjectGeneration;
	}
	if (targetObj->ScriptObjectID > 0)
	{
		targetHandle.id = targetObj->ScriptObjectID;
		targetHandle.generation = targetObj->ScriptObjectGeneration;
	}

	PangeaScriptWeaponHitContext context =
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.weaponId = weaponId ? weaponId : "ottomatic.weaponHit",
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
		OttoScript_UnregisterObjectNode(targetObj);
		DeleteObject(targetObj);
		return true;
	}
	if (context.applyDamage)
	{
		*ioDamage = context.damage < 0.0f ? 0.0f : context.damage;
		return false;
	}

	return true;
}

Boolean OttoScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage)
{
	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (sourceObj && sourceObj->CType != INVALID_NODE_FLAG)
		OttoScript_RegisterTaggedObjectNode(sourceObj, damageId ? damageId : "ottomatic.objectDamage", "damageSource");
	OttoScript_RegisterTaggedObjectNode(targetObj, "ottomatic.damageTarget", "damageTarget");

	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle targetHandle = {0};
	if (sourceObj && sourceObj->ScriptObjectID > 0)
	{
		sourceHandle.id = sourceObj->ScriptObjectID;
		sourceHandle.generation = sourceObj->ScriptObjectGeneration;
	}
	if (targetObj->ScriptObjectID > 0)
	{
		targetHandle.id = targetObj->ScriptObjectID;
		targetHandle.generation = targetObj->ScriptObjectGeneration;
	}

	PangeaScriptObjectDamageContext context =
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.damageId = damageId ? damageId : "ottomatic.objectDamage",
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
		OttoScript_UnregisterObjectNode(targetObj);
		DeleteObject(targetObj);
		return true;
	}
	if (context.applyDamage)
	{
		*ioDamage = context.damage < 0.0f ? 0.0f : context.damage;
		return false;
	}

	return true;
}

Boolean OttoScript_OnPlayerDamage(float* ioDamage, Byte deathType, ObjNode* sourceObj)
{
	PangeaScriptObjectHandle sourceHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	ObjNode* playerObj = gPlayerInfo.objNode;

	if (!ioDamage || !PangeaScript_HasRunnableModule())
		return false;

	if (sourceObj && sourceObj->ScriptObjectID > 0)
	{
		sourceHandle.id = sourceObj->ScriptObjectID;
		sourceHandle.generation = sourceObj->ScriptObjectGeneration;
	}
	if (playerObj && playerObj->ScriptObjectID > 0)
	{
		playerHandle.id = playerObj->ScriptObjectID;
		playerHandle.generation = playerObj->ScriptObjectGeneration;
	}

	PangeaScriptPlayerDamageContext context =
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.damageId = "ottomatic.playerDamage",
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
	{
		gPlayerInfo.health += context.healthDelta;
		if (gPlayerInfo.health > 1.0f)
			gPlayerInfo.health = 1.0f;
	}
	else if (context.healthDelta < 0.0f)
	{
		context.damage += -context.healthDelta;
	}

	if (context.damage < 0.0f)
		context.damage = 0.0f;
	*ioDamage = context.damage;

	return context.handled && !context.applyDamage;
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

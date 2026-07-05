#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"

#include "structs.h"
#include "player.h"
#include "triggers.h"

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;

static bool CroMagScript_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = obj->Coord.x;
	outPosition->y = obj->Coord.y;
	outPosition->z = obj->Coord.z;
	return true;
}

static bool CroMagScript_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	return true;
}

static bool CroMagScript_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	return true;
}

static bool CroMagScript_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	CroMagScript_UnregisterObject(obj);
	DeleteObject(obj);
	return true;
}

static bool CroMagScript_GetObjectInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
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
		.velocity = { obj->Delta.x, obj->Delta.y, obj->Delta.z },
		.validMask = PANGEA_SCRIPT_OBJECT_INFO_TYPE
			| PANGEA_SCRIPT_OBJECT_INFO_KIND
			| PANGEA_SCRIPT_OBJECT_INFO_MODE
			| PANGEA_SCRIPT_OBJECT_INFO_FLAGS
			| PANGEA_SCRIPT_OBJECT_INFO_COLLISION
			| PANGEA_SCRIPT_OBJECT_INFO_HEALTH
			| PANGEA_SCRIPT_OBJECT_INFO_VELOCITY,
	};
	return true;
}

static bool CroMagScript_SetObjectInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
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
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_VELOCITY)
	{
		obj->Delta.x = info->velocity.x;
		obj->Delta.y = info->velocity.y;
		obj->Delta.z = info->velocity.z;
	}

	return true;
}

static bool CroMagScript_GetObjectBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
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

static bool CroMagScript_SetObjectBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !bounds || obj->CType == INVALID_NODE_FLAG || !obj->CollisionBoxes)
		return false;

	obj->LeftOff = (int) bounds->left;
	obj->RightOff = (int) bounds->right;
	obj->FrontOff = (int) bounds->front;
	obj->BackOff = (int) bounds->back;
	obj->TopOff = (int) bounds->top;
	obj->BottomOff = (int) bounds->bottom;
	CalcObjectBoxFromNode(obj);
	return true;
}

static bool CroMagScript_GetObjectParams(void* nativeObject, PangeaScriptObjectParams* outParams)
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

static bool CroMagScript_SetObjectParams(void* nativeObject, const PangeaScriptObjectParams* params)
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

static const PangeaScriptObjectOps kCroMagPlayerObjectOps =
{
	.getPosition = CroMagScript_GetObjectPosition,
	.setPosition = CroMagScript_SetObjectPosition,
	.setVelocity = CroMagScript_SetObjectVelocity,
	.deleteObject = CroMagScript_DeleteObject,
	.getInfo = CroMagScript_GetObjectInfo,
	.setInfo = CroMagScript_SetObjectInfo,
	.getBounds = CroMagScript_GetObjectBounds,
	.setBounds = CroMagScript_SetObjectBounds,
	.getParams = CroMagScript_GetObjectParams,
	.setParams = CroMagScript_SetObjectParams,
};

static bool CroMagScript_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS || !outInfo)
		return false;

	*outInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = playerNum,
		.health = gPlayerInfo[playerNum].health,
		.inventoryType = gPlayerInfo[playerNum].powType,
		.inventoryQuantity = gPlayerInfo[playerNum].powQuantity,
		.boostTimer = gPlayerInfo[playerNum].nitroTimer,
		.tractionTimer = gPlayerInfo[playerNum].stickyTiresTimer,
		.invisibilityTimer = gPlayerInfo[playerNum].invisibilityTimer,
		.hazardTimer = gPlayerInfo[playerNum].greasedTiresTimer,
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY
			| PANGEA_SCRIPT_PLAYER_INFO_BOOST_TIMER
			| PANGEA_SCRIPT_PLAYER_INFO_TRACTION_TIMER
			| PANGEA_SCRIPT_PLAYER_INFO_INVISIBILITY_TIMER
			| PANGEA_SCRIPT_PLAYER_INFO_HAZARD_TIMER,
	};
	return true;
}

static bool CroMagScript_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
		gPlayerInfo[playerNum].health = info->health;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE)
		gPlayerInfo[playerNum].powType = (short) info->inventoryType;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY)
		gPlayerInfo[playerNum].powQuantity = (short) (info->inventoryQuantity < 0 ? 0 : info->inventoryQuantity);
	if (gPlayerInfo[playerNum].powQuantity == 0)
		gPlayerInfo[playerNum].powType = POW_TYPE_NONE;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_BOOST_TIMER)
		gPlayerInfo[playerNum].nitroTimer = info->boostTimer < 0.0f ? 0.0f : info->boostTimer;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_TRACTION_TIMER)
		gPlayerInfo[playerNum].stickyTiresTimer = info->tractionTimer < 0.0f ? 0.0f : info->tractionTimer;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVISIBILITY_TIMER)
		gPlayerInfo[playerNum].invisibilityTimer = info->invisibilityTimer < 0.0f ? 0.0f : info->invisibilityTimer;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HAZARD_TIMER)
		gPlayerInfo[playerNum].greasedTiresTimer = info->hazardTimer < 0.0f ? 0.0f : info->hazardTimer;
	return true;
}

void CroMagScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void CroMagScript_ResetObjectRegistry(void)
{
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	PangeaScript_ResetObjects();
}

void CroMagScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
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
		.ops = &kCroMagPlayerObjectOps,
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

void CroMagScript_UnregisterObject(ObjNode* obj)
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

void CroMagScript_RegisterPlayerObject(ObjNode* playerObj)
{
	CroMagScript_RegisterObject(playerObj, "cromag.player", "player");
}

void CroMagScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	CroMagScript_UnregisterObject(playerObj);
}

void CroMagScript_ApplyObjectScripting(ObjNode* obj)
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

void CroMagScript_RunObjectFrame(ObjNode* obj)
{
	CroMagScript_ApplyObjectScripting(obj);
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "cromag.pow",
		.nativeType = 5,
		.category = "pickup",
		.dependencySummary = "powerup trigger assets, player inventory, and terrain systems",
	},
	{
		.id = "cromag.token",
		.nativeType = 11,
		.category = "pickup",
		.dependencySummary = "token scoring state, infobar, and terrain systems",
	},
	{
		.id = "cromag.stickyTiresPow",
		.nativeType = 12,
		.category = "pickup",
		.dependencySummary = "sticky tire pickup assets, vehicle physics state, and terrain systems",
	},
	{
		.id = "cromag.suspensionPow",
		.nativeType = 13,
		.category = "pickup",
		.dependencySummary = "suspension pickup assets, vehicle physics state, and terrain systems",
	},
	{
		.id = "cromag.invisibilityPow",
		.nativeType = 29,
		.category = "pickup",
		.dependencySummary = "invisibility pickup assets, player visibility state, and terrain systems",
	},
	{
		.id = "cromag.cactus",
		.nativeType = TRIGTYPE_CACTUS,
		.category = "trigger",
		.dependencySummary = "track trap collision, player health, and object manager",
	},
	{
		.id = "cromag.snowman",
		.nativeType = TRIGTYPE_SNOMAN,
		.category = "trigger",
		.dependencySummary = "track trap collision, shatter effects, and object manager",
	},
	{
		.id = "cromag.campfire",
		.nativeType = TRIGTYPE_CAMPFIRE,
		.category = "trigger",
		.dependencySummary = "track trap collision, player fire state, and object manager",
	},
	{
		.id = "cromag.teamTorch",
		.nativeType = TRIGTYPE_TEAMTORCH,
		.category = "trigger",
		.dependencySummary = "capture-the-flag team state and object manager",
	},
	{
		.id = "cromag.teamBase",
		.nativeType = TRIGTYPE_TEAMBASE,
		.category = "trigger",
		.dependencySummary = "capture-the-flag scoring state and object manager",
	},
	{
		.id = "cromag.vase",
		.nativeType = TRIGTYPE_VASE,
		.category = "trigger",
		.dependencySummary = "track prop collision, shatter effects, and object manager",
	},
	{
		.id = "cromag.cauldron",
		.nativeType = TRIGTYPE_CAULDRON,
		.category = "trigger",
		.dependencySummary = "track prop collision, effects, and object manager",
	},
	{
		.id = "cromag.gong",
		.nativeType = TRIGTYPE_GONG,
		.category = "trigger",
		.dependencySummary = "track prop collision, sound/effects, and object manager",
	},
	{
		.id = "cromag.landMine",
		.nativeType = TRIGTYPE_LANDMINE,
		.category = "trigger",
		.dependencySummary = "mine collision, explosion effects, player physics, and object manager",
	},
	{
		.id = "cromag.seaMine",
		.nativeType = TRIGTYPE_SEAMINE,
		.category = "trigger",
		.dependencySummary = "mine collision, explosion effects, player physics, and object manager",
	},
	{
		.id = "cromag.lava",
		.nativeType = TRIGTYPE_LAVA,
		.category = "trigger",
		.dependencySummary = "lava collision and player health state",
	},
	{
		.id = "cromag.druid",
		.nativeType = TRIGTYPE_DRUID,
		.category = "trigger",
		.dependencySummary = "druid collision, effects, and object manager",
	},
};

static bool CroMagScript_PlaySound(const PangeaScriptSoundRequest* request)
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

static bool CroMagScript_SpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request || !request->hasPosition)
		return false;

	OGLPoint3D position = { request->position.x, request->position.y, request->position.z };
	float scale = request->hasScale ? request->scale : 1.0f;

	switch (request->effectId)
	{
		case 0:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f * scale, PARTICLE_SObjType_WhiteSpark);
			return true;

		case 1:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f * scale, PARTICLE_SObjType_Dirt);
			return true;

		case 2:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f * scale, PARTICLE_SObjType_Fire);
			return true;

		case 3:
			MakeShockwave(position.x, position.y, position.z);
			return true;

		case 4:
			MakeConeBlast(position.x, position.y, position.z);
			return true;

		case 5:
			MakeSparkExplosion(position.x, position.y, position.z, 300.0f * scale, PARTICLE_SObjType_WhiteSpark);
			MakeShockwave(position.x, position.y + 100.0f, position.z);
			MakeConeBlast(position.x, position.y, position.z);
			return true;

		case 6:
			MakeSnowExplosion(&position);
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

	SDL_Log("Cro-Mag Rally scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void CroMagScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "CroMagRally-Android",
		.gameName = "Cro-Mag Rally",
		.getPlayerInfo = CroMagScript_GetPlayerInfo,
		.setPlayerInfo = CroMagScript_SetPlayerInfo,
		.playSound = CroMagScript_PlaySound,
		.spawnEffect = CroMagScript_SpawnEffect,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	CroMagScript_ResetObjectRegistry();
}

void CroMagScript_Shutdown(void)
{
	CroMagScript_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void CroMagScript_LoadTrackConfig(int trackNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(trackNum);
	LogScriptStatus("track config load", status);
}

static void CallRaceHook(int trackNum, const char* hookName)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = trackNum,
		.levelName = NULL,
	};

	PangeaScriptStatus status = PangeaScript_CallNamedLevelHook(hookName, &context);
	LogScriptStatus(hookName, status);
}

void CroMagScript_OnRaceLoad(int trackNum)
{
	CallRaceHook(trackNum, "onRaceLoad");
}

void CroMagScript_OnRaceStart(int trackNum)
{
	CallRaceHook(trackNum, "onRaceStart");
}

void CroMagScript_OnRaceFrame(int trackNum, unsigned int frameNum, float deltaSeconds, float raceTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = trackNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = raceTimeSeconds,
	};
	CroMagScript_CacheFrameContext(&context);

	PangeaScriptStatus status = PangeaScript_CallNamedFrameHook("onRaceFrame", &context);
	LogScriptStatus("onRaceFrame", status);
}

void CroMagScript_OnRaceComplete(int trackNum)
{
	CallRaceHook(trackNum, "onRaceComplete");
}

void CroMagScript_OnRaceUnload(int trackNum)
{
	CallRaceHook(trackNum, "onRaceUnload");
	PangeaScript_ResetObjects();
}

int CroMagScript_RemapTerrainItemType(int trackNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(trackNum, itemType);
}

Boolean CroMagScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int trackNum, int playerNum, int originalType, int remappedType, float x, float z)
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
		.levelNum = trackNum,
		.itemType = originalType,
		.remappedItemType = remappedType,
		.playerNum = playerNum,
		.networked = false,
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

static int CroMagScript_GetPlayerNum(ObjNode* obj)
{
	if (!obj)
		return 0;

	if (obj->PlayerNum >= 0 && obj->PlayerNum < MAX_PLAYERS)
		return obj->PlayerNum;

	return 0;
}

static void CroMagScript_DisableTriggerObject(ObjNode* obj)
{
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return;

	CroMagScript_UnregisterObject(obj);
	obj->CType = 0;
	obj->CBits = 0;
	obj->StatusBits |= STATUS_BIT_HIDDEN;
	if (obj->ShadowNode)
		obj->ShadowNode->StatusBits |= STATUS_BIT_HIDDEN;
}

static void CroMagScript_ApplyHealthDelta(int playerNum, float healthDelta)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return;

	if (healthDelta > 0.0f)
	{
		gPlayerInfo[playerNum].health += healthDelta;
		if (gPlayerInfo[playerNum].health > 1.0f)
			gPlayerInfo[playerNum].health = 1.0f;
		return;
	}

	if (healthDelta < 0.0f)
	{
		PlayerLoseHealth((short) playerNum, -healthDelta);
	}
}

Boolean CroMagScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid)
{
	PangeaScriptTriggerContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};
	int playerNum = CroMagScript_GetPlayerNum(otherObj);

	if (!triggerObj || triggerObj->CType == INVALID_NODE_FLAG)
		return false;

	if (triggerObj->ScriptObjectID == 0)
		CroMagScript_RegisterObject(triggerObj, triggerId, "trigger");

	if (otherObj && otherObj->ScriptObjectID == 0)
		CroMagScript_RegisterObject(otherObj, "cromag.player", "player");

	(void) CroMagScript_GetObjectPosition(triggerObj, &position);
	context = (PangeaScriptTriggerContext)
	{
		.levelNum = gTrackNum,
		.playerNum = playerNum,
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

	if (context.scoreDelta != 0 && playerNum >= 0 && playerNum < MAX_PLAYERS)
		gPlayerInfo[playerNum].numTokens += context.scoreDelta;

	if (context.damagePlayer > 0.0f && playerNum >= 0 && playerNum < MAX_PLAYERS)
		PlayerLoseHealth((short) playerNum, context.damagePlayer);

	if (context.healthDelta != 0.0f)
		CroMagScript_ApplyHealthDelta(playerNum, context.healthDelta);

	if (context.deleteOther && otherObj && otherObj != gPlayerInfo[playerNum].objNode)
		CroMagScript_DisableTriggerObject(otherObj);

	if (context.deleteSelf)
		CroMagScript_DisableTriggerObject(triggerObj);

	if (outSolid)
		*outSolid = context.solid;
	return true;
}

Boolean CroMagScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits)
{
	PangeaScriptObjectCollisionContext context;
	PangeaScriptVector3 position = {0};
	PangeaScriptStatus status;
	int playerNum = CroMagScript_GetPlayerNum(selfObj);

	if (!selfObj || !otherObj || selfObj->CType == INVALID_NODE_FLAG || otherObj->CType == INVALID_NODE_FLAG)
		return false;

	if (!PangeaScript_HasRunnableModule())
		return false;

	if (selfObj->ScriptObjectID == 0)
	{
		if (playerNum >= 0 && playerNum < MAX_PLAYERS && selfObj == gPlayerInfo[playerNum].objNode)
			CroMagScript_RegisterObject(selfObj, "cromag.player", "player");
		else
			CroMagScript_RegisterObject(selfObj, "cromag.object", "object");
	}
	if (otherObj->ScriptObjectID == 0)
		CroMagScript_RegisterObject(otherObj, "cromag.object", "object");

	(void) CroMagScript_GetObjectPosition(otherObj, &position);
	context = (PangeaScriptObjectCollisionContext)
	{
		.levelNum = gTrackNum,
		.playerNum = playerNum,
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
		.damage = 0.0f,
	};

	status = PangeaScript_CallObjectCollisionHook(&context);
	LogScriptStatus("onObjectCollision", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0 && playerNum >= 0 && playerNum < MAX_PLAYERS)
		gPlayerInfo[playerNum].numTokens += context.scoreDelta;

	if (context.applyDamage && context.damage != 0.0f)
	{
		if (playerNum >= 0 && playerNum < MAX_PLAYERS && selfObj == gPlayerInfo[playerNum].objNode)
			PlayerLoseHealth((short) playerNum, context.damage);
		else
			selfObj->Health -= context.damage;
	}

	if (context.healthDelta != 0.0f)
		CroMagScript_ApplyHealthDelta(playerNum, context.healthDelta);

	if (context.deleteOther && (playerNum < 0 || playerNum >= MAX_PLAYERS || otherObj != gPlayerInfo[playerNum].objNode) && otherObj->CType != INVALID_NODE_FLAG)
		CroMagScript_DeleteObject(otherObj);

	if (context.deleteSelf && (playerNum < 0 || playerNum >= MAX_PLAYERS || selfObj != gPlayerInfo[playerNum].objNode) && selfObj->CType != INVALID_NODE_FLAG)
		CroMagScript_DeleteObject(selfObj);

	return context.suppressNative || context.deleteSelf || context.deleteOther;
}

Boolean CroMagScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount)
{
	PangeaScriptPickupContext context;
	PangeaScriptVector3 position = {0};
	PangeaScriptStatus status;
	int playerNum = CroMagScript_GetPlayerNum(playerObj);

	if (!pickupObj || pickupObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (pickupObj->ScriptObjectID == 0)
		CroMagScript_RegisterObject(pickupObj, pickupId, "pickup");

	if (playerObj && playerObj->ScriptObjectID == 0)
		CroMagScript_RegisterObject(playerObj, "cromag.player", "player");

	(void) CroMagScript_GetObjectPosition(pickupObj, &position);
	context = (PangeaScriptPickupContext)
	{
		.levelNum = gTrackNum,
		.playerNum = playerNum,
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

	if (context.scoreDelta != 0 && playerNum >= 0 && playerNum < MAX_PLAYERS)
		gPlayerInfo[playerNum].numTokens += context.scoreDelta;

	if (context.healthDelta != 0.0f)
		CroMagScript_ApplyHealthDelta(playerNum, context.healthDelta);

	return context.consumePickup;
}

Boolean CroMagScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, short targetPlayerNum, const OGLPoint3D* position, float* ioDamage)
{
	if (!targetObj || !ioDamage || targetPlayerNum < 0 || targetPlayerNum >= MAX_PLAYERS || !PangeaScript_HasRunnableModule())
		return false;

	if (weaponObj && weaponObj->CType != INVALID_NODE_FLAG)
		CroMagScript_RegisterObject(weaponObj, weaponId ? weaponId : "cromag.weaponHit", "weapon");
	if (targetObj->ScriptObjectID == 0)
		CroMagScript_RegisterPlayerObject(targetObj);

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

	PangeaScriptVector3 hitPosition = position
		? (PangeaScriptVector3){ position->x, position->y, position->z }
		: (PangeaScriptVector3){ targetObj->Coord.x, targetObj->Coord.y, targetObj->Coord.z };

	PangeaScriptWeaponHitContext context =
	{
		.levelNum = gTrackNum,
		.playerNum = targetPlayerNum,
		.weaponId = weaponId ? weaponId : "cromag.weaponHit",
		.weaponType = weaponType,
		.weapon = weaponHandle,
		.target = targetHandle,
		.position = hitPosition,
		.targetType = targetObj->Type,
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

	if (context.scoreDelta != 0)
		gPlayerInfo[targetPlayerNum].numTokens += context.scoreDelta;
	if (context.destroyTarget)
	{
		*ioDamage = gPlayerInfo[targetPlayerNum].health + 1.0f;
		return false;
	}
	if (context.applyDamage)
	{
		*ioDamage = context.damage < 0.0f ? 0.0f : context.damage;
		return false;
	}

	return true;
}

Boolean CroMagScript_OnPlayerDamage(short playerNum, float* ioDamage)
{
	if (!ioDamage || playerNum < 0 || playerNum >= MAX_PLAYERS || !PangeaScript_HasRunnableModule())
		return false;

	ObjNode* playerObj = gPlayerInfo[playerNum].objNode;
	if (playerObj && playerObj->ScriptObjectID == 0)
		CroMagScript_RegisterPlayerObject(playerObj);

	PangeaScriptObjectHandle playerHandle = {0};
	if (playerObj && playerObj->ScriptObjectID > 0)
	{
		playerHandle.id = playerObj->ScriptObjectID;
		playerHandle.generation = (uint32_t) playerObj->ScriptObjectGeneration;
	}

	PangeaScriptPlayerDamageContext context =
	{
		.levelNum = gTrackNum,
		.playerNum = playerNum,
		.damageId = "cromag.playerDamage",
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
		gPlayerInfo[playerNum].numTokens += context.scoreDelta;

	if (context.healthDelta > 0.0f)
		CroMagScript_ApplyHealthDelta(playerNum, context.healthDelta);
	else if (context.healthDelta < 0.0f)
		context.damage += -context.healthDelta;

	if (context.damage < 0.0f)
		context.damage = 0.0f;
	*ioDamage = context.damage;

	return context.handled && !context.applyDamage;
}

#endif

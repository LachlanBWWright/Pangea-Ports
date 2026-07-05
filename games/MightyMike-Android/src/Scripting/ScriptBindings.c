#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"

#include "bonus.h"
#include "externs.h"
#include "enemy.h"
#include "miscanims.h"
#include "misc.h"
#include "myglobals.h"
#include "myguy.h"
#include "object.h"
#include "shape.h"
#include "sound2.h"
#include "triggers.h"
#include "weapon.h"

#include <SDL3/SDL.h>

static PangeaScriptFrameContext gScriptFrameContext;

void GetPoints(long points);
void ShowHealth(void);
void ShowKeys(void);
void ShowWeaponIcon(void);

enum
{
	MIKE_SCRIPT_MAX_WEAPON_AMMO = 500,
	MIKE_SCRIPT_MAX_EFFECT_QUANTITY = 24,
};

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

static bool MikeScript_GetObjectBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outBounds || obj->CType == INVALID_NODE_FLAG)
		return false;

	*outBounds = (PangeaScriptObjectBounds)
	{
		.left = (float) obj->LeftOff,
		.right = (float) obj->RightOff,
		.front = 0.0f,
		.back = 0.0f,
		.top = (float) obj->TopOff,
		.bottom = (float) obj->BottomOff,
	};
	return true;
}

static bool MikeScript_SetObjectBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !bounds || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->LeftOff = (long) bounds->left;
	obj->RightOff = (long) bounds->right;
	obj->TopOff = (long) bounds->top;
	obj->BottomOff = (long) bounds->bottom;
	CalcObjectBox2(obj);
	return true;
}

static bool MikeScript_GetObjectParams(void* nativeObject, PangeaScriptObjectParams* outParams)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outParams || obj->CType == INVALID_NODE_FLAG)
		return false;

	*outParams = (PangeaScriptObjectParams){0};
	if (!obj->ItemIndex)
		return true;

	outParams->count = 4;
	for (int i = 0; i < outParams->count; i++)
		outParams->values[i] = obj->ItemIndex->parm[i];
	return true;
}

static bool MikeScript_SetObjectParams(void* nativeObject, const PangeaScriptObjectParams* params)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !params || obj->CType == INVALID_NODE_FLAG || params->count < 0 || params->count > 4 || !obj->ItemIndex)
		return false;

	for (int i = 0; i < params->count; i++)
		obj->ItemIndex->parm[i] = (Byte) params->values[i];
	return true;
}

static bool MikeScript_DeletePlayerObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	MikeScript_UnregisterObject(obj);
	DeleteObject(obj);
	return true;
}

static bool MikeScript_GetObjectInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outInfo || obj->CType == INVALID_NODE_FLAG)
		return false;

	*outInfo = (PangeaScriptObjectInfo)
	{
		.type = obj->Type,
		.kind = obj->Kind,
		.cType = obj->CType,
		.cBits = obj->CBits,
		.health = (float) obj->Health,
		.velocity = { MikeScript_FixedToFloat(obj->DX), MikeScript_FixedToFloat(obj->DY), MikeScript_FixedToFloat(obj->DZ) },
		.validMask = PANGEA_SCRIPT_OBJECT_INFO_TYPE
			| PANGEA_SCRIPT_OBJECT_INFO_KIND
			| PANGEA_SCRIPT_OBJECT_INFO_COLLISION
			| PANGEA_SCRIPT_OBJECT_INFO_HEALTH
			| PANGEA_SCRIPT_OBJECT_INFO_VELOCITY,
	};
	return true;
}

static bool MikeScript_SetObjectInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !info || obj->CType == INVALID_NODE_FLAG)
		return false;

	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_TYPE)
		obj->Type = info->type;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_KIND)
		obj->Kind = info->kind;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_COLLISION)
	{
		obj->CType = info->cType;
		obj->CBits = info->cBits;
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_HEALTH)
		obj->Health = (long) info->health;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_VELOCITY)
	{
		obj->DX = MikeScript_FloatToFixed(info->velocity.x);
		obj->DY = MikeScript_FloatToFixed(info->velocity.y);
		obj->DZ = MikeScript_FloatToFixed(info->velocity.z);
		MikeScript_SyncPlayerGlobals(obj);
	}

	return true;
}

static const PangeaScriptObjectOps kMikePlayerObjectOps =
{
	.getPosition = MikeScript_GetObjectPosition,
	.setPosition = MikeScript_SetObjectPosition,
	.setVelocity = MikeScript_SetObjectVelocity,
	.getBounds = MikeScript_GetObjectBounds,
	.setBounds = MikeScript_SetObjectBounds,
	.getParams = MikeScript_GetObjectParams,
	.setParams = MikeScript_SetObjectParams,
	.deleteObject = MikeScript_DeletePlayerObject,
	.getInfo = MikeScript_GetObjectInfo,
	.setInfo = MikeScript_SetObjectInfo,
};

static bool MikeScript_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum != 0 || !outInfo)
		return false;

	int keyMask = 0;
	for (int i = 0; i < 6; i++)
	{
		if (gMyKeys[i])
			keyMask |= 1 << i;
	}

	int currentWeaponQuantity = 0;
	if (gCurrentWeaponIndex < gNumWeaponsIHave)
		currentWeaponQuantity = gMyWeapons[gCurrentWeaponIndex].life;

	*outInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = playerNum,
		.score = (int) gScore,
		.health = (float) gMyHealth,
		.lives = gNumLives,
		.currency = keyMask,
		.inventoryType = gCurrentWeaponType,
		.inventoryQuantity = currentWeaponQuantity,
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_SCORE
			| PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_LIVES
			| PANGEA_SCRIPT_PLAYER_INFO_CURRENCY
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE
			| PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY,
	};
	return true;
}

static bool MikeScript_SetWeaponInventory(int weaponType, int quantity)
{
	if (weaponType < 0 || weaponType >= NUM_WEAPON_TYPES || quantity < 0)
		return false;

	if (quantity > MIKE_SCRIPT_MAX_WEAPON_AMMO)
		quantity = MIKE_SCRIPT_MAX_WEAPON_AMMO;
	if (weaponType == WEAPON_TYPE_SUCTIONCUP && quantity == 0)
		quantity = 1;

	for (Byte i = 0; i < gNumWeaponsIHave; i++)
	{
		if (gMyWeapons[i].type != weaponType)
			continue;

		if (quantity == 0)
		{
			gCurrentWeaponIndex = i;
			gCurrentWeaponType = (Byte) weaponType;
			RemoveCurrentWeaponFromInventory();
			return true;
		}

		gMyWeapons[i].life = (uint16_t) quantity;
		gCurrentWeaponIndex = i;
		gCurrentWeaponType = (Byte) weaponType;
		ShowWeaponIcon();
		return true;
	}

	if (quantity == 0)
		return true;

	if (gNumWeaponsIHave >= MAX_WEAPONS)
		return false;

	gMyWeapons[gNumWeaponsIHave].type = (uint8_t) weaponType;
	gMyWeapons[gNumWeaponsIHave].life = (uint16_t) quantity;
	gCurrentWeaponIndex = gNumWeaponsIHave;
	gCurrentWeaponType = (Byte) weaponType;
	gNumWeaponsIHave++;
	ShowWeaponIcon();
	return true;
}

static bool MikeScript_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum != 0 || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SCORE)
		gScore = info->score < 0 ? 0 : (long) info->score;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
	{
		gMyHealth = (short) info->health;
		ShowHealth();
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
		gNumLives = (short) info->lives;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_CURRENCY)
	{
		for (int i = 0; i < 6; i++)
			gMyKeys[i] = (info->currency & (1 << i)) != 0;
		ShowKeys();
	}
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE)
	{
		int quantity = info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY
			? info->inventoryQuantity
			: 1;
		if (!MikeScript_SetWeaponInventory(info->inventoryType, quantity))
			return false;
	}
	else if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY)
	{
		if (!MikeScript_SetWeaponInventory(gCurrentWeaponType, info->inventoryQuantity))
			return false;
	}
	return true;
}

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
	{
		.id = "mightymike.teleport",
		.nativeType = TRIGTYPE_TELEPORT,
		.category = "trigger",
		.dependencySummary = "teleport destination lookup, player movement globals, and object manager",
	},
	{
		.id = "mightymike.door",
		.nativeType = TRIGTYPE_DOOR,
		.category = "trigger",
		.dependencySummary = "key inventory, door animation, item memory, and object manager",
	},
	{
		.id = "mightymike.fairyDoor",
		.nativeType = TRIGTYPE_FAIRYDOOR,
		.category = "trigger",
		.dependencySummary = "key inventory, door animation, item memory, and object manager",
	},
	{
		.id = "mightymike.bargainDoor",
		.nativeType = TRIGTYPE_BARGAINDOOR,
		.category = "trigger",
		.dependencySummary = "key inventory, door animation, item memory, and object manager",
	},
};

static bool MikeScript_PlaySound(const PangeaScriptSoundRequest* request)
{
	if (!request)
		return false;

	PlaySound((short) request->soundId);
	return true;
}

static int MikeScript_GetEffectQuantity(const PangeaScriptEffectRequest* request, int defaultQuantity)
{
	if (!request->hasQuantity)
		return defaultQuantity;
	if (request->quantity < 1)
		return 1;
	if (request->quantity > MIKE_SCRIPT_MAX_EFFECT_QUANTITY)
		return MIKE_SCRIPT_MAX_EFFECT_QUANTITY;
	return request->quantity;
}

static bool MikeScript_SpawnSmokePuff(short x, short y, short z, const PangeaScriptEffectRequest* request)
{
	(void) request;

	ObjNode* newObj = MakeNewShape(GroupNum_RocketGun, ObjType_RocketGun, 8, x, y, z, nil, PLAYFIELD_RELATIVE);
	if (!newObj)
		return false;

	newObj->AnimSpeed += MyRandomLong() & 0xff;
	InitYOffset(newObj, 24);
	return true;
}

static bool MikeScript_SpawnSplat(short x, short y, short z, const PangeaScriptEffectRequest* request)
{
	(void) request;

	ObjNode* newObj = MakeNewShape(GroupNum_Splat, ObjType_Splat, 0, x, y, z, MoveEnemySplat, PLAYFIELD_RELATIVE);
	if (!newObj)
		return false;

	newObj->Special1 = (MyRandomLong() & 0b11111) + (GAME_FPS * 2);
	InitYOffset(newObj, -15);
	newObj->DZ = -0x80000L - MyRandomShort();
	newObj->DX = ((long) MyRandomShort() * 4) - 0x10000L;
	newObj->DY = ((long) MyRandomShort() * 4) - 0x10000L;
	return true;
}

static bool MikeScript_SpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request || !request->hasPosition)
		return false;

	short x = (short) SDL_roundf(request->position.x);
	short y = (short) SDL_roundf(request->position.y);
	short z = (short) SDL_roundf(request->position.z);
	int quantity;

	switch (request->effectId)
	{
		case 0:
			MakeSplash(x, y, z);
			return true;

		case 1:
			MakeCoins(x, y, z, (short) MikeScript_GetEffectQuantity(request, 1));
			return true;

		case 2:
			quantity = MikeScript_GetEffectQuantity(request, 1);
			for (int i = 0; i < quantity; i++)
			{
				if (!MikeScript_SpawnSmokePuff(x, y, z, request))
					return i > 0;
			}
			return true;

		case 3:
			quantity = MikeScript_GetEffectQuantity(request, 4);
			for (int i = 0; i < quantity; i++)
			{
				if (!MikeScript_SpawnSplat(x, y, z, request))
					return i > 0;
			}
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

void MikeScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
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
		.ops = &kMikePlayerObjectOps,
		.tags = tags,
		.tagCount = tagCount,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	obj->ScriptVisualOffsetX = 0;
	obj->ScriptVisualOffsetY = 0;
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

void MikeScript_UnregisterObject(ObjNode* obj)
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
	obj->ScriptVisualOffsetX = 0;
	obj->ScriptVisualOffsetY = 0;
}

void MikeScript_RegisterPlayerObject(ObjNode* playerObj)
{
	MikeScript_RegisterObject(playerObj, "mightymike.player", "player");
}

void MikeScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	MikeScript_UnregisterObject(playerObj);
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
		.getPlayerInfo = MikeScript_GetPlayerInfo,
		.setPlayerInfo = MikeScript_SetPlayerInfo,
		.playSound = MikeScript_PlaySound,
		.spawnEffect = MikeScript_SpawnEffect,
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

static void CallAreaHook(int sceneNum, int areaNum, const char* hookName)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = GetAreaLevelNum(sceneNum, areaNum),
		.levelName = NULL,
	};

	PangeaScriptStatus status = PangeaScript_CallNamedLevelHook(hookName, &context);
	LogScriptStatus(hookName, status);
}

void MikeScript_OnAreaLoad(int sceneNum, int areaNum)
{
	CallAreaHook(sceneNum, areaNum, "onAreaLoad");
}

void MikeScript_OnAreaStart(int sceneNum, int areaNum)
{
	CallAreaHook(sceneNum, areaNum, "onAreaStart");
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

	PangeaScriptStatus status = PangeaScript_CallNamedFrameHook("onAreaFrame", &context);
	LogScriptStatus("onAreaFrame", status);
}

void MikeScript_OnAreaComplete(int sceneNum, int areaNum)
{
	CallAreaHook(sceneNum, areaNum, "onAreaComplete");
}

void MikeScript_OnAreaUnload(int sceneNum, int areaNum)
{
	CallAreaHook(sceneNum, areaNum, "onAreaUnload");
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

static void MikeScript_DeleteTriggerObject(ObjNode* obj)
{
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return;

	MikeScript_UnregisterObject(obj);
	obj->CType = 0;
	obj->CBits = 0;
}

static void MikeScript_ApplyHealthDelta(float healthDelta)
{
	if (healthDelta > 0.0f)
	{
		gMyHealth += (short) healthDelta;
		if (gMyHealth > gMyMaxHealth)
			gMyHealth = gMyMaxHealth;
		ShowHealth();
		return;
	}

	if (healthDelta < 0.0f)
	{
		gMyHealth += (short) healthDelta;
		ShowHealth();
		if (gMyHealth < 0)
			IGotHurt();
	}
}

Boolean MikeScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid)
{
	PangeaScriptTriggerContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};

	if (!triggerObj || triggerObj->CType == INVALID_NODE_FLAG)
		return false;

	if (triggerObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(triggerObj, triggerId, "trigger");

	if (otherObj && otherObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(otherObj, "mightymike.player", "player");

	(void) MikeScript_GetObjectPosition(triggerObj, &position);
	context = (PangeaScriptTriggerContext)
	{
		.levelNum = GetAreaLevelNum(gSceneNum, gAreaNum),
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
		.otherType = otherObj ? (int) otherObj->Type : 0,
		.otherFlags = otherObj ? otherObj->CType : 0,
		.solid = true,
	};

	status = PangeaScript_CallTriggerEnterHook(&context);
	LogScriptStatus("onTriggerEnter", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
	{
		GetPoints(context.scoreDelta);
	}

	if (context.damagePlayer > 0.0f)
	{
		gMyHealth -= (short) context.damagePlayer;
		ShowHealth();
		if (gMyHealth < 0)
			IGotHurt();
	}

	if (context.healthDelta != 0.0f)
	{
		MikeScript_ApplyHealthDelta(context.healthDelta);
	}

	if (context.deleteOther && otherObj && otherObj != gMyNodePtr)
	{
		MikeScript_UnregisterObject(otherObj);
		DeleteObject(otherObj);
	}

	if (context.deleteSelf)
	{
		MikeScript_DeleteTriggerObject(triggerObj);
	}

	if (outSolid)
		*outSolid = context.solid;
	return true;
}

Boolean MikeScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits)
{
	PangeaScriptObjectCollisionContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};
	float damage = 0.0f;

	if (!selfObj || !otherObj || selfObj->CType == INVALID_NODE_FLAG || otherObj->CType == INVALID_NODE_FLAG)
		return false;

	if (!PangeaScript_HasRunnableModule())
		return false;

	if (selfObj->ScriptObjectID == 0)
	{
		if (selfObj == gMyNodePtr)
			MikeScript_RegisterObject(selfObj, "mightymike.player", "player");
		else
			MikeScript_RegisterObject(selfObj, "mightymike.object", "object");
	}
	if (otherObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(otherObj, "mightymike.object", "object");

	if (otherObj->CType & CTYPE_MYBULLET)
		damage = (float) otherObj->WeaponPower;

	(void) MikeScript_GetObjectPosition(otherObj, &position);
	context = (PangeaScriptObjectCollisionContext)
	{
		.levelNum = GetAreaLevelNum(gSceneNum, gAreaNum),
		.playerNum = selfObj == gMyNodePtr ? 0 : -1,
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
		.selfType = (int) selfObj->Type,
		.selfFlags = selfObj->CType,
		.otherType = (int) otherObj->Type,
		.otherFlags = otherObj->CType,
		.damage = damage,
	};

	status = PangeaScript_CallObjectCollisionHook(&context);
	LogScriptStatus("onObjectCollision", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		GetPoints(context.scoreDelta);

	if (context.applyDamage && context.damage != 0.0f)
	{
		if (selfObj == gMyNodePtr)
		{
			MikeScript_ApplyHealthDelta(-context.damage);
		}
		else
		{
			selfObj->Health -= (long) context.damage;
		}
	}

	if (context.healthDelta != 0.0f && selfObj == gMyNodePtr)
		MikeScript_ApplyHealthDelta(context.healthDelta);

	if (context.deleteOther && otherObj != gMyNodePtr && otherObj->CType != INVALID_NODE_FLAG)
	{
		MikeScript_UnregisterObject(otherObj);
		DeleteObject(otherObj);
	}

	if (context.deleteSelf && selfObj != gMyNodePtr && selfObj->CType != INVALID_NODE_FLAG)
	{
		MikeScript_UnregisterObject(selfObj);
		DeleteObject(selfObj);
	}

	MikeScript_SyncPlayerGlobals(selfObj);
	return context.suppressNative || context.deleteSelf || context.deleteOther;
}

Boolean MikeScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount)
{
	PangeaScriptPickupContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};

	if (!pickupObj || pickupObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (pickupObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(pickupObj, pickupId, "pickup");

	if (playerObj && playerObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(playerObj, "mightymike.player", "player");

	(void) MikeScript_GetObjectPosition(pickupObj, &position);
	context = (PangeaScriptPickupContext)
	{
		.levelNum = GetAreaLevelNum(gSceneNum, gAreaNum),
		.playerNum = 0,
		.pickupId = pickupId,
		.pickupType = pickupType,
		.amount = amount,
		.pickup = {
			.id = (int) pickupObj->ScriptObjectID,
			.generation = pickupObj->ScriptObjectGeneration,
		},
		.player = {
			.id = playerObj ? (int) playerObj->ScriptObjectID : 0,
			.generation = playerObj ? playerObj->ScriptObjectGeneration : 0,
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
		GetPoints(context.scoreDelta);

	if (context.healthDelta != 0.0f)
		MikeScript_ApplyHealthDelta(context.healthDelta);

	return context.consumePickup;
}

Boolean MikeScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage)
{
	PangeaScriptWeaponHitContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};

	if (!weaponObj || !targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (weaponObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(weaponObj, weaponId, "weapon");

	if (targetObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(targetObj, "mightymike.enemy", "enemy");

	(void) MikeScript_GetObjectPosition(targetObj, &position);
	context = (PangeaScriptWeaponHitContext)
	{
		.levelNum = GetAreaLevelNum(gSceneNum, gAreaNum),
		.playerNum = 0,
		.weaponId = weaponId,
		.weaponType = weaponType,
		.damage = *ioDamage,
		.weapon = {
			.id = (int) weaponObj->ScriptObjectID,
			.generation = weaponObj->ScriptObjectGeneration,
		},
		.target = {
			.id = (int) targetObj->ScriptObjectID,
			.generation = targetObj->ScriptObjectGeneration,
		},
		.position = position,
		.targetType = (int) targetObj->Type,
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
		GetPoints(context.scoreDelta);

	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		KillEnemy(targetObj);
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

Boolean MikeScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage)
{
	PangeaScriptObjectDamageContext context;
	PangeaScriptStatus status;
	PangeaScriptVector3 position = {0};

	if (!targetObj || !ioDamage || targetObj->CType == INVALID_NODE_FLAG || !PangeaScript_HasRunnableModule())
		return false;

	if (sourceObj && sourceObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(sourceObj, damageId ? damageId : "mightymike.objectDamage", "damageSource");

	if (targetObj->ScriptObjectID == 0)
		MikeScript_RegisterObject(targetObj, "mightymike.damageTarget", "damageTarget");

	(void) MikeScript_GetObjectPosition(targetObj, &position);
	context = (PangeaScriptObjectDamageContext)
	{
		.levelNum = GetAreaLevelNum(gSceneNum, gAreaNum),
		.playerNum = 0,
		.damageId = damageId ? damageId : "mightymike.objectDamage",
		.damageType = damageType,
		.damage = *ioDamage,
		.source = {
			.id = sourceObj ? (int) sourceObj->ScriptObjectID : 0,
			.generation = sourceObj ? sourceObj->ScriptObjectGeneration : 0,
		},
		.target = {
			.id = (int) targetObj->ScriptObjectID,
			.generation = targetObj->ScriptObjectGeneration,
		},
		.position = position,
		.targetType = (int) targetObj->Type,
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
		GetPoints(context.scoreDelta);

	if (context.destroyTarget && targetObj->CType != INVALID_NODE_FLAG)
	{
		KillEnemy(targetObj);
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

Boolean MikeScript_OnPlayerDamage(float* ioDamage, const char* damageId, int damageType)
{
	ObjNode* playerObj = gMyNodePtr;
	PangeaScriptPlayerDamageContext context;
	PangeaScriptVector3 position = {0};
	PangeaScriptObjectHandle playerHandle = {0};
	PangeaScriptStatus status;

	if (!ioDamage || !PangeaScript_HasRunnableModule())
		return false;

	if (playerObj && playerObj->CType != INVALID_NODE_FLAG)
	{
		if (playerObj->ScriptObjectID == 0)
			MikeScript_RegisterObject(playerObj, "mightymike.player", "player");

		playerHandle = (PangeaScriptObjectHandle)
		{
			.id = (int) playerObj->ScriptObjectID,
			.generation = playerObj->ScriptObjectGeneration,
		};
		(void) MikeScript_GetObjectPosition(playerObj, &position);
	}

	context = (PangeaScriptPlayerDamageContext)
	{
		.levelNum = GetAreaLevelNum(gSceneNum, gAreaNum),
		.playerNum = 0,
		.damageId = damageId ? damageId : "mightymike.playerDamage",
		.damageType = damageType,
		.damage = *ioDamage,
		.player = playerHandle,
		.position = position,
		.handled = false,
		.applyDamage = true,
	};

	status = PangeaScript_CallPlayerDamageHook(&context);
	LogScriptStatus("onPlayerDamage", status);
	if (status != PANGEA_SCRIPT_OK || !context.handled)
		return false;

	if (context.scoreDelta != 0)
		GetPoints(context.scoreDelta);

	if (context.healthDelta > 0.0f)
		MikeScript_ApplyHealthDelta(context.healthDelta);
	else if (context.healthDelta < 0.0f)
		context.damage += -context.healthDelta;

	if (context.damage < 0.0f)
		context.damage = 0.0f;
	*ioDamage = context.damage;

	return context.handled && !context.applyDamage;
}

#endif

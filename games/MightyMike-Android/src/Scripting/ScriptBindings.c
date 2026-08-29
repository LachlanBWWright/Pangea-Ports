#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"

#include "externs.h"
#include "main.h"
#include "misc.h"
#include "myglobals.h"
#include "object.h"
#include "objecttypes.h"
#include "infobar.h"
#include "playfield.h"
#include "shape.h"
#include "triggers.h"


#include <SDL3/SDL.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

static ObjectEntryType gScriptItems[256];
static bool gScriptItemOccupied[256];
static bool gScriptItemReclaimable[256];
static int gScriptSelectedMapItemIndex = -1;

jmp_buf gMightyMikeScriptAssetJump;
bool gMightyMikeScriptAssetBoundaryActive = false;

static ObjectEntryType* AcquireScriptItem(void)
{
	for (int i = 0; i < 256; i++)
	{
		bool referenced = false;
		if (gScriptItemOccupied[i] && !gScriptItemReclaimable[i]) continue;
		for (ObjNode* node = FirstNodePtr; gScriptItemOccupied[i] && node; node = node->NextNode) referenced |= node->ItemIndex == &gScriptItems[i];
		if (referenced) continue;
		gScriptItemOccupied[i] = true; gScriptItemReclaimable[i] = false; memset(&gScriptItems[i], 0, sizeof(gScriptItems[i])); return &gScriptItems[i];
	}
	return NULL;
}

static PangeaScriptStatus SpawnNativeItem(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	(void) z;
	ObjectEntryType* item = AcquireScriptItem();
	if (!item) return PANGEA_SCRIPT_RUNTIME_ERROR;
	item->x = (int32_t) x; item->y = (int32_t) y;
	for (int i = 0; i < 4; i++) item->parm[i] = (Byte) params[i];
	if (!MightyMikeSpawnItem(PangeaScript_ResolveNativeItemType(id), item)) { gScriptItemOccupied[item - gScriptItems] = false; return PANGEA_SCRIPT_INCOMPATIBLE_ITEM; }
	for (ObjNode* node = FirstNodePtr; node; node = node->NextNode) if (node->ItemIndex == item)
	{
		MikeScript_RegisterObject(node, "mightymike.item", "item");
		gScriptItemReclaimable[item - gScriptItems] = true;
		if (outHandle) *outHandle = (PangeaScriptObjectHandle){node->ScriptObjectID, node->ScriptObjectGeneration};
		break;
	}
	return PANGEA_SCRIPT_OK;
}
#include <string.h>

static PangeaScriptFrameContext gScriptFrameContext;
static int gScriptInvulnerableFrames = 0;

#define MIKE_SCRIPT_SHAPE_GROUP_BASE 7
#define MIKE_SCRIPT_SHAPE_GROUP_COUNT 3

typedef struct ScriptShapeCacheEntry { char path[260]; } ScriptShapeCacheEntry;
static ScriptShapeCacheEntry gScriptShapeCache[MIKE_SCRIPT_SHAPE_GROUP_COUNT];

static int32_t MikeScript_FloatToFixed(float value)
{
	return (int32_t) SDL_roundf(value * 65536.0f);
}

static float MikeScript_FixedToFloat(int32_t value)
{
	return (float) value / 65536.0f;
}

static bool MakeShapeAssetPath(const char* source, char* destination, size_t capacity)
{
	const char prefix[] = "Data/";
	size_t length;
	if (!source || strncmp(source, prefix, sizeof(prefix) - 1) != 0) return false;
	length = strlen(source + sizeof(prefix) - 1);
	if (length + 2 > capacity) return false;
	destination[0] = ':';
	for (size_t i = 0; i <= length; i++)
	{
		char c = source[sizeof(prefix) - 1 + i];
		destination[i + 1] = c == '/' ? ':' : c;
	}
	return true;
}

static int GetCustomShapeGroup(const char* modelPath)
{
	char dataPath[260];
	short refNum;
	long size;
	if (!modelPath || modelPath[0] == '\0')
		return -1;
	for (int i = 0; i < MIKE_SCRIPT_SHAPE_GROUP_COUNT; i++)
	{
		int group = MIKE_SCRIPT_SHAPE_GROUP_BASE + i;
		if (!gShapeTableHandle[group]) gScriptShapeCache[i].path[0] = '\0';
		if (strcmp(gScriptShapeCache[i].path, modelPath) == 0) return group;
	}
	if (!MakeShapeAssetPath(modelPath, dataPath, sizeof(dataPath))) return -1;
	refNum = MightyMikeScript_OpenDataFile(dataPath);
	if (refNum < 0 || GetEOF(refNum, &size) != noErr || size <= 0 || size > 16 * 1024 * 1024)
	{
		if (refNum >= 0) FSClose(refNum);
		return -1;
	}
	FSClose(refNum);
	for (int i = 0; i < MIKE_SCRIPT_SHAPE_GROUP_COUNT; i++)
	{
		volatile int group = MIKE_SCRIPT_SHAPE_GROUP_BASE + i;
		if (gShapeTableHandle[group]) continue;
		int boundaryResult = setjmp(gMightyMikeScriptAssetJump);
		gMightyMikeScriptAssetBoundaryActive = true;
		if (boundaryResult == 0)
			LoadShapeTable(dataPath, group);
		else
			ZapShapeTable(group);
		gMightyMikeScriptAssetBoundaryActive = false;
		if (!gShapeTableHandle[group])
		{
			gScriptShapeCache[i].path[0] = '\0';
			return -1;
		}
		snprintf(gScriptShapeCache[i].path, sizeof(gScriptShapeCache[i].path), "%s", modelPath);
		return group;
	}
	return -1;
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

static bool MikeScript_SetObjectRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	(void)nativeObject;
	(void)rotation;
	return false;
}

static bool MikeScript_SetObjectScale(void* nativeObject, float scale)
{
	(void)nativeObject;
	return scale == 1.0f;
}

static bool MikeScript_SetObjectAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	(void)blendSeconds;
	if (!obj || obj->CType == INVALID_NODE_FLAG || animation < 0 || speed <= 0) return false;
	obj->SubType = animation;
	obj->AnimLine = 0;
	obj->CurrentFrame = 0;
	obj->AnimCount = 0;
	obj->AnimSpeed = (unsigned long)(speed * 256.0f);
	obj->AnimFlag = true;
	obj->ScriptAnimationCompletionSent = false;
	return true;
}

static bool MikeScript_SetObjectAnimationNamed(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	const PangeaScriptCustomObjectDefinition* definition;
	if (!obj || !animation || !obj->ScriptDefinitionID[0]) return false;
	definition = PangeaScript_GetCustomObjectDefinition(obj->ScriptDefinitionID);
	if (!definition) return false;
	for (int i = 0; i < definition->animationCount; i++)
		if (strcmp(definition->animationNames[i], animation) == 0)
			return MikeScript_SetObjectAnimation(obj, definition->animationIndices[i], speed, blendSeconds);
	return false;
}

static bool MikeScript_DeletePlayerObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	if (obj->ScriptDefinitionID[0])
	{
		if (obj->ItemIndex && !obj->ScriptStreamOutSent)
		{
			PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
			if (PangeaScript_ObjectExists(handle))
				(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		obj->ScriptDeleteRequested = true;
	}
	else { MikeScript_UnregisterObject(obj); DeleteObject(obj); }
	return true;
}

static const PangeaScriptObjectOps kMikePlayerObjectOps =
{
	.getPosition = MikeScript_GetObjectPosition,
	.setPosition = MikeScript_SetObjectPosition,
	.setVelocity = MikeScript_SetObjectVelocity,
	.setRotation = MikeScript_SetObjectRotation,
	.setScale = MikeScript_SetObjectScale,
	.setAnimation = MikeScript_SetObjectAnimation,
	.setAnimationNamed = MikeScript_SetObjectAnimationNamed,
	.deleteObject = MikeScript_DeletePlayerObject,
};

static int GetAreaLevelNum(int sceneNum, int areaNum)
{
	return (sceneNum * 3) + areaNum;
}

static const char* MikeScript_SceneName(int sceneNum)
{
	switch (sceneNum)
	{
		case SCENE_JURASSIC: return "jurassic";
		case SCENE_CANDY: return "candy";
		case SCENE_FAIRY: return "fairy";
		case SCENE_CLOWN: return "clown";
		case SCENE_BARGAIN: return "bargain";
		default: return NULL;
	}
}

static const char* MikeScript_AreaName(int areaNum)
{
	static const char* areaNames[] = {"area-1", "area-2", "area-3"};
	if (areaNum < 0 || areaNum >= (int)(sizeof(areaNames) / sizeof(areaNames[0]))) return NULL;
	return areaNames[areaNum];
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

static bool CompleteScriptReplacement(PangeaScriptObjectHandle handle, const char* action)
{
	PangeaScriptStatus status = PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "spawn");
	if (status == PANGEA_SCRIPT_OK && !PangeaScript_ObjectExists(handle))
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
	if (status != PANGEA_SCRIPT_OK)
	{
		LogScriptStatus(action, status);
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_DeleteObject(handle);
		return false;
	}
	status = PangeaScript_ApplyObjectLifecycle(
		handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_IN);
	if (status == PANGEA_SCRIPT_OK && !PangeaScript_ObjectExists(handle))
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
	if (status == PANGEA_SCRIPT_OK && PangeaScript_ObjectExists(handle))
		return true;
	LogScriptStatus(action, status);
	if (PangeaScript_ObjectExists(handle))
		(void)PangeaScript_DeleteObject(handle);
	return false;
}

void MikeScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void MikeScript_ResetObjectRegistry(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	memset(gScriptItemOccupied, 0, sizeof(gScriptItemOccupied));
	memset(gScriptItemReclaimable, 0, sizeof(gScriptItemReclaimable));
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

	if (category)
	{
		tags[tagCount++] = category;
	}

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = obj,
		.ops = &kMikePlayerObjectOps,
		.objectType = nativeId,
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
	if (PangeaScript_ObjectExists(handle))
		(void) PangeaScript_UnregisterObject(handle);
	obj->ScriptObjectID = 0;
	obj->ScriptObjectGeneration = 0;
	obj->ScriptVisualOffsetX = 0;
	obj->ScriptVisualOffsetY = 0;
}

void MikeScript_RegisterPlayerObject(ObjNode* playerObj)
{
	MikeScript_RegisterObject(playerObj, "mightymike.player", "player");
	MikeScript_OnPlayerSpawn(playerObj);
}

void MikeScript_OnPlayerSpawn(ObjNode* playerObj)
{
	PangeaScriptPlayerEventContext context;
	if (!playerObj || playerObj->ScriptObjectID <= 0 || playerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = 0,
		.player = {playerObj->ScriptObjectID, playerObj->ScriptObjectGeneration},
		.position = {
			MikeScript_FixedToFloat(playerObj->X.L),
			MikeScript_FixedToFloat(playerObj->Y.L),
			0.0f,
		},
	};
	LogScriptStatus("onPlayerSpawn", PangeaScript_CallPlayerEvent(&context, "onPlayerSpawn"));
}

void MikeScript_OnDeath(ObjNode* playerObj, int eventValue)
{
	PangeaScriptPlayerEventContext context;
	if (!playerObj || playerObj->ScriptObjectID <= 0 || playerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = eventValue,
		.player = {playerObj->ScriptObjectID, playerObj->ScriptObjectGeneration},
		.position = {
			MikeScript_FixedToFloat(playerObj->X.L),
			MikeScript_FixedToFloat(playerObj->Y.L),
			0.0f,
		},
	};
	LogScriptStatus("onDeath", PangeaScript_CallPlayerEvent(&context, "onDeath"));
}

void MikeScript_OnPlayerRespawn(ObjNode* playerObj)
{
	PangeaScriptPlayerEventContext context;
	if (!playerObj || playerObj->ScriptObjectID <= 0 || playerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = 0,
		.player = {playerObj->ScriptObjectID, playerObj->ScriptObjectGeneration},
		.position = {
			MikeScript_FixedToFloat(playerObj->X.L),
			MikeScript_FixedToFloat(playerObj->Y.L),
			0.0f,
		},
	};
	LogScriptStatus("onPlayerRespawn", PangeaScript_CallPlayerEvent(&context, "onPlayerRespawn"));
}

Boolean MikeScript_OnDamage(float damage, float* outDamage)
{
	PangeaScriptDamageContext context;
	PangeaScriptDamageResult result = {0};
	PangeaScriptStatus status;
	if (!outDamage)
		return true;
	*outDamage = damage;
	if (!gMyNodePtr || gMyNodePtr->ScriptObjectID <= 0 || gMyNodePtr->ScriptObjectGeneration <= 0)
		return true;
	context = (PangeaScriptDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.cause = 0,
		.damage = damage,
		.source = {0},
		.target = {gMyNodePtr->ScriptObjectID, gMyNodePtr->ScriptObjectGeneration},
		.position = {
			MikeScript_FixedToFloat(gMyNodePtr->X.L),
			MikeScript_FixedToFloat(gMyNodePtr->Y.L),
			0.0f,
		},
	};
	status = PangeaScript_CallDamageHook(&context, &result);
	LogScriptStatus("onDamage", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	if (result.hasDamage)
		*outDamage = result.damage;
	return result.hasApplyDamage ? result.applyDamage : true;
}

void MikeScript_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId)
{
	PangeaScriptPickupContext context;
	PangeaScriptPickupResult result = {0};
	PangeaScriptStatus status;
	if (!pickup || !player || pickup->ScriptObjectID <= 0 || pickup->ScriptObjectGeneration <= 0 ||
		player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPickupContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.pickupType = pickupType,
		.amount = amount,
		.pickupId = pickupId,
		.pickup = {pickup->ScriptObjectID, (uint32_t)pickup->ScriptObjectGeneration},
		.player = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration},
		.position = {MikeScript_FixedToFloat(pickup->X.L), MikeScript_FixedToFloat(pickup->Y.L), 0.0f},
	};
	status = PangeaScript_CallPickupHook(&context, &result);
	LogScriptStatus("onPickupCollected", status);
	if (status != PANGEA_SCRIPT_OK || !isfinite(result.healthDelta))
		return;
	if (result.healthDelta > 0.0f)
		gMyHealth += (short) SDL_ceilf(result.healthDelta);
	else if (result.healthDelta < 0.0f)
		gMyHealth += (short) SDL_floorf(result.healthDelta);
	if (gMyHealth < 0)
		gMyHealth = 0;
	else if (gMyHealth > gMyMaxHealth)
		gMyHealth = gMyMaxHealth;
	ShowHealth();
}

Boolean MikeScript_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget)
{
	PangeaScriptWeaponHitContext context;
	PangeaScriptWeaponHitResult result = {0};
	PangeaScriptStatus status;
	if (!outDamage || !outDestroyTarget)
		return true;
	*outDamage = damage;
	*outDestroyTarget = false;
	context = (PangeaScriptWeaponHitContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.weaponType = weapon ? weapon->Type : -1,
		.targetType = target ? target->Type : -1,
		.targetFlags = 0,
		.damage = damage,
		.weaponId = "mightymike.projectile",
		.weapon = {0},
		.target = {0},
		.position = target ? (PangeaScriptVector3){MikeScript_FixedToFloat(target->X.L), MikeScript_FixedToFloat(target->Y.L), 0.0f} : (PangeaScriptVector3){0},
	};
	if (weapon && weapon->ScriptObjectID > 0 && weapon->ScriptObjectGeneration > 0)
		context.weapon = (PangeaScriptObjectHandle){weapon->ScriptObjectID, (uint32_t)weapon->ScriptObjectGeneration};
	if (target && target->ScriptObjectID > 0 && target->ScriptObjectGeneration > 0)
		context.target = (PangeaScriptObjectHandle){target->ScriptObjectID, (uint32_t)target->ScriptObjectGeneration};
	status = PangeaScript_CallWeaponHitHook(&context, &result);
	LogScriptStatus("onWeaponHit", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	*outDamage = result.damage;
	*outDestroyTarget = result.destroyTarget;
	return result.hasApplyDamage ? result.applyDamage : true;
}

void MikeScript_OnDamageApplied(float damage)
{
	PangeaScriptDamageContext context;
	if (!gMyNodePtr || gMyNodePtr->ScriptObjectID <= 0 || gMyNodePtr->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.cause = 0,
		.damage = damage,
		.source = {0},
		.target = {gMyNodePtr->ScriptObjectID, gMyNodePtr->ScriptObjectGeneration},
		.position = {
			MikeScript_FixedToFloat(gMyNodePtr->X.L),
			MikeScript_FixedToFloat(gMyNodePtr->Y.L),
			0.0f,
		},
	};
	LogScriptStatus("onDamageApplied", PangeaScript_CallDamageAppliedHook(&context));
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

	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return;
	if (obj->ScriptDeleteRequested)
	{
		DeleteObject(obj);
		return;
	}
	if (obj->ScriptObjectID == 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = (int) obj->ScriptObjectID,
		.generation = obj->ScriptObjectGeneration,
	};
	if (!PangeaScript_ObjectExists(handle))
	{
		obj->ScriptObjectID = 0;
		obj->ScriptObjectGeneration = 0;
		return;
	}

	status = PangeaScript_CallObjectFrame(handle, &gScriptFrameContext, &result);
	if (!PangeaScript_ObjectExists(handle))
	{
		obj->ScriptObjectID = 0;
		obj->ScriptObjectGeneration = 0;
		obj->ScriptVisualOffsetX = 0;
		obj->ScriptVisualOffsetY = 0;
		PangeaScript_ClearLastError();
		return;
	}
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
	if (obj->ScriptDeleteRequested)
	{
		DeleteObject(obj);
		return;
	}
	if (obj->AnimConst == 0xffff && !obj->ScriptAnimationCompletionSent)
	{
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
		obj->ScriptAnimationCompletionSent = true;
	}
	if (result.hasPositionOffset)
	{
		obj->ScriptVisualOffsetX = MikeScript_FloatToFixed(result.positionOffset.x);
		obj->ScriptVisualOffsetY = MikeScript_FloatToFixed(result.positionOffset.y);
	}
}

void MikeScript_OnObjectDeleted(ObjNode* obj)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
		if (!PangeaScript_ObjectExists(handle))
			return;
		if (obj->ItemIndex && !obj->ScriptStreamOutSent)
		{
			(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		else if (!obj->ScriptStreamOutSent)
			(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	}
}

static int ResolveShapeGroup(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP) return GetCustomShapeGroup(definition->modelPath);
	if (strcmp(definition->nativeGroup, "global") == 0) return GROUP_MAIN;
	if (strcmp(definition->nativeGroup, "levelSpecific") == 0) return GROUP_AREA_SPECIFIC;
	if (strcmp(definition->nativeGroup, "levelSpecific2") == 0) return GROUP_AREA_SPECIFIC2;
	if (strcmp(definition->nativeGroup, "weapons") == 0) return GROUP_WEAPONS;
	return -1;
}

static int ResolveInitialAnimation(const PangeaScriptCustomObjectDefinition* definition)
{
	if (!definition->initialAnimationName[0]) return definition->initialAnimation;
	for (int i = 0; i < definition->animationCount; i++)
		if (strcmp(definition->animationNames[i], definition->initialAnimationName) == 0) return definition->animationIndices[i];
	return 0;
}

static void ApplyScriptedCollision(ObjNode* object, const PangeaScriptCustomObjectDefinition* definition)
{
	const FrameHeader* frame;
	PangeaScriptCollisionPreset preset = definition->collisionPreset;
	if (preset == PANGEA_SCRIPT_COLLISION_NONE) return;
	if (definition->collisionBoundsSet)
	{
		object->LeftOff = (short)(-definition->collisionWidth * 0.5f);
		object->RightOff = (short)(definition->collisionWidth * 0.5f);
		object->TopOff = (short)(-definition->collisionHeight * 0.5f);
		object->BottomOff = (short)(definition->collisionHeight * 0.5f);
	}
	else
	{
		frame = GetFrameHeader(object->SpriteGroupNum, object->Type, object->CurrentFrame, NULL, NULL);
		object->LeftOff = -frame->width / 2;
		object->RightOff = object->LeftOff + frame->width;
		object->TopOff = -frame->height / 2;
		object->BottomOff = object->TopOff + frame->height;
	}
	object->CBits = ALL_SOLID_SIDES;
	if (preset == PANGEA_SCRIPT_COLLISION_ENEMY) object->CType = CTYPE_ENEMYA;
	else if (preset == PANGEA_SCRIPT_COLLISION_PICKUP || preset == PANGEA_SCRIPT_COLLISION_TRIGGER_BOX)
	{
		object->CType = CTYPE_TRIGGER;
		object->TriggerType = TRIGTYPE_SCRIPTED;
		object->TriggerSides = ALL_SOLID_SIDES;
	}
	else if (preset == PANGEA_SCRIPT_COLLISION_PLATFORM) object->CType = CTYPE_MPLATFORM;
	else object->CType = CTYPE_MISC;
	CalcObjectBox2(object);
}

void MikeScript_OnCustomTrigger(ObjNode* trigger, Byte sideBits)
{
	PangeaScriptObjectHandle player = {0};
	if (!trigger || !trigger->ScriptObjectID) return;
	PangeaScriptObjectHandle handle = {(int)trigger->ScriptObjectID, trigger->ScriptObjectGeneration};
	if (!PangeaScript_ObjectExists(handle)) return;
	if (gMyNodePtr && gMyNodePtr->ScriptObjectID > 0)
	{
		player = (PangeaScriptObjectHandle){(int)gMyNodePtr->ScriptObjectID, gMyNodePtr->ScriptObjectGeneration};
		if (!PangeaScript_ObjectExists(player)) player = (PangeaScriptObjectHandle){0};
	}
	(void)PangeaScript_CallObjectTriggerWithOther(handle, &gScriptFrameContext, sideBits, true, player);
}

static PangeaScriptStatus SpawnScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
	const PangeaScriptCustomObjectDefinition* definition = PangeaScript_GetCustomObjectDefinition(id);
	ObjNode* object;
	int group;
	int animation;
	if (!definition || (definition->visualKind != PANGEA_SCRIPT_VISUAL_NATIVE_DISPLAY_GROUP && definition->visualKind != PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	group = ResolveShapeGroup(definition);
	animation = ResolveInitialAnimation(definition);
	if (group < 0 || definition->modelObject < 0 || definition->modelObject >= MAX_SHAPES_IN_FILE || !gSHAPE_HEADER_Ptrs[group][definition->modelObject]) return PANGEA_SCRIPT_RUNTIME_ERROR;
	object = MakeNewShape(group, definition->modelObject, animation, (short)x, (short)y, (short)z, nil, PLAYFIELD_RELATIVE);
	if (!object) return PANGEA_SCRIPT_RUNTIME_ERROR;
	ApplyScriptedCollision(object, definition);
	snprintf(object->ScriptDefinitionID, sizeof(object->ScriptDefinitionID), "%s", definition->id);
	MikeScript_RegisterObject(object, definition->id, "customObject");
	if (!object->ScriptObjectID) { DeleteObject(object); return PANGEA_SCRIPT_RUNTIME_ERROR; }
	if (outHandle) *outHandle = (PangeaScriptObjectHandle){(int)object->ScriptObjectID, object->ScriptObjectGeneration};
	return PANGEA_SCRIPT_OK;
}

static int GetScriptPlayerCount(void) { return gMyNodePtr ? 1 : 0; }

static PangeaScriptStatus SetScriptPlayerHealth(int playerNum, float health)
{
	if (playerNum != 0 || !gMyNodePtr || !isfinite(health) || health < 0.0f || health > 1.0f || gMyMaxHealth <= 0)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gMyHealth = (short) SDL_roundf(health * (float)gMyMaxHealth);
	ShowHealth();
	return PANGEA_SCRIPT_OK;
}

Boolean MikeScript_IsInvulnerable(void)
{
	return gScriptInvulnerableFrames > 0;
}

static PangeaScriptStatus SetScriptPlayerInvulnerable(int playerNum, float durationSeconds)
{
	if (playerNum != 0 || !gMyNodePtr || !isfinite(durationSeconds) || durationSeconds < 0.0f || durationSeconds > 3600.0f)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gScriptInvulnerableFrames = (int)SDL_ceilf(durationSeconds * (float)GAME_FPS);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerPosition(int playerNum, const PangeaScriptVector3* position)
{
	if (playerNum != 0 || !position || !gMyNodePtr)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	return MikeScript_SetObjectPosition(gMyNodePtr, position)
		? PANGEA_SCRIPT_OK
		: PANGEA_SCRIPT_RUNTIME_ERROR;
}

static PangeaScriptStatus SetScriptPlayerVelocity(int playerNum, const PangeaScriptVector3* velocity)
{
	if (playerNum != 0 || !velocity || !gMyNodePtr)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	return MikeScript_SetObjectVelocity(gMyNodePtr, velocity) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
}

static bool GetScriptPlayer(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	float health;
	if (playerNum != 0 || !outPlayer || !gMyNodePtr) return false;
	health = gMyMaxHealth > 0 ? (float)gMyHealth / (float)gMyMaxHealth : 0.0f;
	if (health < 0.0f) health = 0.0f;
	else if (health > 1.0f) health = 1.0f;
	*outPlayer = (PangeaScriptPlayerSnapshot){
		.position = {MikeScript_FixedToFloat(gMyNodePtr->X.L), MikeScript_FixedToFloat(gMyNodePtr->Y.L), (float)gMyNodePtr->Z},
		.health = health,
		.hasHealth = gMyMaxHealth > 0,
		.active = true,
	};
	return true;
}

void MikeScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "MightyMike-Android",
		.gameName = "Mighty Mike",
		.spawnNative = SpawnNativeItem,
		.spawnScripted = SpawnScriptedObject,
		.getPlayerCount = GetScriptPlayerCount,
		.getPlayer = GetScriptPlayer,
		.setPlayerHealth = SetScriptPlayerHealth,
		.setPlayerInvulnerable = SetScriptPlayerInvulnerable,
		.setPlayerPosition = SetScriptPlayerPosition,
		.setPlayerVelocity = SetScriptPlayerVelocity,
		.capabilities = PANGEA_SCRIPT_MIGHTY_MIKE_CAPABILITIES,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	MikeScript_ResetObjectRegistry();
	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	if (status == PANGEA_SCRIPT_OK)
		LogScriptStatus("onGameStart", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_START, &(PangeaScriptLevelContext){0}));
}

void MikeScript_Shutdown(void)
{
	LogScriptStatus("onGameShutdown", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN, &(PangeaScriptLevelContext){0}));
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
		.mode = "local",
		.networked = false,
		.sceneName = MikeScript_SceneName(sceneNum),
		.areaName = MikeScript_AreaName(areaNum),
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

void MikeScript_OnAreaLoad(int sceneNum, int areaNum)
{
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
		.mode = "local",
		.networked = false,
		.sceneName = MikeScript_SceneName(sceneNum),
		.areaName = MikeScript_AreaName(areaNum),
	};
	MikeScript_CacheFrameContext(&context);
	if (gScriptInvulnerableFrames > 0)
		gScriptInvulnerableFrames--;
	PangeaScript_ExpireTriggerContacts(&context);
	(void)PangeaScript_ApplyDeferredActions(&context);

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
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
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
		.sceneName = MikeScript_SceneName(sceneNum),
		.areaName = MikeScript_AreaName(areaNum),
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

Boolean MikeScript_TryReplaceMapItem(ObjectEntryType* itemPtr, int itemIndex, int nativeType)
{
	const float x = (float)itemPtr->x;
	const float y = (float)itemPtr->y;
	const PangeaScriptMapReplacement* replacement = PangeaScript_GetMapReplacement(itemIndex, nativeType, x, y);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	void* nativeObject = NULL;
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_MAP, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = y, .z = 0.0f};
	if (!replacement) return false;
	if (PangeaScript_FindObjectBySource(&source, &handle))
		return true;
	status = SpawnScriptedObject(replacement->customObjectId, x, y, 100, &handle);
	if (status == PANGEA_SCRIPT_OK && PangeaScript_GetObjectNativeObject(handle, &nativeObject))
	{
		((ObjNode*)nativeObject)->ItemIndex = itemPtr;
		status = PangeaScript_AssociateObjectSource(handle, &source);
		if (status != PANGEA_SCRIPT_OK)
		{
			LogScriptStatus("map replacement source association", status);
			(void)PangeaScript_DeleteObject(handle);
			if (!replacement->strict)
				PangeaScript_ClearLastError();
			return replacement->strict;
		}
		itemPtr->type |= ITEM_IN_USE;
		if (!CompleteScriptReplacement(handle, "map replacement stream-in"))
		{
			if (!replacement->strict)
				PangeaScript_ClearLastError();
			return replacement->strict;
		}
		return true;
	}
	if (status == PANGEA_SCRIPT_OK)
	{
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_DeleteObject(handle);
	}
	if (replacement->strict)
		LogScriptStatus("map replacement", status);
	else
		PangeaScript_ClearLastError();
	return replacement->strict;
}

EMSCRIPTEN_KEEPALIVE int MikeScript_SelectMapItemForReplacementJS(void)
{
	if (!gMasterItemList || gNumItems <= 0)
		return -1;
	for (int index = 0; index < gNumItems; index++)
	{
		if ((gMasterItemList[index].type & ITEM_IN_USE) != 0)
			continue;
		gScriptSelectedMapItemIndex = index;
		return index;
	}
	return -1;
}

EMSCRIPTEN_KEEPALIVE int MikeScript_GetSelectedMapItemFieldJS(int field)
{
	ObjectEntryType* item;
	if (gScriptSelectedMapItemIndex < 0 || gScriptSelectedMapItemIndex >= gNumItems || !gMasterItemList)
		return -1;
	item = &gMasterItemList[gScriptSelectedMapItemIndex];
	if (field == 0)
		return item->x;
	if (field == 1)
		return item->y;
	if (field == 2)
		return item->type & ITEM_NUM;
	return -1;
}

EMSCRIPTEN_KEEPALIVE int MikeScript_ProbeSelectedMapReplacementJS(void)
{
	ObjectEntryType* item;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptObjectSource source;
	int nativeType;
	if (gScriptSelectedMapItemIndex < 0 || gScriptSelectedMapItemIndex >= gNumItems || !gMasterItemList)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	item = &gMasterItemList[gScriptSelectedMapItemIndex];
	nativeType = item->type & ITEM_NUM;
	if (!PangeaScript_GetMapReplacement(gScriptSelectedMapItemIndex, nativeType, (float)item->x, (float)item->y))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!MikeScript_TryReplaceMapItem(item, gScriptSelectedMapItemIndex, nativeType))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	source = (PangeaScriptObjectSource){
		.kind = PANGEA_SCRIPT_SOURCE_MAP,
		.itemIndex = gScriptSelectedMapItemIndex,
		.nativeType = nativeType,
		.x = (float)item->x,
		.y = (float)item->y,
		.z = 0.0f};
	if (!PangeaScript_FindObjectBySource(&source, &handle) || handle.id <= 0 || handle.generation == 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PangeaScript_DeleteObject(handle) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
}

EMSCRIPTEN_KEEPALIVE int MikeScript_ProbeSaveLoadJS(int saveSlot)
{
	if (saveSlot < 0)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	SaveGame((short)saveSlot, false);
	LoadGame((short)saveSlot);
	return PANGEA_SCRIPT_OK;
}

#endif

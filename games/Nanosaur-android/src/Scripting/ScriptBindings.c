#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "structs.h"

#include <stdio.h>
#include <math.h>
#include <string.h>


static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static TerrainItemEntryType gScriptTerrainItems[256];
static bool gScriptTerrainItemOccupied[256];
static bool gScriptTerrainItemReclaimable[256];
static int gScriptStartupRetryFrames;

static TerrainItemEntryType* AcquireScriptTerrainItem(void)
{
	for (int i = 0; i < 256; i++)
	{
		bool referenced = false;
		if (gScriptTerrainItemOccupied[i] && !gScriptTerrainItemReclaimable[i]) continue;
		for (ObjNode* node = gFirstNodePtr; gScriptTerrainItemOccupied[i] && node; node = node->NextNode) referenced |= node->TerrainItemPtr == &gScriptTerrainItems[i];
		if (referenced) continue;
		gScriptTerrainItemOccupied[i] = true; gScriptTerrainItemReclaimable[i] = false; memset(&gScriptTerrainItems[i], 0, sizeof(gScriptTerrainItems[i])); return &gScriptTerrainItems[i];
	}
	return NULL;
}

static PangeaScriptStatus SpawnNativeItem(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	(void) y;
	TerrainItemEntryType* item = AcquireScriptTerrainItem();
	if (!item) return PANGEA_SCRIPT_RUNTIME_ERROR;
	item->x = (uint32_t) x; item->y = (uint32_t) z;
	for (int i = 0; i < 4; i++) item->parm[i] = (Byte) params[i];
	if (!NanosaurSpawnTerrainItem(PangeaScript_ResolveNativeItemType(id), item, (long) x, (long) z)) { gScriptTerrainItemOccupied[item - gScriptTerrainItems] = false; return PANGEA_SCRIPT_INCOMPATIBLE_ITEM; }
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode) if (node->TerrainItemPtr == item)
	{
		NanosaurScript_RegisterObject(node, "nanosaur.terrain-item", "terrain-item");
		gScriptTerrainItemReclaimable[item - gScriptTerrainItems] = true;
		if (outHandle) *outHandle = (PangeaScriptObjectHandle){node->ScriptObjectID, node->ScriptObjectGeneration};
		break;
	}
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptFrameContext gScriptFrameContext;

static PangeaScriptStatus gLastTerrainReplacementStatus = PANGEA_SCRIPT_OK;

static PangeaScriptStatus CompleteScriptReplacement(PangeaScriptObjectHandle handle, const char* action)
{
	PangeaScriptStatus status = PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "spawn");
	if (status == PANGEA_SCRIPT_OK && !PangeaScript_ObjectExists(handle))
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
	if (status != PANGEA_SCRIPT_OK)
	{
		LogScriptStatus(action, status);
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_DeleteObject(handle);
		return status;
	}
	status = PangeaScript_ApplyObjectLifecycle(
		handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_IN);
	if (status == PANGEA_SCRIPT_OK && !PangeaScript_ObjectExists(handle))
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
	if (status == PANGEA_SCRIPT_OK && PangeaScript_ObjectExists(handle))
		return PANGEA_SCRIPT_OK;
	LogScriptStatus(action, status);
	if (PangeaScript_ObjectExists(handle))
		(void)PangeaScript_DeleteObject(handle);
	return status;
}

typedef struct ScriptModelCacheEntry
{
	char path[260];
} ScriptModelCacheEntry;

static ScriptModelCacheEntry gScriptModelCache[MODEL_GROUP_SCRIPT_CUSTOM_COUNT];
typedef struct ScriptSkeletonCacheEntry { char modelPath[260]; char skeletonPath[260]; } ScriptSkeletonCacheEntry;
static ScriptSkeletonCacheEntry gScriptSkeletonCache[SKELETON_TYPE_SCRIPT_CUSTOM_COUNT];

static void NanosaurScript_ReleaseCustomAssets(void)
{
	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		Byte type = (Byte)(SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i);
		if (IsSkeletonTypeLoaded(type))
			FreeSkeletonFile(type);
		gScriptSkeletonCache[i].modelPath[0] = '\0';
		gScriptSkeletonCache[i].skeletonPath[0] = '\0';
	}
	for (int i = 0; i < MODEL_GROUP_SCRIPT_CUSTOM_COUNT; i++)
	{
		short group = (short)(MODEL_GROUP_SCRIPT_CUSTOM_BASE + i);
		if (gNumObjectsInGroupList[group] != 0)
			Free3DMFGroup(group);
		gScriptModelCache[i].path[0] = '\0';
	}
}

extern bool NanosaurScript_LoadCustom3DMF(FSSpec* spec, Byte group);
extern bool NanosaurScript_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec);

static bool MakeDataAssetPath(const char* source, char* destination, size_t capacity)
{
	const char prefix[] = "Data/";
	size_t sourceLength;

	if (!source || strncmp(source, prefix, sizeof(prefix) - 1) != 0)
		return false;
	sourceLength = strlen(source + sizeof(prefix) - 1);
	if (sourceLength + 2 > capacity)
		return false;
	destination[0] = ':';
	for (size_t i = 0; i <= sourceLength; i++)
	{
		char c = source[sizeof(prefix) - 1 + i];
		destination[i + 1] = c == '/' ? ':' : c;
	}
	return true;
}

static Boolean IsSafeCustomAsset(const FSSpec* spec)
{
	short refNum;
	long size;
	if (!spec || FSpOpenDF(spec, fsRdPerm, &refNum) != noErr) return false;
	Boolean valid = GetEOF(refNum, &size) == noErr && size > 0 && size <= 16 * 1024 * 1024;
	FSClose(refNum);
	return valid;
}

static int GetCustomModelGroup(const char* modelPath)
{
	char dataPath[260];
	FSSpec spec;
	if (!modelPath || modelPath[0] == '\0')
		return -1;

	for (int i = 0; i < MODEL_GROUP_SCRIPT_CUSTOM_COUNT; i++)
	{
		int group = MODEL_GROUP_SCRIPT_CUSTOM_BASE + i;
		if (gNumObjectsInGroupList[group] == 0)
			gScriptModelCache[i].path[0] = '\0';
		if (strcmp(gScriptModelCache[i].path, modelPath) == 0)
			return group;
	}
	if (!MakeDataAssetPath(modelPath, dataPath, sizeof(dataPath)) ||
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, dataPath, &spec) != noErr)
		return -1;
	if (!IsSafeCustomAsset(&spec)) return -1;
	for (int i = 0; i < MODEL_GROUP_SCRIPT_CUSTOM_COUNT; i++)
	{
		int group = MODEL_GROUP_SCRIPT_CUSTOM_BASE + i;
		if (gNumObjectsInGroupList[group] != 0)
			continue;
		if (!NanosaurScript_LoadCustom3DMF(&spec, group))
		{
			gNumObjectsInGroupList[group] = 0;
			return -1;
		}
		snprintf(gScriptModelCache[i].path, sizeof(gScriptModelCache[i].path), "%s", modelPath);
		return group;
	}
	return -1;
}

static int GetCustomSkeletonType(const PangeaScriptCustomObjectDefinition* definition)
{
	char modelPath[260], skeletonPath[260];
	FSSpec modelSpec, skeletonSpec;
	if (!definition || definition->modelPath[0] == '\0' || definition->skeletonPath[0] == '\0')
		return -1;
	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		int type = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i;
		if (!IsSkeletonTypeLoaded(type)) { gScriptSkeletonCache[i].modelPath[0] = '\0'; gScriptSkeletonCache[i].skeletonPath[0] = '\0'; }
		if (strcmp(gScriptSkeletonCache[i].modelPath, definition->modelPath) == 0 && strcmp(gScriptSkeletonCache[i].skeletonPath, definition->skeletonPath) == 0) return type;
	}
	if (!MakeDataAssetPath(definition->modelPath, modelPath, sizeof(modelPath)) || !MakeDataAssetPath(definition->skeletonPath, skeletonPath, sizeof(skeletonPath))) return -1;
	if (FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, modelPath, &modelSpec) != noErr || FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, skeletonPath, &skeletonSpec) != noErr) return -1;
	if (!IsSafeCustomAsset(&modelSpec) || !IsSafeCustomAsset(&skeletonSpec)) return -1;
	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		int type = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i;
		if (IsSkeletonTypeLoaded(type)) continue;
		if (!NanosaurScript_LoadCustomSkeleton(type, &skeletonSpec, &modelSpec)) return -1;
		snprintf(gScriptSkeletonCache[i].modelPath, sizeof(gScriptSkeletonCache[i].modelPath), "%s", definition->modelPath);
		snprintf(gScriptSkeletonCache[i].skeletonPath, sizeof(gScriptSkeletonCache[i].skeletonPath), "%s", definition->skeletonPath);
		return type;
	}
	return -1;
}

static int ResolveDisplayGroup(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
		return GetCustomModelGroup(definition->modelPath);
	if (strcmp(definition->nativeGroup, "global") == 0)
		return MODEL_GROUP_GLOBAL;
	if (strcmp(definition->nativeGroup, "levelSpecific") == 0)
		return MODEL_GROUP_LEVEL0;
	return -1;
}

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

static bool NanosaurScript_GetObjectVelocity(void* nativeObject, PangeaScriptVector3* outVelocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outVelocity || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outVelocity = (PangeaScriptVector3){obj->Delta.x, obj->Delta.y, obj->Delta.z};
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

static bool NanosaurScript_GetObjectRotation(void* nativeObject, PangeaScriptVector3* outRotation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outRotation || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outRotation = (PangeaScriptVector3){obj->Rot.x, obj->Rot.y, obj->Rot.z};
	return true;
}

static bool NanosaurScript_SetObjectRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !rotation || obj->CType == INVALID_NODE_FLAG)
		return false;
	obj->Rot = (TQ3Vector3D){rotation->x, rotation->y, rotation->z};
	UpdateObjectTransforms(obj);
	return true;
}

static bool NanosaurScript_GetObjectScale(void* nativeObject, float* outScale)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outScale || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outScale = obj->Scale.x;
	return true;
}

static bool NanosaurScript_SetObjectScale(void* nativeObject, float scale)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG || scale <= 0.0f)
		return false;
	obj->Scale = (TQ3Vector3D){scale, scale, scale};
	UpdateObjectTransforms(obj);
	return true;
}

static bool NanosaurScript_SetObjectCollisionEnabled(void* nativeObject, bool enabled)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG) return false;
	if (enabled && (!obj->ScriptActiveStateInitialized || obj->ScriptActive)) obj->StatusBits &= ~STATUS_BIT_NOCOLLISION;
	else obj->StatusBits |= STATUS_BIT_NOCOLLISION;
	return true;
}

static bool NanosaurScript_GetObjectCollisionEnabled(void* nativeObject, bool* outEnabled)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outEnabled || obj->CType == INVALID_NODE_FLAG) return false;
	*outEnabled = (obj->StatusBits & STATUS_BIT_NOCOLLISION) == 0;
	return true;
}

static bool NanosaurScript_GetObjectActive(void* nativeObject, bool* outActive)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outActive || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outActive = !obj->ScriptActiveStateInitialized || obj->ScriptActive;
	return true;
}

static bool NanosaurScript_SetObjectActive(void* nativeObject, bool active)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG) return false;
	if (!obj->ScriptDefinitionID[0]) return true;
	obj->ScriptActiveStateInitialized = true;
	obj->ScriptActive = active;
	if (active) obj->StatusBits &= ~(STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION);
	else obj->StatusBits |= STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION;
	return true;
}

static int ResolveNamedAnimation(const ObjNode* obj, const char* animation)
{
	const PangeaScriptCustomObjectDefinition* definition;
	if (!obj || !animation || !obj->ScriptDefinitionID[0])
		return -1;
	definition = PangeaScript_GetCustomObjectDefinition(obj->ScriptDefinitionID);
	if (!definition)
		return -1;
	for (int i = 0; i < definition->animationCount; i++)
	{
		if (strcmp(definition->animationNames[i], animation) == 0)
			return definition->animationIndices[i];
	}
	return -1;
}

static bool NanosaurScript_GetObjectAnimation(void* nativeObject, int* outAnimation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outAnimation || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outAnimation = obj->Skeleton->AnimNum;
	return true;
}

static bool NanosaurScript_SetObjectAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG || animation < 0)
		return false;
	if (blendSeconds > 0.0f)
		MorphToSkeletonAnim(obj->Skeleton, animation, 1.0f / blendSeconds);
	else
		SetSkeletonAnim(obj->Skeleton, animation);
	obj->Skeleton->AnimSpeed = speed;
	obj->ScriptAnimationCompletionSent = false;
	return true;
}

static bool NanosaurScript_SetObjectAnimationNamed(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	int animationIndex = ResolveNamedAnimation(obj, animation);
	return animationIndex >= 0 && NanosaurScript_SetObjectAnimation(obj, animationIndex, speed, blendSeconds);
}

static bool NanosaurScript_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	if (obj->ScriptDefinitionID[0])
	{
		if (obj->TerrainItemPtr && !obj->ScriptStreamOutSent)
		{
			PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
			if (PangeaScript_ObjectExists(handle))
				(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		obj->ScriptDeleteRequested = true;
		return true;
	}
	NanosaurScript_UnregisterObject(obj);
	DeleteObject(obj);
	return true;
}

static const PangeaScriptObjectOps kNanosaurPlayerObjectOps =
{
	.getPosition = NanosaurScript_GetObjectPosition,
	.getVelocity = NanosaurScript_GetObjectVelocity,
	.getRotation = NanosaurScript_GetObjectRotation,
	.getScale = NanosaurScript_GetObjectScale,
	.getAnimation = NanosaurScript_GetObjectAnimation,
	.getActive = NanosaurScript_GetObjectActive,
	.setPosition = NanosaurScript_SetObjectPosition,
	.setVelocity = NanosaurScript_SetObjectVelocity,
	.setRotation = NanosaurScript_SetObjectRotation,
	.setScale = NanosaurScript_SetObjectScale,
	.setAnimation = NanosaurScript_SetObjectAnimation,
	.setAnimationNamed = NanosaurScript_SetObjectAnimationNamed,
	.setCollisionEnabled = NanosaurScript_SetObjectCollisionEnabled,
	.getCollisionEnabled = NanosaurScript_GetObjectCollisionEnabled,
	.setActive = NanosaurScript_SetObjectActive,
	.deleteObject = NanosaurScript_DeleteObject,
};

void NanosaurScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void NanosaurScript_ResetObjectRegistry(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	memset(gScriptTerrainItemOccupied, 0, sizeof(gScriptTerrainItemOccupied));
	memset(gScriptTerrainItemReclaimable, 0, sizeof(gScriptTerrainItemReclaimable));
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

	if (category)
	{
		tags[tagCount++] = category;
	}

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = obj,
		.ops = &kNanosaurPlayerObjectOps,
		.objectType = nativeId,
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
	if (PangeaScript_ObjectExists(handle))
		(void) PangeaScript_UnregisterObject(handle);
	obj->ScriptObjectID = 0;
	obj->ScriptObjectGeneration = 0;
	obj->ScriptVisualOffset = (TQ3Vector3D){0};
}

void NanosaurScript_RegisterPlayerObject(ObjNode* playerObj)
{
	NanosaurScript_RegisterObject(playerObj, "nanosaur.player", "player");
	NanosaurScript_OnPlayerSpawn(playerObj);
}

void NanosaurScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	NanosaurScript_UnregisterObject(playerObj);
}

Boolean NanosaurScript_OnDamage(ObjNode* source, float damage, int cause, float* outDamage)
{
	PangeaScriptDamageContext context;
	PangeaScriptDamageResult result = {0};
	PangeaScriptStatus status;

	if (!outDamage)
		return true;
	*outDamage = damage;
	if (!gPlayerObj || gPlayerObj->ScriptObjectID <= 0 || gPlayerObj->ScriptObjectGeneration <= 0 ||
		!PangeaScript_ObjectExists((PangeaScriptObjectHandle){gPlayerObj->ScriptObjectID, (uint32_t)gPlayerObj->ScriptObjectGeneration}))
		return true;

	context = (PangeaScriptDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.cause = cause,
		.damage = damage,
		.source = {0},
		.target = {gPlayerObj->ScriptObjectID, (uint32_t)gPlayerObj->ScriptObjectGeneration},
		.position = {gPlayerObj->Coord.x, gPlayerObj->Coord.y, gPlayerObj->Coord.z},
	};
	if (source && source->ScriptObjectID > 0 && source->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = {source->ScriptObjectID, (uint32_t)source->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			context.source = handle;
	}

	status = PangeaScript_CallDamageHook(&context, &result);
	LogScriptStatus("onDamage", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	if (result.hasDamage)
		*outDamage = result.damage;
	return result.hasApplyDamage ? result.applyDamage : true;
}

Boolean NanosaurScript_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId)
{
	PangeaScriptPickupContext context;
	PangeaScriptPickupResult result = {0};
	PangeaScriptStatus status;
	PangeaScriptObjectHandle pickupHandle = {0};
	PangeaScriptObjectHandle playerHandle = {0};

	if (!pickup || !player)
		return true;
	if (pickup->ScriptObjectID > 0 && pickup->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = {pickup->ScriptObjectID, (uint32_t)pickup->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			pickupHandle = handle;
	}
	if (player->ScriptObjectID > 0 && player->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			playerHandle = handle;
	}
	context = (PangeaScriptPickupContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.pickupType = pickupType,
		.amount = amount,
		.pickupId = pickupId,
		.pickup = pickupHandle,
		.player = playerHandle,
		.position = {pickup->Coord.x, pickup->Coord.y, pickup->Coord.z},
	};
	status = PangeaScript_CallPickupHook(&context, &result);
	LogScriptStatus("onPickupCollected", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	if (isfinite(result.healthDelta))
	{
		gMyHealth += result.healthDelta;
		if (gMyHealth < 0.0f)
			gMyHealth = 0.0f;
		else if (gMyHealth > 1.0f)
			gMyHealth = 1.0f;
	}
	if (result.scoreDelta != 0)
	{
		int64_t score = (int64_t)gScore + (int64_t)result.scoreDelta;
		if (score < 0) score = 0;
		if (score > UINT32_MAX) score = UINT32_MAX;
		gScore = (uint32_t)score;
	}
	return result.hasConsumePickup ? result.consumePickup : true;
}

Boolean NanosaurScript_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget)
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
		.weaponType = weapon ? weapon->Kind : -1,
		.targetType = target ? target->Kind : -1,
		.targetFlags = target ? target->StatusBits : 0,
		.damage = damage,
		.weaponId = "nanosaur.projectile",
		.weapon = {0},
		.target = {0},
		.position = target ? (PangeaScriptVector3){target->Coord.x, target->Coord.y, target->Coord.z} : (PangeaScriptVector3){0},
	};
	if (weapon && weapon->ScriptObjectID > 0 && weapon->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = {weapon->ScriptObjectID, (uint32_t)weapon->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			context.weapon = handle;
	}
	if (target && target->ScriptObjectID > 0 && target->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = {target->ScriptObjectID, (uint32_t)target->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			context.target = handle;
	}
	status = PangeaScript_CallWeaponHitHook(&context, &result);
	LogScriptStatus("onWeaponHit", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	if (result.scoreDelta != 0)
	{
		int64_t score = (int64_t)gScore + (int64_t)result.scoreDelta;
		if (score < 0) score = 0;
		if (score > UINT32_MAX) score = UINT32_MAX;
		gScore = (uint32_t)score;
	}
	*outDamage = result.damage;
	*outDestroyTarget = result.destroyTarget;
	return result.hasApplyDamage ? result.applyDamage : true;
}

void NanosaurScript_OnPlayerSpawn(ObjNode* playerObj)
{
	PangeaScriptPlayerEventContext context;
	if (!playerObj || playerObj->ScriptObjectID <= 0 || playerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = 0,
		.player = {playerObj->ScriptObjectID, (uint32_t)playerObj->ScriptObjectGeneration},
		.position = {playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z},
	};
	LogScriptStatus("onPlayerSpawn", PangeaScript_CallPlayerEvent(&context, "onPlayerSpawn"));
}

void NanosaurScript_OnPlayerRespawn(ObjNode* playerObj)
{
	PangeaScriptPlayerEventContext context;
	if (!playerObj || playerObj->ScriptObjectID <= 0 || playerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = 0,
		.player = {playerObj->ScriptObjectID, (uint32_t)playerObj->ScriptObjectGeneration},
		.position = {playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z},
	};
	LogScriptStatus("onPlayerRespawn", PangeaScript_CallPlayerEvent(&context, "onPlayerRespawn"));
}

void NanosaurScript_OnDamageApplied(float damage, int cause)
{
	PangeaScriptDamageContext context;
	if (!gPlayerObj || gPlayerObj->ScriptObjectID <= 0 || gPlayerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.cause = cause,
		.damage = damage,
		.source = {0},
		.target = {gPlayerObj->ScriptObjectID, (uint32_t)gPlayerObj->ScriptObjectGeneration},
		.position = {gPlayerObj->Coord.x, gPlayerObj->Coord.y, gPlayerObj->Coord.z},
	};
	LogScriptStatus("onDamageApplied", PangeaScript_CallDamageAppliedHook(&context));
}

void NanosaurScript_OnDeath(int eventValue)
{
	PangeaScriptPlayerEventContext context;

	if (!gPlayerObj || gPlayerObj->ScriptObjectID <= 0 || gPlayerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = eventValue,
		.player = {gPlayerObj->ScriptObjectID, (uint32_t)gPlayerObj->ScriptObjectGeneration},
		.position = {gPlayerObj->Coord.x, gPlayerObj->Coord.y, gPlayerObj->Coord.z},
	};
	LogScriptStatus("onDeath", PangeaScript_CallPlayerEvent(&context, "onDeath"));
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
		obj->ScriptVisualOffset = (TQ3Vector3D){0};
		PangeaScript_ClearLastError();
		return;
	}
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
	if (obj->CType == INVALID_NODE_FLAG)
		return;
	if (obj->ScriptDeleteRequested)
	{
		if (obj->TerrainItemPtr && !obj->ScriptStreamOutSent)
		{
			PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
			if (PangeaScript_ObjectExists(handle))
				(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		DeleteObject(obj);
		return;
	}
	if (obj->Skeleton && obj->Skeleton->AnimHasStopped && !obj->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			(void) PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
		obj->ScriptAnimationCompletionSent = true;
	}
}

void NanosaurScript_OnObjectDeleted(ObjNode* obj)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
		if (!PangeaScript_ObjectExists(handle))
			return;
		if (obj->TerrainItemPtr && !obj->ScriptStreamOutSent)
		{
			(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		else if (!obj->ScriptStreamOutSent)
		{
			(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
		}
	}
}

static int ResolveInitialAnimation(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->initialAnimationName[0] == '\0')
		return definition->initialAnimation;
	for (int i = 0; i < definition->animationCount; i++)
	{
		if (strcmp(definition->animationNames[i], definition->initialAnimationName) == 0)
			return definition->animationIndices[i];
	}
	return -1;
}

static ObjNode* MakeScriptedVisual(const PangeaScriptCustomObjectDefinition* definition, float x, float y, float z)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_DISPLAY_GROUP ||
		definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
	{
		int group = ResolveDisplayGroup(definition);
		if (group < 0 || definition->modelObject < 0 || definition->modelObject >= gNumObjectsInGroupList[group])
			return nil;
		gNewObjectDefinition = (NewObjectDefinitionType)
		{
			.group = group,
			.type = definition->modelObject,
			.coord = {x, y, z},
			.flags = 0,
			.slot = definition->slot,
			.moveCall = nil,
			.rot = 0.0f,
			.scale = definition->scale,
		};
		return MakeNewDisplayGroupObject(&gNewObjectDefinition);
	}
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_SKELETON || definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON)
	{
		int animation = ResolveInitialAnimation(definition);
		int skeletonType = definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON ? GetCustomSkeletonType(definition) : definition->skeletonType;
		if (skeletonType < 0 || skeletonType >= MAX_SKELETON_TYPES || animation < 0 || !IsSkeletonTypeLoaded(skeletonType))
			return nil;
		gNewObjectDefinition = (NewObjectDefinitionType)
		{
			.type = skeletonType,
			.animNum = animation,
			.coord = {x, y, z},
			.flags = 0,
			.slot = definition->slot,
			.moveCall = nil,
			.rot = 0.0f,
			.scale = definition->scale,
		};
		ObjNode* object = MakeNewSkeletonObject(&gNewObjectDefinition);
		if (object && object->Skeleton)
			object->Skeleton->AnimSpeed = definition->animationSpeed;
		return object;
	}
	return nil;
}

static void ApplyScriptedCollision(ObjNode* object, const PangeaScriptCustomObjectDefinition* definition)
{
	PangeaScriptCollisionPreset preset = definition->collisionPreset;
	short radius;
	short halfWidth;
	short halfHeight;
	short halfDepth;
	if (preset == PANGEA_SCRIPT_COLLISION_NONE)
		return;
	radius = (short)object->Radius;
	halfWidth = definition->collisionBoundsSet ? (short)(definition->collisionWidth * 0.5f) : radius;
	halfHeight = definition->collisionBoundsSet ? (short)(definition->collisionHeight * 0.5f) : radius;
	halfDepth = definition->collisionBoundsSet ? (short)(definition->collisionDepth * 0.5f) : radius;
	SetObjectCollisionBounds(object, halfHeight, -halfHeight, -halfWidth, halfWidth, halfDepth, -halfDepth);
	object->CBits = CBITS_ALLSOLID;
	if (preset == PANGEA_SCRIPT_COLLISION_ENEMY)
		object->CType = CTYPE_ENEMY;
	else if (preset == PANGEA_SCRIPT_COLLISION_PLATFORM)
		object->CType = CTYPE_MISC;
	else if (preset == PANGEA_SCRIPT_COLLISION_TRIGGER_BOX || preset == PANGEA_SCRIPT_COLLISION_PICKUP)
	{
		object->CType = CTYPE_TRIGGER | CTYPE_PLAYERTRIGGERONLY;
		object->TriggerType = TRIGTYPE_SCRIPTED;
		object->TriggerSides = ALL_SOLID_SIDES;
	}
	else
		object->CType = CTYPE_MISC;
}

void NanosaurScript_OnCustomTrigger(ObjNode* triggerNode, ObjNode* whoNode, Byte sideBits)
{
	PangeaScriptObjectHandle handle;
	if (!triggerNode || triggerNode->ScriptObjectID == 0)
		return;
	handle = (PangeaScriptObjectHandle){(int)triggerNode->ScriptObjectID, triggerNode->ScriptObjectGeneration};
	PangeaScriptObjectHandle other = {0};
	if (!PangeaScript_ObjectExists(handle))
		return;
	if (whoNode && whoNode->ScriptObjectID)
		other = (PangeaScriptObjectHandle){(int)whoNode->ScriptObjectID, whoNode->ScriptObjectGeneration};
	if (other.id > 0 && !PangeaScript_ObjectExists(other))
		other = (PangeaScriptObjectHandle){0};
	(void) PangeaScript_CallObjectTriggerWithOther(handle, &gScriptFrameContext, sideBits, true, other);
}

void NanosaurScript_OnAnimationEvent(ObjNode* obj, int eventValue)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, obj->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			(void) PangeaScript_CallObjectEventWithValue(handle, &gScriptFrameContext, "animationEvent", eventValue);
	}
}

static PangeaScriptStatus SpawnScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
	const PangeaScriptCustomObjectDefinition* definition = PangeaScript_GetCustomObjectDefinition(id);
	ObjNode* object;
	if (!definition)
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	object = MakeScriptedVisual(definition, x, y, z);
	if (!object)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	ApplyScriptedCollision(object, definition);
	snprintf(object->ScriptDefinitionID, sizeof(object->ScriptDefinitionID), "%s", definition->id);
	NanosaurScript_RegisterObject(object, definition->id, "customObject");
	if (object->ScriptObjectID == 0)
	{
		DeleteObject(object);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	if (outHandle)
	{
		outHandle->id = (int)object->ScriptObjectID;
		outHandle->generation = object->ScriptObjectGeneration;
	}
	return PANGEA_SCRIPT_OK;
}

static Boolean TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z, PangeaScriptObjectHandle* outHandle)
{
	const PangeaScriptTerrainReplacement* replacement = PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	void* nativeObject = NULL;
	const float y = GetTerrainHeightAtCoord(x, z);
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = y, .z = z};
	if (!replacement)
	{
		gLastTerrainReplacementStatus = PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		return false;
	}
	if (PangeaScript_FindObjectBySource(&source, &handle))
	{
		if (outHandle)
			*outHandle = handle;
		gLastTerrainReplacementStatus = PANGEA_SCRIPT_OK;
		return true;
	}
	status = SpawnScriptedObject(replacement->customObjectId, x, y, z, &handle);
	if (status == PANGEA_SCRIPT_OK && PangeaScript_GetObjectNativeObject(handle, &nativeObject))
	{
		((ObjNode*)nativeObject)->TerrainItemPtr = itemPtr;
		status = PangeaScript_AssociateObjectSource(handle, &source);
		if (status != PANGEA_SCRIPT_OK)
		{
			gLastTerrainReplacementStatus = status;
			LogScriptStatus("terrain replacement source association", status);
			(void)PangeaScript_DeleteObject(handle);
			if (!replacement->strict)
				PangeaScript_ClearLastError();
			return replacement->strict;
		}
		itemPtr->flags |= ITEM_FLAGS_INUSE;
		status = CompleteScriptReplacement(handle, "terrain replacement stream-in");
		gLastTerrainReplacementStatus = status;
		if (status != PANGEA_SCRIPT_OK)
		{
			if (!replacement->strict)
				PangeaScript_ClearLastError();
			return replacement->strict;
		}
		if (outHandle)
			*outHandle = handle;
		return true;
	}
	if (status == PANGEA_SCRIPT_OK)
	{
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_DeleteObject(handle);
	}
	gLastTerrainReplacementStatus = status;
	if (replacement->strict)
		LogScriptStatus("terrain replacement", status);
	else
		PangeaScript_ClearLastError();
	return replacement->strict;
}

Boolean NanosaurScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z)
{
	return TryReplaceTerrainItem(itemPtr, itemIndex, nativeType, x, z, NULL);
}

int NanosaurScript_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z)
{
	static TerrainItemEntryType probeItem;
	PangeaScriptObjectHandle handle = {0};
	memset(&probeItem, 0, sizeof(probeItem));
	if (!PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!TryReplaceTerrainItem(&probeItem, itemIndex, nativeType, x, z, &handle))
		return (int)gLastTerrainReplacementStatus;
	if (handle.id <= 0 || handle.generation == 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (PangeaScript_DeleteObject(handle))
		return PANGEA_SCRIPT_OK;
	return PangeaScript_ObjectExists(handle) ? PANGEA_SCRIPT_RUNTIME_ERROR : PANGEA_SCRIPT_OK;
}

int NanosaurScript_ProbeCheckpointResetJS(void)
{
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;

	if (!gPlayerObj)
		return 0;
	status = PangeaScript_RegisterScriptedObject(
		"browser-custom-object",
		gPlayerObj->Coord.x,
		gPlayerObj->Coord.y,
		gPlayerObj->Coord.z,
		&handle);
	if (status != PANGEA_SCRIPT_OK)
		return (int)status;
	ResetPlayer();
	if (!PangeaScript_DeleteObject(handle))
		return (int)PANGEA_SCRIPT_RUNTIME_ERROR;
	return 1;
}

int NanosaurScript_ProbePickupJS(void)
{
	TerrainItemEntryType item = {0};
	ObjNode* pickup = NULL;
	float healthBefore;
	uint32_t scoreBefore;

	if (!gPlayerObj)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	item.parm[0] = 3;
	item.parm[1] = 1;
	if (!AddPowerUp(&item, (long) gPlayerObj->Coord.x, (long) gPlayerObj->Coord.z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
	{
		if (node != gPlayerObj && node->TerrainItemPtr == &item &&
			node->TriggerType == TRIGTYPE_POWERUP &&
			node->ScriptObjectID > 0 && node->ScriptObjectGeneration > 0)
		{
			pickup = node;
			break;
		}
	}
	if (!pickup)
	{
		for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
			if (node->TerrainItemPtr == &item)
			{
				DeleteObject(node);
				break;
			}
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	scoreBefore = gScore;
	healthBefore = gMyHealth;
	(void) HandleTrigger(pickup, gPlayerObj, SIDE_BITS_FRONT);
	{
		int scoreDelta = (int) ((int64_t) gScore - (int64_t) scoreBefore);
		gScore = scoreBefore;
		gMyHealth = healthBefore;
		DeleteObject(pickup);
		return scoreDelta;
	}
}

int NanosaurScript_ProbeCrystalPickupJS(void)
{
	TerrainItemEntryType item = {0};
	ObjNode* pickup = NULL;
	uint32_t scoreBefore;

	if (!gPlayerObj)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	item.parm[0] = 0;
	if (!AddCrystal(&item, (long) gPlayerObj->Coord.x, (long) gPlayerObj->Coord.z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
	{
		if (node != gPlayerObj && node->TerrainItemPtr == &item &&
			node->TriggerType == TRIGTYPE_CRYSTAL &&
			node->ScriptObjectID > 0 && node->ScriptObjectGeneration > 0)
		{
			pickup = node;
			break;
		}
	}
	if (!pickup)
	{
		for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
			if (node->TerrainItemPtr == &item)
			{
				DeleteObject(node);
				break;
			}
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	scoreBefore = gScore;
	{
		Boolean handled = HandleTrigger(pickup, gPlayerObj, SIDE_BITS_FRONT);
		if (handled || pickup->TerrainItemPtr != &item || !(pickup->CType & CTYPE_TRIGGER))
		{
			DeleteObject(pickup);
			return PANGEA_SCRIPT_RUNTIME_ERROR;
		}
	}
	{
		int scoreDelta = (int) ((int64_t) gScore - (int64_t) scoreBefore);
		gScore = scoreBefore;
		DeleteObject(pickup);
		return scoreDelta;
	}
}

int NanosaurScript_ProbeEggRecoveryJS(void)
{
	TerrainItemEntryType item = {0};
	ObjNode* pickup = NULL;
	ObjNode* portal = NULL;
	TQ3Point3D previousCoord;
	uint32_t scoreBefore;

	if (!gPlayerObj)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	item.parm[0] = 0;
	if (!AddEgg(&item, (long) gPlayerObj->Coord.x, (long) gPlayerObj->Coord.z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
		if (node != gPlayerObj && node->TerrainItemPtr == &item &&
			node->ScriptObjectID > 0 && node->ScriptObjectGeneration > 0)
		{
			pickup = node;
			break;
		}
	if (!pickup)
	{
		for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
			if (node->TerrainItemPtr == &item)
			{
				DeleteObject(node);
				break;
			}
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	portal = MakeTimePortal(PORTAL_TYPE_EGG, gPlayerObj->Coord.x, gPlayerObj->Coord.z);
	if (!portal)
	{
		DeleteObject(pickup);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	pickup->SpecialRef[0] = gPlayerObj;
	previousCoord = gCoord;
	gCoord = portal->Coord;
	scoreBefore = gScore;
	pickup->MoveCall(pickup);
	{
		int scoreDelta = (int) ((int64_t) gScore - (int64_t) scoreBefore);
		gScore = scoreBefore;
		gCoord = previousCoord;
		DeleteObject(pickup);
		DeleteObject(portal);
		return scoreDelta;
	}
}

int NanosaurScript_ProbeShieldPickupJS(void)
{
	TerrainItemEntryType item = {0};
	ObjNode* pickup = NULL;
	float shieldBefore;
	uint32_t scoreBefore;

	if (!gPlayerObj)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	item.parm[0] = 4;
	item.parm[1] = 1;
	if (!AddPowerUp(&item, (long) gPlayerObj->Coord.x, (long) gPlayerObj->Coord.z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
		if (node != gPlayerObj && node->TerrainItemPtr == &item)
		{
			pickup = node;
			break;
		}
	if (!pickup)
	{
		for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
			if (node->TerrainItemPtr == &item)
			{
				DeleteObject(node);
				break;
			}
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	shieldBefore = gShieldTimer;
	scoreBefore = gScore;
	(void) HandleTrigger(pickup, gPlayerObj, SIDE_BITS_FRONT);
	{
		int scoreDelta = (int) ((int64_t) gScore - (int64_t) scoreBefore);
		int activated = gShieldTimer > shieldBefore ? 1 : 0;
		gShieldTimer = shieldBefore;
		gScore = scoreBefore;
		DeleteObject(pickup);
		return activated == 1 ? scoreDelta : PANGEA_SCRIPT_RUNTIME_ERROR;
	}
}

int NanosaurScript_ProbeWeaponPowerPickupJS(void)
{
	TerrainItemEntryType item = {0};
	ObjNode* pickup = NULL;
	short weaponBefore;
	Boolean possibleBefore;
	uint32_t scoreBefore;

	if (!gPlayerObj)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	item.parm[0] = 0;
	item.parm[1] = 1;
	if (!AddPowerUp(&item, (long) gPlayerObj->Coord.x, (long) gPlayerObj->Coord.z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
		if (node != gPlayerObj && node->TerrainItemPtr == &item)
		{
			pickup = node;
			break;
		}
	if (!pickup)
	{
		for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
			if (node->TerrainItemPtr == &item)
			{
				DeleteObject(node);
				break;
			}
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	weaponBefore = gWeaponInventory[ATTACK_MODE_HEATSEEK];
	possibleBefore = gPossibleAttackModes[ATTACK_MODE_HEATSEEK];
	scoreBefore = gScore;
	(void) HandleTrigger(pickup, gPlayerObj, SIDE_BITS_FRONT);
	{
		int scoreDelta = (int) ((int64_t) gScore - (int64_t) scoreBefore);
		int weaponDelta = gWeaponInventory[ATTACK_MODE_HEATSEEK] - weaponBefore;
		gWeaponInventory[ATTACK_MODE_HEATSEEK] = weaponBefore;
		gPossibleAttackModes[ATTACK_MODE_HEATSEEK] = possibleBefore;
		gScore = scoreBefore;
		DeleteObject(pickup);
		return weaponDelta == 1 ? scoreDelta : PANGEA_SCRIPT_RUNTIME_ERROR;
	}
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
#define NANOSAUR_TERRAIN_NATIVE_ITEM(type) { .id = #type, .nativeType = type, .category = "terrain", .dependencySummary = "current level assets, terrain systems, and the native item initializer" },
	NANOSAUR_TERRAIN_NATIVE_ITEM(1) NANOSAUR_TERRAIN_NATIVE_ITEM(2) NANOSAUR_TERRAIN_NATIVE_ITEM(3) NANOSAUR_TERRAIN_NATIVE_ITEM(4) NANOSAUR_TERRAIN_NATIVE_ITEM(5) NANOSAUR_TERRAIN_NATIVE_ITEM(6) NANOSAUR_TERRAIN_NATIVE_ITEM(7) NANOSAUR_TERRAIN_NATIVE_ITEM(8) NANOSAUR_TERRAIN_NATIVE_ITEM(9) NANOSAUR_TERRAIN_NATIVE_ITEM(10)
	NANOSAUR_TERRAIN_NATIVE_ITEM(11) NANOSAUR_TERRAIN_NATIVE_ITEM(12) NANOSAUR_TERRAIN_NATIVE_ITEM(13) NANOSAUR_TERRAIN_NATIVE_ITEM(14) NANOSAUR_TERRAIN_NATIVE_ITEM(15) NANOSAUR_TERRAIN_NATIVE_ITEM(16) NANOSAUR_TERRAIN_NATIVE_ITEM(17) NANOSAUR_TERRAIN_NATIVE_ITEM(18) NANOSAUR_TERRAIN_NATIVE_ITEM(19)
#undef NANOSAUR_TERRAIN_NATIVE_ITEM
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

	SDL_Log("Nanosaur scripting %s failed: %s", action, PangeaScript_GetLastError());
}

static int GetScriptPlayerCount(void) { return gPlayerObj ? 1 : 0; }

static bool GetScriptPlayer(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (playerNum != 0 || !outPlayer || !gPlayerObj) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {gPlayerObj->Coord.x, gPlayerObj->Coord.y, gPlayerObj->Coord.z}, .velocity = {gPlayerObj->Delta.x, gPlayerObj->Delta.y, gPlayerObj->Delta.z}, .hasVelocity = true, .collisionEnabled = gPlayerObj->CType != 0 && (gPlayerObj->StatusBits & STATUS_BIT_NOCOLLISION) == 0, .hasCollisionEnabled = true, .health = gMyHealth, .hasHealth = true, .fuel = gFuel, .hasFuelState = true, .score = (int64_t) gScore, .hasScore = true, .lives = gNumLives, .hasLives = true, .activeWeapon = gCurrentAttackMode, .hasWeaponState = true, .weaponCount = NUM_ATTACK_MODES, .shieldActive = gShieldTimer > 0.0f, .hasShieldState = true, .active = true};
	if (gGameViewInfoPtr)
	{
		outPlayer->camera = (PangeaScriptVector3){gGameViewInfoPtr->cameraPlacement.cameraLocation.x, gGameViewInfoPtr->cameraPlacement.cameraLocation.y, gGameViewInfoPtr->cameraPlacement.cameraLocation.z};
		outPlayer->hasCameraState = true;
	}
	outPlayer->rotation = (PangeaScriptVector3){gPlayerObj->Rot.x, gPlayerObj->Rot.y, gPlayerObj->Rot.z};
	outPlayer->hasRotation = true;
	outPlayer->aim = (PangeaScriptVector3){-sinf(gPlayerObj->Rot.y), 0.0f, -cosf(gPlayerObj->Rot.y)};
	outPlayer->hasAimState = true;
	for (int weaponType = 0; weaponType < NUM_ATTACK_MODES && weaponType < PANGEA_SCRIPT_PLAYER_INVENTORY_CAPACITY; weaponType++)
		outPlayer->weapons[weaponType] = (PangeaScriptPlayerInventoryEntry){weaponType, gWeaponInventory[weaponType]};
	outPlayer->eggCount = NUM_EGG_SPECIES < PANGEA_SCRIPT_PLAYER_EGG_CAPACITY ? NUM_EGG_SPECIES : PANGEA_SCRIPT_PLAYER_EGG_CAPACITY;
	for (int eggType = 0; eggType < outPlayer->eggCount; eggType++)
	{
		outPlayer->eggs[eggType] = gRecoveredEggs[eggType];
		outPlayer->eggRequired[eggType] = 1;
	}
	outPlayer->hasEggState = true;
	return true;
}

static PangeaScriptStatus SetScriptPlayerPosition(int playerNum, const PangeaScriptVector3* position)
{
	if (playerNum != 0 || !position || !gPlayerObj)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerObj->Coord = (TQ3Point3D){position->x, position->y, position->z};
	UpdateObjectTransforms(gPlayerObj);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerVelocity(int playerNum, const PangeaScriptVector3* velocity)
{
	if (playerNum != 0 || !velocity || !gPlayerObj)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	return NanosaurScript_SetObjectVelocity(gPlayerObj, velocity) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
}

static PangeaScriptStatus SetScriptPlayerHealth(int playerNum, float health)
{
	if (playerNum != 0 || !gPlayerObj)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gMyHealth = health;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerLives(int playerNum, int lives)
{
	if (playerNum != 0 || lives < 0 || !gPlayerObj)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gNumLives = (short) lives;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerWeaponQuantity(int playerNum, int weaponType, int quantity)
{
	if (playerNum != 0 || weaponType < 0 || weaponType >= NUM_ATTACK_MODES || quantity < 0 || quantity > 999 || !gPlayerObj)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gWeaponInventory[weaponType] = (short) quantity;
	if (quantity > 0)
		gCurrentAttackMode = (Byte) weaponType;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerScore(int playerNum, int64_t score)
{
	if (playerNum != 0 || score < 0 || score > UINT32_MAX || !gPlayerObj)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gScore = (uint32_t) score;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerShieldActive(int playerNum, bool active)
{
	if (playerNum != 0 || !gPlayerObj) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gShieldTimer = active ? 20.0f : 0.0f;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerInvulnerable(int playerNum, float durationSeconds)
{
	if (playerNum != 0 || durationSeconds < 0.0f || !gPlayerObj)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerObj->InvincibleTimer = durationSeconds;
	return PANGEA_SCRIPT_OK;
}

void NanosaurScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "Nanosaur-android",
		.gameName = "Nanosaur",
		.spawnNative = SpawnNativeItem,
		.spawnScripted = SpawnScriptedObject,
		.getPlayerCount = GetScriptPlayerCount,
		.getPlayer = GetScriptPlayer,
		.setPlayerPosition = SetScriptPlayerPosition,
		.setPlayerVelocity = SetScriptPlayerVelocity,
		.setPlayerHealth = SetScriptPlayerHealth,
		.setPlayerLives = SetScriptPlayerLives,
		.setPlayerWeaponQuantity = SetScriptPlayerWeaponQuantity,
		.setPlayerScore = SetScriptPlayerScore,
		.setPlayerShieldActive = SetScriptPlayerShieldActive,
		.setPlayerInvulnerable = SetScriptPlayerInvulnerable,
		.capabilities = PANGEA_SCRIPT_NANOSAUR_CAPABILITIES,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	NanosaurScript_ResetObjectRegistry();
	gScriptStartupRetryFrames = 0;
	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	if (status == PANGEA_SCRIPT_OK)
		LogScriptStatus("onGameStart", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_START, &(PangeaScriptLevelContext){0}));
}

void NanosaurScript_Shutdown(void)
{
	LogScriptStatus("onGameShutdown", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN, &(PangeaScriptLevelContext){0}));
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
		.levelName = levelNum == LEVEL_NUM_0 ? "level1" : NULL,
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

void NanosaurScript_OnCheckpointReset(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(
		&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET);
}

void NanosaurScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	if (gScriptStartupRetryFrames < 120 && PangeaScript_IsEnabled() && !PangeaScript_GetStatusBundleLoaded())
	{
		gScriptStartupRetryFrames++;
		(void) PangeaScript_Reload();
	}
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.levelName = levelNum == LEVEL_NUM_0 ? "level1" : NULL,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	NanosaurScript_CacheFrameContext(&context);
	PangeaScript_ExpireTriggerContacts(&context);
	(void)PangeaScript_ApplyDeferredActions(&context);

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
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	NanosaurScript_ReleaseCustomAssets();
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

#endif

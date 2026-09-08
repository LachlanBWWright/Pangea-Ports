#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "structs.h"
#include "splineitems.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif


static void LogScriptStatus(const char* action, PangeaScriptStatus status);
static PangeaScriptFrameContext gScriptFrameContext;
extern bool Nanosaur2Script_LoadCustomBG3D(FSSpec* spec, int group);
extern bool Nanosaur2Script_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec);

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

static TerrainItemEntryType gScriptTerrainItems[256];
static bool gScriptTerrainItemOccupied[256];
static bool gScriptTerrainItemReclaimable[256];

static void MoveScriptedSplineObject(ObjNode* object)
{
	bool wasAttached;
	bool isAttached;
	PangeaScriptObjectHandle handle;

	if (!object || object->ScriptObjectID == 0)
		return;
	wasAttached = (object->StatusBits & STATUS_BIT_DETACHED) == 0;
	(void) IsSplineItemOnActiveTerrain(object);
	if (object->ScriptObjectID == 0)
		return;
	isAttached = (object->StatusBits & STATUS_BIT_DETACHED) == 0;
	if (wasAttached == isAttached)
		return;
	handle = (PangeaScriptObjectHandle){(int)object->ScriptObjectID, object->ScriptObjectGeneration};
	(void) PangeaScript_ApplyObjectLifecycle(
		handle,
		&gScriptFrameContext,
		isAttached ? PANGEA_SCRIPT_OBJECT_ACTIVATE : PANGEA_SCRIPT_OBJECT_DEACTIVATE);
}

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
	if (!Nanosaur2SpawnTerrainItem(PangeaScript_ResolveNativeItemType(id), item, x, z)) { gScriptTerrainItemOccupied[item - gScriptTerrainItems] = false; return PANGEA_SCRIPT_INCOMPATIBLE_ITEM; }
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode) if (node->TerrainItemPtr == item)
	{
		Nanosaur2Script_RegisterObject(node, "nanosaur2.terrain-item", "terrain-item");
		gScriptTerrainItemReclaimable[item - gScriptTerrainItems] = true;
		if (outHandle) *outHandle = (PangeaScriptObjectHandle){node->ScriptObjectID, node->ScriptObjectGeneration};
		break;
	}
	return PANGEA_SCRIPT_OK;
}

typedef struct ScriptModelCacheEntry
{
	char path[260];
} ScriptModelCacheEntry;

static ScriptModelCacheEntry gScriptModelCache[MODEL_GROUP_SCRIPT_CUSTOM_COUNT];
typedef struct ScriptSkeletonCacheEntry { char modelPath[260]; char skeletonPath[260]; } ScriptSkeletonCacheEntry;
static ScriptSkeletonCacheEntry gScriptSkeletonCache[SKELETON_TYPE_SCRIPT_CUSTOM_COUNT];

static void Nanosaur2Script_ReleaseCustomAssets(void)
{
	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		Byte type = (Byte)(SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i);
		FreeSkeletonFile(type);
		gScriptSkeletonCache[i].modelPath[0] = '\0';
		gScriptSkeletonCache[i].skeletonPath[0] = '\0';
	}
	for (int i = 0; i < MODEL_GROUP_SCRIPT_CUSTOM_COUNT; i++)
	{
		short group = (short)(MODEL_GROUP_SCRIPT_CUSTOM_BASE + i);
		if (gNumObjectsInBG3DGroupList[group] != 0)
			DisposeBG3DContainer(group);
		gNumObjectsInBG3DGroupList[group] = 0;
		gScriptModelCache[i].path[0] = '\0';
	}
}

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
		if (gNumObjectsInBG3DGroupList[group] == 0)
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
		if (gNumObjectsInBG3DGroupList[group] != 0)
			continue;
		if (!Nanosaur2Script_LoadCustomBG3D(&spec, group))
		{
			gNumObjectsInBG3DGroupList[group] = 0;
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
		if (!Nanosaur2Script_LoadCustomSkeleton(type, &skeletonSpec, &modelSpec)) return -1;
		snprintf(gScriptSkeletonCache[i].modelPath, sizeof(gScriptSkeletonCache[i].modelPath), "%s", definition->modelPath);
		snprintf(gScriptSkeletonCache[i].skeletonPath, sizeof(gScriptSkeletonCache[i].skeletonPath), "%s", definition->skeletonPath);
		return type;
	}
	return -1;
}

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

static bool Nanosaur2Script_GetObjectVelocity(void* nativeObject, PangeaScriptVector3* outVelocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outVelocity || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outVelocity = (PangeaScriptVector3){obj->Delta.x, obj->Delta.y, obj->Delta.z};
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

static bool Nanosaur2Script_GetObjectRotation(void* nativeObject, PangeaScriptVector3* outRotation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outRotation || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outRotation = (PangeaScriptVector3){obj->Rot.x, obj->Rot.y, obj->Rot.z};
	return true;
}

static bool Nanosaur2Script_SetObjectRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !rotation || obj->CType == INVALID_NODE_FLAG)
		return false;
	obj->Rot = (OGLVector3D){rotation->x, rotation->y, rotation->z};
	UpdateObjectTransforms(obj);
	return true;
}

static bool Nanosaur2Script_GetObjectScale(void* nativeObject, float* outScale)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outScale || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outScale = obj->Scale.x;
	return true;
}

static bool Nanosaur2Script_SetObjectScale(void* nativeObject, float scale)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG || scale <= 0.0f)
		return false;
	obj->Scale = (OGLVector3D){scale, scale, scale};
	UpdateObjectTransforms(obj);
	return true;
}

static bool Nanosaur2Script_SetObjectCollisionEnabled(void* nativeObject, bool enabled)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG) return false;
	if (enabled && (!obj->ScriptActiveStateInitialized || obj->ScriptActive)) obj->StatusBits &= ~STATUS_BIT_NOCOLLISION;
	else obj->StatusBits |= STATUS_BIT_NOCOLLISION;
	return true;
}

static bool Nanosaur2Script_GetObjectCollisionEnabled(void* nativeObject, bool* outEnabled)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outEnabled || obj->CType == INVALID_NODE_FLAG) return false;
	*outEnabled = (obj->StatusBits & STATUS_BIT_NOCOLLISION) == 0;
	return true;
}

static bool Nanosaur2Script_GetObjectActive(void* nativeObject, bool* outActive)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outActive || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outActive = !obj->ScriptActiveStateInitialized || obj->ScriptActive;
	return true;
}

static bool Nanosaur2Script_SetObjectActive(void* nativeObject, bool active)
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
		if (strcmp(definition->animationNames[i], animation) == 0)
			return definition->animationIndices[i];
	return -1;
}

static bool Nanosaur2Script_GetObjectAnimation(void* nativeObject, int* outAnimation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outAnimation || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outAnimation = obj->Skeleton->AnimNum;
	return true;
}

static bool Nanosaur2Script_SetObjectAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
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

static bool Nanosaur2Script_SetObjectAnimationNamed(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	int animationIndex = ResolveNamedAnimation((ObjNode*)nativeObject, animation);
	return animationIndex >= 0 && Nanosaur2Script_SetObjectAnimation(nativeObject, animationIndex, speed, blendSeconds);
}

static bool Nanosaur2Script_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	if (obj->ScriptDefinitionID[0])
	{
		if ((obj->TerrainItemPtr || obj->SplineItemPtr) && !obj->ScriptStreamOutSent)
		{
			PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
			if (PangeaScript_ObjectExists(handle))
				(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		obj->ScriptDeleteRequested = true;
	}
	else
	{
		Nanosaur2Script_UnregisterObject(obj);
		DeleteObject(obj);
	}
	return true;
}

static const PangeaScriptObjectOps kNanosaur2PlayerObjectOps =
{
	.getPosition = Nanosaur2Script_GetObjectPosition,
	.getVelocity = Nanosaur2Script_GetObjectVelocity,
	.getRotation = Nanosaur2Script_GetObjectRotation,
	.getScale = Nanosaur2Script_GetObjectScale,
	.getAnimation = Nanosaur2Script_GetObjectAnimation,
	.getActive = Nanosaur2Script_GetObjectActive,
	.setPosition = Nanosaur2Script_SetObjectPosition,
	.setVelocity = Nanosaur2Script_SetObjectVelocity,
	.setRotation = Nanosaur2Script_SetObjectRotation,
	.setScale = Nanosaur2Script_SetObjectScale,
	.setAnimation = Nanosaur2Script_SetObjectAnimation,
	.setAnimationNamed = Nanosaur2Script_SetObjectAnimationNamed,
	.setCollisionEnabled = Nanosaur2Script_SetObjectCollisionEnabled,
	.getCollisionEnabled = Nanosaur2Script_GetObjectCollisionEnabled,
	.setActive = Nanosaur2Script_SetObjectActive,
	.deleteObject = Nanosaur2Script_DeleteObject,
};

void Nanosaur2Script_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void Nanosaur2Script_ResetObjectRegistry(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	Nanosaur2Script_ReleaseCustomAssets();
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	memset(gScriptTerrainItemOccupied, 0, sizeof(gScriptTerrainItemOccupied));
	memset(gScriptTerrainItemReclaimable, 0, sizeof(gScriptTerrainItemReclaimable));
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

	if (category)
	{
		tags[tagCount++] = category;
	}

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = obj,
		.ops = &kNanosaur2PlayerObjectOps,
		.objectType = nativeId,
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
	if (PangeaScript_ObjectExists(handle))
		(void) PangeaScript_UnregisterObject(handle);
	obj->ScriptObjectID = 0;
	obj->ScriptObjectGeneration = 0;
	obj->ScriptVisualOffset = (OGLVector3D){0};
}

void Nanosaur2Script_RegisterPlayerObject(ObjNode* playerObj)
{
	Nanosaur2Script_RegisterObject(playerObj, "nanosaur2.player", "player");
	Nanosaur2Script_OnPlayerSpawn(playerObj);
}

void Nanosaur2Script_UnregisterPlayerObject(ObjNode* playerObj)
{
	Nanosaur2Script_UnregisterObject(playerObj);
}

Boolean Nanosaur2Script_OnDamage(short playerNum, ObjNode* source, float damage, int cause, float* outDamage)
{
	PangeaScriptDamageContext context;
	PangeaScriptDamageResult result = {0};
	PangeaScriptStatus status;
	ObjNode* player;
	PangeaScriptObjectHandle target;

	if (!outDamage)
		return true;
	*outDamage = damage;
	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return true;
	player = gPlayerInfo[playerNum].objNode;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return true;
	target = (PangeaScriptObjectHandle){player->ScriptObjectID, (uint32_t) player->ScriptObjectGeneration};
	if (!PangeaScript_ObjectExists(target))
		return true;

	context = (PangeaScriptDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerNum,
		.cause = cause,
		.damage = damage,
		.source = {0},
		.target = target,
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	if (source && source->ScriptObjectID > 0 && source->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = (PangeaScriptObjectHandle){source->ScriptObjectID, (uint32_t) source->ScriptObjectGeneration};
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

Boolean Nanosaur2Script_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId)
{
	PangeaScriptPickupContext context;
	PangeaScriptPickupResult result = {0};
	PangeaScriptStatus status;
	PangeaScriptObjectHandle pickupHandle;
	PangeaScriptObjectHandle playerHandle;
	int playerNum;
	if (!pickup || pickup->ScriptObjectID <= 0 || pickup->ScriptObjectGeneration <= 0)
		return true;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return true;
	pickupHandle = (PangeaScriptObjectHandle){pickup->ScriptObjectID, (uint32_t) pickup->ScriptObjectGeneration};
	playerHandle = (PangeaScriptObjectHandle){player->ScriptObjectID, (uint32_t) player->ScriptObjectGeneration};
	if (!PangeaScript_ObjectExists(pickupHandle))
		return true;
	if (!PangeaScript_ObjectExists(playerHandle))
		return true;
	playerNum = player->PlayerNum;
	context = (PangeaScriptPickupContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerNum,
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
	if (isfinite(result.healthDelta) && result.healthDelta != 0.0f)
	{
		gPlayerInfo[playerNum].health += result.healthDelta;
		if (gPlayerInfo[playerNum].health < 0.0f)
			gPlayerInfo[playerNum].health = 0.0f;
		else if (gPlayerInfo[playerNum].health > 1.0f)
			gPlayerInfo[playerNum].health = 1.0f;
	}
	return result.hasConsumePickup ? result.consumePickup : true;
}

Boolean Nanosaur2Script_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget)
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
		.playerNum = weapon ? weapon->PlayerNum : 0,
		.weaponType = weapon ? weapon->Kind : -1,
		.targetType = target ? target->Kind : -1,
		.targetFlags = target ? target->StatusBits : 0,
		.damage = damage,
		.weaponId = "nanosaur2.projectile",
		.weapon = {0},
		.target = {0},
		.position = target ? (PangeaScriptVector3){target->Coord.x, target->Coord.y, target->Coord.z} : (PangeaScriptVector3){0},
	};
	if (weapon && weapon->ScriptObjectID > 0 && weapon->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = (PangeaScriptObjectHandle){weapon->ScriptObjectID, (uint32_t) weapon->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			context.weapon = handle;
	}
	if (target && target->ScriptObjectID > 0 && target->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = (PangeaScriptObjectHandle){target->ScriptObjectID, (uint32_t) target->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			context.target = handle;
	}
	status = PangeaScript_CallWeaponHitHook(&context, &result);
	LogScriptStatus("onWeaponHit", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	*outDamage = result.damage;
	*outDestroyTarget = result.destroyTarget;
	return result.hasApplyDamage ? result.applyDamage : true;
}

void Nanosaur2Script_OnPlayerSpawn(ObjNode* playerObj)
{
	PangeaScriptPlayerEventContext context;
	if (!playerObj || playerObj->ScriptObjectID <= 0 || playerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerObj->PlayerNum,
		.eventValue = 0,
		.player = {playerObj->ScriptObjectID, playerObj->ScriptObjectGeneration},
		.position = {playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z},
	};
	LogScriptStatus("onPlayerSpawn", PangeaScript_CallPlayerEvent(&context, "onPlayerSpawn"));
}

void Nanosaur2Script_OnPlayerRespawn(ObjNode* playerObj)
{
	PangeaScriptPlayerEventContext context;
	if (!playerObj || playerObj->ScriptObjectID <= 0 || playerObj->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerObj->PlayerNum,
		.eventValue = 0,
		.player = {playerObj->ScriptObjectID, playerObj->ScriptObjectGeneration},
		.position = {playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z},
	};
	LogScriptStatus("onPlayerRespawn", PangeaScript_CallPlayerEvent(&context, "onPlayerRespawn"));
}

void Nanosaur2Script_OnDamageApplied(short playerNum, float damage, int cause)
{
	PangeaScriptDamageContext context;
	ObjNode* player;
	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return;
	player = gPlayerInfo[playerNum].objNode;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerNum,
		.cause = cause,
		.damage = damage,
		.source = {0},
		.target = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onDamageApplied", PangeaScript_CallDamageAppliedHook(&context));
}

void Nanosaur2Script_OnDeath(short playerNum, int eventValue)
{
	PangeaScriptPlayerEventContext context;
	ObjNode* player;

	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return;
	player = gPlayerInfo[playerNum].objNode;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerNum,
		.eventValue = eventValue,
		.player = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onDeath", PangeaScript_CallPlayerEvent(&context, "onDeath"));
}

void Nanosaur2Script_OnCheckpointReached(short playerNum, short checkpointNum)
{
	PangeaScriptPlayerEventContext context;
	ObjNode* player;
	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return;
	player = gPlayerInfo[playerNum].objNode;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerNum,
		.eventValue = checkpointNum,
		.player = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onCheckpointReached", PangeaScript_CallPlayerEvent(&context, "onCheckpointReached"));
}

void Nanosaur2Script_OnLapComplete(short playerNum, short lapNum)
{
	PangeaScriptPlayerEventContext context;
	ObjNode* player;

	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return;
	player = gPlayerInfo[playerNum].objNode;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerNum,
		.eventValue = lapNum,
		.player = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onLapComplete", PangeaScript_CallPlayerEvent(&context, "onLapComplete"));
}

void Nanosaur2Script_OnRaceFinish(short playerNum, int placement)
{
	PangeaScriptPlayerEventContext context;
	ObjNode* player;
	PangeaScriptObjectHandle handle;

	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return;
	player = gPlayerInfo[playerNum].objNode;
	if (!player)
		return;
	handle = (PangeaScriptObjectHandle)
	{
		.id = (int) player->ScriptObjectID,
		.generation = player->ScriptObjectGeneration,
	};
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0 || !PangeaScript_ObjectExists(handle))
	{
		player->ScriptObjectID = 0;
		player->ScriptObjectGeneration = 0;
		Nanosaur2Script_RegisterPlayerObject(player);
	}
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerNum,
		.eventValue = placement,
		.player = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onRaceFinish", PangeaScript_CallPlayerEvent(&context, "onRaceFinish"));
}

void Nanosaur2Script_OnObjectiveComplete(short playerNum, int outcome)
{
	PangeaScriptPlayerEventContext context;
	ObjNode* player;
	PangeaScriptObjectHandle handle;

	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		return;
	player = gPlayerInfo[playerNum].objNode;
	if (!player)
		return;
	handle = (PangeaScriptObjectHandle){(int)player->ScriptObjectID, player->ScriptObjectGeneration};
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0 || !PangeaScript_ObjectExists(handle))
	{
		player->ScriptObjectID = 0;
		player->ScriptObjectGeneration = 0;
		Nanosaur2Script_RegisterPlayerObject(player);
	}
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = playerNum,
		.eventValue = outcome,
		.player = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onObjectiveComplete", PangeaScript_CallPlayerEvent(&context, "onObjectiveComplete"));
}

int Nanosaur2Script_ProbeObjectiveCompletionJS(void)
{
	ObjNode* player;
	PangeaScriptObjectHandle handle;

	if (!gIsInGame || !gPlayerInfo[0].objNode)
		return 0;
	player = gPlayerInfo[0].objNode;
	handle = (PangeaScriptObjectHandle){(int)player->ScriptObjectID, player->ScriptObjectGeneration};
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0 || !PangeaScript_ObjectExists(handle))
	{
		player->ScriptObjectID = 0;
		player->ScriptObjectGeneration = 0;
		Nanosaur2Script_RegisterPlayerObject(player);
	}
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return 0;
	Nanosaur2Script_OnObjectiveComplete(0, 0);
	return 1;
}

EMSCRIPTEN_KEEPALIVE int Nanosaur2Script_ProbeSaveLoadJS(int saveSlot)
{
	SaveGameType saveData;
	if (saveSlot < 0)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	(void) DeleteSavedGame(saveSlot);
	if (!SaveGame(saveSlot) || !LoadSavedGame(saveSlot, &saveData))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	UseSaveGame(&saveData);
	return PANGEA_SCRIPT_OK;
}

EMSCRIPTEN_KEEPALIVE int Nanosaur2Script_ProbePowerupPickupJS(int pickupKind)
{
	TerrainItemEntryType item = {0};
	ObjNode* pickup = NULL;
	ObjNode* player;
	ObjNode* previousShield = gPlayerInfo[0].shieldObj;
	float savedHealth;
	float savedFuel;
	float savedShield;
	short savedLives;
	Boolean (*addPickup)(TerrainItemEntryType*, float, float) = NULL;
	Boolean nativeMutation = false;

	if (!gIsInGame || !gPlayerInfo[0].objNode)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	player = gPlayerInfo[0].objNode;
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0 ||
		!PangeaScript_ObjectExists((PangeaScriptObjectHandle){player->ScriptObjectID, player->ScriptObjectGeneration}))
	{
		player->ScriptObjectID = 0;
		player->ScriptObjectGeneration = 0;
		Nanosaur2Script_RegisterPlayerObject(player);
	}
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (pickupKind == 0)
		addPickup = AddFuelPOW;
	else if (pickupKind == 1)
		addPickup = AddShieldPOW;
	else if (pickupKind == 2)
		addPickup = AddFreeLifePOW;
	else
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	savedHealth = gPlayerInfo[0].health;
	savedFuel = gPlayerInfo[0].jetpackFuel;
	savedShield = gPlayerInfo[0].shieldPower;
	savedLives = gPlayerInfo[0].numFreeLives;
	gPlayerInfo[0].health = .5f;
	if (pickupKind == 0)
		gPlayerInfo[0].jetpackFuel = .25f;
	else if (pickupKind == 1)
		gPlayerInfo[0].shieldPower = 0.0f;
	else
		gPlayerInfo[0].numFreeLives = 1;

	if (!addPickup(&item, gPlayerInfo[0].coord.x, gPlayerInfo[0].coord.z))
	{
		gPlayerInfo[0].health = savedHealth;
		gPlayerInfo[0].jetpackFuel = savedFuel;
		gPlayerInfo[0].shieldPower = savedShield;
		gPlayerInfo[0].numFreeLives = savedLives;
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
	{
		if (node != gPlayerInfo[0].objNode && node->TerrainItemPtr == &item &&
			node->ScriptObjectID > 0 && node->ScriptObjectGeneration > 0)
		{
			pickup = node;
			break;
		}
	}
	if (!pickup || !pickup->TriggerCallback)
	{
		for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
			if (node->TerrainItemPtr == &item)
			{
				DeleteObject(node);
				break;
			}
		gPlayerInfo[0].health = savedHealth;
		gPlayerInfo[0].jetpackFuel = savedFuel;
		gPlayerInfo[0].shieldPower = savedShield;
		gPlayerInfo[0].numFreeLives = savedLives;
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	pickup->TriggerCallback(pickup, player);
	if (pickupKind == 0)
		nativeMutation = gPlayerInfo[0].jetpackFuel > .25f;
	else if (pickupKind == 1)
		nativeMutation = gPlayerInfo[0].shieldPower > 0.0f;
	else
		nativeMutation = gPlayerInfo[0].numFreeLives > 1;

	Boolean scriptMutation = fabsf(gPlayerInfo[0].health - .5f) > .1f;
	if (pickupKind == 1 && !previousShield && gPlayerInfo[0].shieldObj)
	{
		DeleteObject(gPlayerInfo[0].shieldObj);
		gPlayerInfo[0].shieldObj = NULL;
	}
	gPlayerInfo[0].health = savedHealth;
	gPlayerInfo[0].jetpackFuel = savedFuel;
	gPlayerInfo[0].shieldPower = savedShield;
	gPlayerInfo[0].numFreeLives = savedLives;
	DeleteObject(pickup);
	if (pickupKind == 2)
		nativeMutation = !nativeMutation;
	return nativeMutation && scriptMutation ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
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
		obj->ScriptVisualOffset = (OGLVector3D){0};
		PangeaScript_ClearLastError();
		return;
	}
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
	if (obj->CType == INVALID_NODE_FLAG)
		return;
	if (obj->ScriptDeleteRequested)
	{
		if ((obj->TerrainItemPtr || obj->SplineItemPtr) && !obj->ScriptStreamOutSent)
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
			(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
		obj->ScriptAnimationCompletionSent = true;
	}
}

void Nanosaur2Script_OnObjectDeleted(ObjNode* obj)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
		if (!PangeaScript_ObjectExists(handle))
			return;
		if ((obj->TerrainItemPtr || obj->SplineItemPtr) && !obj->ScriptStreamOutSent)
		{
			(void)PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		else if (!obj->ScriptStreamOutSent)
		{
			(void)PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
		}
	}
}

static int ResolveDisplayGroup(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
		return GetCustomModelGroup(definition->modelPath);
	if (strcmp(definition->nativeGroup, "global") == 0)
		return MODEL_GROUP_GLOBAL;
	if (strcmp(definition->nativeGroup, "levelSpecific") == 0)
		return MODEL_GROUP_LEVELSPECIFIC;
	if (strcmp(definition->nativeGroup, "weapons") == 0)
		return MODEL_GROUP_WEAPONS;
	if (strcmp(definition->nativeGroup, "player") == 0)
		return MODEL_GROUP_PLAYER;
	return -1;
}

static int ResolveInitialAnimation(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->initialAnimationName[0] == '\0')
		return definition->initialAnimation;
	for (int i = 0; i < definition->animationCount; i++)
		if (strcmp(definition->animationNames[i], definition->initialAnimationName) == 0)
			return definition->animationIndices[i];
	return -1;
}

static ObjNode* MakeScriptedVisual(const PangeaScriptCustomObjectDefinition* definition, float x, float y, float z)
{
	NewObjectDefinitionType objectDefinition;
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_DISPLAY_GROUP ||
		definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
	{
		int group = ResolveDisplayGroup(definition);
		if (group < 0 || definition->modelObject < 0 || definition->modelObject >= gNumObjectsInBG3DGroupList[group])
			return nil;
		objectDefinition = (NewObjectDefinitionType){
			.group = group, .type = definition->modelObject, .coord = {x, y, z},
			.flags = 0, .slot = definition->slot, .moveCall = nil,
			.rot = 0.0f, .scale = definition->scale,
		};
		return MakeNewDisplayGroupObject(&objectDefinition);
	}
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_SKELETON || definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON)
	{
		int animation = ResolveInitialAnimation(definition);
		int skeletonType = definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON ? GetCustomSkeletonType(definition) : definition->skeletonType;
		if (skeletonType < 0 || skeletonType >= MAX_SKELETON_TYPES || animation < 0 || !IsSkeletonTypeLoaded(skeletonType))
			return nil;
		objectDefinition = (NewObjectDefinitionType){
			.type = skeletonType, .animNum = animation, .coord = {x, y, z},
			.flags = 0, .slot = definition->slot, .moveCall = nil,
			.rot = 0.0f, .scale = definition->scale,
		};
		ObjNode* object = MakeNewSkeletonObject(&objectDefinition);
		if (object && object->Skeleton)
			object->Skeleton->AnimSpeed = definition->animationSpeed;
		return object;
	}
	return nil;
}

static Boolean ScriptedTriggerCallback(ObjNode* trigger, ObjNode* who)
{
	if (trigger && trigger->ScriptObjectID)
	{
		PangeaScriptObjectHandle handle = {(int)trigger->ScriptObjectID, trigger->ScriptObjectGeneration};
		PangeaScriptObjectHandle other = {0};
		int playerNum = -1;
		if (!PangeaScript_ObjectExists(handle))
			return true;
		if (who && who->ScriptObjectID)
			other = (PangeaScriptObjectHandle){(int)who->ScriptObjectID, who->ScriptObjectGeneration};
		if (who)
			for (int index = 0; index < gNumPlayers; index++)
				if (gPlayerInfo[index].objNode == who) { playerNum = index; break; }
		if (other.id > 0 && !PangeaScript_ObjectExists(other))
			other = (PangeaScriptObjectHandle){0};
		return PangeaScript_CallObjectTriggerWithOtherAndPlayer(handle, &gScriptFrameContext, 0, true, other, playerNum);
	}
	return true;
}

void Nanosaur2Script_OnAnimationEvent(ObjNode* obj, int eventValue)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_CallObjectEventWithValue(handle, &gScriptFrameContext, "animationEvent", eventValue);
	}
}

static void ApplyScriptedCollision(ObjNode* object, const PangeaScriptCustomObjectDefinition* definition)
{
	PangeaScriptCollisionPreset preset = definition->collisionPreset;
	float radius;
	float halfWidth;
	float halfHeight;
	float halfDepth;
	if (preset == PANGEA_SCRIPT_COLLISION_NONE)
		return;
	radius = object->BoundingSphereRadius;
	halfWidth = definition->collisionBoundsSet ? definition->collisionWidth * 0.5f : radius;
	halfHeight = definition->collisionBoundsSet ? definition->collisionHeight * 0.5f : radius;
	halfDepth = definition->collisionBoundsSet ? definition->collisionDepth * 0.5f : radius;
	SetObjectCollisionBounds(object, halfHeight, -halfHeight, -halfWidth, halfWidth, halfDepth, -halfDepth);
	object->CBits = CBITS_ALLSOLID;
	if (preset == PANGEA_SCRIPT_COLLISION_ENEMY)
		object->CType = CTYPE_ENEMY | CTYPE_MISC;
	else if (preset == PANGEA_SCRIPT_COLLISION_PICKUP)
	{
		object->CType = CTYPE_PICKUP | CTYPE_TRIGGER;
		object->TriggerCallback = ScriptedTriggerCallback;
	}
	else if (preset == PANGEA_SCRIPT_COLLISION_TRIGGER_BOX)
	{
		object->CType = CTYPE_TRIGGER;
		object->TriggerCallback = ScriptedTriggerCallback;
	}
	else
		object->CType = CTYPE_MISC;
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
	Nanosaur2Script_RegisterObject(object, definition->id, "customObject");
	if (object->ScriptObjectID == 0)
	{
		DeleteObject(object);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	if (outHandle)
		*outHandle = (PangeaScriptObjectHandle){(int)object->ScriptObjectID, object->ScriptObjectGeneration};
	return PANGEA_SCRIPT_OK;
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
		.dependencySummary = "fuel pickup assets, player jetpack state, and terrain systems",
	},
	{
		.id = "nanosaur2.shieldPow",
		.nativeType = 33,
		.category = "pickup",
		.dependencySummary = "shield pickup assets, player shield state, and terrain systems",
	},
	{
		.id = "nanosaur2.freeLifePow",
		.nativeType = 47,
		.category = "pickup",
		.dependencySummary = "free-life pickup assets, player lives, and terrain systems",
	},
#define NANOSAUR2_TERRAIN_NATIVE_ITEM(type) { .id = #type, .nativeType = type, .category = "terrain", .dependencySummary = "current level assets, terrain systems, and the native item initializer" },
	NANOSAUR2_TERRAIN_NATIVE_ITEM(1) NANOSAUR2_TERRAIN_NATIVE_ITEM(2) NANOSAUR2_TERRAIN_NATIVE_ITEM(3) NANOSAUR2_TERRAIN_NATIVE_ITEM(4) NANOSAUR2_TERRAIN_NATIVE_ITEM(5) NANOSAUR2_TERRAIN_NATIVE_ITEM(6) NANOSAUR2_TERRAIN_NATIVE_ITEM(7) NANOSAUR2_TERRAIN_NATIVE_ITEM(8) NANOSAUR2_TERRAIN_NATIVE_ITEM(9) NANOSAUR2_TERRAIN_NATIVE_ITEM(10)
	NANOSAUR2_TERRAIN_NATIVE_ITEM(11) NANOSAUR2_TERRAIN_NATIVE_ITEM(12) NANOSAUR2_TERRAIN_NATIVE_ITEM(13) NANOSAUR2_TERRAIN_NATIVE_ITEM(14) NANOSAUR2_TERRAIN_NATIVE_ITEM(15) NANOSAUR2_TERRAIN_NATIVE_ITEM(16) NANOSAUR2_TERRAIN_NATIVE_ITEM(17) NANOSAUR2_TERRAIN_NATIVE_ITEM(18) NANOSAUR2_TERRAIN_NATIVE_ITEM(19) NANOSAUR2_TERRAIN_NATIVE_ITEM(20)
	NANOSAUR2_TERRAIN_NATIVE_ITEM(21) NANOSAUR2_TERRAIN_NATIVE_ITEM(22) NANOSAUR2_TERRAIN_NATIVE_ITEM(23) NANOSAUR2_TERRAIN_NATIVE_ITEM(24) NANOSAUR2_TERRAIN_NATIVE_ITEM(25) NANOSAUR2_TERRAIN_NATIVE_ITEM(26) NANOSAUR2_TERRAIN_NATIVE_ITEM(27) NANOSAUR2_TERRAIN_NATIVE_ITEM(28) NANOSAUR2_TERRAIN_NATIVE_ITEM(29) NANOSAUR2_TERRAIN_NATIVE_ITEM(30)
	NANOSAUR2_TERRAIN_NATIVE_ITEM(31) NANOSAUR2_TERRAIN_NATIVE_ITEM(32) NANOSAUR2_TERRAIN_NATIVE_ITEM(33) NANOSAUR2_TERRAIN_NATIVE_ITEM(34) NANOSAUR2_TERRAIN_NATIVE_ITEM(35) NANOSAUR2_TERRAIN_NATIVE_ITEM(36) NANOSAUR2_TERRAIN_NATIVE_ITEM(37) NANOSAUR2_TERRAIN_NATIVE_ITEM(38) NANOSAUR2_TERRAIN_NATIVE_ITEM(39) NANOSAUR2_TERRAIN_NATIVE_ITEM(40)
	NANOSAUR2_TERRAIN_NATIVE_ITEM(41) NANOSAUR2_TERRAIN_NATIVE_ITEM(42) NANOSAUR2_TERRAIN_NATIVE_ITEM(43) NANOSAUR2_TERRAIN_NATIVE_ITEM(44) NANOSAUR2_TERRAIN_NATIVE_ITEM(45) NANOSAUR2_TERRAIN_NATIVE_ITEM(46) NANOSAUR2_TERRAIN_NATIVE_ITEM(47) NANOSAUR2_TERRAIN_NATIVE_ITEM(48)
#undef NANOSAUR2_TERRAIN_NATIVE_ITEM
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

	SDL_Log("Nanosaur 2 scripting %s failed: %s", action, PangeaScript_GetLastError());
}

static int GetScriptPlayerCount(void) { return gNumPlayers; }

static bool GetScriptPlayer(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (!outPlayer || playerNum < 0 || playerNum >= gNumPlayers || !gPlayerInfo[playerNum].objNode) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){
		.position = {gPlayerInfo[playerNum].coord.x, gPlayerInfo[playerNum].coord.y, gPlayerInfo[playerNum].coord.z},
		.velocity = {gPlayerInfo[playerNum].objNode->Delta.x, gPlayerInfo[playerNum].objNode->Delta.y, gPlayerInfo[playerNum].objNode->Delta.z},
		.hasVelocity = true,
		.collisionEnabled = gPlayerInfo[playerNum].objNode->CType != 0 && (gPlayerInfo[playerNum].objNode->StatusBits & STATUS_BIT_NOCOLLISION) == 0,
		.hasCollisionEnabled = true,
		.health = gPlayerInfo[playerNum].health,
		.hasHealth = true,
		.fuel = gPlayerInfo[playerNum].jetpackFuel,
		.hasFuelState = true,
		.lives = gPlayerInfo[playerNum].numFreeLives,
		.hasLives = true,
		.activeWeapon = gPlayerInfo[playerNum].currentWeapon,
		.hasWeaponState = true,
		.weaponCount = NUM_WEAPON_TYPES,
		.lapNum = gPlayerInfo[playerNum].lapNum,
		.checkpointNum = gPlayerInfo[playerNum].raceCheckpointNum,
		.placement = gPlayerInfo[playerNum].place,
		.raceComplete = gPlayerInfo[playerNum].raceComplete,
		.hasRaceState = gVSMode == VS_MODE_RACE,
		.shieldActive = gPlayerInfo[playerNum].shieldPower > 0.0f,
		.hasShieldState = true,
		.eggCount = NUM_EGG_TYPES < PANGEA_SCRIPT_PLAYER_EGG_CAPACITY ? NUM_EGG_TYPES : PANGEA_SCRIPT_PLAYER_EGG_CAPACITY,
		.hasEggState = true,
		.team = playerNum & 1,
		.hasTeamState = gVSMode == VS_MODE_CAPTURETHEFLAG,
		.carryingFlag = gPlayerInfo[playerNum].carriedObj != NULL,
		.captureScore = gVSMode == VS_MODE_CAPTURETHEFLAG ? gNumEggsSaved[(playerNum & 1) ^ 1] : 0,
		.hasCaptureState = gVSMode == VS_MODE_CAPTURETHEFLAG,
		.active = true,
	};
	if (gGameViewInfoPtr)
	{
		outPlayer->camera = (PangeaScriptVector3){gGameViewInfoPtr->cameraPlacement[playerNum].cameraLocation.x, gGameViewInfoPtr->cameraPlacement[playerNum].cameraLocation.y, gGameViewInfoPtr->cameraPlacement[playerNum].cameraLocation.z};
		outPlayer->hasCameraState = true;
	}
	outPlayer->rotation = (PangeaScriptVector3){gPlayerInfo[playerNum].objNode->Rot.x, gPlayerInfo[playerNum].objNode->Rot.y, gPlayerInfo[playerNum].objNode->Rot.z};
	outPlayer->hasRotation = true;
	outPlayer->aim = (PangeaScriptVector3){-sinf(gPlayerInfo[playerNum].objNode->Rot.y), 0.0f, -cosf(gPlayerInfo[playerNum].objNode->Rot.y)};
	outPlayer->hasAimState = true;
	for (int weaponType = 0; weaponType < NUM_WEAPON_TYPES && weaponType < PANGEA_SCRIPT_PLAYER_INVENTORY_CAPACITY; weaponType++)
	{
		outPlayer->weapons[weaponType] = (PangeaScriptPlayerInventoryEntry){weaponType, gPlayerInfo[playerNum].weaponQuantity[weaponType]};
	}
	for (int eggType = 0; eggType < outPlayer->eggCount; eggType++)
	{
		outPlayer->eggs[eggType] = gNumEggsSaved[eggType];
		outPlayer->eggRequired[eggType] = gNumEggsToSave[eggType];
	}
	return true;
}

static PangeaScriptStatus SetScriptPlayerHealth(int playerNum, float health)
{
	if (playerNum < 0 || playerNum >= gNumPlayers || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo[playerNum].health = health;
	gPlayerInfo[playerNum].objNode->Health = health;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerLives(int playerNum, int lives)
{
	if (playerNum < 0 || playerNum >= gNumPlayers || lives < 0 || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo[playerNum].numFreeLives = (short) lives;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerWeaponQuantity(int playerNum, int weaponType, int quantity)
{
	if (playerNum < 0 || playerNum >= gNumPlayers || weaponType < 0 || weaponType >= NUM_WEAPON_TYPES || quantity < 0 || quantity > 999 || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo[playerNum].weaponQuantity[weaponType] = (short) quantity;
	if (quantity > 0 && gPlayerInfo[playerNum].currentWeapon == WEAPON_TYPE_NONE)
		gPlayerInfo[playerNum].currentWeapon = weaponType;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerShieldActive(int playerNum, bool active)
{
	if (playerNum < 0 || playerNum >= gNumPlayers || !gPlayerInfo[playerNum].objNode) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo[playerNum].shieldPower = active ? MAX_SHIELD_POWER : 0.0f;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerInvulnerable(int playerNum, float durationSeconds)
{
	if (playerNum < 0 || playerNum >= gNumPlayers || durationSeconds < 0.0f || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo[playerNum].invincibilityTimer = durationSeconds;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerPosition(int playerNum, const PangeaScriptVector3* position)
{
	if (playerNum < 0 || playerNum >= gNumPlayers || !position || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo[playerNum].coord = (OGLPoint3D){position->x, position->y, position->z};
	gPlayerInfo[playerNum].objNode->Coord = gPlayerInfo[playerNum].coord;
	UpdateObjectTransforms(gPlayerInfo[playerNum].objNode);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerVelocity(int playerNum, const PangeaScriptVector3* velocity)
{
	if (playerNum < 0 || playerNum >= gNumPlayers || !velocity || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	return Nanosaur2Script_SetObjectVelocity(gPlayerInfo[playerNum].objNode, velocity) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
}

void Nanosaur2Script_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "Nanosaur2-Android",
		.gameName = "Nanosaur 2",
		.spawnNative = SpawnNativeItem,
		.spawnScripted = SpawnScriptedObject,
		.getPlayerCount = GetScriptPlayerCount,
		.getPlayer = GetScriptPlayer,
		.setPlayerHealth = SetScriptPlayerHealth,
		.setPlayerLives = SetScriptPlayerLives,
		.setPlayerWeaponQuantity = SetScriptPlayerWeaponQuantity,
		.setPlayerShieldActive = SetScriptPlayerShieldActive,
		.setPlayerInvulnerable = SetScriptPlayerInvulnerable,
		.setPlayerPosition = SetScriptPlayerPosition,
		.setPlayerVelocity = SetScriptPlayerVelocity,
		.capabilities = PANGEA_SCRIPT_NANOSAUR2_CAPABILITIES,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	Nanosaur2Script_ResetObjectRegistry();
	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	if (status == PANGEA_SCRIPT_OK)
		LogScriptStatus("onGameStart", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_START, &(PangeaScriptLevelContext){0}));
}

void Nanosaur2Script_Shutdown(void)
{
	LogScriptStatus("onGameShutdown", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN, &(PangeaScriptLevelContext){0}));
	Nanosaur2Script_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void Nanosaur2Script_LoadLevelConfig(int levelNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(levelNum);
	LogScriptStatus("level config load", status);
}

static const char* Nanosaur2Script_ModeName(int levelNum)
{
	switch (levelNum)
	{
		case LEVEL_NUM_RACE1:
		case LEVEL_NUM_RACE2:
			return "race";
		case LEVEL_NUM_BATTLE1:
		case LEVEL_NUM_BATTLE2:
			return "battle";
		case LEVEL_NUM_FLAG1:
		case LEVEL_NUM_FLAG2:
			return "capture";
		default:
			break;
	}
	switch (gVSMode)
	{
		case VS_MODE_RACE: return "race";
		case VS_MODE_BATTLE: return "battle";
		case VS_MODE_CAPTURETHEFLAG: return "capture";
		default: return "adventure";
	}
}

static const char* Nanosaur2Script_LevelName(int levelNum)
{
	static const char* levelNames[] =
	{
		"adventure1", "adventure2", "adventure3", "race1", "race2",
		"battle1", "battle2", "flag1", "flag2",
	};
	if (levelNum < 0 || levelNum >= NUM_LEVELS) return NULL;
	return levelNames[levelNum];
}

static void CallLevelHook(PangeaScriptHook hook, int levelNum, const char* action)
{
	const char* mode = Nanosaur2Script_ModeName(levelNum);
	const PangeaScriptLevelContext context =
	{
		.levelNum = levelNum,
		.levelName = Nanosaur2Script_LevelName(levelNum),
		.mode = mode,
		.networked = gNumPlayers > 1,
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

void Nanosaur2Script_OnCheckpointReset(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(
		&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET);
}

void Nanosaur2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const char* mode = Nanosaur2Script_ModeName(levelNum);
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.levelName = Nanosaur2Script_LevelName(levelNum),
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
		.mode = mode,
		.networked = gNumPlayers > 1,
	};
	Nanosaur2Script_CacheFrameContext(&context);
	PangeaScript_ExpireTriggerContacts(&context);
	(void)PangeaScript_ApplyDeferredActions(&context);

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
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	PangeaScript_ResetObjects();
}

int Nanosaur2Script_RemapTerrainItemType(int levelNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(levelNum, itemType);
}

Boolean Nanosaur2Script_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z)
{
	const char* mode = Nanosaur2Script_ModeName(levelNum);
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
		.networked = gNumPlayers > 1,
		.mode = mode,
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
	const char* mode = Nanosaur2Script_ModeName(levelNum);
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
		.mode = mode,
		.networked = gNumPlayers > 1,
		.params = params,
		.paramCount = (int)(sizeof(params) / sizeof(params[0])),
		.handled = false,
		.markInUse = false,
	};

	PangeaScriptStatus status = PangeaScript_CallSplineItemHook(&context);
	LogScriptStatus("onSplineItem", status);
	return context.handled && context.markInUse;
}

Boolean Nanosaur2Script_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z)
{
	const PangeaScriptTerrainReplacement* replacement = PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	void* nativeObject = NULL;
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = GetTerrainY(x, z), .z = z};
	if (!replacement)
		return false;
	if (PangeaScript_FindObjectBySource(&source, &handle))
	{
		itemPtr->flags |= ITEM_FLAGS_INUSE;
		return true;
	}
	status = SpawnScriptedObject(replacement->customObjectId, source.x, source.y, source.z, &handle);
	if (status == PANGEA_SCRIPT_OK && PangeaScript_GetObjectNativeObject(handle, &nativeObject))
	{
		((ObjNode*)nativeObject)->TerrainItemPtr = itemPtr;
		status = PangeaScript_AssociateObjectSource(handle, &source);
		if (status != PANGEA_SCRIPT_OK)
		{
			LogScriptStatus("terrain replacement source association", status);
			(void)PangeaScript_DeleteObject(handle);
			if (!replacement->strict)
				PangeaScript_ClearLastError();
			return replacement->strict;
		}
		itemPtr->flags |= ITEM_FLAGS_INUSE;
		if (!CompleteScriptReplacement(handle, "terrain replacement stream-in"))
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
		LogScriptStatus("terrain replacement", status);
	else
		PangeaScript_ClearLastError();
	return replacement->strict;
}

int Nanosaur2Script_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z)
{
	static TerrainItemEntryType probeItem;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptObjectSource source;
	memset(&probeItem, 0, sizeof(probeItem));
	if (!PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!Nanosaur2Script_TryReplaceTerrainItem(&probeItem, itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	source = (PangeaScriptObjectSource){
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = GetTerrainY(x, z), .z = z};
	if (!PangeaScript_FindObjectBySource(&source, &handle) || handle.id <= 0 || handle.generation == 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_DeleteObject(handle))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PANGEA_SCRIPT_OK;
}

Boolean Nanosaur2Script_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex)
{
	const PangeaScriptSplineReplacement* replacement = PangeaScript_GetSplineReplacement(
		splineNum, itemIndex, itemPtr->type, itemPtr->placement);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	void* nativeObject = NULL;
	float x;
	float z;
	if (!replacement)
		return false;
	GetCoordOnSpline(&gSplineList[splineNum], itemPtr->placement, &x, &z);
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_SPLINE, .itemIndex = itemIndex, .nativeType = itemPtr->type,
		.splineNum = splineNum, .x = x, .y = GetTerrainY(x, z), .z = z,
		.placement = itemPtr->placement};
	if (PangeaScript_FindObjectBySource(&source, &handle))
		return true;
	status = SpawnScriptedObject(replacement->customObjectId, source.x, source.y, source.z, &handle);
	if (status == PANGEA_SCRIPT_OK && PangeaScript_GetObjectNativeObject(handle, &nativeObject))
	{
		ObjNode* object = (ObjNode*)nativeObject;
		object->SplineItemPtr = itemPtr;
		object->SplineNum = (uint8_t)splineNum;
		object->SplinePlacement = itemPtr->placement;
		object->SplineMoveCall = MoveScriptedSplineObject;
		object->StatusBits |= STATUS_BIT_ONSPLINE;
		DetachObject(object, true);
		AddToSplineObjectList(object, true);
		status = PangeaScript_AssociateObjectSource(handle, &source);
		if (status != PANGEA_SCRIPT_OK)
		{
			LogScriptStatus("spline replacement source association", status);
			(void)PangeaScript_DeleteObject(handle);
			if (!replacement->strict)
				PangeaScript_ClearLastError();
			return replacement->strict;
		}
		if (!CompleteScriptReplacement(handle, "spline replacement stream-in"))
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
		LogScriptStatus("spline replacement", status);
	else
		PangeaScript_ClearLastError();
	return replacement->strict;
}

int Nanosaur2Script_ProbeSplineReplacementJS(int splineNum, int itemIndex, int nativeType, float placement)
{
	static SplineItemType probeItem;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptObjectSource source;
	float x;
	float z;
	memset(&probeItem, 0, sizeof(probeItem));
	if (!gSplineList || splineNum < 0 || splineNum >= gNumSplines || placement < 0.0f || placement > 1.0f)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	probeItem.type = (Byte)nativeType;
	probeItem.placement = placement;
	if (!PangeaScript_GetSplineReplacement(splineNum, itemIndex, nativeType, placement))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!Nanosaur2Script_TryReplaceSplineItem(&probeItem, splineNum, itemIndex))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	GetCoordOnSpline(&gSplineList[splineNum], placement, &x, &z);
	source = (PangeaScriptObjectSource){
		.kind = PANGEA_SCRIPT_SOURCE_SPLINE, .itemIndex = itemIndex, .nativeType = nativeType,
		.splineNum = splineNum, .x = x, .y = GetTerrainY(x, z), .z = z, .placement = placement};
	if (!PangeaScript_FindObjectBySource(&source, &handle) || handle.id <= 0 || handle.generation == 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_DeleteObject(handle))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PANGEA_SCRIPT_OK;
}

EMSCRIPTEN_KEEPALIVE int Nanosaur2Script_ProbeFirstSplineJS(void)
{
	if (!gSplineList)
	{
		PangeaScript_ClearLastError();
		return -1;
	}
	for (int splineNum = 0; splineNum < gNumSplines; splineNum++)
	{
		SplineDefType* spline = &gSplineList[splineNum];
		if (!spline->itemList || spline->numItems <= 0)
			continue;
		return Nanosaur2Script_OnSplineItem(&spline->itemList[0], gLevelNum, splineNum) ? 1 : 0;
	}
	PangeaScript_ClearLastError();
	return -1;
}

EMSCRIPTEN_KEEPALIVE int Nanosaur2Script_ProbeFirstSplineReplacementJS(void)
{
	if (!gSplineList)
		return -1;
	for (int splineNum = 0; splineNum < gNumSplines; splineNum++)
	{
		SplineDefType* spline = &gSplineList[splineNum];
		if (!spline->itemList || spline->numItems <= 0)
			continue;
		return Nanosaur2Script_ProbeSplineReplacementJS(
			splineNum,
			0,
			spline->itemList[0].type,
			spline->itemList[0].placement);
	}
	PangeaScript_ClearLastError();
	return -1;
}

int Nanosaur2Script_ProbeCheckpointResetJS(void)
{
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptObjectHandle playerHandle;
	PangeaScriptStatus status;
	ObjNode* player;

	if (!gIsInGame || !gPlayerInfo[0].objNode)
		return 0;
	player = gPlayerInfo[0].objNode;
	playerHandle = (PangeaScriptObjectHandle)
	{
		.id = (int) player->ScriptObjectID,
		.generation = player->ScriptObjectGeneration,
	};
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0 || !PangeaScript_ObjectExists(playerHandle))
	{
		player->ScriptObjectID = 0;
		player->ScriptObjectGeneration = 0;
		Nanosaur2Script_RegisterPlayerObject(player);
	}
	status = PangeaScript_RegisterScriptedObject(
		"browser-custom-object",
		player->Coord.x,
		player->Coord.y,
		player->Coord.z,
		&handle);
	if (status != PANGEA_SCRIPT_OK)
		return (int)status;
	if (gPlayerInfo[0].numFreeLives <= 0)
		gPlayerInfo[0].numFreeLives = 1;
	ResetPlayerAtBestCheckpoint(0);
	if (!PangeaScript_DeleteObject(handle))
		return (int)PANGEA_SCRIPT_RUNTIME_ERROR;
	return 1;
}

EMSCRIPTEN_KEEPALIVE int Nanosaur2Script_ProbeDeathRespawnJS(void)
{
	short savedLives;
	bool respawned;

	if (!gIsInGame || !gPlayerInfo[0].objNode)
		return 0;
	savedLives = gPlayerInfo[0].numFreeLives;
	if (gVSMode == VS_MODE_BATTLE && gPlayerInfo[0].numFreeLives < 2)
		gPlayerInfo[0].numFreeLives = 2;
	KillPlayer(0, PLAYER_DEATH_TYPE_DEATHDIVE, NULL);
	if (!gPlayerIsDead[0])
	{
		gPlayerInfo[0].numFreeLives = savedLives;
		return 0;
	}
	ResetPlayerAtBestCheckpoint(0);
	respawned = !gPlayerIsDead[0];
	gPlayerInfo[0].numFreeLives = savedLives;
	return respawned ? 1 : 0;
}

#endif

#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"

#include "structs.h"
#include "splineitems.h"
#include "checkpoints.h"
#include "triggers.h"

#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif


extern bool PangeaScript_LoadCustomBG3D(FSSpec* spec, int group);
extern bool PangeaScript_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec);

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE int CroMagScript_ProbeRaceCompletionJS(void)
{
	PlayerCompletedRace(0);
	return 0;
}
#endif

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
	(void) IsSplineItemVisible(object);
	if (object->ScriptObjectID == 0)
		return;
	isAttached = (object->StatusBits & STATUS_BIT_DETACHED) == 0;
	if (wasAttached == isAttached)
		return;
	handle = (PangeaScriptObjectHandle){object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
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
	if (!CroMagSpawnTerrainItem(PangeaScript_ResolveNativeItemType(id), item, (long) x, (long) z)) { gScriptTerrainItemOccupied[item - gScriptTerrainItems] = false; return PANGEA_SCRIPT_INCOMPATIBLE_ITEM; }
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode) if (node->TerrainItemPtr == item)
	{
		CroMagScript_RegisterObject(node, "cromag.terrain-item", "terrain-item");
		gScriptTerrainItemReclaimable[item - gScriptTerrainItems] = true;
		if (outHandle) *outHandle = (PangeaScriptObjectHandle){node->ScriptObjectID, node->ScriptObjectGeneration};
		break;
	}
	return PANGEA_SCRIPT_OK;
}

typedef struct ScriptModelCacheEntry { char path[260]; } ScriptModelCacheEntry;
static ScriptModelCacheEntry gScriptModelCache[MODEL_GROUP_SCRIPT_CUSTOM_COUNT];
typedef struct ScriptSkeletonCacheEntry { char modelPath[260]; char skeletonPath[260]; } ScriptSkeletonCacheEntry;
static ScriptSkeletonCacheEntry gScriptSkeletonCache[SKELETON_TYPE_SCRIPT_CUSTOM_COUNT];

static void CroMagScript_ReleaseCustomAssets(void)
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
		if (gBG3DContainerList[group])
			DisposeBG3DContainer(group);
		gNumObjectsInBG3DGroupList[group] = 0;
		gScriptModelCache[i].path[0] = '\0';
	}
}

static bool MakeDataAssetPath(const char* source, char* destination, size_t capacity)
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
		if (gNumObjectsInBG3DGroupList[group] == 0) gScriptModelCache[i].path[0] = '\0';
		if (strcmp(gScriptModelCache[i].path, modelPath) == 0) return group;
	}
	if (!MakeDataAssetPath(modelPath, dataPath, sizeof(dataPath)) ||
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, dataPath, &spec) != noErr) return -1;
	if (!IsSafeCustomAsset(&spec)) return -1;
	for (int i = 0; i < MODEL_GROUP_SCRIPT_CUSTOM_COUNT; i++)
	{
		int group = MODEL_GROUP_SCRIPT_CUSTOM_BASE + i;
		if (gNumObjectsInBG3DGroupList[group] != 0) continue;
		if (!PangeaScript_LoadCustomBG3D(&spec, group))
		{
			if (gBG3DContainerList[group])
				DisposeBG3DContainer(group);
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
		if (!PangeaScript_LoadCustomSkeleton(type, &skeletonSpec, &modelSpec)) return -1;
		snprintf(gScriptSkeletonCache[i].modelPath, sizeof(gScriptSkeletonCache[i].modelPath), "%s", definition->modelPath);
		snprintf(gScriptSkeletonCache[i].skeletonPath, sizeof(gScriptSkeletonCache[i].skeletonPath), "%s", definition->skeletonPath);
		return type;
	}
	return -1;
}

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

static bool CroMagScript_GetObjectVelocity(void* nativeObject, PangeaScriptVector3* outVelocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outVelocity || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outVelocity = (PangeaScriptVector3){obj->Delta.x, obj->Delta.y, obj->Delta.z};
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

static bool CroMagScript_GetObjectRotation(void* nativeObject, PangeaScriptVector3* outRotation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outRotation || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outRotation = (PangeaScriptVector3){obj->Rot.x, obj->Rot.y, obj->Rot.z};
	return true;
}

static bool CroMagScript_SetObjectRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || !rotation || obj->CType == INVALID_NODE_FLAG) return false;
	obj->Rot = (OGLVector3D){rotation->x, rotation->y, rotation->z};
	UpdateObjectTransforms(obj);
	return true;
}

static bool CroMagScript_GetObjectScale(void* nativeObject, float* outScale)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outScale || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outScale = obj->Scale.x;
	return true;
}

static bool CroMagScript_SetObjectScale(void* nativeObject, float scale)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG || scale <= 0.0f) return false;
	obj->Scale = (OGLVector3D){scale, scale, scale};
	UpdateObjectTransforms(obj);
	return true;
}

static bool CroMagScript_SetObjectCollisionEnabled(void* nativeObject, bool enabled)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG) return false;
	if (enabled && (!obj->ScriptActiveStateInitialized || obj->ScriptActive)) obj->StatusBits &= ~STATUS_BIT_NOCOLLISION;
	else obj->StatusBits |= STATUS_BIT_NOCOLLISION;
	return true;
}

static bool CroMagScript_GetObjectCollisionEnabled(void* nativeObject, bool* outEnabled)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outEnabled || obj->CType == INVALID_NODE_FLAG) return false;
	*outEnabled = (obj->StatusBits & STATUS_BIT_NOCOLLISION) == 0;
	return true;
}

static bool CroMagScript_GetObjectActive(void* nativeObject, bool* outActive)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outActive || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outActive = !obj->ScriptActiveStateInitialized || obj->ScriptActive;
	return true;
}

static bool CroMagScript_SetObjectActive(void* nativeObject, bool active)
{
	ObjNode* obj = (ObjNode*)nativeObject;
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
	if (!obj || !animation || !obj->ScriptDefinitionID[0]) return -1;
	definition = PangeaScript_GetCustomObjectDefinition(obj->ScriptDefinitionID);
	if (!definition) return -1;
	for (int i = 0; i < definition->animationCount; i++)
		if (strcmp(definition->animationNames[i], animation) == 0) return definition->animationIndices[i];
	return -1;
}

static bool CroMagScript_GetObjectAnimation(void* nativeObject, int* outAnimation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outAnimation || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outAnimation = obj->Skeleton->AnimNum;
	return true;
}

static bool CroMagScript_SetObjectAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG || animation < 0) return false;
	if (blendSeconds > 0.0f) MorphToSkeletonAnim(obj->Skeleton, animation, 1.0f / blendSeconds);
	else SetSkeletonAnim(obj->Skeleton, animation);
	obj->Skeleton->AnimSpeed = speed;
	obj->ScriptAnimationCompletionSent = false;
	return true;
}

static bool CroMagScript_SetObjectAnimationNamed(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	int index = ResolveNamedAnimation((ObjNode*)nativeObject, animation);
	return index >= 0 && CroMagScript_SetObjectAnimation(nativeObject, index, speed, blendSeconds);
}

static bool CroMagScript_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	if (obj->ScriptDefinitionID[0])
	{
		if ((obj->TerrainItemPtr || obj->SplineItemPtr) && !obj->ScriptStreamOutSent)
		{
			PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
			if (PangeaScript_ObjectExists(handle))
				(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		obj->ScriptDeleteRequested = true;
	}
	else { CroMagScript_UnregisterObject(obj); DeleteObject(obj); }
	return true;
}

static const PangeaScriptObjectOps kCroMagPlayerObjectOps =
{
	.getPosition = CroMagScript_GetObjectPosition,
	.getVelocity = CroMagScript_GetObjectVelocity,
	.getRotation = CroMagScript_GetObjectRotation,
	.getScale = CroMagScript_GetObjectScale,
	.getAnimation = CroMagScript_GetObjectAnimation,
	.getActive = CroMagScript_GetObjectActive,
	.setPosition = CroMagScript_SetObjectPosition,
	.setVelocity = CroMagScript_SetObjectVelocity,
	.setRotation = CroMagScript_SetObjectRotation,
	.setScale = CroMagScript_SetObjectScale,
	.setAnimation = CroMagScript_SetObjectAnimation,
	.setAnimationNamed = CroMagScript_SetObjectAnimationNamed,
	.setCollisionEnabled = CroMagScript_SetObjectCollisionEnabled,
	.getCollisionEnabled = CroMagScript_GetObjectCollisionEnabled,
	.setActive = CroMagScript_SetObjectActive,
	.deleteObject = CroMagScript_DeleteObject,
};

void CroMagScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void CroMagScript_ResetObjectRegistry(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	CroMagScript_ReleaseCustomAssets();
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	memset(gScriptTerrainItemOccupied, 0, sizeof(gScriptTerrainItemOccupied));
	memset(gScriptTerrainItemReclaimable, 0, sizeof(gScriptTerrainItemReclaimable));
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

	if (category)
	{
		tags[tagCount++] = category;
	}

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = obj,
		.ops = &kCroMagPlayerObjectOps,
		.objectType = nativeId,
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
	if (PangeaScript_ObjectExists(handle))
		(void) PangeaScript_UnregisterObject(handle);
	obj->ScriptObjectID = 0;
	obj->ScriptObjectGeneration = 0;
	obj->ScriptVisualOffset = (OGLVector3D){0};
}

void CroMagScript_RegisterPlayerObject(ObjNode* playerObj, short playerNum)
{
	CroMagScript_RegisterObject(playerObj, "cromag.player", "player");
	if (playerObj && playerObj->ScriptObjectID > 0 && playerNum >= 0 && playerNum < MAX_PLAYERS)
	{
		PangeaScriptPlayerEventContext context = {
			.levelNum = gScriptFrameContext.levelNum,
			.playerNum = playerNum,
			.player = {playerObj->ScriptObjectID, (uint32_t)playerObj->ScriptObjectGeneration},
			.position = {playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z},
		};
		LogScriptStatus("onPlayerSpawn", PangeaScript_CallPlayerEvent(&context, "onPlayerSpawn"));
	}
}

void CroMagScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	CroMagScript_UnregisterObject(playerObj);
}

Boolean CroMagScript_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId)
{
	PangeaScriptPickupContext context;
	PangeaScriptPickupResult result = {0};
	PangeaScriptStatus status;
	PangeaScriptObjectHandle pickupHandle;
	PangeaScriptObjectHandle playerHandle;
	if (!pickup || !player || pickup->ScriptObjectID <= 0 || pickup->ScriptObjectGeneration <= 0 ||
		player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return true;
	pickupHandle = (PangeaScriptObjectHandle){pickup->ScriptObjectID, (uint32_t) pickup->ScriptObjectGeneration};
	playerHandle = (PangeaScriptObjectHandle){player->ScriptObjectID, (uint32_t) player->ScriptObjectGeneration};
	if (!PangeaScript_ObjectExists(pickupHandle) || !PangeaScript_ObjectExists(playerHandle))
		return true;
	context = (PangeaScriptPickupContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = player->PlayerNum,
		.pickupType = pickupType,
		.amount = amount,
		.pickupId = pickupId,
		.pickup = pickupHandle,
		.player = playerHandle,
		.position = {pickup->Coord.x, pickup->Coord.y, pickup->Coord.z},
	};
	status = PangeaScript_CallPickupHook(&context, &result);
	(void) result;
	LogScriptStatus("onPickupCollected", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	return result.hasConsumePickup ? result.consumePickup : true;
}

Boolean CroMagScript_OnDamage(short playerNum, float damage, int cause, float* outDamage)
{
	ObjNode* player;
	PangeaScriptObjectHandle target;
	PangeaScriptDamageContext context;
	PangeaScriptDamageResult result;
	PangeaScriptStatus status;

	if (!outDamage || playerNum < 0 || playerNum >= MAX_PLAYERS)
		return true;
	*outDamage = damage;
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
	status = PangeaScript_CallDamageHook(&context, &result);
	LogScriptStatus("onDamage", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	if (result.hasDamage)
		*outDamage = result.damage;
	return result.hasApplyDamage ? result.applyDamage : true;
}

void CroMagScript_OnDamageApplied(short playerNum, float damage, int cause)
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
		.target = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onDamageApplied", PangeaScript_CallDamageAppliedHook(&context));
}

void CroMagScript_OnDeath(short playerNum, int eventValue)
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
		.player = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onDeath", PangeaScript_CallPlayerEvent(&context, "onDeath"));
}

void CroMagScript_OnCheckpointReached(short playerNum, short checkpointNum)
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

void CroMagScript_OnLapComplete(short playerNum, short lapNum)
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
		.player = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onLapComplete", PangeaScript_CallPlayerEvent(&context, "onLapComplete"));
}

void CroMagScript_OnRaceFinish(short playerNum, int placement)
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
		.eventValue = placement,
		.player = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onRaceFinish", PangeaScript_CallPlayerEvent(&context, "onRaceFinish"));
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

void CroMagScript_RunObjectFrame(ObjNode* obj)
{
	if (!obj->ScriptDeleteRequested)
		CroMagScript_ApplyObjectScripting(obj);
	if (obj->CType == INVALID_NODE_FLAG) return;
	if (obj->ScriptDeleteRequested) { DeleteObject(obj); return; }
	if (obj->Skeleton && obj->Skeleton->AnimHasStopped && !obj->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
		obj->ScriptAnimationCompletionSent = true;
	}
}

void CroMagScript_OnObjectDeleted(ObjNode* obj)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		if (!PangeaScript_ObjectExists(handle))
			return;
		if ((obj->TerrainItemPtr || obj->SplineItemPtr) && !obj->ScriptStreamOutSent)
		{
			(void)PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		else if (!obj->ScriptStreamOutSent)
			(void)PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	}
}

static int ResolveDisplayGroup(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP) return GetCustomModelGroup(definition->modelPath);
	if (strcmp(definition->nativeGroup, "global") == 0) return MODEL_GROUP_GLOBAL;
	if (strcmp(definition->nativeGroup, "levelSpecific") == 0) return MODEL_GROUP_LEVELSPECIFIC;
	if (strcmp(definition->nativeGroup, "weapons") == 0) return MODEL_GROUP_WEAPONS;
	if (strcmp(definition->nativeGroup, "carParts") == 0) return MODEL_GROUP_CARPARTS;
	return -1;
}

static int ResolveInitialAnimation(const PangeaScriptCustomObjectDefinition* definition)
{
	if (!definition->initialAnimationName[0]) return definition->initialAnimation;
	for (int i = 0; i < definition->animationCount; i++)
		if (strcmp(definition->animationNames[i], definition->initialAnimationName) == 0) return definition->animationIndices[i];
	return -1;
}

static ObjNode* MakeScriptedVisual(const PangeaScriptCustomObjectDefinition* definition, float x, float y, float z)
{
	NewObjectDefinitionType objectDefinition;
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_DISPLAY_GROUP || definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
	{
		int group = ResolveDisplayGroup(definition);
		if (group < 0 || definition->modelObject < 0 || definition->modelObject >= gNumObjectsInBG3DGroupList[group]) return nil;
		objectDefinition = (NewObjectDefinitionType){.group=group, .type=definition->modelObject, .coord={x,y,z}, .flags=0, .slot=definition->slot, .moveCall=nil, .rot=0, .scale=definition->scale};
		return MakeNewDisplayGroupObject(&objectDefinition);
	}
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_SKELETON || definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON)
	{
		int animation = ResolveInitialAnimation(definition);
		int skeletonType = definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON ? GetCustomSkeletonType(definition) : definition->skeletonType;
		if (skeletonType < 0 || skeletonType >= MAX_SKELETON_TYPES || animation < 0 || !IsSkeletonTypeLoaded(skeletonType)) return nil;
		objectDefinition = (NewObjectDefinitionType){.type=skeletonType, .animNum=animation, .coord={x,y,z}, .flags=0, .slot=definition->slot, .moveCall=nil, .rot=0, .scale=definition->scale};
		ObjNode* object = MakeNewSkeletonObject(&objectDefinition);
		if (object && object->Skeleton) object->Skeleton->AnimSpeed = definition->animationSpeed;
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
	if (preset == PANGEA_SCRIPT_COLLISION_NONE) return;
	radius = (short)object->BoundingSphereRadius;
	halfWidth = definition->collisionBoundsSet ? (short)(definition->collisionWidth * 0.5f) : radius;
	halfHeight = definition->collisionBoundsSet ? (short)(definition->collisionHeight * 0.5f) : radius;
	halfDepth = definition->collisionBoundsSet ? (short)(definition->collisionDepth * 0.5f) : radius;
	SetObjectCollisionBounds(object, halfHeight, -halfHeight, -halfWidth, halfWidth, halfDepth, -halfDepth);
	object->CBits = CBITS_ALLSOLID;
	if (preset == PANGEA_SCRIPT_COLLISION_ENEMY) object->CType = CTYPE_AVOID | CTYPE_AUTOTARGET | CTYPE_MISC;
	else if (preset == PANGEA_SCRIPT_COLLISION_PICKUP || preset == PANGEA_SCRIPT_COLLISION_TRIGGER_BOX)
	{
		object->CType = CTYPE_TRIGGER;
		object->Kind = TRIGTYPE_SCRIPTED;
		object->TriggerSides = ALL_SOLID_SIDES;
	}
	else object->CType = CTYPE_MISC;
}

void CroMagScript_OnCustomTrigger(ObjNode* trigger, ObjNode* who, Byte sideBits)
{
	if (!trigger || !trigger->ScriptObjectID) return;
	PangeaScriptObjectHandle handle = {trigger->ScriptObjectID, (uint32_t)trigger->ScriptObjectGeneration};
	PangeaScriptObjectHandle other = {0};
	int playerNum = -1;
	if (!PangeaScript_ObjectExists(handle)) return;
	if (who && who->ScriptObjectID)
		other = (PangeaScriptObjectHandle){who->ScriptObjectID, (uint32_t)who->ScriptObjectGeneration};
	if (who)
		for (int index = 0; index < gNumTotalPlayers; index++)
			if (gPlayerInfo[index].objNode == who) { playerNum = index; break; }
	if (other.id > 0 && !PangeaScript_ObjectExists(other))
		other = (PangeaScriptObjectHandle){0};
	(void)PangeaScript_CallObjectTriggerWithOtherAndPlayer(handle, &gScriptFrameContext, sideBits, true, other, playerNum);
}

void CroMagScript_OnAnimationEvent(ObjNode* obj, int eventValue)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_CallObjectEventWithValue(handle, &gScriptFrameContext, "animationEvent", eventValue);
	}
}

static PangeaScriptStatus SpawnScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
	const PangeaScriptCustomObjectDefinition* definition = PangeaScript_GetCustomObjectDefinition(id);
	ObjNode* object;
	if (!definition) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	object = MakeScriptedVisual(definition, x, y, z);
	if (!object) return PANGEA_SCRIPT_RUNTIME_ERROR;
	ApplyScriptedCollision(object, definition);
	snprintf(object->ScriptDefinitionID, sizeof(object->ScriptDefinitionID), "%s", definition->id);
	CroMagScript_RegisterObject(object, definition->id, "customObject");
	if (!object->ScriptObjectID) { DeleteObject(object); return PANGEA_SCRIPT_RUNTIME_ERROR; }
	if (outHandle) *outHandle = (PangeaScriptObjectHandle){object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
	return PANGEA_SCRIPT_OK;
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
#define CROMAG_TERRAIN_NATIVE_ITEM(type) { .id = #type, .nativeType = type, .category = "terrain", .dependencySummary = "current track assets, terrain systems, and the native item initializer" },
	CROMAG_TERRAIN_NATIVE_ITEM(1) CROMAG_TERRAIN_NATIVE_ITEM(2) CROMAG_TERRAIN_NATIVE_ITEM(3) CROMAG_TERRAIN_NATIVE_ITEM(4) CROMAG_TERRAIN_NATIVE_ITEM(5) CROMAG_TERRAIN_NATIVE_ITEM(6) CROMAG_TERRAIN_NATIVE_ITEM(7) CROMAG_TERRAIN_NATIVE_ITEM(8) CROMAG_TERRAIN_NATIVE_ITEM(9) CROMAG_TERRAIN_NATIVE_ITEM(10)
	CROMAG_TERRAIN_NATIVE_ITEM(11) CROMAG_TERRAIN_NATIVE_ITEM(12) CROMAG_TERRAIN_NATIVE_ITEM(13) CROMAG_TERRAIN_NATIVE_ITEM(14) CROMAG_TERRAIN_NATIVE_ITEM(15) CROMAG_TERRAIN_NATIVE_ITEM(16) CROMAG_TERRAIN_NATIVE_ITEM(17) CROMAG_TERRAIN_NATIVE_ITEM(18) CROMAG_TERRAIN_NATIVE_ITEM(19) CROMAG_TERRAIN_NATIVE_ITEM(20)
	CROMAG_TERRAIN_NATIVE_ITEM(21) CROMAG_TERRAIN_NATIVE_ITEM(22) CROMAG_TERRAIN_NATIVE_ITEM(23) CROMAG_TERRAIN_NATIVE_ITEM(24) CROMAG_TERRAIN_NATIVE_ITEM(25) CROMAG_TERRAIN_NATIVE_ITEM(26) CROMAG_TERRAIN_NATIVE_ITEM(27) CROMAG_TERRAIN_NATIVE_ITEM(28) CROMAG_TERRAIN_NATIVE_ITEM(29) CROMAG_TERRAIN_NATIVE_ITEM(30)
	CROMAG_TERRAIN_NATIVE_ITEM(31) CROMAG_TERRAIN_NATIVE_ITEM(32) CROMAG_TERRAIN_NATIVE_ITEM(33) CROMAG_TERRAIN_NATIVE_ITEM(34) CROMAG_TERRAIN_NATIVE_ITEM(35) CROMAG_TERRAIN_NATIVE_ITEM(36) CROMAG_TERRAIN_NATIVE_ITEM(37)
#undef CROMAG_TERRAIN_NATIVE_ITEM
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

	SDL_Log("Cro-Mag Rally scripting %s failed: %s", action, PangeaScript_GetLastError());
}

static int GetScriptPlayerCount(void) { return gNumTotalPlayers; }

static bool GetScriptPlayer(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (!outPlayer || playerNum < 0 || playerNum >= gNumTotalPlayers || !gPlayerInfo[playerNum].objNode) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){
		.position = {gPlayerInfo[playerNum].coord.x, gPlayerInfo[playerNum].coord.y, gPlayerInfo[playerNum].coord.z},
		.velocity = {gPlayerInfo[playerNum].objNode->Delta.x, gPlayerInfo[playerNum].objNode->Delta.y, gPlayerInfo[playerNum].objNode->Delta.z},
		.hasVelocity = true,
		.collisionEnabled = gPlayerInfo[playerNum].objNode->CType != 0 && (gPlayerInfo[playerNum].objNode->StatusBits & STATUS_BIT_NOCOLLISION) == 0,
		.hasCollisionEnabled = true,
		.health = gPlayerInfo[playerNum].health,
		.hasHealth = true,
		.lapNum = gPlayerInfo[playerNum].lapNum,
		.checkpointNum = gPlayerInfo[playerNum].checkpointNum,
		.placement = gPlayerInfo[playerNum].place,
		.raceComplete = gPlayerInfo[playerNum].raceComplete,
		.hasRaceState = IsRaceMode(),
		.vehicleType = gPlayerInfo[playerNum].vehicleType,
		.vehicleMaxSpeed = gPlayerInfo[playerNum].carStats.maxSpeed,
		.vehicleAcceleration = gPlayerInfo[playerNum].carStats.acceleration,
		.vehicleTraction = gPlayerInfo[playerNum].carStats.tireTraction,
		.vehicleSuspension = gPlayerInfo[playerNum].carStats.suspension,
		.hasVehicleState = true,
		.team = gPlayerInfo[playerNum].team,
		.hasTeamState = gGameMode == GAME_MODE_CAPTUREFLAG,
		.carryingFlag = gPlayerInfo[playerNum].objNode->CapturedFlag != NULL,
		.captureScore = gCapturedFlagCount[gPlayerInfo[playerNum].team & 1],
		.hasCaptureState = gGameMode == GAME_MODE_CAPTUREFLAG,
		.camera = {gPlayerInfo[playerNum].camera.cameraLocation.x, gPlayerInfo[playerNum].camera.cameraLocation.y, gPlayerInfo[playerNum].camera.cameraLocation.z},
		.hasCameraState = true,
		.activeWeapon = gPlayerInfo[playerNum].powType,
		.hasWeaponState = true,
		.weaponCount = gPlayerInfo[playerNum].powType >= 0 && gPlayerInfo[playerNum].powQuantity > 0 ? 1 : 0,
		.tokenCount = gPlayerInfo[playerNum].numTokens,
		.hasTokenState = true,
		.active = true,
	};
	outPlayer->rotation = (PangeaScriptVector3){gPlayerInfo[playerNum].objNode->Rot.x, gPlayerInfo[playerNum].objNode->Rot.y, gPlayerInfo[playerNum].objNode->Rot.z};
	outPlayer->hasRotation = true;
	outPlayer->aim = (PangeaScriptVector3){-sinf(gPlayerInfo[playerNum].objNode->Rot.y), 0.0f, -cosf(gPlayerInfo[playerNum].objNode->Rot.y)};
	outPlayer->hasAimState = true;
	if (outPlayer->weaponCount > 0)
		outPlayer->weapons[0] = (PangeaScriptPlayerInventoryEntry){gPlayerInfo[playerNum].powType, gPlayerInfo[playerNum].powQuantity};
	return true;
}

static PangeaScriptStatus SetScriptPlayerHealth(int playerNum, float health)
{
	if (playerNum < 0 || playerNum >= gNumTotalPlayers || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo[playerNum].health = health;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerWeaponQuantity(int playerNum, int weaponType, int quantity)
{
	if (playerNum < 0 || playerNum >= gNumTotalPlayers || weaponType < 0 || weaponType >= MAX_POW_TYPES || quantity < 0 || quantity > 32767 || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (quantity == 0)
	{
		gPlayerInfo[playerNum].powType = POW_TYPE_NONE;
		gPlayerInfo[playerNum].powQuantity = 0;
	}
	else
	{
		gPlayerInfo[playerNum].powType = (short) weaponType;
		gPlayerInfo[playerNum].powQuantity = (short) quantity;
	}
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerPosition(int playerNum, const PangeaScriptVector3* position)
{
	if (playerNum < 0 || playerNum >= gNumTotalPlayers || !position || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo[playerNum].coord = (OGLPoint3D){position->x, position->y, position->z};
	gPlayerInfo[playerNum].objNode->Coord = gPlayerInfo[playerNum].coord;
	UpdateObjectTransforms(gPlayerInfo[playerNum].objNode);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerVelocity(int playerNum, const PangeaScriptVector3* velocity)
{
	if (playerNum < 0 || playerNum >= gNumTotalPlayers || !velocity || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	return CroMagScript_SetObjectVelocity(gPlayerInfo[playerNum].objNode, velocity) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
}

void CroMagScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "CroMagRally-Android",
		.gameName = "Cro-Mag Rally",
		.spawnNative = SpawnNativeItem,
		.spawnScripted = SpawnScriptedObject,
		.getPlayerCount = GetScriptPlayerCount,
		.getPlayer = GetScriptPlayer,
		.setPlayerHealth = SetScriptPlayerHealth,
		.setPlayerWeaponQuantity = SetScriptPlayerWeaponQuantity,
		.setPlayerPosition = SetScriptPlayerPosition,
		.setPlayerVelocity = SetScriptPlayerVelocity,
		.capabilities = PANGEA_SCRIPT_CRO_MAG_RALLY_CAPABILITIES,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	CroMagScript_ResetObjectRegistry();
	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	if (status == PANGEA_SCRIPT_OK)
		LogScriptStatus("onGameStart", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_START, &(PangeaScriptLevelContext){0}));
}

void CroMagScript_Shutdown(void)
{
	LogScriptStatus("onGameShutdown", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN, &(PangeaScriptLevelContext){0}));
	CroMagScript_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void CroMagScript_LoadTrackConfig(int trackNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(trackNum);
	LogScriptStatus("track config load", status);
}

static const char* CroMagScript_TrackName(int trackNum)
{
	static const char* trackNames[] =
	{
		"desert", "jungle", "ice", "crete", "china", "egypt", "europe", "scandinavia", "atlantis",
		"stonehenge", "aztec", "coliseum", "maze", "celtic", "tarpits", "spiral", "ramps",
	};
	if (trackNum < 0 || trackNum >= (int)(sizeof(trackNames) / sizeof(trackNames[0]))) return NULL;
	return trackNames[trackNum];
}

static const char* CroMagScript_ModeName(void)
{
	if (gGameMode == GAME_MODE_PRACTICE) return "practice";
	if (gGameMode == GAME_MODE_MULTIPLAYERRACE || gNetGameInProgress) return "network";
	return "local";
}

static bool CroMagScript_IsNetworked(void)
{
	return gGameMode == GAME_MODE_MULTIPLAYERRACE || gNetGameInProgress;
}

static void CallRaceHook(PangeaScriptHook hook, int trackNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = trackNum,
		.levelName = NULL,
		.mode = CroMagScript_ModeName(),
		.networked = CroMagScript_IsNetworked(),
		.trackName = CroMagScript_TrackName(trackNum),
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

void CroMagScript_OnRaceLoad(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, trackNum, "onRaceLoad");
}

void CroMagScript_OnRaceStart(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_START, trackNum, "onRaceStart");
}

void CroMagScript_OnRaceFrame(int trackNum, unsigned int frameNum, float deltaSeconds, float raceTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = trackNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = raceTimeSeconds,
		.mode = CroMagScript_ModeName(),
		.networked = CroMagScript_IsNetworked(),
		.trackName = CroMagScript_TrackName(trackNum),
	};
	CroMagScript_CacheFrameContext(&context);
	PangeaScript_ExpireTriggerContacts(&context);
	(void)PangeaScript_ApplyDeferredActions(&context);

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onRaceFrame", status);
}

void CroMagScript_OnRaceComplete(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, trackNum, "onRaceComplete");
}

void CroMagScript_OnRaceUnload(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, trackNum, "onRaceUnload");
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
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
		.networked = CroMagScript_IsNetworked(),
		.x = x,
		.z = z,
		.flags = itemPtr->flags,
		.mode = CroMagScript_ModeName(),
		.trackName = CroMagScript_TrackName(trackNum),
		.params = params,
		.paramCount = (int)(sizeof(params) / sizeof(params[0])),
		.handled = false,
		.markInUse = false,
	};

	PangeaScriptStatus status = PangeaScript_CallTerrainItemHook(&context);
	LogScriptStatus("onTerrainItem", status);
	return context.handled && context.markInUse;
}

Boolean CroMagScript_OnSplineItem(SplineItemType* itemPtr, int trackNum, int splineNum)
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
		.levelNum = trackNum,
		.itemType = itemPtr->type,
		.splineNum = splineNum,
		.placement = itemPtr->placement,
		.mode = CroMagScript_ModeName(),
		.networked = CroMagScript_IsNetworked(),
		.trackName = CroMagScript_TrackName(trackNum),
		.params = params,
		.paramCount = (int)(sizeof(params) / sizeof(params[0])),
		.handled = false,
		.markInUse = false,
	};

	PangeaScriptStatus status = PangeaScript_CallSplineItemHook(&context);
	LogScriptStatus("onSplineItem", status);
	return context.handled && context.markInUse;
}

Boolean CroMagScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z)
{
	const PangeaScriptTerrainReplacement* replacement = PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	void* nativeObject = NULL;
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = GetTerrainY(x, z), .z = z};
	if (!replacement) return false;
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

int CroMagScript_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z)
{
	static TerrainItemEntryType probeItem;
	PangeaScriptObjectHandle handle = {0};
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = GetTerrainY(x, z), .z = z};
	memset(&probeItem, 0, sizeof(probeItem));
	if (!PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!CroMagScript_TryReplaceTerrainItem(&probeItem, itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_FindObjectBySource(&source, &handle) || handle.id <= 0 || handle.generation == 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_DeleteObject(handle))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PANGEA_SCRIPT_OK;
}

int CroMagScript_ProbePickupSuppressionJS(int pickupKind)
{
	ObjNode* player = gPlayerInfo[0].objNode;
	ObjNode* pickup = NULL;
	NewObjectDefinitionType definition;
	int savedGameMode;
	short savedPowType;
	short savedPowQuantity;
	float savedAttackTimer;
	short savedTokens;
	short savedTotalTokens;
	Boolean handled;
	Boolean valid;

	if (!player || pickupKind < 0 || pickupKind > 1)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0 ||
		!PangeaScript_ObjectExists((PangeaScriptObjectHandle){player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration}))
	{
		player->ScriptObjectID = 0;
		player->ScriptObjectGeneration = 0;
		CroMagScript_RegisterPlayerObject(player, 0);
	}
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	savedGameMode = gGameMode;
	savedPowType = gPlayerInfo[0].powType;
	savedPowQuantity = gPlayerInfo[0].powQuantity;
	savedAttackTimer = gPlayerInfo[0].attackTimer;
	savedTokens = gPlayerInfo[0].numTokens;
	savedTotalTokens = gTotalTokens;
	definition = (NewObjectDefinitionType){
		.coord = player->Coord,
		.slot = TRIGGER_SLOT,
		.scale = 1.0f,
	};
	pickup = MakeNewObject(&definition);
	if (!pickup)
	{
		gGameMode = savedGameMode;
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	pickup->Coord = player->Coord;
	pickup->CType = CTYPE_TRIGGER;
	pickup->CBits = CBITS_ALLSOLID;
	pickup->TriggerSides = ALL_SOLID_SIDES;

	if (pickupKind == 0)
	{
		pickup->Kind = TRIGTYPE_POW;
		pickup->Special[0] = POW_TYPE_BONE;
	}
	else
	{
		gGameMode = GAME_MODE_TOURNAMENT;
		pickup->Kind = TRIGTYPE_TOKEN;
	}
	CroMagScript_RegisterObject(pickup, pickupKind == 0 ? "cromag.pow" : "cromag.token", "pickup");
	if (pickup->ScriptObjectID <= 0 || pickup->ScriptObjectGeneration <= 0)
	{
		pickup->CType = 0;
		pickup->StatusBits |= STATUS_BIT_HIDDEN;
		gGameMode = savedGameMode;
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	handled = HandleTrigger(pickup, player, SIDE_BITS_FRONT);
	valid = !handled && pickup->CType == CTYPE_TRIGGER &&
		gPlayerInfo[0].powType == savedPowType &&
		gPlayerInfo[0].powQuantity == savedPowQuantity &&
		gPlayerInfo[0].attackTimer == savedAttackTimer &&
		gPlayerInfo[0].numTokens == savedTokens && gTotalTokens == savedTotalTokens;
	pickup->CType = 0;
	pickup->StatusBits |= STATUS_BIT_HIDDEN;
	gGameMode = savedGameMode;
	gPlayerInfo[0].powType = savedPowType;
	gPlayerInfo[0].powQuantity = savedPowQuantity;
	gPlayerInfo[0].attackTimer = savedAttackTimer;
	gPlayerInfo[0].numTokens = savedTokens;
	gTotalTokens = savedTotalTokens;
	return valid ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
}

Boolean CroMagScript_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex)
{
	const PangeaScriptSplineReplacement* replacement = PangeaScript_GetSplineReplacement(splineNum, itemIndex, itemPtr->type, itemPtr->placement);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	void* nativeObject = NULL;
	float x;
	float z;
	if (!replacement) return false;
	GetCoordOnSpline(&(*gSplineList)[splineNum], itemPtr->placement, &x, &z);
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
		DetachObject(object);
		AddToSplineObjectList(object);
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

#endif

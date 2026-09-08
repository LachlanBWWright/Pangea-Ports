#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "splineitems.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif


extern bool PangeaScript_LoadCustomBG3D(FSSpec* spec, int group);
extern bool PangeaScript_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec);

static void LogScriptStatus(const char* action, PangeaScriptStatus status);
static TerrainItemEntryType gScriptTerrainItems[256];
static bool gScriptTerrainItemOccupied[256];
static bool gScriptTerrainItemReclaimable[256];

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

static PangeaScriptFrameContext gScriptFrameContext;
static bool gCheckpointResetProbePending;
static int gSelectedSplineNum = -1;
static int gSelectedSplineItemIndex = -1;
static int gSelectedSplineNativeType = -1;
static float gSelectedSplinePlacement;

void Bugdom2Script_RequestCheckpointResetProbe(void)
{
	gCheckpointResetProbePending = true;
}

void Bugdom2Script_ProcessCheckpointResetProbe(void)
{
	if (!gCheckpointResetProbePending)
		return;
	gCheckpointResetProbePending = false;
	if (gInGameNow && gPlayerInfo.objNode && gPlayerInfo.lives > 1)
		ResetPlayerAtBestCheckpoint();
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
	handle = (PangeaScriptObjectHandle){object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
	(void) PangeaScript_ApplyObjectLifecycle(
		handle,
		&gScriptFrameContext,
		isAttached ? PANGEA_SCRIPT_OBJECT_ACTIVATE : PANGEA_SCRIPT_OBJECT_DEACTIVATE);
}

typedef struct ScriptModelCacheEntry { char path[260]; } ScriptModelCacheEntry;
static ScriptModelCacheEntry gScriptModelCache[MODEL_GROUP_SCRIPT_CUSTOM_COUNT];
typedef struct ScriptSkeletonCacheEntry { char modelPath[260]; char skeletonPath[260]; } ScriptSkeletonCacheEntry;
static ScriptSkeletonCacheEntry gScriptSkeletonCache[SKELETON_TYPE_SCRIPT_CUSTOM_COUNT];

static void Bugdom2Script_ReleaseCustomAssets(void)
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
	char modelPath[260];
	char skeletonPath[260];
	FSSpec modelSpec;
	FSSpec skeletonSpec;
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

static bool Bugdom2Script_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = obj->Coord.x;
	outPosition->y = obj->Coord.y;
	outPosition->z = obj->Coord.z;
	return true;
}

static bool Bugdom2Script_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	return true;
}

static bool Bugdom2Script_GetObjectVelocity(void* nativeObject, PangeaScriptVector3* outVelocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outVelocity || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outVelocity = (PangeaScriptVector3){obj->Delta.x, obj->Delta.y, obj->Delta.z};
	return true;
}

static bool Bugdom2Script_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	return true;
}

static bool Bugdom2Script_GetObjectRotation(void* nativeObject, PangeaScriptVector3* outRotation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outRotation || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outRotation = (PangeaScriptVector3){obj->Rot.x, obj->Rot.y, obj->Rot.z};
	return true;
}

static bool Bugdom2Script_SetObjectRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !rotation || obj->CType == INVALID_NODE_FLAG)
		return false;
	obj->Rot = (OGLVector3D){rotation->x, rotation->y, rotation->z};
	UpdateObjectTransforms(obj);
	return true;
}

static bool Bugdom2Script_GetObjectScale(void* nativeObject, float* outScale)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outScale || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outScale = obj->Scale.x;
	return true;
}

static bool Bugdom2Script_SetObjectScale(void* nativeObject, float scale)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG || scale <= 0.0f)
		return false;
	obj->Scale = (OGLVector3D){scale, scale, scale};
	UpdateObjectTransforms(obj);
	return true;
}

static bool Bugdom2Script_SetObjectCollisionEnabled(void* nativeObject, bool enabled)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG) return false;
	if (enabled && (!obj->ScriptActiveStateInitialized || obj->ScriptActive)) obj->StatusBits &= ~STATUS_BIT_NOCOLLISION;
	else obj->StatusBits |= STATUS_BIT_NOCOLLISION;
	return true;
}

static bool Bugdom2Script_GetObjectCollisionEnabled(void* nativeObject, bool* outEnabled)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outEnabled || obj->CType == INVALID_NODE_FLAG) return false;
	*outEnabled = (obj->StatusBits & STATUS_BIT_NOCOLLISION) == 0;
	return true;
}

static bool Bugdom2Script_GetObjectActive(void* nativeObject, bool* outActive)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outActive || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outActive = !obj->ScriptActiveStateInitialized || obj->ScriptActive;
	return true;
}

static bool Bugdom2Script_SetObjectActive(void* nativeObject, bool active)
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

static bool Bugdom2Script_GetObjectAnimation(void* nativeObject, int* outAnimation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outAnimation || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outAnimation = obj->Skeleton->AnimNum;
	return true;
}

static bool Bugdom2Script_SetObjectAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
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

static bool Bugdom2Script_SetObjectAnimationNamed(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	int animationIndex = ResolveNamedAnimation((ObjNode*)nativeObject, animation);
	return animationIndex >= 0 && Bugdom2Script_SetObjectAnimation(nativeObject, animationIndex, speed, blendSeconds);
}

static bool Bugdom2Script_DeleteObject(void* nativeObject)
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
	else
	{
		Bugdom2Script_UnregisterObject(obj);
		DeleteObject(obj);
	}
	return true;
}

static const PangeaScriptObjectOps kBugdom2PlayerObjectOps =
{
	.getPosition = Bugdom2Script_GetObjectPosition,
	.getVelocity = Bugdom2Script_GetObjectVelocity,
	.getRotation = Bugdom2Script_GetObjectRotation,
	.getScale = Bugdom2Script_GetObjectScale,
	.getAnimation = Bugdom2Script_GetObjectAnimation,
	.getActive = Bugdom2Script_GetObjectActive,
	.getCollisionEnabled = Bugdom2Script_GetObjectCollisionEnabled,
	.setPosition = Bugdom2Script_SetObjectPosition,
	.setVelocity = Bugdom2Script_SetObjectVelocity,
	.setRotation = Bugdom2Script_SetObjectRotation,
	.setScale = Bugdom2Script_SetObjectScale,
	.setAnimation = Bugdom2Script_SetObjectAnimation,
	.setAnimationNamed = Bugdom2Script_SetObjectAnimationNamed,
	.setCollisionEnabled = Bugdom2Script_SetObjectCollisionEnabled,
	.setActive = Bugdom2Script_SetObjectActive,
	.deleteObject = Bugdom2Script_DeleteObject,
};

void Bugdom2Script_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void Bugdom2Script_ResetObjectRegistry(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	Bugdom2Script_ReleaseCustomAssets();
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	memset(gScriptTerrainItemOccupied, 0, sizeof(gScriptTerrainItemOccupied));
	memset(gScriptTerrainItemReclaimable, 0, sizeof(gScriptTerrainItemReclaimable));
	PangeaScript_ResetObjects();
}

void Bugdom2Script_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
{
	PangeaScriptObjectRegistration registration;
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	const char* tags[4];
	int tagCount = 0;

	if (!obj)
		return;

	if (obj->ScriptObjectID > 0)
		return;

	if (category)
	{
		tags[tagCount++] = category;
	}

	Boolean isCollectible = false;
	if (nativeId && (strcmp(nativeId, "bugdom2.dcell") == 0 ||
	                 strcmp(nativeId, "bugdom2.gliderPart") == 0 ||
	                 strcmp(nativeId, "bugdom2.hobobag") == 0 ||
	                 strcmp(nativeId, "bugdom2.acorn") == 0))
	{
		isCollectible = true;
	}
	else if (obj->Kind == PICKUP_KIND_POW)
	{
		if (obj->Special[0] == POW_KIND_GREENCLOVER ||
		    obj->Special[0] == POW_KIND_BLUECLOVER ||
		    obj->Special[0] == POW_KIND_GOLDCLOVER ||
		    obj->Special[0] == POW_KIND_REDKEY ||
		    obj->Special[0] == POW_KIND_GREENKEY ||
		    obj->Special[0] == POW_KIND_BLUEKEY)
		{
			isCollectible = true;
		}
	}

	if (isCollectible)
	{
		tags[tagCount++] = "bugdom2.collectible";
	}

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = obj,
		.ops = &kBugdom2PlayerObjectOps,
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

void Bugdom2Script_UnregisterObject(ObjNode* obj)
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

void Bugdom2Script_RegisterPlayerObject(ObjNode* playerObj)
{
	Bugdom2Script_RegisterObject(playerObj, "bugdom2.player", "player");
	Bugdom2Script_OnPlayerSpawn(playerObj);
}

void Bugdom2Script_UnregisterPlayerObject(ObjNode* playerObj)
{
	Bugdom2Script_UnregisterObject(playerObj);
}

static PangeaScriptObjectHandle Bugdom2Script_ObjectHandle(const ObjNode* object)
{
	PangeaScriptObjectHandle handle;
	if (!object || object->ScriptObjectID <= 0 || object->ScriptObjectGeneration <= 0)
		return (PangeaScriptObjectHandle){0};
	handle = (PangeaScriptObjectHandle){object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
	if (!PangeaScript_ObjectExists(handle))
		return (PangeaScriptObjectHandle){0};
	return handle;
}

Boolean Bugdom2Script_OnDamage(ObjNode* source, float damage, int cause, float* outDamage)
{
	ObjNode* player = gPlayerInfo.objNode;
	PangeaScriptDamageContext context;
	PangeaScriptDamageResult result;
	PangeaScriptStatus status;
	PangeaScriptObjectHandle target;
	if (!player || !outDamage)
		return true;
	target = Bugdom2Script_ObjectHandle(player);
	if (target.id <= 0)
		return true;
	*outDamage = damage;
	context = (PangeaScriptDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.cause = cause,
		.damage = damage,
		.source = Bugdom2Script_ObjectHandle(source),
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

Boolean Bugdom2Script_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId)
{
	PangeaScriptPickupContext context;
	PangeaScriptPickupResult result = {0};
	PangeaScriptStatus status;
	PangeaScriptObjectHandle pickupHandle = Bugdom2Script_ObjectHandle(pickup);
	PangeaScriptObjectHandle playerHandle = Bugdom2Script_ObjectHandle(player);

	if (pickupHandle.id <= 0 || playerHandle.id <= 0)
		return true;
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
		gPlayerInfo.health += result.healthDelta;
		if (gPlayerInfo.health < 0.0f)
			gPlayerInfo.health = 0.0f;
		else if (gPlayerInfo.health > 1.0f)
			gPlayerInfo.health = 1.0f;
		if (gPlayerInfo.objNode)
			gPlayerInfo.objNode->Health = gPlayerInfo.health;
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

void Bugdom2Script_OnMouseRescued(ObjNode* mouse, ObjNode* player, Boolean drowning)
{
	Bugdom2Script_OnPickupCollected(mouse, player, drowning ? 1 : 0, 1.0f, "bugdom2.mouse");
}

void Bugdom2Script_OnObjectiveComplete(int playerNum, int outcome)
{
	ObjNode* player;
	PangeaScriptObjectHandle handle;
	PangeaScriptPlayerEventContext context;

	if (playerNum != 0)
		return;
	player = gPlayerInfo.objNode;
	if (!player)
		return;
	handle = (PangeaScriptObjectHandle){(int)player->ScriptObjectID, player->ScriptObjectGeneration};
	if (player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0 || !PangeaScript_ObjectExists(handle))
	{
		player->ScriptObjectID = 0;
		player->ScriptObjectGeneration = 0;
		Bugdom2Script_RegisterPlayerObject(player);
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

Boolean Bugdom2Script_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget)
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
		.weaponId = "bugdom2.projectile",
		.weapon = {0},
		.target = {0},
		.position = target ? (PangeaScriptVector3){target->Coord.x, target->Coord.y, target->Coord.z} : (PangeaScriptVector3){0},
	};
	if (weapon && weapon->ScriptObjectID > 0 && weapon->ScriptObjectGeneration > 0)
		context.weapon = (PangeaScriptObjectHandle){weapon->ScriptObjectID, (uint32_t)weapon->ScriptObjectGeneration};
	if (target && target->ScriptObjectID > 0 && target->ScriptObjectGeneration > 0)
		context.target = (PangeaScriptObjectHandle){target->ScriptObjectID, (uint32_t)target->ScriptObjectGeneration};
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

void Bugdom2Script_OnDamageApplied(float damage, int cause)
{
	ObjNode* player = gPlayerInfo.objNode;
	PangeaScriptDamageContext context;
	PangeaScriptObjectHandle handle;
	if (!player)
		return;
	handle = Bugdom2Script_ObjectHandle(player);
	if (handle.id <= 0)
		return;
	context = (PangeaScriptDamageContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.cause = cause,
		.damage = damage,
		.source = {0},
		.target = handle,
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onDamageApplied", PangeaScript_CallDamageAppliedHook(&context));
}

void Bugdom2Script_OnDeath(int deathType)
{
	ObjNode* player = gPlayerInfo.objNode;
	PangeaScriptPlayerEventContext context;
	PangeaScriptObjectHandle handle;
	if (!player)
		return;
	handle = Bugdom2Script_ObjectHandle(player);
	if (handle.id <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = deathType,
		.player = handle,
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onDeath", PangeaScript_CallPlayerEvent(&context, "onDeath"));
}

void Bugdom2Script_OnPlayerSpawn(ObjNode* playerObj)
{
	PangeaScriptPlayerEventContext context;
	PangeaScriptObjectHandle handle;
	if (!playerObj)
		return;
	handle = Bugdom2Script_ObjectHandle(playerObj);
	if (handle.id <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.player = handle,
		.position = {playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z},
	};
	LogScriptStatus("onPlayerSpawn", PangeaScript_CallPlayerEvent(&context, "onPlayerSpawn"));
}

void Bugdom2Script_OnPlayerRespawn(ObjNode* playerObj)
{
	PangeaScriptPlayerEventContext context;
	PangeaScriptObjectHandle handle;
	if (!playerObj)
		return;
	handle = Bugdom2Script_ObjectHandle(playerObj);
	if (handle.id <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.player = handle,
		.position = {playerObj->Coord.x, playerObj->Coord.y, playerObj->Coord.z},
	};
	LogScriptStatus("onPlayerRespawn", PangeaScript_CallPlayerEvent(&context, "onPlayerRespawn"));
}

void Bugdom2Script_ApplyObjectScripting(ObjNode* obj)
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

void Bugdom2Script_RunObjectFrame(ObjNode* obj)
{
	Bugdom2Script_ApplyObjectScripting(obj);
	if (obj->CType == INVALID_NODE_FLAG)
		return;
	if (obj->ScriptDeleteRequested)
	{
		if ((obj->TerrainItemPtr || obj->SplineItemPtr) && !obj->ScriptStreamOutSent)
		{
			PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
			if (PangeaScript_ObjectExists(handle))
				(void) PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			obj->ScriptStreamOutSent = true;
		}
		DeleteObject(obj);
		return;
	}
	if (obj->Skeleton && obj->Skeleton->AnimHasStopped && !obj->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
		obj->ScriptAnimationCompletionSent = true;
	}
}

void Bugdom2Script_OnObjectDeleted(ObjNode* obj)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		if (!PangeaScript_ObjectExists(handle))
			return;
		if ((obj->TerrainItemPtr || obj->SplineItemPtr) && !obj->ScriptStreamOutSent)
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

static int ResolveDisplayGroup(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
		return GetCustomModelGroup(definition->modelPath);
	if (strcmp(definition->nativeGroup, "global") == 0)
		return MODEL_GROUP_GLOBAL;
	if (strcmp(definition->nativeGroup, "levelSpecific") == 0)
		return MODEL_GROUP_LEVELSPECIFIC;
	if (strcmp(definition->nativeGroup, "foliage") == 0)
		return MODEL_GROUP_FOLIAGE;
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

static Boolean ScriptedTriggerCallback(ObjNode* trigger, ObjNode* who, Byte sides)
{
	if (trigger && trigger->ScriptObjectID)
	{
		PangeaScriptObjectHandle handle = {trigger->ScriptObjectID, (uint32_t)trigger->ScriptObjectGeneration};
		PangeaScriptObjectHandle other = {0};
		if (!PangeaScript_ObjectExists(handle))
			return true;
		if (who && who->ScriptObjectID)
			other = (PangeaScriptObjectHandle){who->ScriptObjectID, (uint32_t)who->ScriptObjectGeneration};
		if (other.id > 0 && !PangeaScript_ObjectExists(other))
			other = (PangeaScriptObjectHandle){0};
		return PangeaScript_CallObjectTriggerWithOther(handle, &gScriptFrameContext, sides, true, other);
	}
	return true;
}

void Bugdom2Script_OnAnimationEvent(ObjNode* obj, int eventValue)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
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
	Bugdom2Script_RegisterObject(object, definition->id, "customObject");
	if (object->ScriptObjectID == 0)
	{
		DeleteObject(object);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	if (outHandle)
		*outHandle = (PangeaScriptObjectHandle){object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
	return PANGEA_SCRIPT_OK;
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "bugdom2.powerup",
		.nativeType = 35,
		.category = "powerup",
		.dependencySummary = "global powerup models and terrain/collision systems",
	},
	{
		.id = "bugdom2.dcell",
		.nativeType = 49,
		.category = "pickup",
		.dependencySummary = "global pickup models and terrain/collision systems",
	},
	{
		.id = "bugdom2.gliderPart",
		.nativeType = 85,
		.category = "pickup",
		.dependencySummary = "level-specific glider part models and terrain/collision systems",
	},
#define BUGDOM2_TERRAIN_NATIVE_ITEM(type) { .id = #type, .nativeType = type, .category = "terrain", .dependencySummary = "current level assets, terrain systems, and the native item initializer" },
	BUGDOM2_TERRAIN_NATIVE_ITEM(1) BUGDOM2_TERRAIN_NATIVE_ITEM(2) BUGDOM2_TERRAIN_NATIVE_ITEM(3) BUGDOM2_TERRAIN_NATIVE_ITEM(4) BUGDOM2_TERRAIN_NATIVE_ITEM(5) BUGDOM2_TERRAIN_NATIVE_ITEM(6) BUGDOM2_TERRAIN_NATIVE_ITEM(7) BUGDOM2_TERRAIN_NATIVE_ITEM(8) BUGDOM2_TERRAIN_NATIVE_ITEM(9) BUGDOM2_TERRAIN_NATIVE_ITEM(10)
	BUGDOM2_TERRAIN_NATIVE_ITEM(11) BUGDOM2_TERRAIN_NATIVE_ITEM(12) BUGDOM2_TERRAIN_NATIVE_ITEM(13) BUGDOM2_TERRAIN_NATIVE_ITEM(14) BUGDOM2_TERRAIN_NATIVE_ITEM(15) BUGDOM2_TERRAIN_NATIVE_ITEM(16) BUGDOM2_TERRAIN_NATIVE_ITEM(17) BUGDOM2_TERRAIN_NATIVE_ITEM(18) BUGDOM2_TERRAIN_NATIVE_ITEM(19) BUGDOM2_TERRAIN_NATIVE_ITEM(20)
	BUGDOM2_TERRAIN_NATIVE_ITEM(21) BUGDOM2_TERRAIN_NATIVE_ITEM(22) BUGDOM2_TERRAIN_NATIVE_ITEM(23) BUGDOM2_TERRAIN_NATIVE_ITEM(24) BUGDOM2_TERRAIN_NATIVE_ITEM(25) BUGDOM2_TERRAIN_NATIVE_ITEM(26) BUGDOM2_TERRAIN_NATIVE_ITEM(27) BUGDOM2_TERRAIN_NATIVE_ITEM(28) BUGDOM2_TERRAIN_NATIVE_ITEM(29) BUGDOM2_TERRAIN_NATIVE_ITEM(30)
	BUGDOM2_TERRAIN_NATIVE_ITEM(31) BUGDOM2_TERRAIN_NATIVE_ITEM(32) BUGDOM2_TERRAIN_NATIVE_ITEM(33) BUGDOM2_TERRAIN_NATIVE_ITEM(34) BUGDOM2_TERRAIN_NATIVE_ITEM(35) BUGDOM2_TERRAIN_NATIVE_ITEM(36) BUGDOM2_TERRAIN_NATIVE_ITEM(37) BUGDOM2_TERRAIN_NATIVE_ITEM(38) BUGDOM2_TERRAIN_NATIVE_ITEM(39) BUGDOM2_TERRAIN_NATIVE_ITEM(40)
	BUGDOM2_TERRAIN_NATIVE_ITEM(41) BUGDOM2_TERRAIN_NATIVE_ITEM(42) BUGDOM2_TERRAIN_NATIVE_ITEM(43) BUGDOM2_TERRAIN_NATIVE_ITEM(44) BUGDOM2_TERRAIN_NATIVE_ITEM(45) BUGDOM2_TERRAIN_NATIVE_ITEM(46) BUGDOM2_TERRAIN_NATIVE_ITEM(47) BUGDOM2_TERRAIN_NATIVE_ITEM(48) BUGDOM2_TERRAIN_NATIVE_ITEM(49) BUGDOM2_TERRAIN_NATIVE_ITEM(50)
	BUGDOM2_TERRAIN_NATIVE_ITEM(51) BUGDOM2_TERRAIN_NATIVE_ITEM(52) BUGDOM2_TERRAIN_NATIVE_ITEM(53) BUGDOM2_TERRAIN_NATIVE_ITEM(54) BUGDOM2_TERRAIN_NATIVE_ITEM(55) BUGDOM2_TERRAIN_NATIVE_ITEM(56) BUGDOM2_TERRAIN_NATIVE_ITEM(57) BUGDOM2_TERRAIN_NATIVE_ITEM(58) BUGDOM2_TERRAIN_NATIVE_ITEM(59) BUGDOM2_TERRAIN_NATIVE_ITEM(60)
	BUGDOM2_TERRAIN_NATIVE_ITEM(61) BUGDOM2_TERRAIN_NATIVE_ITEM(62) BUGDOM2_TERRAIN_NATIVE_ITEM(63) BUGDOM2_TERRAIN_NATIVE_ITEM(64) BUGDOM2_TERRAIN_NATIVE_ITEM(65) BUGDOM2_TERRAIN_NATIVE_ITEM(66) BUGDOM2_TERRAIN_NATIVE_ITEM(67) BUGDOM2_TERRAIN_NATIVE_ITEM(68) BUGDOM2_TERRAIN_NATIVE_ITEM(69) BUGDOM2_TERRAIN_NATIVE_ITEM(70)
	BUGDOM2_TERRAIN_NATIVE_ITEM(71) BUGDOM2_TERRAIN_NATIVE_ITEM(72) BUGDOM2_TERRAIN_NATIVE_ITEM(73) BUGDOM2_TERRAIN_NATIVE_ITEM(74) BUGDOM2_TERRAIN_NATIVE_ITEM(75) BUGDOM2_TERRAIN_NATIVE_ITEM(76) BUGDOM2_TERRAIN_NATIVE_ITEM(77) BUGDOM2_TERRAIN_NATIVE_ITEM(78) BUGDOM2_TERRAIN_NATIVE_ITEM(79) BUGDOM2_TERRAIN_NATIVE_ITEM(80)
	BUGDOM2_TERRAIN_NATIVE_ITEM(81) BUGDOM2_TERRAIN_NATIVE_ITEM(82) BUGDOM2_TERRAIN_NATIVE_ITEM(83) BUGDOM2_TERRAIN_NATIVE_ITEM(84) BUGDOM2_TERRAIN_NATIVE_ITEM(85)
#undef BUGDOM2_TERRAIN_NATIVE_ITEM
};

typedef struct Bugdom2NamedAsset
{
	const char* id;
	int nativeId;
} Bugdom2NamedAsset;

static const Bugdom2NamedAsset kSkeletonDependencies[] =
{
	{ "skipExplore", SKELETON_TYPE_SKIP_EXPLORE },
	{ "skipTunnel", SKELETON_TYPE_SKIP_TUNNEL },
	{ "skipTitle", SKELETON_TYPE_SKIP_TITLE },
	{ "snail", SKELETON_TYPE_SNAIL },
	{ "gnome", SKELETON_TYPE_GNOME },
	{ "houseFly", SKELETON_TYPE_HOUSEFLY },
	{ "evilPlant", SKELETON_TYPE_EVILPLANT },
	{ "chipmunk", SKELETON_TYPE_CHIPMUNK },
	{ "snakeHead", SKELETON_TYPE_SNAKEHEAD },
	{ "buddyBug", SKELETON_TYPE_BUDDYBUG },
	{ "checkpoint", SKELETON_TYPE_CHECKPOINT },
	{ "flea", SKELETON_TYPE_FLEA },
	{ "tick", SKELETON_TYPE_TICK },
	{ "mouseTrap", SKELETON_TYPE_MOUSETRAP },
	{ "mouse", SKELETON_TYPE_MOUSE },
	{ "toySoldier", SKELETON_TYPE_TOYSOLDIER },
	{ "otto", SKELETON_TYPE_OTTO },
	{ "bumbleBee", SKELETON_TYPE_BUMBLEBEE },
	{ "hoboBag", SKELETON_TYPE_HOBOBAG },
	{ "dragonfly", SKELETON_TYPE_DRAGONFLY },
	{ "frog", SKELETON_TYPE_FROG },
	{ "moth", SKELETON_TYPE_MOTH },
	{ "computerBug", SKELETON_TYPE_COMPUTERBUG },
	{ "roach", SKELETON_TYPE_ROACH },
	{ "ant", SKELETON_TYPE_ANT },
	{ "fish", SKELETON_TYPE_FISH },
};

static int FindNamedAsset(const Bugdom2NamedAsset* assets, int count, const char* id)
{
	if (!assets || !id)
		return -1;

	for (int i = 0; i < count; i++)
	{
		if (strcmp(assets[i].id, id) == 0)
			return assets[i].nativeId;
	}

	return -1;
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

	SDL_Log("Bugdom2 scripting %s failed: %s", action, PangeaScript_GetLastError());
}

static PangeaScriptStatus Bugdom2Script_SpawnNativeCallback(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	if (strcmp(id, "bugdom2.dcell") == 0)
	{
		if (gBG3DContainerList[GetBugdom2ItemModelGroup(PLAYROOM_ObjType_DCell)] == nil)
		{
			return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		}

		ObjNode* newObj;
		gNewObjectDefinition.genre = 0;
		gNewObjectDefinition.group = GetBugdom2ItemModelGroup(PLAYROOM_ObjType_DCell);
		gNewObjectDefinition.type = PLAYROOM_ObjType_DCell;
		gNewObjectDefinition.scale = 1.0;
		gNewObjectDefinition.coord.x = x;
		gNewObjectDefinition.coord.z = z;
		gNewObjectDefinition.coord.y = y;
		gNewObjectDefinition.flags = gAutoFadeStatusBits;
		gNewObjectDefinition.slot = 358;
		gNewObjectDefinition.moveCall = MoveStaticObject;
		gNewObjectDefinition.rot = RandomFloat() * PI2;
		newObj = MakeNewDisplayGroupObject(&gNewObjectDefinition);
		if (!newObj)
			return PANGEA_SCRIPT_RUNTIME_ERROR;

		newObj->TerrainItemPtr = NULL;
		newObj->CType = CTYPE_MISC | CTYPE_BLOCKCAMERA | CTYPE_BLOCKSHADOW;
		newObj->CBits = CBITS_ALLSOLID;
		CreateCollisionBoxFromBoundingBox(newObj, 1, 1);

		Bugdom2Script_RegisterObject(newObj, "bugdom2.dcell", "pickup");

		if (outHandle)
		{
			outHandle->id = newObj->ScriptObjectID;
			outHandle->generation = (uint32_t) newObj->ScriptObjectGeneration;
		}
		return PANGEA_SCRIPT_OK;
	}

	if (strcmp(id, "bugdom2.powerup") == 0)
	{
		int powKind = params[0] >= 0 ? params[0] : 0;
		OGLPoint3D where = { x, y, z };
		ObjNode* pow = MakePOW(powKind, &where);
		if (!pow)
			return PANGEA_SCRIPT_RUNTIME_ERROR;

		pow->TerrainItemPtr = NULL;
		Bugdom2Script_RegisterObject(pow, "bugdom2.powerup", "powerup");

		if (outHandle)
		{
			outHandle->id = pow->ScriptObjectID;
			outHandle->generation = (uint32_t) pow->ScriptObjectGeneration;
		}
		return PANGEA_SCRIPT_OK;
	}

	TerrainItemEntryType* item = AcquireScriptTerrainItem();
	if (!item) return PANGEA_SCRIPT_RUNTIME_ERROR;
	item->x = (uint32_t) x; item->y = (uint32_t) z;
	for (int i = 0; i < 4; i++) item->parm[i] = (Byte) params[i];
	if (!Bugdom2SpawnTerrainItem(PangeaScript_ResolveNativeItemType(id), item, x, z))
	{
		gScriptTerrainItemOccupied[item - gScriptTerrainItems] = false;
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
	{
		if (node->TerrainItemPtr != item) continue;
		Bugdom2Script_RegisterObject(node, "bugdom2.terrain-item", "terrain-item");
		gScriptTerrainItemReclaimable[item - gScriptTerrainItems] = true;
		if (outHandle) *outHandle = (PangeaScriptObjectHandle){node->ScriptObjectID, node->ScriptObjectGeneration};
		break;
	}
	return PANGEA_SCRIPT_OK;
}

static int GetScriptPlayerCount(void) { return gPlayerInfo.objNode ? 1 : 0; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE int Bugdom2Script_ProbeBuddyLaunchJS(void)
{
	int previousCount;
	OGLPoint3D where;

	if (!gPlayerInfo.objNode)
		return 0;
	previousCount = gPlayerInfo.numBuddyBugs;
	where = gPlayerInfo.coord;
	CreateMyBuddy(&where);
	return gPlayerInfo.numBuddyBugs > previousCount ? 1 : 0;
}
#endif

static bool GetScriptPlayer(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (playerNum != 0 || !outPlayer || !gPlayerInfo.objNode) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {gPlayerInfo.coord.x, gPlayerInfo.coord.y, gPlayerInfo.coord.z}, .collisionEnabled = gPlayerInfo.objNode->CType != 0 && (gPlayerInfo.objNode->StatusBits & STATUS_BIT_NOCOLLISION) == 0, .hasCollisionEnabled = true, .health = gPlayerInfo.health, .hasHealth = true, .score = (int64_t) gScore, .hasScore = true, .lives = gPlayerInfo.lives, .hasLives = true, .keyCount = 0, .hasKeyState = true, .greenCloverCount = gPlayerInfo.numGreenClovers, .blueCloverCount = gPlayerInfo.numBlueClovers, .goldCloverCount = gPlayerInfo.numGoldClovers, .hasCollectibleState = true, .shieldActive = gPlayerInfo.shieldTimer > 0.0f, .hasShieldState = true, .miceRescued = gPlayerInfo.numMiceRescued, .miceTotal = gNumMice, .drowningMiceRescued = gNumDrowningMiceRescued, .drowningMiceRequired = gNumDrowingMiceToRescue, .hasMiceState = true, .childObjectCount = gPlayerInfo.numBuddyBugs, .hasChildObjectState = true, .active = true};
	outPlayer->camera = (PangeaScriptVector3){gGameView.cameraPlacement.cameraLocation.x, gGameView.cameraPlacement.cameraLocation.y, gGameView.cameraPlacement.cameraLocation.z};
	outPlayer->hasCameraState = true;
	outPlayer->rotation = (PangeaScriptVector3){gPlayerInfo.objNode->Rot.x, gPlayerInfo.objNode->Rot.y, gPlayerInfo.objNode->Rot.z};
	outPlayer->hasRotation = true;
	outPlayer->aim = (PangeaScriptVector3){-sinf(gPlayerInfo.objNode->Rot.y), 0.0f, -cosf(gPlayerInfo.objNode->Rot.y)};
	outPlayer->hasAimState = true;
	outPlayer->velocity = (PangeaScriptVector3){gPlayerInfo.objNode->Delta.x, gPlayerInfo.objNode->Delta.y, gPlayerInfo.objNode->Delta.z};
	outPlayer->hasVelocity = true;
	for (int keyID = 0; keyID < NUM_KEY_TYPES && keyID < PANGEA_SCRIPT_PLAYER_KEY_CAPACITY; keyID++)
		if (gPlayerInfo.hasKey[keyID]) outPlayer->keys[outPlayer->keyCount++] = keyID;
	return true;
}

static PangeaScriptStatus SetScriptPlayerHealth(int playerNum, float health)
{
	if (playerNum != 0 || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo.health = health;
	gPlayerInfo.objNode->Health = health;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerLives(int playerNum, int lives)
{
	if (playerNum != 0 || lives < 0 || lives > 255 || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo.lives = (Byte) lives;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerScore(int playerNum, int64_t score)
{
	if (playerNum != 0 || score < 0 || score > UINT32_MAX || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gScore = (uint32_t) score;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerKey(int playerNum, int keyId, bool enabled)
{
	if (playerNum != 0 || keyId < 0 || keyId >= NUM_KEY_TYPES || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo.hasKey[keyId] = enabled;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerCloverCount(int playerNum, int color, int count)
{
	if (playerNum != 0 || color < 0 || color > 2 || count < 0 || count > 999 || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (color == 0) gPlayerInfo.numGreenClovers = (short) count;
	else if (color == 1) gPlayerInfo.numBlueClovers = (short) count;
	else gPlayerInfo.numGoldClovers = (short) count;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerShieldActive(int playerNum, bool active)
{
	if (playerNum != 0 || !gPlayerInfo.objNode) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo.shieldTimer = active ? 15.0f : 0.0f;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerInvulnerable(int playerNum, float durationSeconds)
{
	if (playerNum != 0 || durationSeconds < 0.0f || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo.invincibilityTimer = durationSeconds;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerPosition(int playerNum, const PangeaScriptVector3* position)
{
	if (playerNum != 0 || !position || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo.coord = (OGLPoint3D){position->x, position->y, position->z};
	gPlayerInfo.objNode->Coord = gPlayerInfo.coord;
	UpdateObjectTransforms(gPlayerInfo.objNode);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerVelocity(int playerNum, const PangeaScriptVector3* velocity)
{
	if (playerNum != 0 || !velocity || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	return Bugdom2Script_SetObjectVelocity(gPlayerInfo.objNode, velocity) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
}

void Bugdom2Script_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "Bugdom2-Android",
		.gameName = "Bugdom 2",
		.spawnNative = Bugdom2Script_SpawnNativeCallback,
		.spawnScripted = SpawnScriptedObject,
		.getPlayerCount = GetScriptPlayerCount,
		.getPlayer = GetScriptPlayer,
		.setPlayerHealth = SetScriptPlayerHealth,
		.setPlayerLives = SetScriptPlayerLives,
		.setPlayerScore = SetScriptPlayerScore,
		.setPlayerKey = SetScriptPlayerKey,
		.setPlayerCloverCount = SetScriptPlayerCloverCount,
		.setPlayerShieldActive = SetScriptPlayerShieldActive,
		.setPlayerInvulnerable = SetScriptPlayerInvulnerable,
		.setPlayerPosition = SetScriptPlayerPosition,
		.setPlayerVelocity = SetScriptPlayerVelocity,
		.capabilities = PANGEA_SCRIPT_BUGDOM2_CAPABILITIES,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	Bugdom2Script_ResetObjectRegistry();
	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	if (status == PANGEA_SCRIPT_OK)
		LogScriptStatus("onGameStart", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_START, &(PangeaScriptLevelContext){0}));
}

void Bugdom2Script_Shutdown(void)
{
	LogScriptStatus("onGameShutdown", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN, &(PangeaScriptLevelContext){0}));
	Bugdom2Script_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void Bugdom2Script_LoadLevelConfig(int levelNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(levelNum);
	LogScriptStatus("level config load", status);
}

void Bugdom2Script_LoadLevelAssetDependencies(int levelNum)
{
	int dependencyCount = PangeaScript_GetLevelAssetDependencyCount();

	for (int i = 0; i < dependencyCount; i++)
	{
		PangeaScriptAssetDependency dependency;
		if (!PangeaScript_GetLevelAssetDependency(i, &dependency))
			continue;

		if (strcmp(dependency.kind, "skeleton") == 0)
		{
			int skeletonType = FindNamedAsset(kSkeletonDependencies, (int)(sizeof(kSkeletonDependencies) / sizeof(kSkeletonDependencies[0])), dependency.id);
			if (skeletonType < 0)
			{
				SDL_Log("Bugdom2 scripting asset dependency ignored: unknown skeleton '%s' for level %d", dependency.id, levelNum);
				continue;
			}
			LoadASkeleton((Byte)skeletonType);
			continue;
		}

		if (strcmp(dependency.kind, "modelGroup") == 0 && strcmp(dependency.id, "foliage") == 0)
		{
			if (gBG3DContainerList[MODEL_GROUP_FOLIAGE] == nil)
				LoadFoliage();
			continue;
		}

		if (strcmp(dependency.kind, "modelGroup") == 0 && strcmp(dependency.id, "global") == 0)
		{
			continue;
		}

		SDL_Log("Bugdom2 scripting asset dependency ignored: unsupported %s '%s' for level %d", dependency.kind, dependency.id, levelNum);
	}
}

static const char* Bugdom2Script_LevelName(int levelNum)
{
	static const char* levelNames[] =
	{
		"gnomegarden", "sidewalk", "fido", "plumbing", "playroom",
		"closet", "gutter", "garbage", "balsa", "park",
	};
	if (levelNum < 0 || levelNum >= NUM_LEVELS) return NULL;
	return levelNames[levelNum];
}

static void CallLevelHook(PangeaScriptHook hook, int levelNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = levelNum,
		.levelName = Bugdom2Script_LevelName(levelNum),
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

void Bugdom2Script_OnLevelLoad(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, levelNum, "onLevelLoad");
}

void Bugdom2Script_OnLevelStart(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, levelNum, "onLevelStart");
}

void Bugdom2Script_OnCheckpointReset(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(
		&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET);
}

void Bugdom2Script_OnCheckpointReached(int checkpointNum)
{
	PangeaScriptPlayerEventContext context;
	ObjNode* player = gPlayerInfo.objNode;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gScriptFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = checkpointNum,
		.player = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onCheckpointReached", PangeaScript_CallPlayerEvent(&context, "onCheckpointReached"));
}

void Bugdom2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.levelName = Bugdom2Script_LevelName(levelNum),
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	Bugdom2Script_CacheFrameContext(&context);
	PangeaScript_ExpireTriggerContacts(&context);
	(void)PangeaScript_ApplyDeferredActions(&context);

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onFrame", status);
}

void Bugdom2Script_OnLevelComplete(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, levelNum, "onLevelComplete");
}

void Bugdom2Script_OnLevelUnload(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, levelNum, "onLevelUnload");
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gScriptFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	PangeaScript_ResetObjects();
}

int Bugdom2Script_RemapTerrainItemType(int levelNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(levelNum, itemType);
}

Boolean Bugdom2Script_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z)
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

Boolean Bugdom2Script_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum)
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

Boolean Bugdom2Script_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z)
{
	const PangeaScriptTerrainReplacement* replacement = PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	void* nativeObject = NULL;
	const float y = GetTerrainY(x, z);
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = y, .z = z};
	if (!replacement)
		return false;
	if (PangeaScript_FindObjectBySource(&source, &handle))
		return true;
	status = SpawnScriptedObject(replacement->customObjectId, x, y, z, &handle);
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

int Bugdom2Script_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z)
{
	static TerrainItemEntryType probeItem;
	PangeaScriptObjectHandle handle = {0};
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = GetTerrainY(x, z), .z = z};
	memset(&probeItem, 0, sizeof(probeItem));
	if (!PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!Bugdom2Script_TryReplaceTerrainItem(&probeItem, itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_FindObjectBySource(&source, &handle) || handle.id <= 0 || handle.generation == 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_DeleteObject(handle))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PANGEA_SCRIPT_OK;
}

EMSCRIPTEN_KEEPALIVE int Bugdom2Script_ProbeFirstSplineJS(void)
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
		return Bugdom2Script_OnSplineItem(&spline->itemList[0], gLevelNum, splineNum) ? 1 : 0;
	}
	PangeaScript_ClearLastError();
	return -1;
}

EMSCRIPTEN_KEEPALIVE int Bugdom2Script_ProbeFirstSplineReplacementJS(void)
{
	PangeaScriptObjectHandle handle = {0};
	void* nativeObject = NULL;
	if (!gSplineList)
		return -1;
	for (int splineNum = 0; splineNum < gNumSplines; splineNum++)
	{
		SplineDefType* spline = &gSplineList[splineNum];
		float x;
		float z;
		if (!spline->itemList || spline->numItems <= 0)
			continue;
		if (!Bugdom2Script_TryReplaceSplineItem(&spline->itemList[0], splineNum, 0))
			return 0;
		GetCoordOnSpline(spline, spline->itemList[0].placement, &x, &z);
		if (!PangeaScript_FindObjectBySource(
			&(PangeaScriptObjectSource){
				.kind = PANGEA_SCRIPT_SOURCE_SPLINE,
				.itemIndex = 0,
				.nativeType = spline->itemList[0].type,
				.splineNum = splineNum,
				.x = x,
				.y = GetTerrainY(x, z),
				.z = z,
				.placement = spline->itemList[0].placement,
			}, &handle))
			return 0;
		if (!PangeaScript_GetObjectNativeObject(handle, &nativeObject) || !nativeObject)
			return 0;
		RemoveFromSplineObjectList((ObjNode*)nativeObject);
		if (!PangeaScript_DeleteObject(handle))
			return 0;
		return 1;
	}
	return -1;
}

Boolean Bugdom2Script_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex)
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
	const float y = GetTerrainY(x, z);
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_SPLINE, .itemIndex = itemIndex, .nativeType = itemPtr->type,
		.splineNum = splineNum, .x = x, .y = y, .z = z, .placement = itemPtr->placement};
	if (PangeaScript_FindObjectBySource(&source, &handle))
		return true;
	status = SpawnScriptedObject(replacement->customObjectId, x, y, z, &handle);
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

int Bugdom2Script_ProbeSplineReplacementJS(int splineNum, int itemIndex, int nativeType, float placement)
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
	if (!Bugdom2Script_TryReplaceSplineItem(&probeItem, splineNum, itemIndex))
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

int Bugdom2Script_SelectSplineItemForReplacementJS(void)
{
	gSelectedSplineNum = -1;
	gSelectedSplineItemIndex = -1;
	gSelectedSplineNativeType = -1;
	gSelectedSplinePlacement = 0.0f;
	if (!gSplineList)
		return -1;
	for (int splineNum = 0; splineNum < gNumSplines; splineNum++)
	{
		SplineDefType* spline = &gSplineList[splineNum];
		if (!spline->itemList || spline->numItems <= 0)
			continue;
		gSelectedSplineNum = splineNum;
		gSelectedSplineItemIndex = 0;
		gSelectedSplineNativeType = spline->itemList[0].type;
		gSelectedSplinePlacement = spline->itemList[0].placement;
		return 0;
	}
	return -1;
}

int Bugdom2Script_GetSelectedSplineItemFieldJS(int field)
{
	switch (field)
	{
		case 0: return gSelectedSplineNum;
		case 1: return gSelectedSplineItemIndex;
		case 2: return gSelectedSplineNativeType;
		default: return -1;
	}
}

float Bugdom2Script_GetSelectedSplinePlacementJS(void)
{
	return gSelectedSplinePlacement;
}

EMSCRIPTEN_KEEPALIVE int Bugdom2Script_ProbeSaveLoadJS(int saveSlot)
{
	int previousLevel;
	Boolean saved;
	Boolean loaded;
	if (saveSlot < 0)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	previousLevel = gLevelNum;
	gLevelNum = 0;
	saved = SaveGame(saveSlot);
	loaded = saved && LoadSavedGame(saveSlot);
	gLevelNum = previousLevel;
	if (!loaded)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PANGEA_SCRIPT_OK;
}

#endif

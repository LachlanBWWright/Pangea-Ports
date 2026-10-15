#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"
#include "pangea_presentation_game.h"

#include "structs.h"
#include "splineitems.h"
#include "checkpoints.h"
#include "triggers.h"
#include "pangea_net.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif


extern bool PangeaScript_LoadCustomBG3D(FSSpec* spec, int group);
extern bool PangeaScript_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec);
static int GetScriptSkeletonFrame(const SkeletonObjDataType* skeleton);

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;
static float gScriptSimulationTime;
static uint32_t gScriptPreviousControlBits[MAX_PLAYERS];
static uint32_t gScriptReleasedControlBits[MAX_PLAYERS];

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE int CroMagScript_ProbeRaceCompletionJS(void)
{
	PlayerCompletedRace(0);
	return 0;
}

EMSCRIPTEN_KEEPALIVE int CroMagScript_ProbeRaceProgressJS(void)
{
	CroMagScript_OnCheckpointReached(0, 1);
	CroMagScript_OnLapComplete(0, 1);
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
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG ||
		!isfinite(position->x) || !isfinite(position->y) || !isfinite(position->z))
		return false;

	PangeaScriptVector3 offset = {position->x - obj->Coord.x, position->y - obj->Coord.y, position->z - obj->Coord.z};
	if (!isfinite(offset.x) || !isfinite(offset.y) || !isfinite(offset.z)) return false;
	for (int i = 0; obj->CollisionBoxes && i < obj->NumCollisionBoxes; i++)
	{
		const CollisionBoxType* box = &obj->CollisionBoxes[i];
		long double coordinates[] = {(long double)box->left + offset.x, (long double)box->right + offset.x,
			(long double)box->bottom + offset.y, (long double)box->top + offset.y,
			(long double)box->back + offset.z, (long double)box->front + offset.z};
		for (int axis = 0; axis < 6; axis++)
			if (coordinates[axis] < LONG_MIN || coordinates[axis] > LONG_MAX) return false;
	}
	for (int i = 0; obj->CollisionBoxes && i < obj->NumCollisionBoxes; i++)
	{
		CollisionBoxType* box = &obj->CollisionBoxes[i];
		box->left = (long)((long double)box->left + offset.x);
		box->right = (long)((long double)box->right + offset.x);
		box->bottom = (long)((long double)box->bottom + offset.y);
		box->top = (long)((long double)box->top + offset.y);
		box->back = (long)((long double)box->back + offset.z);
		box->front = (long)((long double)box->front + offset.z);
	}
	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	for (int player = 0; player < gNumTotalPlayers; player++)
	{
		if (gPlayerInfo[player].objNode != obj) continue;
		gPlayerInfo[player].coord = obj->Coord;
		if (offset.x != 0 || offset.y != 0 || offset.z != 0) obj->ScriptContactStateInvalidated = true;
	}
	UpdateObjectTransforms(obj);
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
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG ||
		!isfinite(velocity->x) || !isfinite(velocity->y) || !isfinite(velocity->z))
		return false;
	float speed2D = hypotf(velocity->x, velocity->z);
	float speed3D = hypotf(speed2D, velocity->y);
	if (!isfinite(speed3D)) return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	obj->Speed2D = speed2D;
	obj->Speed3D = speed3D;
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
	if (!obj || !rotation || obj->CType == INVALID_NODE_FLAG ||
		!isfinite(rotation->x) || !isfinite(rotation->y) || !isfinite(rotation->z)) return false;
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
	if (!obj || obj->CType == INVALID_NODE_FLAG || !isfinite(scale) || scale <= 0.0f) return false;
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

static bool CroMagScript_GetObjectHealth(void* nativeObject, float* outHealth)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outHealth || obj->CType == INVALID_NODE_FLAG)
		return false;
	*outHealth = obj->Health;
	for (int i = 0; i < gNumTotalPlayers; i++)
		if (gPlayerInfo[i].objNode == obj) *outHealth = gPlayerInfo[i].health;
	return true;
}

static bool CroMagScript_SetObjectHealth(void* nativeObject, float health)
{
	ObjNode* obj = nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG || !isfinite(health) || health < 0) return false;
	obj->Health = health;
	for (int i = 0; i < gNumTotalPlayers; i++)
		if (gPlayerInfo[i].objNode == obj) gPlayerInfo[i].health = health;
	return true;
}

static bool CroMagScript_SetObjectActive(void* nativeObject, bool active)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG) return false;
	obj->ScriptActiveStateInitialized = true;
	obj->ScriptActive = active;
	if (active) obj->StatusBits &= ~(STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE);
	else obj->StatusBits |= STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE;
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

static bool CroMagScript_GetObjectAnimationSpeed(void* nativeObject, float* outSpeed)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outSpeed || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG) return false;
	*outSpeed = obj->Skeleton->AnimSpeed;
	return true;
}

static bool CroMagScript_GetObjectAnimationFrame(void* nativeObject, int* outFrame)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outFrame || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG) return false;
	*outFrame = GetScriptSkeletonFrame(obj->Skeleton);
	return true;
}

static bool CroMagScript_SetObjectAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || !obj->Skeleton || !obj->Skeleton->skeletonDefinition ||
		obj->CType == INVALID_NODE_FLAG || animation < 0 ||
		animation >= obj->Skeleton->skeletonDefinition->NumAnims ||
		!isfinite(speed) || speed < 0 || !isfinite(blendSeconds) || blendSeconds < 0) return false;
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

static bool GetScriptJointPosition(void* nativeObject, int joint, PangeaScriptVector3* output)
{
	ObjNode* node = nativeObject;
	if (!node || !output || node->CType == INVALID_NODE_FLAG || !node->Skeleton ||
		!node->Skeleton->skeletonDefinition || joint < 0 ||
		joint >= node->Skeleton->skeletonDefinition->NumBones) return false;
	OGLPoint3D local = {0, 0, 0}, world;
	FindCoordOnJoint(node, joint, &local, &world);
	*output = (PangeaScriptVector3){world.x, world.y, world.z};
	return true;
}

static bool GetScriptObjectBounds(void* nativeObject, PangeaScriptBounds* output)
{
	ObjNode* obj = nativeObject;
	if (!obj || !output || obj->CType == INVALID_NODE_FLAG || !obj->CollisionBoxes || obj->NumCollisionBoxes <= 0)
		return false;
	for (int i = 0; i < obj->NumCollisionBoxes; i++)
	{
		const CollisionBoxType* box = &obj->CollisionBoxes[i];
		if (box->left > box->right || box->bottom > box->top || box->back > box->front) return false;
		if (i == 0)
			*output = (PangeaScriptBounds){{box->left, box->bottom, box->back}, {box->right, box->top, box->front}};
		else
		{
			output->min.x = fminf(output->min.x, box->left);
			output->min.y = fminf(output->min.y, box->bottom);
			output->min.z = fminf(output->min.z, box->back);
			output->max.x = fmaxf(output->max.x, box->right);
			output->max.y = fmaxf(output->max.y, box->top);
			output->max.z = fmaxf(output->max.z, box->front);
		}
	}
	return true;
}

static const PangeaScriptObjectOps kCroMagPlayerObjectOps =
{
	.getPosition = CroMagScript_GetObjectPosition,
	.getVelocity = CroMagScript_GetObjectVelocity,
	.getRotation = CroMagScript_GetObjectRotation,
	.getScale = CroMagScript_GetObjectScale,
	.getAnimation = CroMagScript_GetObjectAnimation,
	.getAnimationSpeed = CroMagScript_GetObjectAnimationSpeed,
	.getAnimationFrame = CroMagScript_GetObjectAnimationFrame,
	.getActive = CroMagScript_GetObjectActive,
	.getHealth = CroMagScript_GetObjectHealth,
	.getJointPosition = GetScriptJointPosition,
	.getBounds = GetScriptObjectBounds,
	.setHealth = CroMagScript_SetObjectHealth,
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

	if (!obj) return;
	for (int i = 0; i < gNumTotalPlayers; i++)
	{
		ObjNode* car = gPlayerInfo[i].objNode;
		if (car && (ObjNode*)car->CapturedFlag == obj) car->CapturedFlag = nil;
		if (car == obj && car->CapturedFlag) PlayerResetCarriedFlag(car);
	}
	if (obj->ScriptObjectID == 0)
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
		return CroMagScript_NativeRulesEnabled();
	pickupHandle = (PangeaScriptObjectHandle){pickup->ScriptObjectID, (uint32_t) pickup->ScriptObjectGeneration};
	playerHandle = (PangeaScriptObjectHandle){player->ScriptObjectID, (uint32_t) player->ScriptObjectGeneration};
	if (!PangeaScript_ObjectExists(pickupHandle) || !PangeaScript_ObjectExists(playerHandle))
		return CroMagScript_NativeRulesEnabled();
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
		return CroMagScript_NativeRulesEnabled();
	return CroMagScript_NativeRulesEnabled() && (result.hasConsumePickup ? result.consumePickup : true);
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
	void* liveObject = NULL;
	if (!PangeaScript_GetObjectNativeObject(handle, &liveObject) || liveObject != obj)
	{
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
	if (obj->CollisionBoxes && obj->NumCollisionBoxes == 1) CalcObjectBoxFromNode(obj);
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
	PangeaScriptObjectHandle saved = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
	if (!obj->ScriptDeleteRequested)
		CroMagScript_ApplyObjectScripting(obj);
	void* liveObject = NULL;
	if (!PangeaScript_GetObjectNativeObject(saved, &liveObject) || liveObject != obj) return;
	if (obj->CType == INVALID_NODE_FLAG) return;
	if (obj->ScriptDeleteRequested) { DeleteObject(obj); return; }
	if (obj->Skeleton && obj->Skeleton->AnimHasStopped && !obj->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		obj->ScriptAnimationCompletionSent = true;
		if (PangeaScript_ObjectExists(handle))
			(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
	}
}

void CroMagScript_OnObjectDeleted(ObjNode* obj)
{
	if (!obj) return;
	for (int i = 0; i < gNumTotalPlayers; i++)
	{
		if (gPlayerInfo[i].objNode == obj)
		{
			if (obj->CapturedFlag) PlayerResetCarriedFlag(obj);
			gPlayerInfo[i].objNode = NULL;
		}
		if (gPlayerInfo[i].headObj == obj) gPlayerInfo[i].headObj = NULL;
		for (int wheel = 0; wheel < 4; wheel++)
			if (gPlayerInfo[i].wheelObj[wheel] == obj) gPlayerInfo[i].wheelObj[wheel] = NULL;
	}
	for (int i = 0; i < gNumTorches; i++)
	{
		if (gTorchObjs[i] != obj) continue;
		for (int next = i + 1; next < gNumTorches; next++) gTorchObjs[next - 1] = gTorchObjs[next];
		gTorchObjs[--gNumTorches] = NULL;
		break;
	}
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

static bool GetScriptCollisionExtent(float value, short* output)
{
	if (!isfinite(value) || value <= 0 || value > SHRT_MAX) return false;
	*output = (short)ceilf(value);
	return true;
}

static bool ApplyScriptedCollision(ObjNode* object, const PangeaScriptCustomObjectDefinition* definition)
{
	PangeaScriptCollisionPreset preset = definition->collisionPreset;
	short halfWidth;
	short halfHeight;
	short halfDepth;
	if (preset == PANGEA_SCRIPT_COLLISION_NONE) return true;
	if (!GetScriptCollisionExtent(definition->collisionBoundsSet ? definition->collisionWidth * 0.5f : object->BoundingSphereRadius, &halfWidth) ||
		!GetScriptCollisionExtent(definition->collisionBoundsSet ? definition->collisionHeight * 0.5f : object->BoundingSphereRadius, &halfHeight) ||
		!GetScriptCollisionExtent(definition->collisionBoundsSet ? definition->collisionDepth * 0.5f : object->BoundingSphereRadius, &halfDepth)) return false;
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
	return true;
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
	if (!ApplyScriptedCollision(object, definition)) { DeleteObject(object); return PANGEA_SCRIPT_BAD_ARGUMENT; }
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
	{
		.id = "cromag.waterPatch",
		.nativeType = 2,
		.category = "hazard",
		.dependencySummary = "water-patch assets, track, and native vehicle water behavior",
	},
	{
		.id = "cromag.campFire",
		.nativeType = 17,
		.category = "hazard",
		.dependencySummary = "campfire assets, track, and native hazard behavior",
	},
	{
		.id = "cromag.seaMine",
		.nativeType = 57,
		.category = "hazard",
		.dependencySummary = "sea-mine assets, track, and native hazard collision",
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

static int GetScriptSkeletonFrame(const SkeletonObjDataType* skeleton)
{
	const JointKeyFrameHeader* header;
	int frame;
	int frameCount;

	if (!skeleton || !skeleton->skeletonDefinition || skeleton->AnimNum >= MAX_ANIMS)
		return 0;
	header = &skeleton->skeletonDefinition->JointKeyframes[0];
	frameCount = header->numKeyFrames[skeleton->AnimNum];
	if (frameCount <= 0 || !header->keyFrames || !header->keyFrames[skeleton->AnimNum])
		return 0;
	frame = 0;
	while (frame + 1 < frameCount && skeleton->CurrentAnimTime >= header->keyFrames[skeleton->AnimNum][frame + 1].tick)
		frame++;
	return frame;
}

static bool GetScriptPlayer(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (!outPlayer || playerNum < 0 || playerNum >= gNumTotalPlayers || !gPlayerInfo[playerNum].objNode) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){
		.position = {gPlayerInfo[playerNum].coord.x, gPlayerInfo[playerNum].coord.y, gPlayerInfo[playerNum].coord.z},
		.velocity = {gPlayerInfo[playerNum].objNode->Delta.x, gPlayerInfo[playerNum].objNode->Delta.y, gPlayerInfo[playerNum].objNode->Delta.z},
		.hasVelocity = true,
		.grounded = (gPlayerInfo[playerNum].objNode->StatusBits & STATUS_BIT_ONGROUND) != 0,
		.hasGroundedState = !gPlayerInfo[playerNum].objNode->ScriptContactStateInvalidated,
		.heightOffGround = gPlayerInfo[playerNum].distToFloor,
		.hasTerrainHeightState = !gPlayerInfo[playerNum].objNode->ScriptContactStateInvalidated,
		.onWater = gPlayerInfo[playerNum].onWater,
		.hasWaterState = !gPlayerInfo[playerNum].objNode->ScriptContactStateInvalidated,
		.collisionEnabled = gPlayerInfo[playerNum].objNode->CType != 0 && (gPlayerInfo[playerNum].objNode->StatusBits & STATUS_BIT_NOCOLLISION) == 0,
		.hasCollisionEnabled = true,
		.health = gPlayerInfo[playerNum].health,
		.hasHealth = true,
		.dead = gPlayerInfo[playerNum].isEliminated,
		.hasDeathState = true,
		.lapNum = gPlayerInfo[playerNum].lapNum,
		.checkpointNum = gPlayerInfo[playerNum].checkpointNum,
		.placement = gPlayerInfo[playerNum].place,
		.raceComplete = gPlayerInfo[playerNum].raceComplete,
		.hasRaceState = IsRaceMode(),
		.levelComplete = gPlayerInfo[playerNum].raceComplete,
		.hasLevelCompletionState = IsRaceMode(),
		.vehicleType = gPlayerInfo[playerNum].vehicleType,
		.vehicleMaxSpeed = gPlayerInfo[playerNum].carStats.maxSpeed,
		.vehicleAcceleration = gPlayerInfo[playerNum].carStats.acceleration,
		.vehicleTraction = gPlayerInfo[playerNum].carStats.tireTraction,
		.vehicleSuspension = gPlayerInfo[playerNum].carStats.suspension,
		.groundTraction = gPlayerInfo[playerNum].groundTraction,
		.groundFriction = gPlayerInfo[playerNum].groundFriction,
		.groundSteering = gPlayerInfo[playerNum].groundSteering,
		.groundAcceleration = gPlayerInfo[playerNum].groundAcceleration,
		.hasVehicleState = true,
		.hasGroundPhysicsState = !gPlayerInfo[playerNum].objNode->ScriptContactStateInvalidated,
		.hasGroundSteeringState = !gPlayerInfo[playerNum].objNode->ScriptContactStateInvalidated,
		.nitroActive = gPlayerInfo[playerNum].nitroTimer > 0.0f,
		.nitroTimeRemaining = gPlayerInfo[playerNum].nitroTimer,
		.greasedTiresActive = gPlayerInfo[playerNum].greasedTiresTimer > 0.0f,
		.greasedTiresTimeRemaining = gPlayerInfo[playerNum].greasedTiresTimer,
		.stickyTiresActive = gPlayerInfo[playerNum].stickyTiresTimer > 0.0f,
		.stickyTiresTimeRemaining = gPlayerInfo[playerNum].stickyTiresTimer,
		.superSuspensionActive = gPlayerInfo[playerNum].superSuspensionTimer > 0.0f,
		.superSuspensionTimeRemaining = gPlayerInfo[playerNum].superSuspensionTimer,
		.invisibilityActive = gPlayerInfo[playerNum].invisibilityTimer > 0.0f,
		.invisibilityTimeRemaining = gPlayerInfo[playerNum].invisibilityTimer,
		.frozen = gPlayerInfo[playerNum].frozenTimer > 0.0f,
		.frozenTimeRemaining = gPlayerInfo[playerNum].frozenTimer,
		.flaming = gPlayerInfo[playerNum].flamingTimer > 0.0f,
		.flamingTimeRemaining = gPlayerInfo[playerNum].flamingTimer,
		.submarineImmobilized = gPlayerInfo[playerNum].submarineImmobilized > 0.0f,
		.submarineImmobilizedTimeRemaining = gPlayerInfo[playerNum].submarineImmobilized,
		.planing = gPlayerInfo[playerNum].isPlaning,
		.hasVehicleModifierState = true,
		.team = gPlayerInfo[playerNum].team,
		.hasTeamState = gGameMode == GAME_MODE_CAPTUREFLAG,
		.carryingFlag = gPlayerInfo[playerNum].objNode->CapturedFlag != NULL,
		.captureScore = gCapturedFlagCount[gPlayerInfo[playerNum].team & 1],
		.hasCaptureState = gGameMode == GAME_MODE_CAPTUREFLAG,
		.tagged = gPlayerInfo[playerNum].isIt,
		.tagTimeRemaining = gPlayerInfo[playerNum].tagTimer,
		.hasTagState = gGameMode == GAME_MODE_TAG1 || gGameMode == GAME_MODE_TAG2,
		.camera = {gPlayerInfo[playerNum].camera.cameraLocation.x, gPlayerInfo[playerNum].camera.cameraLocation.y, gPlayerInfo[playerNum].camera.cameraLocation.z},
		.hasCameraState = true,
		.activeWeapon = gPlayerInfo[playerNum].powType,
		.hasWeaponState = true,
		.weaponCount = gPlayerInfo[playerNum].powType >= 0 && gPlayerInfo[playerNum].powQuantity > 0 ? 1 : 0,
		.invulnerable = gPlayerInfo[playerNum].objNode->InvincibleTimer > 0.0f,
		.hasInvulnerabilityState = true,
		.invulnerabilityTimeRemaining = gPlayerInfo[playerNum].objNode->InvincibleTimer,
		.hasInvulnerabilityTimerState = true,
		.tokenCount = gPlayerInfo[playerNum].numTokens,
		.hasTokenState = true,
		.active = !gPlayerInfo[playerNum].objNode->ScriptActiveStateInitialized || gPlayerInfo[playerNum].objNode->ScriptActive,
	};
	outPlayer->rotation = (PangeaScriptVector3){gPlayerInfo[playerNum].objNode->Rot.x, gPlayerInfo[playerNum].objNode->Rot.y, gPlayerInfo[playerNum].objNode->Rot.z};
	outPlayer->hasRotation = true;
	outPlayer->aim = (PangeaScriptVector3){-sinf(gPlayerInfo[playerNum].objNode->Rot.y), 0.0f, -cosf(gPlayerInfo[playerNum].objNode->Rot.y)};
	outPlayer->hasAimState = true;
	if (gPlayerInfo[playerNum].objNode->Skeleton)
	{
		outPlayer->animation = gPlayerInfo[playerNum].objNode->Skeleton->AnimNum;
		outPlayer->hasAnimationState = true;
		outPlayer->animationSpeed = gPlayerInfo[playerNum].objNode->Skeleton->AnimSpeed;
		outPlayer->hasAnimationSpeedState = true;
		outPlayer->animationFrame = GetScriptSkeletonFrame(gPlayerInfo[playerNum].objNode->Skeleton);
		outPlayer->hasAnimationFrameState = true;
	}
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
	return CroMagScript_SetObjectPosition(gPlayerInfo[playerNum].objNode, position) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_BAD_ARGUMENT;
}

static PangeaScriptStatus SetScriptPlayerVelocity(int playerNum, const PangeaScriptVector3* velocity)
{
	if (playerNum < 0 || playerNum >= gNumTotalPlayers || !velocity || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	return CroMagScript_SetObjectVelocity(gPlayerInfo[playerNum].objNode, velocity) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
}

static bool GetScriptInput(int playerNum, PangeaScriptInputSnapshot* output)
{
	static const char* names[] = {"throwForward", "throwBackward", "brake", "throttle", "reverse", "camera", "rearView"};
	if (!output || playerNum < 0 || playerNum >= gNumRealPlayers) return false;
	*output = (PangeaScriptInputSnapshot){0};
	output->actionCount = NUM_CONTROL_BITS;
	output->moveX = gPlayerInfo[playerNum].analogSteering.x;
	output->moveY = gPlayerInfo[playerNum].analogSteering.y;
	for (int i = 0; i < NUM_CONTROL_BITS; i++)
	{
		uint32_t mask = 1u << i;
		output->actions[i] = (PangeaScriptInputAction){names[i],
			(gPlayerInfo[playerNum].controlBits & mask) != 0,
			(gPlayerInfo[playerNum].controlBits_New & mask) != 0,
			(gScriptReleasedControlBits[playerNum] & mask) != 0};
	}
	return true;
}

static bool SampleScriptTerrain(float x, float z, PangeaScriptTerrainSample* output)
{
	if (!output || !isfinite(x) || !isfinite(z) || !gMapYCoords ||
		x < 0 || z < 0 || x >= gTerrainUnitWidth || z >= gTerrainUnitDepth) return false;
	OGLVector3D savedNormal = gRecentTerrainNormal;
	float height = GetTerrainY(x, z);
	*output = (PangeaScriptTerrainSample){.height = height,
		.normal = {gRecentTerrainNormal.x, gRecentTerrainNormal.y, gRecentTerrainNormal.z},
		.surfaceType = gTileGrid && gTileAttribList && *gTileAttribList ? GetTileAttribsAtRowCol(x, z) : 0};
	gRecentTerrainNormal = savedNormal;
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
	{
		if (node->CType == INVALID_NODE_FLAG || !(node->CType & CTYPE_LIQUID) ||
			!node->CollisionBoxes || node->NumCollisionBoxes == 0) continue;
		CollisionBoxType* box = &node->CollisionBoxes[0];
		if (x < box->left || x > box->right || z < box->back || z > box->front) continue;
		output->hasWater = true;
		output->waterHeight = box->top;
		break;
	}
	return true;
}

static bool OverlapScriptBox(const PangeaScriptBounds* bounds, unsigned int mask,
	PangeaScriptObjectHandle* results, int capacity, int* count)
{
	if (!bounds || !results || !count || capacity <= 0 || mask == 0) return false;
	if (!isfinite(bounds->min.x) || !isfinite(bounds->min.y) || !isfinite(bounds->min.z) ||
		!isfinite(bounds->max.x) || !isfinite(bounds->max.y) || !isfinite(bounds->max.z) ||
		bounds->min.x > bounds->max.x || bounds->min.y > bounds->max.y || bounds->min.z > bounds->max.z) return false;
	*count = 0;
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
	{
		if (node->CType == INVALID_NODE_FLAG || !(node->CType & mask) ||
			(node->StatusBits & STATUS_BIT_NOCOLLISION) || !node->CollisionBoxes) continue;
		PangeaScriptObjectHandle handle = {node->ScriptObjectID, (uint32_t)node->ScriptObjectGeneration};
		if (!PangeaScript_ObjectExists(handle)) continue;
		for (int boxIndex = 0; boxIndex < node->NumCollisionBoxes; boxIndex++)
		{
			const CollisionBoxType* box = &node->CollisionBoxes[boxIndex];
			if (bounds->max.x < box->left || bounds->min.x > box->right ||
				bounds->max.y < box->bottom || bounds->min.y > box->top ||
				bounds->max.z < box->back || bounds->min.z > box->front) continue;
			results[(*count)++] = handle;
			break;
		}
		if (*count == capacity) break;
	}
	return true;
}

static const PangeaScriptCollisionCategory kScriptCollisionCategories[] =
{
	{"player", CTYPE_PLAYER}, {"obstacle", CTYPE_MISC | CTYPE_AVOID},
	{"trigger", CTYPE_TRIGGER}, {"target", CTYPE_AUTOTARGET},
	{"liquid", CTYPE_LIQUID}, {"solid", CTYPE_IMPENETRABLE},
};

static const PangeaScriptStateField kScriptStateFields[] =
{
	{"nitroTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, true},
	{"steering", PANGEA_SCRIPT_STATE_NUMBER, true, -1, 1, true},
	{"thrust", PANGEA_SCRIPT_STATE_NUMBER, true, -100000, 100000, true},
	{"angularVelocityX", PANGEA_SCRIPT_STATE_NUMBER, true, -100, 100, true},
	{"angularVelocityY", PANGEA_SCRIPT_STATE_NUMBER, true, -100, 100, true},
	{"angularVelocityZ", PANGEA_SCRIPT_STATE_NUMBER, true, -100, 100, true},
	{"braking", PANGEA_SCRIPT_STATE_BOOLEAN, true, 0, 1, true},
	{"invisibilityTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, true},
	{"frozenTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, true},
	{"flamingTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, true},
	{"greasedTiresTime", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 3600, true},
	{"stickyTiresTime", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 3600, true},
	{"superSuspensionTime", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 3600, true},
	{"lap", PANGEA_SCRIPT_STATE_NUMBER, true, -1, 32767, true},
	{"checkpoint", PANGEA_SCRIPT_STATE_NUMBER, true, -1, MAX_CHECKPOINTS - 1, true},
	{"place", PANGEA_SCRIPT_STATE_NUMBER, true, 0, MAX_PLAYERS - 1, true},
	{"tagTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, true},
	{"captureScore", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, true},
	{"raceComplete", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, true},
	{"eliminated", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, true},
	{"tagged", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, true},
	{"gameMode", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 32767, false},
};

static float* GetWritableScriptTimer(int playerNum, const char* key)
{
	if (strcmp(key, "nitroTime") == 0) return &gPlayerInfo[playerNum].nitroTimer;
	if (strcmp(key, "steering") == 0) return &gPlayerInfo[playerNum].steering;
	if (strcmp(key, "thrust") == 0) return &gPlayerInfo[playerNum].currentThrust;
	if (strcmp(key, "angularVelocityX") == 0) return &gPlayerInfo[playerNum].objNode->DeltaRot.x;
	if (strcmp(key, "angularVelocityY") == 0) return &gPlayerInfo[playerNum].objNode->DeltaRot.y;
	if (strcmp(key, "angularVelocityZ") == 0) return &gPlayerInfo[playerNum].objNode->DeltaRot.z;
	if (strcmp(key, "invisibilityTime") == 0) return &gPlayerInfo[playerNum].invisibilityTimer;
	if (strcmp(key, "frozenTime") == 0) return &gPlayerInfo[playerNum].frozenTimer;
	if (strcmp(key, "flamingTime") == 0) return &gPlayerInfo[playerNum].flamingTimer;
	if (strcmp(key, "tagTime") == 0) return &gPlayerInfo[playerNum].tagTimer;
	return NULL;
}

static bool GetScriptState(int playerNum, const char* key, PangeaScriptStateValue* output)
{
	if (!key || !output) return false;
	*output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER};
	if (strcmp(key, "gameMode") == 0) { output->number = gGameMode; return true; }
	if (playerNum < 0 || playerNum >= gNumTotalPlayers || !gPlayerInfo[playerNum].objNode) return false;
	float* timer = GetWritableScriptTimer(playerNum, key);
	if (timer) { output->number = *timer; return true; }
	if (strcmp(key, "braking") == 0)
	{
		output->type = PANGEA_SCRIPT_STATE_BOOLEAN;
		output->boolean = gPlayerInfo[playerNum].braking;
		return true;
	}
	if (strcmp(key, "greasedTiresTime") == 0) output->number = gPlayerInfo[playerNum].greasedTiresTimer;
	else if (strcmp(key, "stickyTiresTime") == 0) output->number = gPlayerInfo[playerNum].stickyTiresTimer;
	else if (strcmp(key, "superSuspensionTime") == 0) output->number = gPlayerInfo[playerNum].superSuspensionTimer;
	else if (strcmp(key, "lap") == 0) output->number = gPlayerInfo[playerNum].lapNum;
	else if (strcmp(key, "checkpoint") == 0) output->number = gPlayerInfo[playerNum].checkpointNum;
	else if (strcmp(key, "place") == 0) output->number = gPlayerInfo[playerNum].place;
	else if (strcmp(key, "tagTime") == 0) output->number = gPlayerInfo[playerNum].tagTimer;
	else if (strcmp(key, "captureScore") == 0) output->number = gCapturedFlagCount[gPlayerInfo[playerNum].team & 1];
	else if (strcmp(key, "raceComplete") == 0) { output->type = PANGEA_SCRIPT_STATE_BOOLEAN; output->boolean = gPlayerInfo[playerNum].raceComplete; }
	else if (strcmp(key, "eliminated") == 0) { output->type = PANGEA_SCRIPT_STATE_BOOLEAN; output->boolean = gPlayerInfo[playerNum].isEliminated; }
	else if (strcmp(key, "tagged") == 0) { output->type = PANGEA_SCRIPT_STATE_BOOLEAN; output->boolean = gPlayerInfo[playerNum].isIt; }
	else return false;
	return true;
}

static PangeaScriptStatus SetScriptState(int playerNum, const char* key, const PangeaScriptStateValue* value)
{
	if (!key || !value || playerNum < 0 || playerNum >= gNumTotalPlayers || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	bool physical = strcmp(key, "steering") == 0 || strcmp(key, "thrust") == 0 ||
		strncmp(key, "angularVelocity", 15) == 0 || strcmp(key, "braking") == 0;
	if (physical)
	{
		ObjNode* car = gPlayerInfo[playerNum].objNode;
		if (!PangeaScript_ObjectIsScriptControlled((PangeaScriptObjectHandle){car->ScriptObjectID, car->ScriptObjectGeneration}) ||
			!gPlayerInfo[playerNum].onThisMachine)
			return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		if (strcmp(key, "braking") == 0)
		{
			if (value->type != PANGEA_SCRIPT_STATE_BOOLEAN) return PANGEA_SCRIPT_BAD_ARGUMENT;
			gPlayerInfo[playerNum].braking = value->boolean;
			return PANGEA_SCRIPT_OK;
		}
		float* field = GetWritableScriptTimer(playerNum, key);
		double limit = strcmp(key, "steering") == 0 ? 1 : strcmp(key, "thrust") == 0 ? 100000 : 100;
		if (!field || value->type != PANGEA_SCRIPT_STATE_NUMBER || !isfinite(value->number) || fabs(value->number) > limit)
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		*field = (float)value->number;
		return PANGEA_SCRIPT_OK;
	}
	short* raceValue = NULL;
	int minimum = -1;
	int maximum = 32767;
	if (strcmp(key, "lap") == 0) raceValue = &gPlayerInfo[playerNum].lapNum;
	else if (strcmp(key, "checkpoint") == 0) { raceValue = &gPlayerInfo[playerNum].checkpointNum; maximum = MAX_CHECKPOINTS - 1; }
	else if (strcmp(key, "place") == 0) { raceValue = &gPlayerInfo[playerNum].place; minimum = 0; maximum = gNumTotalPlayers - 1; }
	else if (strcmp(key, "captureScore") == 0) { raceValue = &gCapturedFlagCount[gPlayerInfo[playerNum].team & 1]; minimum = 0; }
	if (raceValue)
	{
		if (CroMagScript_NativeRulesEnabled()) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		if (value->type != PANGEA_SCRIPT_STATE_NUMBER || !isfinite(value->number) ||
			value->number < minimum || value->number > maximum || trunc(value->number) != value->number)
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		*raceValue = (short)value->number;
		return PANGEA_SCRIPT_OK;
	}
	float* timer = GetWritableScriptTimer(playerNum, key);
	if (!timer) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (strcmp(key, "tagTime") == 0 && CroMagScript_NativeRulesEnabled()) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (value->type != PANGEA_SCRIPT_STATE_NUMBER || !isfinite(value->number) || value->number < 0 || value->number > 3600)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	*timer = (float)value->number;
	if (strcmp(key, "nitroTime") == 0 || strcmp(key, "tagTime") == 0) return PANGEA_SCRIPT_OK;
	OGLColorRGBA color = {1, 1, 1, 1};
	if (gPlayerInfo[playerNum].flamingTimer > 0) color = (OGLColorRGBA){0.7f, 0, 0, 1};
	if (gPlayerInfo[playerNum].frozenTimer > 0) color = (OGLColorRGBA){0.3f, 0.3f, 1, 0.8f};
	float invisible = gPlayerInfo[playerNum].invisibilityTimer;
	if (invisible > 0) color.a = invisible > 1 ? 0.1f : 1 - invisible;
	for (ObjNode* node = gPlayerInfo[playerNum].objNode; node; node = node->ChainNode)
		node->ColorFilter = color;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus CompleteScriptRace(int playerNum, bool success)
{
	if (playerNum < 0 || playerNum >= gNumTotalPlayers || !gPlayerInfo[playerNum].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!success) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!IsRaceMode())
	{
		if (CroMagScript_NativeRulesEnabled()) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		if (gTrackCompleted) return PANGEA_SCRIPT_OK;
		gTrackCompleted = true;
		gTrackCompletedCoolDownTimer = TRACK_COMPLETE_COOLDOWN_TIME;
		for (short i = 0; i < gNumTotalPlayers; i++)
		{
			bool won = i == playerNum || (gGameMode == GAME_MODE_CAPTUREFLAG && gPlayerInfo[i].team == gPlayerInfo[playerNum].team);
			ShowWinLose(i, won ? 1 : 2, (short)playerNum);
		}
		return PANGEA_SCRIPT_OK;
	}
	PlayerCompletedRace((short)playerNum);
	return PANGEA_SCRIPT_OK;
}

#include "ScriptPlayerOperations.inc"

static const PangeaScriptGameplayOps kScriptGameplayOps =
{
	.getInput = GetScriptInput,
	.sampleTerrain = SampleScriptTerrain,
	.overlapBox = OverlapScriptBox,
	.collisionCategories = kScriptCollisionCategories,
	.collisionCategoryCount = sizeof(kScriptCollisionCategories) / sizeof(kScriptCollisionCategories[0]),
	.getState = GetScriptState,
	.setState = SetScriptState,
	.stateFields = kScriptStateFields,
	.stateFieldCount = sizeof(kScriptStateFields) / sizeof(kScriptStateFields[0]),
	.complete = CompleteScriptRace,
	.operationMask = (1u << PANGEA_SCRIPT_OP_CARRY) | (1u << PANGEA_SCRIPT_OP_CAPTURE) |
		(1u << PANGEA_SCRIPT_OP_HANDOVER) |
		(1u << PANGEA_SCRIPT_OP_DROP) | (1u << PANGEA_SCRIPT_OP_DELIVER) |
		(1u << PANGEA_SCRIPT_OP_RESET_CARRIED) | (1u << PANGEA_SCRIPT_OP_FIRE) | (1u << PANGEA_SCRIPT_OP_RESPAWN),
	.runOperation = RunScriptPlayerOperation,
	.getRelations = GetScriptPlayerRelations,
};

void CroMagScript_Init(void)
{
	ScriptPresentation_Configure();
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
		.gameplay = &kScriptGameplayOps,
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
	if (gNetGameInProgress || PangeaNetBridge_IsEnabled()) return "network";
	if (gGameMode == GAME_MODE_MULTIPLAYERRACE) return "multiplayerRace";
	if (gGameMode == GAME_MODE_TAG1) return "tag1";
	if (gGameMode == GAME_MODE_TAG2) return "tag2";
	if (gGameMode == GAME_MODE_SURVIVAL) return "survival";
	if (gGameMode == GAME_MODE_CAPTUREFLAG) return "capture";
	return "local";
}

static bool CroMagScript_IsNetworked(void)
{
	return gNetGameInProgress || PangeaNetBridge_IsEnabled();
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
	gScriptSimulationTime = 0;
	memset(gScriptPreviousControlBits, 0, sizeof(gScriptPreviousControlBits));
	memset(gScriptReleasedControlBits, 0, sizeof(gScriptReleasedControlBits));
	PangeaScript_SetNetworkedMode(CroMagScript_IsNetworked());
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, trackNum, "onRaceLoad");
}

void CroMagScript_OnRaceStart(int trackNum)
{
	CallRaceHook(PANGEA_SCRIPT_HOOK_LEVEL_START, trackNum, "onRaceStart");
}

void CroMagScript_OnRaceFrame(int trackNum, unsigned int frameNum, float deltaSeconds, float raceTimeSeconds)
{
	(void)raceTimeSeconds;
	PangeaScript_SetNetworkedMode(CroMagScript_IsNetworked());
	if (gGamePaused) return;
	gScriptSimulationTime += deltaSeconds;
	for (int i = 0; i < gNumRealPlayers; i++)
	{
		gScriptReleasedControlBits[i] = gScriptPreviousControlBits[i] & ~gPlayerInfo[i].controlBits;
		gScriptPreviousControlBits[i] = gPlayerInfo[i].controlBits;
	}
	const PangeaScriptFrameContext context =
	{
		.levelNum = trackNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = gScriptSimulationTime,
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

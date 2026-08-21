#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "splineitems.h"
#include <stdio.h>
#include <string.h>

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

static bool CompleteScriptReplacement(PangeaScriptObjectHandle handle, const char* action)
{
	PangeaScriptStatus status = PangeaScript_ApplyObjectLifecycle(
		handle, &gScriptFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_IN);
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
	if (FSpOpenDF(spec, fsRdPerm, &refNum) != noErr) return false;
	Boolean valid = GetEOF(refNum, &size) == noErr && size > 0 && size <= 16 * 1024 * 1024;
	FSClose(refNum);
	return valid;
}

static int GetCustomModelGroup(const char* modelPath)
{
	char dataPath[260];
	FSSpec spec;
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
		ImportBG3D(&spec, group);
		if (!gBG3DContainerList[group] || gNumObjectsInBG3DGroupList[group] <= 0)
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
	short modelRefNum;
	long modelSize;
	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		int type = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i;
		if (!IsSkeletonTypeLoaded(type)) { gScriptSkeletonCache[i].modelPath[0] = '\0'; gScriptSkeletonCache[i].skeletonPath[0] = '\0'; }
		if (strcmp(gScriptSkeletonCache[i].modelPath, definition->modelPath) == 0 && strcmp(gScriptSkeletonCache[i].skeletonPath, definition->skeletonPath) == 0) return type;
	}
	if (!MakeDataAssetPath(definition->modelPath, modelPath, sizeof(modelPath)) || !MakeDataAssetPath(definition->skeletonPath, skeletonPath, sizeof(skeletonPath))) return -1;
	if (FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, modelPath, &modelSpec) != noErr || FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, skeletonPath, &skeletonSpec) != noErr) return -1;
	if (FSpOpenDF(&modelSpec, fsRdPerm, &modelRefNum) != noErr) return -1;
	if (GetEOF(modelRefNum, &modelSize) != noErr || modelSize <= 0 || modelSize > 16 * 1024 * 1024) { FSClose(modelRefNum); return -1; }
	FSClose(modelRefNum);
	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		int type = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i;
		if (IsSkeletonTypeLoaded(type)) continue;
		if (!LoadCustomSkeleton(type, &skeletonSpec, &modelSpec)) return -1;
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

static bool Bugdom2Script_SetObjectRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !rotation || obj->CType == INVALID_NODE_FLAG)
		return false;
	obj->Rot = (OGLVector3D){rotation->x, rotation->y, rotation->z};
	UpdateObjectTransforms(obj);
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
		Bugdom2Script_UnregisterObject(obj);
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
	.setPosition = Bugdom2Script_SetObjectPosition,
	.setVelocity = Bugdom2Script_SetObjectVelocity,
	.setRotation = Bugdom2Script_SetObjectRotation,
	.setScale = Bugdom2Script_SetObjectScale,
	.setAnimation = Bugdom2Script_SetObjectAnimation,
	.setAnimationNamed = Bugdom2Script_SetObjectAnimationNamed,
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
	if (!object || object->ScriptObjectID <= 0 || object->ScriptObjectGeneration <= 0)
		return (PangeaScriptObjectHandle){0};
	return (PangeaScriptObjectHandle){object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
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

void Bugdom2Script_RunObjectFrame(ObjNode* obj)
{
	Bugdom2Script_ApplyObjectScripting(obj);
	if (obj->CType == INVALID_NODE_FLAG)
		return;
	if (obj->ScriptDeleteRequested)
	{
		DeleteObject(obj);
		return;
	}
	if (obj->Skeleton && obj->Skeleton->AnimHasStopped && !obj->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
		obj->ScriptAnimationCompletionSent = true;
	}
}

void Bugdom2Script_OnObjectDeleted(ObjNode* obj)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		PangeaScriptObjectLifecycle lifecycle = obj->TerrainItemPtr || obj->SplineItemPtr
			? PANGEA_SCRIPT_OBJECT_STREAM_OUT
			: PANGEA_SCRIPT_OBJECT_DESTROY;
		(void)PangeaScript_ApplyObjectLifecycle(handle, &gScriptFrameContext, lifecycle);
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
		if (who && who->ScriptObjectID)
			other = (PangeaScriptObjectHandle){who->ScriptObjectID, (uint32_t)who->ScriptObjectGeneration};
		return PangeaScript_CallObjectTriggerWithOther(handle, &gScriptFrameContext, sides, true, other);
	}
	return true;
}

void Bugdom2Script_OnAnimationEvent(ObjNode* obj, int eventValue)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
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
	PangeaScriptObjectHandle handle = {object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
	PangeaScriptStatus spawnStatus = PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "spawn");
	if (spawnStatus != PANGEA_SCRIPT_OK || !PangeaScript_ObjectExists(handle))
	{
		(void)PangeaScript_DeleteObject(handle);
		if (outHandle) *outHandle = (PangeaScriptObjectHandle){0};
		return spawnStatus == PANGEA_SCRIPT_OK ? PANGEA_SCRIPT_RUNTIME_ERROR : spawnStatus;
	}
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
		if (gBG3DContainerList[MODEL_GROUP_LEVELSPECIFIC] == nil)
		{
			return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		}

		ObjNode* newObj;
		gNewObjectDefinition.genre = 0;
		gNewObjectDefinition.group = MODEL_GROUP_LEVELSPECIFIC;
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

static bool GetScriptPlayer(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (playerNum != 0 || !outPlayer || !gPlayerInfo.objNode) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {gPlayerInfo.coord.x, gPlayerInfo.coord.y, gPlayerInfo.coord.z}, .active = true};
	return true;
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
		.capabilities = {.terrainItems = true, .splineItems = true, .mapItems = false},
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

void Bugdom2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	Bugdom2Script_CacheFrameContext(&context);

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
			return replacement->strict;
		}
		itemPtr->flags |= ITEM_FLAGS_INUSE;
		if (!CompleteScriptReplacement(handle, "terrain replacement stream-in"))
			return replacement->strict;
		return true;
	}
	LogScriptStatus("terrain replacement", status);
	return replacement->strict;
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
			return replacement->strict;
		}
		if (!CompleteScriptReplacement(handle, "spline replacement stream-in"))
			return replacement->strict;
		return true;
	}
	LogScriptStatus("spline replacement", status);
	return replacement->strict;
}

#endif

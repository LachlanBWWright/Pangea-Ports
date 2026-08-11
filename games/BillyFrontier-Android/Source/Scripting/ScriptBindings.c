#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "structs.h"

#include <stdio.h>
#include <string.h>

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;

typedef struct ScriptModelCacheEntry { char path[260]; } ScriptModelCacheEntry;
static ScriptModelCacheEntry gScriptModelCache[MODEL_GROUP_SCRIPT_CUSTOM_COUNT];
typedef struct ScriptSkeletonCacheEntry { char modelPath[260]; char skeletonPath[260]; } ScriptSkeletonCacheEntry;
static ScriptSkeletonCacheEntry gScriptSkeletonCache[SKELETON_TYPE_SCRIPT_CUSTOM_COUNT];

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
		if (gNumObjectsInBG3DGroupList[group] == 0) gScriptModelCache[i].path[0] = '\0';
		if (strcmp(gScriptModelCache[i].path, modelPath) == 0) return group;
	}
	if (!MakeDataAssetPath(modelPath, dataPath, sizeof(dataPath)) || FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, dataPath, &spec) != noErr) return -1;
	if (!IsSafeCustomAsset(&spec)) return -1;
	for (int i = 0; i < MODEL_GROUP_SCRIPT_CUSTOM_COUNT; i++)
	{
		int group = MODEL_GROUP_SCRIPT_CUSTOM_BASE + i;
		if (gNumObjectsInBG3DGroupList[group] != 0) continue;
		ImportBG3D(&spec, group);
		snprintf(gScriptModelCache[i].path, sizeof(gScriptModelCache[i].path), "%s", modelPath);
		return group;
	}
	return -1;
}

static int GetCustomSkeletonType(const PangeaScriptCustomObjectDefinition* definition)
{
	char modelPath[260], skeletonPath[260];
	FSSpec modelSpec, skeletonSpec;
	short refNum;
	long size;
	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		int type = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i;
		if (!IsSkeletonTypeLoaded(type)) { gScriptSkeletonCache[i].modelPath[0] = '\0'; gScriptSkeletonCache[i].skeletonPath[0] = '\0'; }
		if (strcmp(gScriptSkeletonCache[i].modelPath, definition->modelPath) == 0 && strcmp(gScriptSkeletonCache[i].skeletonPath, definition->skeletonPath) == 0) return type;
	}
	if (!MakeDataAssetPath(definition->modelPath, modelPath, sizeof(modelPath)) || !MakeDataAssetPath(definition->skeletonPath, skeletonPath, sizeof(skeletonPath))) return -1;
	if (FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, modelPath, &modelSpec) != noErr || FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, skeletonPath, &skeletonSpec) != noErr) return -1;
	if (FSpOpenDF(&modelSpec, fsRdPerm, &refNum) != noErr) return -1;
	if (GetEOF(refNum, &size) != noErr || size <= 0 || size > 16 * 1024 * 1024) { FSClose(refNum); return -1; }
	FSClose(refNum);
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

static bool BillyScript_GetObjectPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !outPosition || obj->CType == INVALID_NODE_FLAG)
		return false;

	outPosition->x = obj->Coord.x;
	outPosition->y = obj->Coord.y;
	outPosition->z = obj->Coord.z;
	return true;
}

static bool BillyScript_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !position || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Coord.x = position->x;
	obj->Coord.y = position->y;
	obj->Coord.z = position->z;
	return true;
}

static bool BillyScript_SetObjectVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || !velocity || obj->CType == INVALID_NODE_FLAG)
		return false;

	obj->Delta.x = velocity->x;
	obj->Delta.y = velocity->y;
	obj->Delta.z = velocity->z;
	return true;
}

static bool BillyScript_SetObjectRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || !rotation || obj->CType == INVALID_NODE_FLAG) return false;
	obj->Rot = (OGLVector3D){rotation->x, rotation->y, rotation->z};
	UpdateObjectTransforms(obj);
	return true;
}

static bool BillyScript_SetObjectScale(void* nativeObject, float scale)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG || scale <= 0) return false;
	obj->Scale = (OGLVector3D){scale, scale, scale};
	UpdateObjectTransforms(obj);
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

static bool BillyScript_SetObjectAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || !obj->Skeleton || obj->CType == INVALID_NODE_FLAG || animation < 0) return false;
	if (blendSeconds > 0) MorphToSkeletonAnim(obj->Skeleton, animation, 1.0f / blendSeconds);
	else SetSkeletonAnim(obj->Skeleton, animation);
	obj->Skeleton->AnimSpeed = speed;
	obj->ScriptAnimationCompletionSent = false;
	return true;
}

static bool BillyScript_SetObjectAnimationNamed(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	int index = ResolveNamedAnimation((ObjNode*)nativeObject, animation);
	return index >= 0 && BillyScript_SetObjectAnimation(nativeObject, index, speed, blendSeconds);
}

static bool BillyScript_DeleteObject(void* nativeObject)
{
	ObjNode* obj = (ObjNode*) nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG)
		return false;

	if (obj->ScriptDefinitionID[0]) obj->ScriptDeleteRequested = true;
	else { BillyScript_UnregisterObject(obj); DeleteObject(obj); }
	return true;
}

static const PangeaScriptObjectOps kBillyPlayerObjectOps =
{
	.getPosition = BillyScript_GetObjectPosition,
	.setPosition = BillyScript_SetObjectPosition,
	.setVelocity = BillyScript_SetObjectVelocity,
	.setRotation = BillyScript_SetObjectRotation,
	.setScale = BillyScript_SetObjectScale,
	.setAnimation = BillyScript_SetObjectAnimation,
	.setAnimationNamed = BillyScript_SetObjectAnimationNamed,
	.deleteObject = BillyScript_DeleteObject,
};

void BillyScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void BillyScript_ResetObjectRegistry(void)
{
	gScriptFrameContext = (PangeaScriptFrameContext){0};
	PangeaScript_ResetObjects();
}

void BillyScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category)
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
		.ops = &kBillyPlayerObjectOps,
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

void BillyScript_UnregisterObject(ObjNode* obj)
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
	obj->ScriptVisualOffset = (OGLVector3D){0};
}

void BillyScript_RegisterPlayerObject(ObjNode* playerObj)
{
	BillyScript_RegisterObject(playerObj, "billy.player", "player");
}

void BillyScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	BillyScript_UnregisterObject(playerObj);
}

void BillyScript_ApplyObjectScripting(ObjNode* obj)
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

void BillyScript_RunObjectFrame(ObjNode* obj)
{
	BillyScript_ApplyObjectScripting(obj);
	if (obj->CType == INVALID_NODE_FLAG) return;
	if (obj->ScriptDeleteRequested) { DeleteObject(obj); return; }
	if (obj->Skeleton && obj->Skeleton->AnimHasStopped && !obj->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
		(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
		obj->ScriptAnimationCompletionSent = true;
	}
}

void BillyScript_OnObjectDeleted(ObjNode* obj)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {(int)obj->ScriptObjectID, obj->ScriptObjectGeneration};
		(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "destroy");
	}
}

static int ResolveDisplayGroup(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP) return GetCustomModelGroup(definition->modelPath);
	if (strcmp(definition->nativeGroup, "global") == 0) return MODEL_GROUP_GLOBAL;
	if (strcmp(definition->nativeGroup, "levelSpecific") == 0) return MODEL_GROUP_LEVELSPECIFIC;
	if (strcmp(definition->nativeGroup, "buildings") == 0) return MODEL_GROUP_BUILDINGS;
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
		objectDefinition = (NewObjectDefinitionType){.group=group,.type=definition->modelObject,.coord={x,y,z},.flags=0,.slot=definition->slot,.moveCall=nil,.rot=0,.scale=definition->scale};
		return MakeNewDisplayGroupObject(&objectDefinition);
	}
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_SKELETON || definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON)
	{
		int animation = ResolveInitialAnimation(definition);
		int skeletonType = definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON ? GetCustomSkeletonType(definition) : definition->skeletonType;
		if (skeletonType < 0 || skeletonType >= MAX_SKELETON_TYPES || animation < 0 || !IsSkeletonTypeLoaded(skeletonType)) return nil;
		objectDefinition = (NewObjectDefinitionType){.type=skeletonType,.animNum=animation,.coord={x,y,z},.flags=0,.slot=definition->slot,.moveCall=nil,.rot=0,.scale=definition->scale};
		ObjNode* object = MakeNewSkeletonObject(&objectDefinition);
		if (object && object->Skeleton) object->Skeleton->AnimSpeed = definition->animationSpeed;
		return object;
	}
	return nil;
}

static Boolean ScriptedTriggerCallback(ObjNode* trigger, ObjNode* who, Byte sides)
{
	(void)who;
	(void)sides;
	if (trigger && trigger->ScriptObjectID)
	{
		PangeaScriptObjectHandle handle = {(int)trigger->ScriptObjectID, trigger->ScriptObjectGeneration};
		(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "triggerEnter");
	}
	return true;
}

static void ApplyScriptedCollision(ObjNode* object, PangeaScriptCollisionPreset preset)
{
	short radius;
	if (preset == PANGEA_SCRIPT_COLLISION_NONE) return;
	radius = (short)object->BoundingSphereRadius;
	SetObjectCollisionBounds(object, radius, -radius, -radius, radius, radius, -radius);
	object->CBits = CBITS_ALLSOLID;
	if (preset == PANGEA_SCRIPT_COLLISION_ENEMY) object->CType = CTYPE_ENEMY | CTYPE_MISC;
	else if (preset == PANGEA_SCRIPT_COLLISION_PICKUP) { object->CType = CTYPE_PICKUP | CTYPE_TRIGGER; object->TriggerCallback = ScriptedTriggerCallback; }
	else if (preset == PANGEA_SCRIPT_COLLISION_TRIGGER_BOX) { object->CType = CTYPE_TRIGGER; object->TriggerCallback = ScriptedTriggerCallback; }
	else object->CType = CTYPE_MISC;
}

static PangeaScriptStatus SpawnScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
	const PangeaScriptCustomObjectDefinition* definition = PangeaScript_GetCustomObjectDefinition(id);
	ObjNode* object;
	if (!definition) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	object = MakeScriptedVisual(definition, x, y, z);
	if (!object) return PANGEA_SCRIPT_RUNTIME_ERROR;
	ApplyScriptedCollision(object, definition->collisionPreset);
	snprintf(object->ScriptDefinitionID, sizeof(object->ScriptDefinitionID), "%s", definition->id);
	BillyScript_RegisterObject(object, definition->id, "customObject");
	if (!object->ScriptObjectID) { DeleteObject(object); return PANGEA_SCRIPT_RUNTIME_ERROR; }
	if (outHandle) *outHandle = (PangeaScriptObjectHandle){(int)object->ScriptObjectID, object->ScriptObjectGeneration};
	PangeaScriptObjectHandle handle = {(int)object->ScriptObjectID, object->ScriptObjectGeneration};
	(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "spawn");
	return PANGEA_SCRIPT_OK;
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "billy.peso",
		.nativeType = 36,
		.category = "pickup",
		.dependencySummary = "peso pickup assets, score, and terrain systems",
	},
	{
		.id = "billy.freeLifePow",
		.nativeType = 32,
		.category = "pickup",
		.dependencySummary = "free-life powerup assets, player state, and terrain systems",
	},
	{
		.id = "billy.boost",
		.nativeType = 21,
		.category = "pickup",
		.dependencySummary = "stampede boost assets, stampede speed state, and terrain systems",
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

	SDL_Log("Billy Frontier scripting %s failed: %s", action, PangeaScript_GetLastError());
}

void BillyScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "BillyFrontier-Android",
		.gameName = "Billy Frontier",
		.spawnScripted = SpawnScriptedObject,
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	LogScriptStatus("init", status);

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	BillyScript_ResetObjectRegistry();
}

void BillyScript_Shutdown(void)
{
	BillyScript_ResetObjectRegistry();
	PangeaScript_Shutdown();
}

void BillyScript_LoadAreaConfig(int areaNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(areaNum);
	LogScriptStatus("area config load", status);
}

static void CallAreaHook(PangeaScriptHook hook, int areaNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = areaNum,
		.levelName = NULL,
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

void BillyScript_OnAreaLoad(int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, areaNum, "onAreaLoad");
}

void BillyScript_OnAreaStart(int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_START, areaNum, "onAreaStart");
}

void BillyScript_OnAreaFrame(int areaNum, unsigned int frameNum, float deltaSeconds, float areaTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = areaNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = areaTimeSeconds,
	};
	BillyScript_CacheFrameContext(&context);

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onAreaFrame", status);
}

void BillyScript_OnAreaComplete(int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, areaNum, "onAreaComplete");
}

void BillyScript_OnAreaUnload(int areaNum)
{
	CallAreaHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, areaNum, "onAreaUnload");
	PangeaScript_ResetObjects();
}

int BillyScript_RemapTerrainItemType(int areaNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(areaNum, itemType);
}

Boolean BillyScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int areaNum, int originalType, int remappedType, float x, float z)
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
		.levelNum = areaNum,
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

Boolean BillyScript_OnSplineItem(SplineItemType* itemPtr, int areaNum, int splineNum)
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
		.levelNum = areaNum,
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

Boolean BillyScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z)
{
	const PangeaScriptTerrainReplacement* replacement = PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	if (!replacement) return false;
	status = SpawnScriptedObject(replacement->customObjectId, x, GetTerrainY(x, z), z, &handle);
	if (status == PANGEA_SCRIPT_OK) { itemPtr->flags |= ITEM_FLAGS_INUSE; return true; }
	LogScriptStatus("terrain replacement", status);
	return replacement->strict;
}

Boolean BillyScript_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex)
{
	const PangeaScriptSplineReplacement* replacement = PangeaScript_GetSplineReplacement(splineNum, itemIndex, itemPtr->type, itemPtr->placement);
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptStatus status;
	float x;
	float z;
	if (!replacement) return false;
	GetCoordOnSpline(&(*gSplineList)[splineNum], itemPtr->placement, &x, &z);
	status = SpawnScriptedObject(replacement->customObjectId, x, GetTerrainY(x, z), z, &handle);
	if (status == PANGEA_SCRIPT_OK) return true;
	LogScriptStatus("spline replacement", status);
	return replacement->strict;
}

#endif

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "splineitems.h"
#include <stdio.h>
#include <string.h>

#define SCRIPT_TERRAIN_ITEM_CAPACITY 256
static TerrainItemEntryType gScriptTerrainItems[SCRIPT_TERRAIN_ITEM_CAPACITY];
static bool gScriptTerrainItemOccupied[SCRIPT_TERRAIN_ITEM_CAPACITY];
static bool gScriptTerrainItemReclaimable[SCRIPT_TERRAIN_ITEM_CAPACITY];

static TerrainItemEntryType* AcquireScriptTerrainItem(void)
{
	for (int i = 0; i < SCRIPT_TERRAIN_ITEM_CAPACITY; i++)
	{
		bool referenced = false;
		if (gScriptTerrainItemOccupied[i] && !gScriptTerrainItemReclaimable[i]) continue;
		for (ObjNode* node = gFirstNodePtr; gScriptTerrainItemOccupied[i] && node; node = node->NextNode)
			referenced |= node->TerrainItemPtr == &gScriptTerrainItems[i];
		if (referenced) continue;
		gScriptTerrainItemOccupied[i] = true;
		gScriptTerrainItemReclaimable[i] = false;
		memset(&gScriptTerrainItems[i], 0, sizeof(gScriptTerrainItems[i]));
		return &gScriptTerrainItems[i];
	}
	return NULL;
}

static const PangeaScriptNativeItem kNativeItems[] =
{
	{
		.id = "ottomatic.human",
		.nativeType = 4,
		.category = "npc",
		.dependencySummary = "human rescue state, skeletons, terrain, and player systems",
	},
	{
		.id = "ottomatic.powerupPod",
		.nativeType = 6,
		.category = "pickup",
		.dependencySummary = "powerup pod assets, effects, and terrain systems",
	},
	{
		.id = "ottomatic.checkpoint",
		.nativeType = 27,
		.category = "trigger",
		.dependencySummary = "checkpoint state and terrain systems",
	},
	{
		.id = "ottomatic.teleporter",
		.nativeType = 57,
		.category = "trigger",
		.dependencySummary = "teleporter state, terrain, and level transition systems",
	},
};

static PangeaScriptStatus OttoSpawnNativeItem(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	(void) y;
	int type = PangeaScript_ResolveNativeItemType(id);
	TerrainItemEntryType* item = AcquireScriptTerrainItem();
	if (!item) return PANGEA_SCRIPT_RUNTIME_ERROR;
	item->x = (uint32_t) x;
	item->y = (uint32_t) z;
	for (int i = 0; i < 4; i++)
		item->parm[i] = (Byte) params[i];
	if (!OttoSpawnTerrainItem(type, item, (long) x, (long) z))
	{
		gScriptTerrainItemOccupied[item - gScriptTerrainItems] = false;
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
	{
		if (node->TerrainItemPtr != item)
			continue;
		gScriptTerrainItemReclaimable[item - gScriptTerrainItems] = true;
		static const char* tags[] = {"native", "terrain-item"};
		OttoScript_RegisterObjectNode(node, "ottomatic.terrain-item", PANGEA_SCRIPT_CAPABILITY_FULL, tags, 2);
		if (outHandle)
			*outHandle = (PangeaScriptObjectHandle){node->ScriptObjectID, node->ScriptObjectGeneration};
		return PANGEA_SCRIPT_OK;
	}
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptFrameContext gCurrentFrameContext;

static void MoveScriptedSplineObject(ObjNode* object)
{
	bool wasAttached;
	bool isAttached;
	PangeaScriptObjectHandle handle;

	if (!object || object->ScriptObjectID <= 0)
		return;
	wasAttached = (object->StatusBits & STATUS_BIT_DETACHED) == 0;
	(void) IsSplineItemVisible(object);
	if (object->ScriptObjectID <= 0)
		return;
	isAttached = (object->StatusBits & STATUS_BIT_DETACHED) == 0;
	if (wasAttached == isAttached)
		return;
	handle = (PangeaScriptObjectHandle){object->ScriptObjectID, object->ScriptObjectGeneration};
	(void) PangeaScript_ApplyObjectLifecycle(
		handle,
		&gCurrentFrameContext,
		isAttached ? PANGEA_SCRIPT_OBJECT_ACTIVATE : PANGEA_SCRIPT_OBJECT_DEACTIVATE);
}
static ObjNode* gCurrentScriptObject;
static Boolean gCurrentScriptObjectUsesGlobals;

typedef struct ScriptModelCacheEntry
{
	char path[260];
} ScriptModelCacheEntry;

static ScriptModelCacheEntry gScriptModelCache[MODEL_GROUP_SCRIPT_CUSTOM_COUNT];

typedef struct ScriptSkeletonCacheEntry
{
	char modelPath[260];
	char skeletonPath[260];
} ScriptSkeletonCacheEntry;

static ScriptSkeletonCacheEntry gScriptSkeletonCache[SKELETON_TYPE_SCRIPT_CUSTOM_COUNT];

static void MoveScriptedCustomObject(ObjNode* theNode)
{
	GetObjectInfo(theNode);
	OttoScript_RunObjectFrame(theNode, true);
	if (theNode->ScriptDeleteRequested)
	{
		if (theNode->ScriptObjectID > 0)
		{
			PangeaScriptObjectHandle handle = { theNode->ScriptObjectID, theNode->ScriptObjectGeneration };
			(void) PangeaScript_ApplyObjectLifecycle(handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
		}
		OttoScript_UnregisterObjectNode(theNode);
		DeleteObject(theNode);
		return;
	}
	if (theNode->Skeleton && theNode->Skeleton->AnimHasStopped && !theNode->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = { theNode->ScriptObjectID, theNode->ScriptObjectGeneration };
		(void) PangeaScript_CallObjectEvent(handle, &gCurrentFrameContext, "animationComplete");
		theNode->ScriptAnimationCompletionSent = true;
	}
	UpdateObject(theNode);
	OttoScript_ApplyObjectVisualOffset(theNode);
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

static int GetCustomModelGroup(const char* modelPath)
{
	char dataPath[260];
	FSSpec spec;
	short refNum;
	long fileSize;

	for (int i = 0; i < MODEL_GROUP_SCRIPT_CUSTOM_COUNT; i++)
	{
		int group = MODEL_GROUP_SCRIPT_CUSTOM_BASE + i;
		if (!gBG3DContainerList[group])
			gScriptModelCache[i].path[0] = '\0';
		if (strcmp(gScriptModelCache[i].path, modelPath) == 0)
			return group;
	}

	if (!MakeDataAssetPath(modelPath, dataPath, sizeof(dataPath)))
		return -1;
	if (FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, dataPath, &spec) != noErr)
		return -1;
	if (FSpOpenDF(&spec, fsRdPerm, &refNum) != noErr)
		return -1;
	if (GetEOF(refNum, &fileSize) != noErr || fileSize <= 0 || fileSize > 16 * 1024 * 1024)
	{
		FSClose(refNum);
		return -1;
	}
	FSClose(refNum);

	for (int i = 0; i < MODEL_GROUP_SCRIPT_CUSTOM_COUNT; i++)
	{
		int group = MODEL_GROUP_SCRIPT_CUSTOM_BASE + i;
		if (gBG3DContainerList[group])
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
	char modelDataPath[260];
	char skeletonDataPath[260];
	FSSpec modelSpec;
	FSSpec skeletonSpec;
	short modelRefNum;
	long modelFileSize;

	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		int skeletonType = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i;
		if (!gBG3DContainerList[MODEL_GROUP_SKELETONBASE + skeletonType])
		{
			gScriptSkeletonCache[i].modelPath[0] = '\0';
			gScriptSkeletonCache[i].skeletonPath[0] = '\0';
		}
		if (strcmp(gScriptSkeletonCache[i].modelPath, definition->modelPath) == 0 &&
			strcmp(gScriptSkeletonCache[i].skeletonPath, definition->skeletonPath) == 0)
			return skeletonType;
	}

	if (!MakeDataAssetPath(definition->modelPath, modelDataPath, sizeof(modelDataPath)) ||
		!MakeDataAssetPath(definition->skeletonPath, skeletonDataPath, sizeof(skeletonDataPath)))
		return -1;
	if (FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, modelDataPath, &modelSpec) != noErr ||
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, skeletonDataPath, &skeletonSpec) != noErr)
		return -1;
	if (FSpOpenDF(&modelSpec, fsRdPerm, &modelRefNum) != noErr)
		return -1;
	if (GetEOF(modelRefNum, &modelFileSize) != noErr || modelFileSize <= 0 || modelFileSize > 16 * 1024 * 1024)
	{
		FSClose(modelRefNum);
		return -1;
	}
	FSClose(modelRefNum);

	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		int skeletonType = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i;
		if (gBG3DContainerList[MODEL_GROUP_SKELETONBASE + skeletonType])
			continue;
		if (!LoadCustomSkeleton(skeletonType, &skeletonSpec, &modelSpec))
			return -1;
		snprintf(gScriptSkeletonCache[i].modelPath, sizeof(gScriptSkeletonCache[i].modelPath), "%s", definition->modelPath);
		snprintf(gScriptSkeletonCache[i].skeletonPath, sizeof(gScriptSkeletonCache[i].skeletonPath), "%s", definition->skeletonPath);
		return skeletonType;
	}

	return -1;
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

static int ResolveDisplayGroup(const PangeaScriptCustomObjectDefinition* definition)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
		return GetCustomModelGroup(definition->modelPath);
	if (strcmp(definition->nativeGroup, "global") == 0)
		return MODEL_GROUP_GLOBAL;
	if (strcmp(definition->nativeGroup, "levelSpecific") == 0)
		return MODEL_GROUP_LEVELSPECIFIC;
	return -1;
}

static void ApplyScriptedCollision(ObjNode* object, const PangeaScriptCustomObjectDefinition* definition)
{
	PangeaScriptCollisionPreset preset = definition->collisionPreset;
	float scale = object->Scale.x;
	short top = definition->collisionBoundsSet ? (short)(definition->collisionHeight * 0.5f) : (short)(object->BBox.max.y * scale);
	short bottom = definition->collisionBoundsSet ? (short)(-definition->collisionHeight * 0.5f) : (short)(object->BBox.min.y * scale);
	short left = definition->collisionBoundsSet ? (short)(-definition->collisionWidth * 0.5f) : (short)(object->BBox.min.x * scale);
	short right = definition->collisionBoundsSet ? (short)(definition->collisionWidth * 0.5f) : (short)(object->BBox.max.x * scale);
	short front = definition->collisionBoundsSet ? (short)(definition->collisionDepth * 0.5f) : (short)(object->BBox.max.z * scale);
	short back = definition->collisionBoundsSet ? (short)(-definition->collisionDepth * 0.5f) : (short)(object->BBox.min.z * scale);

	if (preset == PANGEA_SCRIPT_COLLISION_NONE)
		return;

	SetObjectCollisionBounds(object, top, bottom, left, right, front, back);
	object->CBits = preset == PANGEA_SCRIPT_COLLISION_TRIGGER_BOX ? CBITS_ALWAYSTRIGGER : CBITS_ALLSOLID;
	switch (preset)
	{
		case PANGEA_SCRIPT_COLLISION_TRIGGER_BOX:
		case PANGEA_SCRIPT_COLLISION_PICKUP:
			object->CType = CTYPE_TRIGGER;
			object->CBits = CBITS_ALWAYSTRIGGER;
			object->Kind = TRIGTYPE_SCRIPTED;
			object->TriggerSides = ALL_SOLID_SIDES;
			break;
		case PANGEA_SCRIPT_COLLISION_ENEMY: object->CType = CTYPE_ENEMY | CTYPE_HURTENEMY; break;
		case PANGEA_SCRIPT_COLLISION_PLATFORM: object->CType = CTYPE_MPLATFORM | CTYPE_MISC; break;
		case PANGEA_SCRIPT_COLLISION_SOLID_BOX: object->CType = CTYPE_MISC; break;
		case PANGEA_SCRIPT_COLLISION_NONE: break;
	}
}

void OttoScript_OnCustomTrigger(ObjNode* triggerNode, ObjNode* whoNode, Byte sideBits)
{
	if (triggerNode && triggerNode->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = { triggerNode->ScriptObjectID, triggerNode->ScriptObjectGeneration };
		PangeaScriptObjectHandle other = {0};
		if (whoNode && whoNode->ScriptObjectID > 0)
			other = (PangeaScriptObjectHandle){whoNode->ScriptObjectID, whoNode->ScriptObjectGeneration};
		(void) PangeaScript_CallObjectTriggerWithOther(handle, &gCurrentFrameContext, sideBits, true, other);
	}
}

void OttoScript_OnAnimationEvent(ObjNode* node, int eventValue)
{
	if (node && node->ScriptDefinitionID[0] && node->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = { node->ScriptObjectID, node->ScriptObjectGeneration };
		(void) PangeaScript_CallObjectEventWithValue(handle, &gCurrentFrameContext, "animationEvent", eventValue);
	}
}

static ObjNode* MakeScriptedVisual(const PangeaScriptCustomObjectDefinition* definition, float x, float y, float z)
{
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_DISPLAY_GROUP ||
		definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
	{
		int group = ResolveDisplayGroup(definition);
		if (group < 0 || definition->modelObject < 0 || definition->modelObject >= gNumObjectsInBG3DGroupList[group])
			return NULL;

		gNewObjectDefinition = (NewObjectDefinitionType)
		{
			.group = group,
			.type = definition->modelObject,
			.coord = {x, y, z},
			.flags = gAutoFadeStatusBits,
			.slot = definition->slot,
			.moveCall = MoveScriptedCustomObject,
			.rot = 0.0f,
			.scale = definition->scale,
		};
		return MakeNewDisplayGroupObject(&gNewObjectDefinition);
	}

	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_NATIVE_SKELETON ||
		definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON)
	{
		int skeletonType = definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON
			? GetCustomSkeletonType(definition)
			: definition->skeletonType;
		int initialAnimation = ResolveInitialAnimation(definition);
		if (skeletonType < 0 || skeletonType >= MAX_SKELETON_TYPES ||
			initialAnimation < 0 || !gBG3DContainerList[MODEL_GROUP_SKELETONBASE + skeletonType])
			return NULL;

		gNewObjectDefinition = (NewObjectDefinitionType)
		{
			.type = skeletonType,
			.animNum = initialAnimation,
			.coord = {x, y, z},
			.flags = gAutoFadeStatusBits,
			.slot = definition->slot,
			.moveCall = MoveScriptedCustomObject,
			.rot = 0.0f,
			.scale = definition->scale,
		};
		ObjNode* object = MakeNewSkeletonObject(&gNewObjectDefinition);
		if (object && object->Skeleton)
			object->Skeleton->AnimSpeed = definition->animationSpeed;
		return object;
	}

	return NULL;
}

static PangeaScriptStatus SpawnScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
	const PangeaScriptCustomObjectDefinition* definition = PangeaScript_GetCustomObjectDefinition(id);
	static const char* tags[] = { "customObject" };
	ObjNode* object;

	if (!definition)
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;

	object = MakeScriptedVisual(definition, x, y, z);
	if (!object)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	ApplyScriptedCollision(object, definition);
	snprintf(object->ScriptDefinitionID, sizeof(object->ScriptDefinitionID), "%s", definition->id);
	OttoScript_RegisterObjectNode(
		object,
		definition->id,
		PANGEA_SCRIPT_CAPABILITY_FULL,
		tags,
		1);

	if (object->ScriptObjectID <= 0)
	{
		DeleteObject(object);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	if (outHandle)
	{
		outHandle->id = object->ScriptObjectID;
		outHandle->generation = object->ScriptObjectGeneration;
	}
	PangeaScriptObjectHandle handle = { object->ScriptObjectID, object->ScriptObjectGeneration };
	PangeaScriptStatus spawnStatus = PangeaScript_CallObjectEvent(handle, &gCurrentFrameContext, "spawn");
	if (spawnStatus != PANGEA_SCRIPT_OK || !PangeaScript_ObjectExists(handle))
	{
		(void) PangeaScript_DeleteObject(handle);
		if (outHandle) *outHandle = (PangeaScriptObjectHandle){0};
		return spawnStatus == PANGEA_SCRIPT_OK ? PANGEA_SCRIPT_RUNTIME_ERROR : spawnStatus;
	}

	return PANGEA_SCRIPT_OK;
}

static const char* const kHumanFarmerTags[] =
{
	"ottomatic.human",
	"ottomatic.human.farmer",
};

static const char* const kHumanBeewomanTags[] =
{
	"ottomatic.human",
	"ottomatic.human.beewoman",
};

static const char* const kHumanScientistTags[] =
{
	"ottomatic.human",
	"ottomatic.human.scientist",
};

static const char* const kHumanSkirtladyTags[] =
{
	"ottomatic.human",
	"ottomatic.human.skirtlady",
};

static const char* const* GetHumanTags(int humanType, int* outTagCount)
{
	if (!outTagCount)
		return NULL;

	*outTagCount = 2;
	switch (humanType)
	{
		case HUMAN_TYPE_FARMER:
			return kHumanFarmerTags;

		case HUMAN_TYPE_BEEWOMAN:
			return kHumanBeewomanTags;

		case HUMAN_TYPE_SCIENTIST:
			return kHumanScientistTags;

		case HUMAN_TYPE_SKIRTLADY:
		default:
			return kHumanSkirtladyTags;
	}
}

static bool OttoObjectGetPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outPosition)
		return false;

	if (node == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		outPosition->x = gCoord.x;
		outPosition->y = gCoord.y;
		outPosition->z = gCoord.z;
		return true;
	}

	outPosition->x = node->Coord.x;
	outPosition->y = node->Coord.y;
	outPosition->z = node->Coord.z;
	return true;
}

static bool OttoObjectSetPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !position)
		return false;

	node->Coord.x = position->x;
	node->Coord.y = position->y;
	node->Coord.z = position->z;

	if (node == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		gCoord.x = position->x;
		gCoord.y = position->y;
		gCoord.z = position->z;
	}

	return true;
}

static bool OttoObjectSetVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !velocity)
		return false;

	node->Delta.x = velocity->x;
	node->Delta.y = velocity->y;
	node->Delta.z = velocity->z;

	if (node == gCurrentScriptObject && gCurrentScriptObjectUsesGlobals)
	{
		gDelta.x = velocity->x;
		gDelta.y = velocity->y;
		gDelta.z = velocity->z;
	}

	return true;
}

static bool OttoObjectSetRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !rotation)
		return false;
	node->Rot.x = rotation->x;
	node->Rot.y = rotation->y;
	node->Rot.z = rotation->z;
	UpdateObjectTransforms(node);
	return true;
}

static bool OttoObjectSetScale(void* nativeObject, float scale)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || scale <= 0.0f)
		return false;
	node->Scale.x = scale;
	node->Scale.y = scale;
	node->Scale.z = scale;
	UpdateObjectTransforms(node);
	return true;
}

static bool OttoObjectSetAnimation(void* nativeObject, int animation, float speed, float blendSeconds)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !node->Skeleton || animation < 0 || animation >= node->Skeleton->skeletonDefinition->NumAnims)
		return false;
	if (blendSeconds > 0.0f)
		MorphToSkeletonAnim(node->Skeleton, animation, 1.0f / blendSeconds);
	else
		SetSkeletonAnim(node->Skeleton, animation);
	node->Skeleton->AnimSpeed = speed;
	node->ScriptAnimationCompletionSent = false;
	return true;
}

static bool OttoObjectSetAnimationNamed(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !animation || !node->ScriptDefinitionID[0])
		return false;
	const PangeaScriptCustomObjectDefinition* definition =
		PangeaScript_GetCustomObjectDefinition(node->ScriptDefinitionID);
	if (!definition)
		return false;
	for (int i = 0; i < definition->animationCount; i++)
	{
		if (strcmp(definition->animationNames[i], animation) == 0)
			return OttoObjectSetAnimation(node, definition->animationIndices[i], speed, blendSeconds);
	}
	return false;
}

static bool OttoObjectDelete(void* nativeObject)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node)
		return false;
	if (node == gCurrentScriptObject || (node->CType & CTYPE_TRIGGER))
	{
		OttoScript_UnregisterObjectNode(node);
		node->ScriptDeleteRequested = true;
		return true;
	}

	PangeaScriptObjectHandle handle = { node->ScriptObjectID, node->ScriptObjectGeneration };
	(void) PangeaScript_ApplyObjectLifecycle(handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	OttoScript_UnregisterObjectNode(node);
	DeleteObject(node);
	return true;
}

static const PangeaScriptObjectOps kOttoObjectNodeOps =
{
	.getPosition = OttoObjectGetPosition,
	.setPosition = OttoObjectSetPosition,
	.setVelocity = OttoObjectSetVelocity,
	.setRotation = OttoObjectSetRotation,
	.setScale = OttoObjectSetScale,
	.setAnimation = OttoObjectSetAnimation,
	.setAnimationNamed = OttoObjectSetAnimationNamed,
	.deleteObject = OttoObjectDelete,
};

static void LogScriptStatus(const char* action, PangeaScriptStatus status)
{
	static PangeaScriptStatus lastStatus = PANGEA_SCRIPT_OK;
	static char lastAction[64];
	static char lastError[256];
	const char* error;

	if (status == PANGEA_SCRIPT_OK || status == PANGEA_SCRIPT_FILE_NOT_FOUND || status == PANGEA_SCRIPT_NOT_ENABLED)
		return;

	error = PangeaScript_GetLastError();
	if (!error)
		error = "";

	if (status == lastStatus && strcmp(action, lastAction) == 0 && strcmp(error, lastError) == 0)
		return;

	lastStatus = status;
	snprintf(lastAction, sizeof(lastAction), "%s", action);
	snprintf(lastError, sizeof(lastError), "%s", error);

	SDL_Log("Otto Matic scripting %s failed: %s", action, error);
}

static bool CompleteScriptReplacement(PangeaScriptObjectHandle handle, const char* action)
{
	PangeaScriptStatus status = PangeaScript_ApplyObjectLifecycle(
		handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_IN);
	if (status == PANGEA_SCRIPT_OK && PangeaScript_ObjectExists(handle))
		return true;
	LogScriptStatus(action, status);
	if (PangeaScript_ObjectExists(handle))
		(void)PangeaScript_DeleteObject(handle);
	return false;
}

static int GetScriptPlayerCount(void) { return gPlayerInfo.objNode ? 1 : 0; }

static bool GetScriptPlayer(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (playerNum != 0 || !outPlayer || !gPlayerInfo.objNode) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {gPlayerInfo.coord.x, gPlayerInfo.coord.y, gPlayerInfo.coord.z}, .active = true};
	return true;
}

void OttoScript_Init(void)
{
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "OttoMatic-Android",
		.gameName = "Otto Matic",
		.spawnNative = OttoSpawnNativeItem,
		.spawnScripted = SpawnScriptedObject,
		.getPlayerCount = GetScriptPlayerCount,
		.getPlayer = GetScriptPlayer,
		.capabilities = {.terrainItems = true, .splineItems = true, .mapItems = false},
	};

	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	memset(gScriptTerrainItemOccupied, 0, sizeof(gScriptTerrainItemOccupied));
	memset(gScriptTerrainItemReclaimable, 0, sizeof(gScriptTerrainItemReclaimable));
	LogScriptStatus("init", status);
	gCurrentFrameContext = (PangeaScriptFrameContext){0};
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;

	status = PangeaScript_RegisterNativeItems(kNativeItems, (int)(sizeof(kNativeItems) / sizeof(kNativeItems[0])));
	LogScriptStatus("native item registration", status);

	status = PangeaScript_Reload();
	LogScriptStatus("reload", status);
	if (status == PANGEA_SCRIPT_OK)
		LogScriptStatus("onGameStart", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_START, &(PangeaScriptLevelContext){0}));
}

void OttoScript_Shutdown(void)
{
	LogScriptStatus("onGameShutdown", PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN, &(PangeaScriptLevelContext){.levelNum = gCurrentFrameContext.levelNum}));
	gCurrentFrameContext = (PangeaScriptFrameContext){0};
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;
	PangeaScript_Shutdown();
}

void OttoScript_LoadLevelConfig(int levelNum)
{
	PangeaScriptStatus status = PangeaScript_LoadLevelConfig(levelNum);
	LogScriptStatus("level config load", status);
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

void OttoScript_OnLevelLoad(int levelNum)
{
	gCurrentFrameContext = (PangeaScriptFrameContext)
	{
		.levelNum = levelNum,
	};
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, levelNum, "onLevelLoad");
}

void OttoScript_OnLevelStart(int levelNum)
{
	gCurrentFrameContext.levelNum = levelNum;
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, levelNum, "onLevelStart");
}

void OttoScript_OnCheckpointReset(void)
{
	(void) PangeaScript_ApplyObjectLifecycleToAll(
		&gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET);
}

void OttoScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	gCurrentFrameContext = context;

	PangeaScriptStatus status = PangeaScript_CallFrameHook(&context);
	LogScriptStatus("onFrame", status);
}

void OttoScript_OnLevelComplete(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, levelNum, "onLevelComplete");
}

void OttoScript_OnLevelUnload(int levelNum)
{
	CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, levelNum, "onLevelUnload");
	(void) PangeaScript_ApplyObjectLifecycleToAll(&gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
	PangeaScript_ResetObjects();
	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;
}

int OttoScript_RemapTerrainItemType(int levelNum, int itemType)
{
	return PangeaScript_RemapTerrainItemType(levelNum, itemType);
}

Boolean OttoScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z)
{
	const PangeaScriptTerrainReplacement* replacement =
		PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z);
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
		if (!CompleteScriptReplacement(handle, "terrain replacement stream-in"))
			return replacement->strict;
		return true;
	}

	LogScriptStatus("terrain replacement", status);
	return replacement->strict;
}

Boolean OttoScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z)
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

Boolean OttoScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum)
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

Boolean OttoScript_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex)
{
	const PangeaScriptSplineReplacement* replacement = PangeaScript_GetSplineReplacement(
		splineNum, itemIndex, itemPtr->type, itemPtr->placement);
	if (!replacement)
		return false;

	float x;
	float z;
	GetCoordOnSpline(&(*gSplineList)[splineNum], itemPtr->placement, &x, &z);
	const float y = GetTerrainY(x, z);
	PangeaScriptObjectHandle handle = {0};
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_SPLINE, .itemIndex = itemIndex, .nativeType = itemPtr->type,
		.splineNum = splineNum, .x = x, .y = y, .z = z, .placement = itemPtr->placement};
	if (PangeaScript_FindObjectBySource(&source, &handle))
		return true;
	PangeaScriptStatus status = SpawnScriptedObject(replacement->customObjectId, x, y, z, &handle);
	void* nativeObject = NULL;
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

void OttoScript_RegisterObjectNode(ObjNode* theNode, const char* objectType, PangeaScriptCapabilityLevel capabilityLevel, const char* const* tags, int tagCount)
{
	PangeaScriptObjectHandle handle = {0};
	PangeaScriptObjectRegistration registration;
	PangeaScriptStatus status;

	if (!theNode)
		return;

	if (theNode->ScriptObjectID > 0)
		return;

	registration = (PangeaScriptObjectRegistration)
	{
		.nativeObject = theNode,
		.ops = &kOttoObjectNodeOps,
		.objectType = objectType,
		.tags = tags,
		.tagCount = tagCount,
		.capabilityLevel = capabilityLevel,
	};

	status = PangeaScript_RegisterObject(&registration, &handle);
	if (status == PANGEA_SCRIPT_OK)
	{
		theNode->ScriptObjectID = handle.id;
		theNode->ScriptObjectGeneration = handle.generation;
	}
	else
	{
		theNode->ScriptObjectID = 0;
		theNode->ScriptObjectGeneration = 0;
	}

	LogScriptStatus("object registration", status);
}

void OttoScript_UnregisterObjectNode(ObjNode* theNode)
{
	PangeaScriptObjectHandle handle;

	if (!theNode || theNode->ScriptObjectID <= 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = theNode->ScriptObjectID,
		.generation = theNode->ScriptObjectGeneration,
	};

	(void) PangeaScript_UnregisterObject(handle);
	theNode->ScriptObjectID = 0;
	theNode->ScriptObjectGeneration = 0;
	theNode->ScriptVisualOffset.x = 0.0f;
	theNode->ScriptVisualOffset.y = 0.0f;
	theNode->ScriptVisualOffset.z = 0.0f;

	if (gCurrentScriptObject == theNode)
	{
		gCurrentScriptObject = NULL;
		gCurrentScriptObjectUsesGlobals = false;
	}
}

void OttoScript_OnObjectDeleted(ObjNode* theNode)
{
	if (theNode && theNode->ScriptDefinitionID[0] && theNode->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {theNode->ScriptObjectID, theNode->ScriptObjectGeneration};
		PangeaScriptObjectLifecycle lifecycle = theNode->TerrainItemPtr || theNode->SplineItemPtr
			? PANGEA_SCRIPT_OBJECT_STREAM_OUT
			: PANGEA_SCRIPT_OBJECT_DESTROY;
		(void) PangeaScript_ApplyObjectLifecycle(handle, &gCurrentFrameContext, lifecycle);
	}
}

void OttoScript_RunObjectFrame(ObjNode* theNode, Boolean usesGlobals)
{
	PangeaScriptObjectHandle handle;
	PangeaScriptObjectFrameResult result = {0};
	PangeaScriptStatus status;

	if (!theNode)
		return;

	theNode->ScriptVisualOffset.x = 0.0f;
	theNode->ScriptVisualOffset.y = 0.0f;
	theNode->ScriptVisualOffset.z = 0.0f;

	if (theNode->ScriptObjectID <= 0)
		return;

	handle = (PangeaScriptObjectHandle)
	{
		.id = theNode->ScriptObjectID,
		.generation = theNode->ScriptObjectGeneration,
	};

	gCurrentScriptObject = theNode;
	gCurrentScriptObjectUsesGlobals = usesGlobals;
	status = PangeaScript_CallObjectFrame(handle, &gCurrentFrameContext, &result);

	const char* error = PangeaScript_GetLastError();
	if (status == PANGEA_SCRIPT_BAD_ARGUMENT && error && strstr(error, "stale object handle"))
	{
		theNode->ScriptObjectID = 0;
		theNode->ScriptObjectGeneration = 0;
		if (theNode->Slot == HUMAN_SLOT)
		{
			OttoScript_RegisterHuman(theNode);
		}

		if (theNode->ScriptObjectID > 0)
		{
			handle = (PangeaScriptObjectHandle)
			{
				.id = theNode->ScriptObjectID,
				.generation = theNode->ScriptObjectGeneration,
			};
			status = PangeaScript_CallObjectFrame(handle, &gCurrentFrameContext, &result);
		}
	}

	gCurrentScriptObject = NULL;
	gCurrentScriptObjectUsesGlobals = false;

	if (status == PANGEA_SCRIPT_OK && result.hasPositionOffset)
	{
		theNode->ScriptVisualOffset.x = result.positionOffset.x;
		theNode->ScriptVisualOffset.y = result.positionOffset.y;
		theNode->ScriptVisualOffset.z = result.positionOffset.z;
	}

	LogScriptStatus("onObjectFrame", status);
}

void OttoScript_ApplyObjectVisualOffset(ObjNode* theNode)
{
	OGLPoint3D baseCoord;

	if (!theNode)
		return;

	if (theNode->ScriptVisualOffset.x == 0.0f &&
		theNode->ScriptVisualOffset.y == 0.0f &&
		theNode->ScriptVisualOffset.z == 0.0f)
	{
		return;
	}

	baseCoord = theNode->Coord;
	theNode->Coord.x += theNode->ScriptVisualOffset.x;
	theNode->Coord.y += theNode->ScriptVisualOffset.y;
	theNode->Coord.z += theNode->ScriptVisualOffset.z;
	UpdateObjectTransforms(theNode);
	theNode->Coord = baseCoord;
}

void OttoScript_RegisterHuman(ObjNode* human)
{
	int tagCount = 0;
	const char* const* tags = GetHumanTags(human->HumanType, &tagCount);
	const char* objectType = tagCount > 1 ? tags[1] : NULL;
	OttoScript_RegisterObjectNode(human, objectType, PANGEA_SCRIPT_CAPABILITY_FULL, tags, tagCount);
}

void OttoScript_RegisterPlayerObject(ObjNode* player)
{
	static const char* const tags[] = {"player"};
	OttoScript_RegisterObjectNode(player, "ottomatic.player", PANGEA_SCRIPT_CAPABILITY_FULL, tags, 1);
	OttoScript_OnPlayerSpawn(player);
}

void OttoScript_UnregisterHuman(ObjNode* human)
{
	OttoScript_UnregisterObjectNode(human);
}

Boolean OttoScript_OnDamage(ObjNode* source, float damage, int cause, float* outDamage)
{
	PangeaScriptDamageContext context;
	PangeaScriptDamageResult result = {0};
	PangeaScriptStatus status;
	ObjNode* player = gPlayerInfo.objNode;

	if (!outDamage)
		return true;
	*outDamage = damage;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return true;

	context = (PangeaScriptDamageContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.cause = cause,
		.damage = damage,
		.source = {0},
		.target = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	if (source && source->ScriptObjectID > 0 && source->ScriptObjectGeneration > 0)
		context.source = (PangeaScriptObjectHandle){source->ScriptObjectID, source->ScriptObjectGeneration};

	status = PangeaScript_CallDamageHook(&context, &result);
	LogScriptStatus("onDamage", status);
	if (status != PANGEA_SCRIPT_OK)
		return true;
	if (result.hasDamage)
		*outDamage = result.damage;
	return result.hasApplyDamage ? result.applyDamage : true;
}

void OttoScript_OnPlayerSpawn(ObjNode* player)
{
	PangeaScriptPlayerEventContext context;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = 0,
		.player = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onPlayerSpawn", PangeaScript_CallPlayerEvent(&context, "onPlayerSpawn"));
}

void OttoScript_OnPlayerRespawn(ObjNode* player)
{
	PangeaScriptPlayerEventContext context;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = 0,
		.player = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onPlayerRespawn", PangeaScript_CallPlayerEvent(&context, "onPlayerRespawn"));
}

void OttoScript_OnDamageApplied(float damage, int cause)
{
	PangeaScriptDamageContext context;
	ObjNode* player = gPlayerInfo.objNode;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptDamageContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.cause = cause,
		.damage = damage,
		.source = {0},
		.target = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onDamageApplied", PangeaScript_CallDamageAppliedHook(&context));
}

void OttoScript_OnDeath(int eventValue)
{
	PangeaScriptPlayerEventContext context;
	ObjNode* player = gPlayerInfo.objNode;

	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = eventValue,
		.player = {player->ScriptObjectID, player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onDeath", PangeaScript_CallPlayerEvent(&context, "onDeath"));
}

void OttoScript_RunHumanObjectFrame(ObjNode* human, Boolean usesGlobals)
{
	OttoScript_RunObjectFrame(human, usesGlobals);
}

void OttoScript_ApplyHumanVisualOffset(ObjNode* human)
{
	OttoScript_ApplyObjectVisualOffset(human);
}

#endif

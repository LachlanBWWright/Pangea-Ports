#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

#include "splineitems.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <limits.h>


extern bool PangeaScript_LoadCustomBG3D(FSSpec* spec, int group);
extern bool PangeaScript_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec);

#define SCRIPT_TERRAIN_ITEM_CAPACITY 256
static TerrainItemEntryType gScriptTerrainItems[SCRIPT_TERRAIN_ITEM_CAPACITY];
static bool gScriptTerrainItemOccupied[SCRIPT_TERRAIN_ITEM_CAPACITY];
static bool gScriptTerrainItemReclaimable[SCRIPT_TERRAIN_ITEM_CAPACITY];
static int gOttoLastWeaponHitScoreDelta = INT_MIN;

int OttoScript_GetLastWeaponHitScoreDelta(void)
{
	return gOttoLastWeaponHitScoreDelta;
}

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
#define OTTO_TERRAIN_NATIVE_ITEM(type) { .id = #type, .nativeType = type, .category = "terrain", .dependencySummary = "current level assets, terrain systems, and the native item initializer" },
	OTTO_TERRAIN_NATIVE_ITEM(1)
	OTTO_TERRAIN_NATIVE_ITEM(2)
	OTTO_TERRAIN_NATIVE_ITEM(3)
	OTTO_TERRAIN_NATIVE_ITEM(4)
	OTTO_TERRAIN_NATIVE_ITEM(5)
	OTTO_TERRAIN_NATIVE_ITEM(6)
	OTTO_TERRAIN_NATIVE_ITEM(7)
	OTTO_TERRAIN_NATIVE_ITEM(8)
	OTTO_TERRAIN_NATIVE_ITEM(9)
	OTTO_TERRAIN_NATIVE_ITEM(10)
	OTTO_TERRAIN_NATIVE_ITEM(11)
	OTTO_TERRAIN_NATIVE_ITEM(12)
	OTTO_TERRAIN_NATIVE_ITEM(13)
	OTTO_TERRAIN_NATIVE_ITEM(14)
	OTTO_TERRAIN_NATIVE_ITEM(15)
	OTTO_TERRAIN_NATIVE_ITEM(16)
	OTTO_TERRAIN_NATIVE_ITEM(17)
	OTTO_TERRAIN_NATIVE_ITEM(18)
	OTTO_TERRAIN_NATIVE_ITEM(19)
	OTTO_TERRAIN_NATIVE_ITEM(20)
	OTTO_TERRAIN_NATIVE_ITEM(21)
	OTTO_TERRAIN_NATIVE_ITEM(22)
	OTTO_TERRAIN_NATIVE_ITEM(23)
	OTTO_TERRAIN_NATIVE_ITEM(24)
	OTTO_TERRAIN_NATIVE_ITEM(25)
	OTTO_TERRAIN_NATIVE_ITEM(26)
	OTTO_TERRAIN_NATIVE_ITEM(27)
	OTTO_TERRAIN_NATIVE_ITEM(28)
	OTTO_TERRAIN_NATIVE_ITEM(29)
	OTTO_TERRAIN_NATIVE_ITEM(30)
	OTTO_TERRAIN_NATIVE_ITEM(31)
	OTTO_TERRAIN_NATIVE_ITEM(32)
	OTTO_TERRAIN_NATIVE_ITEM(33)
	OTTO_TERRAIN_NATIVE_ITEM(34)
	OTTO_TERRAIN_NATIVE_ITEM(35)
	OTTO_TERRAIN_NATIVE_ITEM(36)
	OTTO_TERRAIN_NATIVE_ITEM(37)
	OTTO_TERRAIN_NATIVE_ITEM(38)
	OTTO_TERRAIN_NATIVE_ITEM(39)
	OTTO_TERRAIN_NATIVE_ITEM(40)
	OTTO_TERRAIN_NATIVE_ITEM(41)
	OTTO_TERRAIN_NATIVE_ITEM(42)
	OTTO_TERRAIN_NATIVE_ITEM(43)
	OTTO_TERRAIN_NATIVE_ITEM(44)
	OTTO_TERRAIN_NATIVE_ITEM(45)
	OTTO_TERRAIN_NATIVE_ITEM(46)
	OTTO_TERRAIN_NATIVE_ITEM(47)
	OTTO_TERRAIN_NATIVE_ITEM(48)
	OTTO_TERRAIN_NATIVE_ITEM(49)
	OTTO_TERRAIN_NATIVE_ITEM(50)
	OTTO_TERRAIN_NATIVE_ITEM(51)
	OTTO_TERRAIN_NATIVE_ITEM(52)
	OTTO_TERRAIN_NATIVE_ITEM(53)
	OTTO_TERRAIN_NATIVE_ITEM(54)
	OTTO_TERRAIN_NATIVE_ITEM(55)
	OTTO_TERRAIN_NATIVE_ITEM(56)
	OTTO_TERRAIN_NATIVE_ITEM(57)
	OTTO_TERRAIN_NATIVE_ITEM(58)
	OTTO_TERRAIN_NATIVE_ITEM(59)
	OTTO_TERRAIN_NATIVE_ITEM(60)
	OTTO_TERRAIN_NATIVE_ITEM(61)
	OTTO_TERRAIN_NATIVE_ITEM(62)
	OTTO_TERRAIN_NATIVE_ITEM(63)
	OTTO_TERRAIN_NATIVE_ITEM(64)
	OTTO_TERRAIN_NATIVE_ITEM(65)
	OTTO_TERRAIN_NATIVE_ITEM(66)
	OTTO_TERRAIN_NATIVE_ITEM(67)
	OTTO_TERRAIN_NATIVE_ITEM(68)
	OTTO_TERRAIN_NATIVE_ITEM(69)
	OTTO_TERRAIN_NATIVE_ITEM(70)
	OTTO_TERRAIN_NATIVE_ITEM(71)
	OTTO_TERRAIN_NATIVE_ITEM(72)
	OTTO_TERRAIN_NATIVE_ITEM(73)
	OTTO_TERRAIN_NATIVE_ITEM(74)
	OTTO_TERRAIN_NATIVE_ITEM(75)
	OTTO_TERRAIN_NATIVE_ITEM(76)
	OTTO_TERRAIN_NATIVE_ITEM(77)
	OTTO_TERRAIN_NATIVE_ITEM(78)
	OTTO_TERRAIN_NATIVE_ITEM(79)
	OTTO_TERRAIN_NATIVE_ITEM(80)
	OTTO_TERRAIN_NATIVE_ITEM(81)
	OTTO_TERRAIN_NATIVE_ITEM(82)
	OTTO_TERRAIN_NATIVE_ITEM(83)
	OTTO_TERRAIN_NATIVE_ITEM(84)
	OTTO_TERRAIN_NATIVE_ITEM(85)
	OTTO_TERRAIN_NATIVE_ITEM(86)
	OTTO_TERRAIN_NATIVE_ITEM(87)
	OTTO_TERRAIN_NATIVE_ITEM(88)
	OTTO_TERRAIN_NATIVE_ITEM(89)
	OTTO_TERRAIN_NATIVE_ITEM(90)
	OTTO_TERRAIN_NATIVE_ITEM(91)
	OTTO_TERRAIN_NATIVE_ITEM(92)
	OTTO_TERRAIN_NATIVE_ITEM(93)
	OTTO_TERRAIN_NATIVE_ITEM(94)
	OTTO_TERRAIN_NATIVE_ITEM(95)
	OTTO_TERRAIN_NATIVE_ITEM(96)
	OTTO_TERRAIN_NATIVE_ITEM(97)
	OTTO_TERRAIN_NATIVE_ITEM(98)
	OTTO_TERRAIN_NATIVE_ITEM(99)
	OTTO_TERRAIN_NATIVE_ITEM(100)
	OTTO_TERRAIN_NATIVE_ITEM(101)
	OTTO_TERRAIN_NATIVE_ITEM(102)
	OTTO_TERRAIN_NATIVE_ITEM(103)
	OTTO_TERRAIN_NATIVE_ITEM(104)
	OTTO_TERRAIN_NATIVE_ITEM(105)
	OTTO_TERRAIN_NATIVE_ITEM(106)
	OTTO_TERRAIN_NATIVE_ITEM(107)
	OTTO_TERRAIN_NATIVE_ITEM(108)
#undef OTTO_TERRAIN_NATIVE_ITEM
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
	if (!PangeaScript_ObjectExists(handle))
	{
		object->ScriptObjectID = 0;
		object->ScriptObjectGeneration = 0;
		PangeaScript_ClearLastError();
		return;
	}
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

static void OttoScript_ReleaseCustomAssets(void)
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
		if (gBG3DContainerList[group])
			DisposeBG3DContainer(group);
		gNumObjectsInBG3DGroupList[group] = 0;
		gScriptModelCache[i].path[0] = '\0';
	}
}

static void MoveScriptedCustomObject(ObjNode* theNode)
{
	GetObjectInfo(theNode);
	if (!theNode->ScriptDeleteRequested)
		OttoScript_RunObjectFrame(theNode, true);
	if (theNode->ScriptDeleteRequested)
	{
		if (theNode->ScriptObjectID > 0)
		{
			PangeaScriptObjectHandle handle = { theNode->ScriptObjectID, theNode->ScriptObjectGeneration };
			if (PangeaScript_ObjectExists(handle))
			{
				if ((theNode->TerrainItemPtr || theNode->SplineItemPtr) && !theNode->ScriptStreamOutSent)
				{
					(void) PangeaScript_ApplyObjectLifecycle(handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
					theNode->ScriptStreamOutSent = true;
				}
				else if (!theNode->ScriptStreamOutSent)
					(void) PangeaScript_ApplyObjectLifecycle(handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
			}
		}
		OttoScript_UnregisterObjectNode(theNode);
		DeleteObject(theNode);
		return;
	}
	if (theNode->Skeleton && theNode->Skeleton->AnimHasStopped && !theNode->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = { theNode->ScriptObjectID, theNode->ScriptObjectGeneration };
		if (PangeaScript_ObjectExists(handle))
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

static bool IsSafeCustomAsset(const FSSpec* spec)
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
	short refNum;
	long fileSize;
	if (!modelPath || modelPath[0] == '\0')
		return -1;

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
	char modelDataPath[260];
	char skeletonDataPath[260];
	FSSpec modelSpec;
	FSSpec skeletonSpec;
	if (!definition || definition->modelPath[0] == '\0' || definition->skeletonPath[0] == '\0')
		return -1;

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
	if (!IsSafeCustomAsset(&modelSpec) || !IsSafeCustomAsset(&skeletonSpec))
		return -1;

	for (int i = 0; i < SKELETON_TYPE_SCRIPT_CUSTOM_COUNT; i++)
	{
		int skeletonType = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + i;
		if (gBG3DContainerList[MODEL_GROUP_SKELETONBASE + skeletonType])
			continue;
		if (!PangeaScript_LoadCustomSkeleton(skeletonType, &skeletonSpec, &modelSpec))
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
		if (!PangeaScript_ObjectExists(handle))
			return;
		if (whoNode && whoNode->ScriptObjectID > 0)
			other = (PangeaScriptObjectHandle){whoNode->ScriptObjectID, whoNode->ScriptObjectGeneration};
		if (other.id > 0 && !PangeaScript_ObjectExists(other))
			other = (PangeaScriptObjectHandle){0};
		(void) PangeaScript_CallObjectTriggerWithOther(handle, &gCurrentFrameContext, sideBits, true, other);
	}
}

void OttoScript_OnAnimationEvent(ObjNode* node, int eventValue)
{
	if (node && node->ScriptDefinitionID[0] && node->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = { node->ScriptObjectID, node->ScriptObjectGeneration };
		if (PangeaScript_ObjectExists(handle))
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
	UpdateObjectTransforms(node);

	return true;
}

static bool OttoObjectGetVelocity(void* nativeObject, PangeaScriptVector3* outVelocity)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outVelocity)
		return false;
	*outVelocity = (PangeaScriptVector3){node->Delta.x, node->Delta.y, node->Delta.z};
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

static bool OttoObjectGetRotation(void* nativeObject, PangeaScriptVector3* outRotation)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outRotation)
		return false;
	*outRotation = (PangeaScriptVector3){node->Rot.x, node->Rot.y, node->Rot.z};
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

static bool OttoObjectGetScale(void* nativeObject, float* outScale)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outScale)
		return false;
	*outScale = node->Scale.x;
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

static bool OttoObjectSetCollisionEnabled(void* nativeObject, bool enabled)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node) return false;
	if (enabled && (!node->ScriptActiveStateInitialized || node->ScriptActive)) node->StatusBits &= ~STATUS_BIT_NOCOLLISION;
	else node->StatusBits |= STATUS_BIT_NOCOLLISION;
	return true;
}

static bool OttoObjectGetCollisionEnabled(void* nativeObject, bool* outEnabled)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outEnabled || node->CType == INVALID_NODE_FLAG)
		return false;
	*outEnabled = (node->StatusBits & STATUS_BIT_NOCOLLISION) == 0;
	return true;
}

static bool OttoObjectGetActive(void* nativeObject, bool* outActive)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outActive)
		return false;
	*outActive = !node->ScriptActiveStateInitialized || node->ScriptActive;
	return true;
}

static bool OttoObjectSetActive(void* nativeObject, bool active)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node) return false;
	if (!node->ScriptDefinitionID[0]) return true;
	node->ScriptActiveStateInitialized = true;
	node->ScriptActive = active;
	if (active) node->StatusBits &= ~(STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION);
	else node->StatusBits |= STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION;
	return true;
}

static bool OttoObjectGetAnimation(void* nativeObject, int* outAnimation)
{
	ObjNode* node = (ObjNode*) nativeObject;
	if (!node || !outAnimation || !node->Skeleton)
		return false;
	*outAnimation = node->Skeleton->AnimNum;
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
	if (node == gCurrentScriptObject || (node->CType & CTYPE_TRIGGER) || node->TerrainItemPtr || node->SplineItemPtr)
	{
		if ((node->TerrainItemPtr || node->SplineItemPtr) && !node->ScriptStreamOutSent)
		{
			PangeaScriptObjectHandle handle = { node->ScriptObjectID, node->ScriptObjectGeneration };
			if (PangeaScript_ObjectExists(handle))
				(void) PangeaScript_ApplyObjectLifecycle(handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			node->ScriptStreamOutSent = true;
		}
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
	.getVelocity = OttoObjectGetVelocity,
	.getRotation = OttoObjectGetRotation,
	.getScale = OttoObjectGetScale,
	.getAnimation = OttoObjectGetAnimation,
	.getActive = OttoObjectGetActive,
	.getCollisionEnabled = OttoObjectGetCollisionEnabled,
	.setPosition = OttoObjectSetPosition,
	.setVelocity = OttoObjectSetVelocity,
	.setRotation = OttoObjectSetRotation,
	.setScale = OttoObjectSetScale,
	.setAnimation = OttoObjectSetAnimation,
	.setAnimationNamed = OttoObjectSetAnimationNamed,
	.setCollisionEnabled = OttoObjectSetCollisionEnabled,
	.setActive = OttoObjectSetActive,
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
	PangeaScriptStatus status = PangeaScript_CallObjectEvent(handle, &gCurrentFrameContext, "spawn");
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
		handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_IN);
	if (status == PANGEA_SCRIPT_OK && !PangeaScript_ObjectExists(handle))
		status = PANGEA_SCRIPT_RUNTIME_ERROR;
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
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {gPlayerInfo.coord.x, gPlayerInfo.coord.y, gPlayerInfo.coord.z}, .velocity = {gPlayerInfo.objNode->Delta.x, gPlayerInfo.objNode->Delta.y, gPlayerInfo.objNode->Delta.z}, .hasVelocity = true, .collisionEnabled = gPlayerInfo.objNode->CType != 0 && (gPlayerInfo.objNode->StatusBits & STATUS_BIT_NOCOLLISION) == 0, .hasCollisionEnabled = true, .health = gPlayerInfo.health, .hasHealth = true, .fuel = gPlayerInfo.fuel, .hasFuelState = true, .score = (int64_t) gScore, .hasScore = true, .lives = gPlayerInfo.lives, .hasLives = true, .activeWeapon = gPlayerInfo.currentWeaponType, .hasWeaponState = true, .active = true};
	if (gGameViewInfoPtr)
	{
		outPlayer->camera = (PangeaScriptVector3){gGameViewInfoPtr->cameraPlacement.cameraLocation.x, gGameViewInfoPtr->cameraPlacement.cameraLocation.y, gGameViewInfoPtr->cameraPlacement.cameraLocation.z};
		outPlayer->hasCameraState = true;
	}
	outPlayer->rotation = (PangeaScriptVector3){gPlayerInfo.objNode->Rot.x, gPlayerInfo.objNode->Rot.y, gPlayerInfo.objNode->Rot.z};
	outPlayer->hasRotation = true;
	outPlayer->aim = (PangeaScriptVector3){-sinf(gPlayerInfo.objNode->Rot.y), 0.0f, -cosf(gPlayerInfo.objNode->Rot.y)};
	outPlayer->hasAimState = true;
	for (int slot = 0; slot < MAX_INVENTORY_SLOTS && slot < PANGEA_SCRIPT_PLAYER_INVENTORY_CAPACITY; slot++)
	{
		if (gPlayerInfo.weaponInventory[slot].type == NO_INVENTORY_HERE) continue;
		outPlayer->weapons[outPlayer->weaponCount++] = (PangeaScriptPlayerInventoryEntry){gPlayerInfo.weaponInventory[slot].type, gPlayerInfo.weaponInventory[slot].quantity};
	}
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
	if (playerNum != 0 || lives < 0 || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo.lives = lives;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerScore(int playerNum, int64_t score)
{
	if (playerNum != 0 || score < 0 || score > UINT32_MAX || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gScore = (uint32_t) score;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerWeaponQuantity(int playerNum, int weaponType, int quantity)
{
	int emptySlot = -1;
	if (playerNum != 0 || weaponType < 0 || weaponType >= NUM_WEAPON_TYPES || quantity < 0 || quantity > 99 || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	for (int slot = 0; slot < MAX_INVENTORY_SLOTS; slot++)
	{
		if (gPlayerInfo.weaponInventory[slot].type == weaponType)
		{
			if (quantity == 0) gPlayerInfo.weaponInventory[slot].type = NO_INVENTORY_HERE;
			gPlayerInfo.weaponInventory[slot].quantity = (short) quantity;
			return PANGEA_SCRIPT_OK;
		}
		if (emptySlot < 0 && gPlayerInfo.weaponInventory[slot].type == NO_INVENTORY_HERE) emptySlot = slot;
	}
	if (quantity == 0 || emptySlot < 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerInfo.weaponInventory[emptySlot] = (WeaponInventoryType){weaponType, (short) quantity};
	if (gPlayerInfo.currentWeaponType == NO_INVENTORY_HERE) gPlayerInfo.currentWeaponType = weaponType;
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
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus SetScriptPlayerVelocity(int playerNum, const PangeaScriptVector3* velocity)
{
	if (playerNum != 0 || !velocity || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	return OttoObjectSetVelocity(gPlayerInfo.objNode, velocity) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
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
		.setPlayerHealth = SetScriptPlayerHealth,
		.setPlayerLives = SetScriptPlayerLives,
		.setPlayerScore = SetScriptPlayerScore,
		.setPlayerWeaponQuantity = SetScriptPlayerWeaponQuantity,
		.setPlayerInvulnerable = SetScriptPlayerInvulnerable,
		.setPlayerPosition = SetScriptPlayerPosition,
		.setPlayerVelocity = SetScriptPlayerVelocity,
		.capabilities = PANGEA_SCRIPT_OTTO_MATIC_CAPABILITIES,
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

static const char* OttoScript_LevelName(int levelNum)
{
	static const char* levelNames[] =
	{
		"farm", "blob", "blob-boss", "apocalypse", "cloud",
		"jungle", "jungle-boss", "fire-ice", "saucer", "brain-boss",
	};
	if (levelNum < 0 || levelNum >= (int)(sizeof(levelNames) / sizeof(levelNames[0]))) return NULL;
	return levelNames[levelNum];
}

static const char* OttoScript_PlayerMode(int levelNum)
{
	return levelNum == LEVEL_NUM_SAUCER ? "saucer" : "robot";
}

static void CallLevelHook(PangeaScriptHook hook, int levelNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = levelNum,
		.levelName = OttoScript_LevelName(levelNum),
		.playerMode = OttoScript_PlayerMode(levelNum),
	};

	PangeaScriptStatus status = PangeaScript_CallLevelHook(hook, &context);
	LogScriptStatus(action, status);
}

static void CallSaveLoadHook(PangeaScriptHook hook, int saveSlot, const char* action)
{
	char slotName[32];
	snprintf(slotName, sizeof(slotName), "slot-%d", saveSlot);
	const PangeaScriptLevelContext context =
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.levelName = slotName,
	};
	LogScriptStatus(action, PangeaScript_CallLevelHook(hook, &context));
}

void OttoScript_OnSave(int saveSlot)
{
	CallSaveLoadHook(PANGEA_SCRIPT_HOOK_SAVE, saveSlot, "onSave");
}

void OttoScript_OnLoad(int saveSlot)
{
	CallSaveLoadHook(PANGEA_SCRIPT_HOOK_LOAD, saveSlot, "onLoad");
}

void OttoScript_OnLevelLoad(int levelNum)
{
	gCurrentFrameContext = (PangeaScriptFrameContext)
	{
		.levelNum = levelNum,
		.levelName = OttoScript_LevelName(levelNum),
		.playerMode = OttoScript_PlayerMode(levelNum),
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

void OttoScript_OnCheckpointReached(int checkpointNum)
{
	PangeaScriptPlayerEventContext context;
	ObjNode* player = gPlayerInfo.objNode;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0 ||
		!PangeaScript_ObjectExists((PangeaScriptObjectHandle){player->ScriptObjectID, player->ScriptObjectGeneration}))
		return;
	context = (PangeaScriptPlayerEventContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.eventValue = checkpointNum,
		.player = {player->ScriptObjectID, (uint32_t)player->ScriptObjectGeneration},
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	LogScriptStatus("onCheckpointReached", PangeaScript_CallPlayerEvent(&context, "onCheckpointReached"));
}

void OttoScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds)
{
	const PangeaScriptFrameContext context =
	{
		.levelNum = levelNum,
		.levelName = OttoScript_LevelName(levelNum),
		.playerMode = OttoScript_PlayerMode(levelNum),
		.frameNum = frameNum,
		.deltaSeconds = deltaSeconds,
		.levelTimeSeconds = levelTimeSeconds,
	};
	gCurrentFrameContext = context;
	PangeaScript_ExpireTriggerContacts(&context);
	(void)PangeaScript_ApplyDeferredActions(&context);

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
	OttoScript_ReleaseCustomAssets();
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
			if (!replacement->strict)
				PangeaScript_ClearLastError();
			return replacement->strict;
		}
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

int OttoScript_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z)
{
	static TerrainItemEntryType probeItem;
	PangeaScriptObjectHandle handle = {0};
	const PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN, .itemIndex = itemIndex, .nativeType = nativeType,
		.x = x, .y = GetTerrainY(x, z), .z = z};
	memset(&probeItem, 0, sizeof(probeItem));
	if (!PangeaScript_GetTerrainReplacement(itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!OttoScript_TryReplaceTerrainItem(&probeItem, itemIndex, nativeType, x, z))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_FindObjectBySource(&source, &handle) || handle.id <= 0 || handle.generation == 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_DeleteObject(handle))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PANGEA_SCRIPT_OK;
}

int OttoScript_ProbeSaveLoadJS(int saveSlot)
{
	if (saveSlot < 0 || saveSlot >= NUM_SAVE_SLOTS)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!SaveGame(saveSlot) || !LoadSavedGame(saveSlot))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PANGEA_SCRIPT_OK;
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

int OttoScript_ProbeSplineReplacementJS(int splineNum, int itemIndex, int nativeType, float placement)
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
	if (!OttoScript_TryReplaceSplineItem(&probeItem, splineNum, itemIndex))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	GetCoordOnSpline(&(*gSplineList)[splineNum], placement, &x, &z);
	source = (PangeaScriptObjectSource){
		.kind = PANGEA_SCRIPT_SOURCE_SPLINE, .itemIndex = itemIndex, .nativeType = nativeType,
		.splineNum = splineNum, .x = x, .y = GetTerrainY(x, z), .z = z, .placement = placement};
	if (!PangeaScript_FindObjectBySource(&source, &handle) || handle.id <= 0 || handle.generation == 0)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (!PangeaScript_DeleteObject(handle))
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	return PANGEA_SCRIPT_OK;
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

	if (PangeaScript_ObjectExists(handle))
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
		if (PangeaScript_ObjectExists(handle) && (theNode->TerrainItemPtr || theNode->SplineItemPtr) && !theNode->ScriptStreamOutSent)
		{
			(void) PangeaScript_ApplyObjectLifecycle(handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_STREAM_OUT);
			theNode->ScriptStreamOutSent = true;
		}
		else if (PangeaScript_ObjectExists(handle) && !theNode->ScriptStreamOutSent)
			(void) PangeaScript_ApplyObjectLifecycle(handle, &gCurrentFrameContext, PANGEA_SCRIPT_OBJECT_DESTROY);
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
	if (!PangeaScript_ObjectExists(handle))
	{
		theNode->ScriptObjectID = 0;
		theNode->ScriptObjectGeneration = 0;
		if (theNode->Slot == HUMAN_SLOT)
			OttoScript_RegisterHuman(theNode);
		if (theNode->ScriptObjectID <= 0)
		{
			PangeaScript_ClearLastError();
			return;
		}
		handle = (PangeaScriptObjectHandle)
		{
			.id = theNode->ScriptObjectID,
			.generation = theNode->ScriptObjectGeneration,
		};
		PangeaScript_ClearLastError();
	}

	gCurrentScriptObject = theNode;
	gCurrentScriptObjectUsesGlobals = usesGlobals;
	status = PangeaScript_CallObjectFrame(handle, &gCurrentFrameContext, &result);

	const char* error = PangeaScript_GetLastError();
	if (status == PANGEA_SCRIPT_BAD_ARGUMENT && error && strstr(error, "stale object handle"))
	{
		bool recovered = false;
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
			recovered = status == PANGEA_SCRIPT_OK;
		}
		else
		{
			status = PANGEA_SCRIPT_OK;
			recovered = true;
		}
		if (recovered)
			PangeaScript_ClearLastError();
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
	PangeaScriptObjectHandle target;

	if (!outDamage)
		return true;
	*outDamage = damage;
	if (!player || player->ScriptObjectID <= 0 || player->ScriptObjectGeneration <= 0)
		return true;
	target = (PangeaScriptObjectHandle){player->ScriptObjectID, (uint32_t) player->ScriptObjectGeneration};
	if (!PangeaScript_ObjectExists(target))
		return true;

	context = (PangeaScriptDamageContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.cause = cause,
		.damage = damage,
		.source = {0},
		.target = target,
		.position = {player->Coord.x, player->Coord.y, player->Coord.z},
	};
	if (source && source->ScriptObjectID > 0 && source->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = {source->ScriptObjectID, source->ScriptObjectGeneration};
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

Boolean OttoScript_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget)
{
	PangeaScriptWeaponHitContext context;
	PangeaScriptWeaponHitResult result = {0};
	PangeaScriptStatus status;
	gOttoLastWeaponHitScoreDelta = INT_MIN;
	if (!outDamage || !outDestroyTarget)
		return true;
	*outDamage = damage;
	*outDestroyTarget = false;
	context = (PangeaScriptWeaponHitContext)
	{
		.levelNum = gCurrentFrameContext.levelNum,
		.playerNum = 0,
		.weaponType = weapon ? weapon->Kind : -1,
		.targetType = target ? target->Kind : -1,
		.targetFlags = target ? target->StatusBits : 0,
		.damage = damage,
		.weaponId = "ottomatic.projectile",
		.weapon = {0},
		.target = {0},
		.position = target ? (PangeaScriptVector3){target->Coord.x, target->Coord.y, target->Coord.z} : (PangeaScriptVector3){0},
	};
	if (weapon && weapon->ScriptObjectID > 0 && weapon->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = {weapon->ScriptObjectID, weapon->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			context.weapon = handle;
	}
	if (target && target->ScriptObjectID > 0 && target->ScriptObjectGeneration > 0)
	{
		PangeaScriptObjectHandle handle = {target->ScriptObjectID, target->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			context.target = handle;
	}
	status = PangeaScript_CallWeaponHitHook(&context, &result);
	LogScriptStatus("onWeaponHit", status);
	if (status != PANGEA_SCRIPT_OK)
	{
		gOttoLastWeaponHitScoreDelta = -100 - (int)status;
		return true;
	}
	gOttoLastWeaponHitScoreDelta = result.scoreDelta;
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

Boolean OttoScript_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId)
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
		.levelNum = gCurrentFrameContext.levelNum,
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

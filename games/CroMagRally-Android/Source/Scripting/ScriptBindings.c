#ifdef PANGEA_ENABLE_SCRIPTING

#include "ScriptBindings.h"

#include "structs.h"

#include <stdio.h>
#include <string.h>

static void LogScriptStatus(const char* action, PangeaScriptStatus status);

static PangeaScriptFrameContext gScriptFrameContext;
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
	if (!MakeDataAssetPath(modelPath, dataPath, sizeof(dataPath)) ||
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, dataPath, &spec) != noErr) return -1;
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

static bool CroMagScript_SetObjectRotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	ObjNode* obj = (ObjNode*)nativeObject;
	if (!obj || !rotation || obj->CType == INVALID_NODE_FLAG) return false;
	obj->Rot = (OGLVector3D){rotation->x, rotation->y, rotation->z};
	UpdateObjectTransforms(obj);
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

	if (obj->ScriptDefinitionID[0]) obj->ScriptDeleteRequested = true;
	else { CroMagScript_UnregisterObject(obj); DeleteObject(obj); }
	return true;
}

static const PangeaScriptObjectOps kCroMagPlayerObjectOps =
{
	.getPosition = CroMagScript_GetObjectPosition,
	.setPosition = CroMagScript_SetObjectPosition,
	.setVelocity = CroMagScript_SetObjectVelocity,
	.setRotation = CroMagScript_SetObjectRotation,
	.setScale = CroMagScript_SetObjectScale,
	.setAnimation = CroMagScript_SetObjectAnimation,
	.setAnimationNamed = CroMagScript_SetObjectAnimationNamed,
	.deleteObject = CroMagScript_DeleteObject,
};

void CroMagScript_CacheFrameContext(const PangeaScriptFrameContext* ctx)
{
	if (ctx)
		gScriptFrameContext = *ctx;
}

void CroMagScript_ResetObjectRegistry(void)
{
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
	(void) PangeaScript_UnregisterObject(handle);
	obj->ScriptObjectID = 0;
	obj->ScriptObjectGeneration = 0;
	obj->ScriptVisualOffset = (OGLVector3D){0};
}

void CroMagScript_RegisterPlayerObject(ObjNode* playerObj)
{
	CroMagScript_RegisterObject(playerObj, "cromag.player", "player");
}

void CroMagScript_UnregisterPlayerObject(ObjNode* playerObj)
{
	CroMagScript_UnregisterObject(playerObj);
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

void CroMagScript_RunObjectFrame(ObjNode* obj)
{
	CroMagScript_ApplyObjectScripting(obj);
	if (obj->CType == INVALID_NODE_FLAG) return;
	if (obj->ScriptDeleteRequested) { DeleteObject(obj); return; }
	if (obj->Skeleton && obj->Skeleton->AnimHasStopped && !obj->ScriptAnimationCompletionSent)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "animationComplete");
		obj->ScriptAnimationCompletionSent = true;
	}
}

void CroMagScript_OnObjectDeleted(ObjNode* obj)
{
	if (obj && obj->ScriptDefinitionID[0] && obj->ScriptObjectID > 0)
	{
		PangeaScriptObjectHandle handle = {obj->ScriptObjectID, (uint32_t)obj->ScriptObjectGeneration};
		(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "destroy");
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

static void ApplyScriptedCollision(ObjNode* object, PangeaScriptCollisionPreset preset)
{
	short radius;
	if (preset == PANGEA_SCRIPT_COLLISION_NONE) return;
	radius = (short)object->BoundingSphereRadius;
	SetObjectCollisionBounds(object, radius, -radius, -radius, radius, radius, -radius);
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
	(void)who;
	if (!trigger || !trigger->ScriptObjectID) return;
	PangeaScriptObjectHandle handle = {trigger->ScriptObjectID, (uint32_t)trigger->ScriptObjectGeneration};
	(void)PangeaScript_CallObjectTrigger(handle, &gScriptFrameContext, sideBits, true);
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
	CroMagScript_RegisterObject(object, definition->id, "customObject");
	if (!object->ScriptObjectID) { DeleteObject(object); return PANGEA_SCRIPT_RUNTIME_ERROR; }
	if (outHandle) *outHandle = (PangeaScriptObjectHandle){object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
	PangeaScriptObjectHandle handle = {object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
	(void)PangeaScript_CallObjectEvent(handle, &gScriptFrameContext, "spawn");
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
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {gPlayerInfo[playerNum].coord.x, gPlayerInfo[playerNum].coord.y, gPlayerInfo[playerNum].coord.z}, .active = true};
	return true;
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

static void CallRaceHook(PangeaScriptHook hook, int trackNum, const char* action)
{
	const PangeaScriptLevelContext context =
	{
		.levelNum = trackNum,
		.levelName = NULL,
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
	};
	CroMagScript_CacheFrameContext(&context);

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
		.networked = false,
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

Boolean CroMagScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z)
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

Boolean CroMagScript_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex)
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

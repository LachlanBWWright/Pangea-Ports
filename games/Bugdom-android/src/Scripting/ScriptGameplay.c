#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"
#include <math.h>
#include <string.h>

static const struct
{
	const char* name;
	unsigned short key;
} kActions[] = {
	{"jump", kKey_Jump}, {"attack", kKey_Kick},
	{"morph", kKey_MorphPlayer}, {"buddyAttack", kKey_BuddyAttack},
	{"forward", kKey_Forward}, {"backward", kKey_Backward},
	{"left", kKey_Left}, {"right", kKey_Right},
	{"autoWalk", kKey_AutoWalk}, {"pause", kKey_Pause},
	{"cameraLeft", kKey_SwivelCameraLeft}, {"cameraRight", kKey_SwivelCameraRight},
	{"zoomIn", kKey_ZoomIn}, {"zoomOut", kKey_ZoomOut},
};

static PangeaScriptInputSnapshot gInput;

bool BugdomScript_NativeRulesEnabled(void)
{
	return !PangeaScript_RulesAreScriptControlled();
}

bool BugdomScript_OwnedRuleTrigger(ObjNode* object, ObjNode* player, Byte sides, const char* id)
{
	if (BugdomScript_NativeRulesEnabled())
		return false;
	BugdomScript_RegisterObject(object, id, "rule-trigger");
	BugdomScript_OnCustomTrigger(object, player, sides);
	return true;
}

void BugdomScript_ResetGameplayInput(void)
{
	memset(&gInput, 0, sizeof(gInput));
}

void BugdomScript_CaptureGameplayInput(void)
{
	gInput.actionCount = sizeof(kActions) / sizeof(kActions[0]);
	for (int i = 0; i < gInput.actionCount; i++)
	{
		bool held = GetKeyState(kActions[i].key);
		bool previousHeld = gInput.actions[i].held;
		gInput.actions[i] = (PangeaScriptInputAction){
			.name = kActions[i].name, .held = held,
			.pressed = GetNewKeyState(kActions[i].key),
			.released = previousHeld && !held,
		};
	}
	TQ3Vector2D look = GetThumbStickVector(true);
	float dx, dy;
	GetMouseDelta(&dx, &dy);
	float movementScale = 1600.0f * gFramesPerSecondFrac;
	gInput.moveX = movementScale > 0 ? fmaxf(-1, fminf(1, dx / movementScale)) : 0;
	gInput.moveY = movementScale > 0 ? fmaxf(-1, fminf(1, -dy / movementScale)) : 0;
	gInput.lookX = look.x;
	gInput.lookY = -look.y;
	gInput.hasAim = gPlayerObj != NULL;
	if (gPlayerObj)
		gInput.aim = (PangeaScriptVector3){-sinf(gPlayerObj->Rot.y), 0, -cosf(gPlayerObj->Rot.y)};
}

static bool GetInput(int playerNum, PangeaScriptInputSnapshot* output)
{
	if (playerNum != 0 || !output)
		return false;
	*output = gInput;
	return true;
}

static bool SampleTerrain(float x, float z, PangeaScriptTerrainSample* output)
{
	if (!output || !gFloorMap || !isfinite(x) || !isfinite(z)
		|| x < 0 || z < 0 || x >= gTerrainTileWidth * TERRAIN_POLYGON_SIZE
		|| z >= gTerrainTileDepth * TERRAIN_POLYGON_SIZE)
		return false;
	TQ3Vector3D previousNormal = gRecentTerrainNormal[FLOOR];
	float height = GetTerrainHeightAtCoord(x, z, FLOOR);
	TQ3Vector3D normal = gRecentTerrainNormal[FLOOR];
	gRecentTerrainNormal[FLOOR] = previousNormal;
	*output = (PangeaScriptTerrainSample){
		.height = height, .normal = {normal.x, normal.y, normal.z}, .surfaceType = -1,
	};
	return true;
}

static const PangeaScriptCollisionCategory kCollisionCategories[] = {
	{"player", CTYPE_PLAYER}, {"enemy", CTYPE_ENEMY}, {"trigger", CTYPE_TRIGGER},
	{"solid", CTYPE_MISC | CTYPE_MPLATFORM | CTYPE_IMPENETRABLE},
	{"platform", CTYPE_MPLATFORM}, {"liquid", CTYPE_LIQUID},
	{"hazard", CTYPE_HURTME | CTYPE_HURTNOKNOCK | CTYPE_SPIKED},
	{"kickable", CTYPE_KICKABLE}, {"all", 0xffffffffu},
};

static bool OverlapBox(const PangeaScriptBounds* bounds, unsigned int mask,
	PangeaScriptObjectHandle* results, int capacity, int* count)
{
	if (!bounds || !count || capacity < 0 || (capacity && !results))
		return false;
	*count = 0;
	for (ObjNode* node = gFirstNodePtr; node && *count < capacity; node = node->NextNode)
	{
		if (node->Slot >= SLOT_OF_DUMB)
			break;
		if (node->CType == INVALID_NODE_FLAG || !(node->CType & mask)
			|| !node->CBits || (node->StatusBits & STATUS_BIT_NOCOLLISION))
			continue;
		if (!DoSimpleBoxCollisionAgainstObject(bounds->max.y, bounds->min.y,
			bounds->min.x, bounds->max.x, bounds->max.z, bounds->min.z, node))
			continue;
		BugdomScript_RegisterObject(node, "bugdom.collision-object", "collision-object");
		PangeaScriptObjectHandle handle = {node->ScriptObjectID, (uint32_t) node->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle))
			results[(*count)++] = handle;
	}
	return true;
}

const PangeaScriptGameplayOps kBugdomGameplayOps = {
	.getInput = GetInput, .sampleTerrain = SampleTerrain, .overlapBox = OverlapBox,
	.collisionCategories = kCollisionCategories,
	.collisionCategoryCount = sizeof(kCollisionCategories) / sizeof(kCollisionCategories[0]),
	.getState = BugdomScript_GetState, .setState = BugdomScript_SetState,
	.stateFields = kBugdomStateFields, .stateFieldCount = 15,
	.complete = BugdomScript_Complete,
	.operationMask = (1u << PANGEA_SCRIPT_OP_MOUNT) | (1u << PANGEA_SCRIPT_OP_DISMOUNT)
		| (1u << PANGEA_SCRIPT_OP_GRAB) | (1u << PANGEA_SCRIPT_OP_DROP)
		| (1u << PANGEA_SCRIPT_OP_TRANSFORM) | (1u << PANGEA_SCRIPT_OP_RESPAWN)
		| (1u << PANGEA_SCRIPT_OP_COMMAND_BUDDY) | (1u << PANGEA_SCRIPT_OP_RELEASE_BUDDY),
	.runOperation = BugdomScript_RunOperation, .getRelations = BugdomScript_GetRelations,
};

#endif

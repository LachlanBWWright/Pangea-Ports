#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "ScriptBindings.h"
#include <math.h>
#include <string.h>

static bool GetGameplayInput(int playerNum, PangeaScriptInputSnapshot* output)
{
	if (playerNum != 0 || !output || !gInGameNow)
		return false;
	static const struct { const char* name; int need; } actions[] = {
		{"left", kNeed_TurnLeft}, {"right", kNeed_TurnRight},
		{"forward", kNeed_Forward}, {"backward", kNeed_Backward},
		{"attack", kNeed_Kick}, {"interact", kNeed_PickupDrop},
		{"autoWalk", kNeed_AutoWalk}, {"jump", kNeed_Jump},
		{"launchBuddy", kNeed_LaunchBuddy}, {"cameraMode", kNeed_CameraMode},
		{"pause", kNeed_UIPause}, {"confirm", kNeed_UIConfirm}, {"cancel", kNeed_UIBack},
	};
	*output = (PangeaScriptInputSnapshot){0};
	output->actionCount = sizeof(actions) / sizeof(actions[0]);
	for (int i = 0; i < output->actionCount; i++)
		output->actions[i] = (PangeaScriptInputAction){actions[i].name,
			IsNeedActive(actions[i].need), IsNeedDown(actions[i].need), IsNeedUp(actions[i].need)};
	output->moveX = GetNeedAxis1D(kNeed_TurnLeft, kNeed_TurnRight);
	output->moveY = GetNeedAxis1D(kNeed_Backward, kNeed_Forward);
	output->lookX = GetNeedAxis1D(kNeed_CameraLeft, kNeed_CameraRight);
	return true;
}

static bool SampleGameplayTerrain(float x, float z, PangeaScriptTerrainSample* output)
{
	if (!output || !isfinite(x) || !isfinite(z) || x < 0 || z < 0 ||
		x >= gTerrainUnitWidth || z >= gTerrainUnitDepth)
		return false;
	OGLVector3D previousNormal = gRecentTerrainNormal;
	float height = GetTerrainY(x, z);
	OGLVector3D normal = gRecentTerrainNormal;
	gRecentTerrainNormal = previousNormal;
	if (height == ILLEGAL_TERRAIN_Y)
		return false;
	*output = (PangeaScriptTerrainSample){.height = height, .surfaceType = -1,
		.normal = {normal.x, normal.y, normal.z}};
	output->hasWater = GetWaterY(x, z, &output->waterHeight);
	return true;
}

static bool OverlapGameplayBox(const PangeaScriptBounds* bounds, unsigned int mask,
	PangeaScriptObjectHandle* results, int capacity, int* count)
{
	if (!bounds || !count || capacity < 0 || (capacity > 0 && !results))
		return false;
	if (!isfinite(bounds->min.x) || !isfinite(bounds->min.y) || !isfinite(bounds->min.z) ||
		!isfinite(bounds->max.x) || !isfinite(bounds->max.y) || !isfinite(bounds->max.z) ||
		bounds->min.x > bounds->max.x || bounds->min.y > bounds->max.y || bounds->min.z > bounds->max.z)
		return false;
	*count = 0;
	for (ObjNode* object = gFirstNodePtr; object; object = object->NextNode)
	{
		if (object->CType == INVALID_NODE_FLAG || !(object->CType & mask) ||
			(object->StatusBits & (STATUS_BIT_NOCOLLISION | STATUS_BIT_DETACHED)))
			continue;
		if (!DoSimpleBoxCollisionAgainstObject(bounds->max.y, bounds->min.y,
			bounds->min.x, bounds->max.x, bounds->max.z, bounds->min.z, object))
			continue;
		Bugdom2Script_RegisterObject(object, "bugdom2.collision-object", "collision");
		PangeaScriptObjectHandle handle = {object->ScriptObjectID, object->ScriptObjectGeneration};
		if (PangeaScript_ObjectExists(handle) && *count < capacity)
			results[(*count)++] = handle;
	}
	return true;
}

static const PangeaScriptCollisionCategory kCollisionCategories[] = {
	{"player", CTYPE_PLAYER}, {"enemy", CTYPE_ENEMY}, {"solid", CTYPE_MISC},
	{"trigger", CTYPE_TRIGGER}, {"pickup", CTYPE_PICKUP}, {"powerup", CTYPE_POWERUP},
	{"hazard", CTYPE_HURTME}, {"platform", CTYPE_MPLATFORM},
	{"kickable", CTYPE_KICKABLE}, {"cameraBlocker", CTYPE_BLOCKCAMERA},
	{"rayBlocker", CTYPE_BLOCKRAYS},
};

static const PangeaScriptStateField kStateFields[] = {
	{"miceRescued", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, true},
	{"miceTotal", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 32767, false},
	{"drowningMiceRescued", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, false},
	{"drowningMiceRequired", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, false},
	{"beesFreed", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, false},
	{"glidePower", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 1, true},
	{"shieldTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, true},
	{"invulnerabilityTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, true},
	{"hasMap", PANGEA_SCRIPT_STATE_BOOLEAN, true, 0, 0, true},
	{"levelComplete", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 0, false},
	{"buddyCount", PANGEA_SCRIPT_STATE_NUMBER, false, 0, MAX_BUDDY_BUGS, true},
};

static int FindStateField(const char* key)
{
	if (!key) return -1;
	for (unsigned int i = 0; i < sizeof(kStateFields) / sizeof(kStateFields[0]); i++)
		if (strcmp(key, kStateFields[i].key) == 0) return (int)i;
	return -1;
}

static bool GetGameplayState(int player, const char* key, PangeaScriptStateValue* output)
{
	int field = FindStateField(key);
	if (player != 0 || field < 0 || !output || !gInGameNow || !gPlayerInfo.objNode)
		return false;
	*output = (PangeaScriptStateValue){.type = kStateFields[field].type};
	switch (field)
	{
		case 0: output->number = gPlayerInfo.numMiceRescued; break;
		case 1: output->number = gNumMice; break;
		case 2: output->number = gNumDrowningMiceRescued; break;
		case 3: output->number = gNumDrowingMiceToRescue; break;
		case 4: output->number = gNumFreedBees; break;
		case 5: output->number = gPlayerInfo.glidePower; break;
		case 6: output->number = gPlayerInfo.shieldTimer; break;
		case 7: output->number = gPlayerInfo.invincibilityTimer; break;
		case 8: output->boolean = gPlayerInfo.hasMap; break;
		case 9: output->boolean = gLevelCompleted; break;
		case 10: output->number = gPlayerInfo.numBuddyBugs; break;
	}
	return true;
}

static PangeaScriptStatus SetGameplayState(int player, const char* key, const PangeaScriptStateValue* value)
{
	int field = FindStateField(key);
	if (player != 0 || field < 0 || !value || !gInGameNow || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	const PangeaScriptStateField* descriptor = &kStateFields[field];
	if (!descriptor->writable) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (value->type != descriptor->type) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (value->type == PANGEA_SCRIPT_STATE_NUMBER &&
		(!isfinite(value->number) || value->number < descriptor->min || value->number > descriptor->max ||
		(field <= 4 && floor(value->number) != value->number)))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	switch (field)
	{
		case 0: gPlayerInfo.numMiceRescued = (short)value->number; break;
		case 2: gNumDrowningMiceRescued = (int)value->number; break;
		case 3: gNumDrowingMiceToRescue = (int)value->number; break;
		case 4: gNumFreedBees = (int)value->number; break;
		case 5: gPlayerInfo.glidePower = (float)value->number; break;
		case 6: gPlayerInfo.shieldTimer = (float)value->number; break;
		case 7: gPlayerInfo.invincibilityTimer = (float)value->number; break;
		case 8: gPlayerInfo.hasMap = value->boolean; break;
		default: return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus CompleteGameplay(int player, bool success)
{
	if (player != 0 || !gInGameNow || !gPlayerInfo.objNode) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!success) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	StartLevelCompletion(0);
	return PANGEA_SCRIPT_OK;
}

const PangeaScriptGameplayOps kBugdom2GameplayOps = {
	.getInput = GetGameplayInput, .sampleTerrain = SampleGameplayTerrain,
	.overlapBox = OverlapGameplayBox, .collisionCategories = kCollisionCategories,
	.collisionCategoryCount = sizeof(kCollisionCategories) / sizeof(kCollisionCategories[0]),
	.getState = GetGameplayState, .setState = SetGameplayState,
	.stateFields = kStateFields, .stateFieldCount = sizeof(kStateFields) / sizeof(kStateFields[0]),
	.complete = CompleteGameplay,
	.operationMask = (1u << PANGEA_SCRIPT_OP_CARRY) | (1u << PANGEA_SCRIPT_OP_DROP) |
		(1u << PANGEA_SCRIPT_OP_HANDOVER) | (1u << PANGEA_SCRIPT_OP_INTERACT) |
		(1u << PANGEA_SCRIPT_OP_MOUNT) | (1u << PANGEA_SCRIPT_OP_DISMOUNT) |
		(1u << PANGEA_SCRIPT_OP_COMMAND_BUDDY) | (1u << PANGEA_SCRIPT_OP_RELEASE_BUDDY) |
		(1u << PANGEA_SCRIPT_OP_FIRE) | (1u << PANGEA_SCRIPT_OP_TRANSFORM),
	.runOperation = Bugdom2Script_RunOperation, .getRelations = Bugdom2Script_GetRelations,
};
#endif

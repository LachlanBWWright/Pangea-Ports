#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "ScriptBindings.h"
#include "ScriptEggs.h"
#include <math.h>
#include <string.h>

static bool ReadInput(int player, PangeaScriptInputSnapshot* output)
{
	static const int needs[] = {kNeed_PitchUp, kNeed_PitchDown, kNeed_YawLeft, kNeed_YawRight,
		kNeed_Fire, kNeed_Drop, kNeed_PrevWeapon, kNeed_NextWeapon, kNeed_Jetpack, kNeed_CameraMode};
	static const char* names[] = {"pitchUp", "pitchDown", "yawLeft", "yawRight", "fire",
		"drop", "previousWeapon", "nextWeapon", "jetpack", "cameraMode"};
	if (!output || player < 0 || player >= gNumPlayers) return false;
	*output = (PangeaScriptInputSnapshot){0};
	output->actionCount = sizeof(needs) / sizeof(needs[0]);
	for (int i = 0; i < output->actionCount; i++)
	{
		output->actions[i] = (PangeaScriptInputAction){names[i], IsNeedActive(needs[i], player) != 0,
			IsNeedDown(needs[i], player), IsNeedUp(needs[i], player)};
	}
	output->moveX = gPlayerInfo[player].analogControlX;
	output->moveY = gPlayerInfo[player].analogControlZ;
	ObjNode* actor = gPlayerInfo[player].objNode;
	if (actor && actor->CType != INVALID_NODE_FLAG)
	{
		const OGLVector3D forward = {0, 0, -1};
		OGLVector3D direction;
		OGLVector3D_Transform(&forward, &actor->BaseTransformMatrix, &direction);
		float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if (isfinite(length) && length > 0)
		{
			output->aim = (PangeaScriptVector3){direction.x / length, direction.y / length, direction.z / length};
			output->hasAim = true;
		}
	}
	return true;
}

static bool SampleTerrain(float x, float z, PangeaScriptTerrainSample* output)
{
	if (!output || !isfinite(x) || !isfinite(z) || !gMapYCoords || x < 0 || z < 0 ||
		x >= gTerrainTileWidth * gTerrainPolygonSize || z >= gTerrainTileDepth * gTerrainPolygonSize)
		return false;
	OGLVector3D previousNormal = gRecentTerrainNormal;
	float height = GetTerrainY(x, z);
	OGLVector3D normal = gRecentTerrainNormal;
	gRecentTerrainNormal = previousNormal;
	*output = (PangeaScriptTerrainSample){.height = height, .normal = {normal.x, normal.y, normal.z}, .surfaceType = -1};
	output->hasWater = GetWaterY(x, z, &output->waterHeight);
	return height != ILLEGAL_TERRAIN_Y;
}

static bool OverlapBox(const PangeaScriptBounds* bounds, unsigned int mask,
	PangeaScriptObjectHandle* results, int capacity, int* count)
{
	if (!bounds || !results || !count || capacity <= 0) return false;
	*count = 0;
	for (ObjNode* node = gFirstNodePtr; node && *count < capacity; node = node->NextNode)
	{
		if (node->CType == INVALID_NODE_FLAG || !(node->CType & mask) ||
			(node->StatusBits & STATUS_BIT_NOCOLLISION)) continue;
		if (!DoSimpleBoxCollisionAgainstObject(bounds->max.y, bounds->min.y,
			bounds->min.x, bounds->max.x, bounds->max.z, bounds->min.z, node)) continue;
		Nanosaur2Script_RegisterObject(node, "nanosaur2.collision-object", "collision");
		if (node->ScriptObjectID > 0)
			results[(*count)++] = (PangeaScriptObjectHandle){node->ScriptObjectID, node->ScriptObjectGeneration};
	}
	return true;
}

static const PangeaScriptCollisionCategory kCategories[] = {
	{"player", CTYPE_PLAYER1 | CTYPE_PLAYER2}, {"enemy", CTYPE_ENEMY},
	{"solid", CTYPE_MISC | CTYPE_PLAYERONLY | CTYPE_SOLIDTOENEMY},
	{"pickup", CTYPE_PICKUP | CTYPE_POWERUP}, {"trigger", CTYPE_TRIGGER | CTYPE_TRIGGER2},
	{"egg", CTYPE_EGG}, {"weaponTarget", CTYPE_WEAPONTEST}, {"hazard", CTYPE_HURTME}
};

static const PangeaScriptStateField kStateFields[] = {
	{"fuel", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 1, true},
	{"weaponCharge", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 1, true},
	{"activeWeapon", PANGEA_SCRIPT_STATE_NUMBER, true, WEAPON_TYPE_NONE, NUM_WEAPON_TYPES - 1, true},
	{"checkpoint", PANGEA_SCRIPT_STATE_NUMBER, true, 0, MAX_LINEMARKERS - 1, true},
	{"lap", PANGEA_SCRIPT_STATE_NUMBER, true, -1, 32767, true},
	{"placement", PANGEA_SCRIPT_STATE_NUMBER, true, 0, MAX_PLAYERS - 1, true},
	{"jetpackActive", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, true},
	{"raceComplete", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, true},
	{"levelComplete", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, false},
	{"gameOver", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, false},
	{"eggsSaved", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255 * NUM_EGG_TYPES, false},
	{"eggsRequired", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255 * NUM_EGG_TYPES, false},
};

static bool GetState(int player, const char* key, PangeaScriptStateValue* output)
{
	if (!key || !output || player < 0 || player >= gNumPlayers || !gPlayerInfo[player].objNode) return false;
	*output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER};
	if (!strcmp(key, "fuel")) output->number = gPlayerInfo[player].jetpackFuel;
	else if (!strcmp(key, "weaponCharge")) output->number = gPlayerInfo[player].weaponCharge;
	else if (!strcmp(key, "activeWeapon")) output->number = gPlayerInfo[player].currentWeapon;
	else if (!strcmp(key, "checkpoint")) output->number = gPlayerInfo[player].raceCheckpointNum;
	else if (!strcmp(key, "lap")) output->number = gPlayerInfo[player].lapNum;
	else if (!strcmp(key, "placement")) output->number = gPlayerInfo[player].place;
	else if (!strcmp(key, "eggsSaved") || !strcmp(key, "eggsRequired"))
	{
		for (int i = 0; i < NUM_EGG_TYPES; i++)
			output->number += !strcmp(key, "eggsSaved") ? gNumEggsSaved[i] : gNumEggsToSave[i];
	}
	else
	{
		output->type = PANGEA_SCRIPT_STATE_BOOLEAN;
		if (!strcmp(key, "jetpackActive")) output->boolean = gPlayerInfo[player].jetpackActive;
		else if (!strcmp(key, "raceComplete")) output->boolean = gPlayerInfo[player].raceComplete;
		else if (!strcmp(key, "levelComplete")) output->boolean = gLevelCompleted;
		else if (!strcmp(key, "gameOver")) output->boolean = gGameOver;
		else return false;
	}
	return true;
}

static PangeaScriptStatus SetState(int player, const char* key, const PangeaScriptStateValue* value)
{
	if (!key || !value || player < 0 || player >= gNumPlayers || !gPlayerInfo[player].objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	const PangeaScriptStateField* field = NULL;
	for (unsigned int i = 0; i < sizeof(kStateFields) / sizeof(kStateFields[0]); i++)
		if (!strcmp(key, kStateFields[i].key)) field = &kStateFields[i];
	if (!field || !field->writable) return PANGEA_SCRIPT_CAPABILITY_UNSUPPORTED;
	if (value->type != field->type || !isfinite(value->number) || value->number < field->min || value->number > field->max)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!strcmp(key, "fuel"))
	{
		gPlayerInfo[player].jetpackFuel = value->number;
		if (value->number == 0) JetpackOff(player);
	}
	else if (!strcmp(key, "weaponCharge")) gPlayerInfo[player].weaponCharge = value->number;
	else
	{
		if (floor(value->number) != value->number) return PANGEA_SCRIPT_BAD_ARGUMENT;
		if (!strcmp(key, "activeWeapon"))
		{
			StopAChannel(&gPlayerInfo[player].weaponChargeChannel);
			gPlayerInfo[player].weaponCharge = 0;
			gPlayerInfo[player].currentWeapon = value->number;
		}
		else if (!strcmp(key, "checkpoint"))
		{
			if (value->number >= gNumLineMarkers) return PANGEA_SCRIPT_BAD_ARGUMENT;
			gPlayerInfo[player].raceCheckpointNum = value->number;
			memset(gPlayerInfo[player].raceCheckpointTagged, 0, sizeof(gPlayerInfo[player].raceCheckpointTagged));
			gPlayerInfo[player].raceCheckpointTagged[(int)value->number] = true;
		}
		else if (!strcmp(key, "placement"))
		{
			if (value->number >= gNumPlayers) return PANGEA_SCRIPT_BAD_ARGUMENT;
			gPlayerInfo[player].place = value->number;
		}
		else gPlayerInfo[player].lapNum = value->number;
	}
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus Complete(int player, bool success)
{
	if (player < 0 || player >= gNumPlayers || !gPlayerInfo[player].objNode) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (gLevelCompleted || gGameOver) return PANGEA_SCRIPT_OK;
	if (gVSMode != VS_MODE_NONE)
	{
		StartLevelCompletion(5.0f);
		for (int i = 0; i < gNumPlayers; i++)
		{
			bool won = i == player ? success : !success;
			ShowWinLose(i, won ? 0 : 1);
			Nanosaur2Script_OnObjectiveComplete(i, won ? 0 : 1);
			if (gVSMode == VS_MODE_RACE && won)
			{
				gPlayerInfo[i].raceComplete = true;
				Nanosaur2Script_OnRaceFinish(i, gPlayerInfo[i].place);
			}
		}
	}
	else if (success) StartLevelCompletion(2.0f);
	else gGameOver = true;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptObjectHandle ObjectHandle(ObjNode* object)
{
	if (!object || object->CType == INVALID_NODE_FLAG) return (PangeaScriptObjectHandle){0};
	Nanosaur2Script_RegisterObject(object, "nanosaur2.related-object", "relation");
	return (PangeaScriptObjectHandle){object->ScriptObjectID, object->ScriptObjectGeneration};
}

static bool GetRelations(int player, PangeaScriptPlayerRelations* output)
{
	if (!output || player < 0 || player >= gNumPlayers || !gPlayerInfo[player].objNode ||
		gPlayerInfo[player].objNode->CType == INVALID_NODE_FLAG) return false;
	*output = (PangeaScriptPlayerRelations){.carried = ObjectHandle(gPlayerInfo[player].carriedObj), .form = "flying"};
	return true;
}

static PangeaScriptStatus Fire(const PangeaScriptOperationRequest* request, ObjNode* player)
{
	int p = request->playerNum;
	int weapon = request->hasWeapon ? request->weapon : gPlayerInfo[p].currentWeapon;
	const char* mode = request->mode ? request->mode : "press";
	if (request->hasTarget || request->hasAim || request->hasPosition || request->hasVelocity || request->hasJoint || request->form ||
		weapon < WEAPON_TYPE_NONE || weapon >= NUM_WEAPON_TYPES || (request->hasWeapon && weapon == WEAPON_TYPE_NONE) ||
		(strcmp(mode, "press") && strcmp(mode, "hold") && strcmp(mode, "release"))) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (weapon == WEAPON_TYPE_NONE || gPlayerInfo[p].weaponQuantity[weapon] <= 0)
	{
		if (request->hasWeapon && weapon != gPlayerInfo[p].currentWeapon) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		if (strcmp(mode, "release")) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		StopAChannel(&gPlayerInfo[p].weaponChargeChannel);
		gPlayerInfo[p].weaponCharge = 0;
		return PANGEA_SCRIPT_OK;
	}
	if (request->hasWeapon && weapon != gPlayerInfo[p].currentWeapon)
	{
		StopAChannel(&gPlayerInfo[p].weaponChargeChannel);
		gPlayerInfo[p].weaponCharge = 0;
		gPlayerInfo[p].currentWeapon = weapon;
	}
	if (!strcmp(mode, "release")) PlayerFireButtonReleased(player);
	else PlayerFireButtonPressed(player, !strcmp(mode, "press"));
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus RunOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output)
{
	if (!request || !output || request->playerNum < 0 || request->playerNum >= gNumPlayers) return PANGEA_SCRIPT_BAD_ARGUMENT;
	int p = request->playerNum;
	ObjNode* player = gPlayerInfo[p].objNode;
	if (!player || player->CType == INVALID_NODE_FLAG) return PANGEA_SCRIPT_BAD_ARGUMENT;
	*output = (PangeaScriptOperationResult){0};
	if (gPlayerIsDead[p] && request->operation != PANGEA_SCRIPT_OP_RESPAWN) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (request->operation == PANGEA_SCRIPT_OP_FIRE) return Fire(request, player);
	if (request->hasPosition || request->hasAim || request->hasWeapon || request->hasJoint || request->mode || request->form ||
		(request->hasVelocity && request->operation != PANGEA_SCRIPT_OP_DROP)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	void* nativeTarget = NULL;
	if (request->hasTarget && !PangeaScript_GetObjectNativeObject(request->target, &nativeTarget)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	ObjNode* target = nativeTarget;
	ObjNode* egg = gPlayerInfo[p].carriedObj;
	switch (request->operation)
	{
		case PANGEA_SCRIPT_OP_CARRY:
			if (!request->hasTarget || !Nanosaur2Script_CarryEgg(p, target)) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			output->object = ObjectHandle(target);
			return PANGEA_SCRIPT_OK;
		case PANGEA_SCRIPT_OP_DROP:
			if (request->hasTarget || !egg || !Nanosaur2Script_GetEggNest(egg)) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			if (request->hasVelocity && (!isfinite(request->velocity.x) || !isfinite(request->velocity.y) || !isfinite(request->velocity.z)))
				return PANGEA_SCRIPT_BAD_ARGUMENT;
			DropEgg_NoWormhole(p);
			if (request->hasVelocity) egg->Delta = (OGLVector3D){request->velocity.x, request->velocity.y, request->velocity.z};
			output->object = ObjectHandle(egg);
			return PANGEA_SCRIPT_OK;
		case PANGEA_SCRIPT_OP_HANDOVER:
			for (int i = 0; i < gNumPlayers; i++)
			{
				if (!request->hasTarget || target != gPlayerInfo[i].objNode) continue;
				if (!Nanosaur2Script_HandoverEgg(p, i)) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
				output->object = ObjectHandle(egg);
				return PANGEA_SCRIPT_OK;
			}
			return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		case PANGEA_SCRIPT_OP_DELIVER:
			return Nanosaur2Script_DeliverEgg(p, target) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		case PANGEA_SCRIPT_OP_RESET_CARRIED:
			if (request->hasTarget || !Nanosaur2Script_ResetCarriedEgg(p)) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			output->object = ObjectHandle(egg);
			return PANGEA_SCRIPT_OK;
		case PANGEA_SCRIPT_OP_RESPAWN:
			if (request->hasTarget || !player->Skeleton || !gMapYCoords) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			if (egg) DropEgg_NoWormhole(p);
			ResetPlayerAtBestCheckpoint(p);
			output->object = ObjectHandle(player);
			return gPlayerIsDead[p] ? PANGEA_SCRIPT_INCOMPATIBLE_ITEM : PANGEA_SCRIPT_OK;
		default: return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
}

const PangeaScriptGameplayOps kNanosaur2ScriptGameplay = {
	.getInput = ReadInput, .sampleTerrain = SampleTerrain, .overlapBox = OverlapBox,
	.getState = GetState, .setState = SetState, .complete = Complete,
	.stateFields = kStateFields, .stateFieldCount = sizeof(kStateFields) / sizeof(kStateFields[0]),
	.collisionCategories = kCategories, .collisionCategoryCount = sizeof(kCategories) / sizeof(kCategories[0]),
	.operationMask = (1u << PANGEA_SCRIPT_OP_CARRY) | (1u << PANGEA_SCRIPT_OP_DROP) | (1u << PANGEA_SCRIPT_OP_HANDOVER) |
		(1u << PANGEA_SCRIPT_OP_FIRE) | (1u << PANGEA_SCRIPT_OP_RESPAWN) | (1u << PANGEA_SCRIPT_OP_DELIVER) | (1u << PANGEA_SCRIPT_OP_RESET_CARRIED),
	.runOperation = RunOperation, .getRelations = GetRelations
};
#endif

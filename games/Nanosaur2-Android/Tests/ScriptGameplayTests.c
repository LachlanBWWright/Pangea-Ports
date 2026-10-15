#include "game.h"
#include "ScriptBindings.h"
#include "ScriptGeometry.h"
#include "ScriptRules.h"
#include "ScriptEggs.h"
#include <assert.h>
#include <math.h>
#include <float.h>
#include <string.h>

extern const PangeaScriptGameplayOps kNanosaur2ScriptGameplay;
extern ObjNode* CreateScriptTestEgg(TerrainItemEntryType* item);
extern ObjNode* eggDestination;
Byte gNumPlayers = 2;
PlayerInfoType gPlayerInfo[MAX_PLAYERS];
float** gMapYCoords;
long gTerrainTileWidth = 10, gTerrainTileDepth = 10;
float gTerrainPolygonSize = 100;
OGLVector3D gRecentTerrainNormal = {1, 0, 0};
ObjNode* gFirstNodePtr;
Boolean gPlayerIsDead[MAX_PLAYERS];
static ObjNode* resolvedTarget;
static int firePress, fireRelease, respawns;
bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** output)
{
	if (handle.id != 42 || handle.generation != 7 || !resolvedTarget) return false;
	*output = resolvedTarget;
	return true;
}
void PlayerFireButtonPressed(ObjNode* player, Boolean pressed) { (void)player; (void)pressed; firePress++; }
void PlayerFireButtonReleased(ObjNode* player) { (void)player; fireRelease++; }
void ResetPlayerAtBestCheckpoint(short player) { gPlayerIsDead[player] = false; respawns++; }
int gNumLineMarkers = 4;
short gVSMode;
Boolean gLevelCompleted, gGameOver;
static int stopped, wonPlayer = -1;
static int transforms;
static bool rulesControlled;
bool PangeaScript_RulesAreScriptControlled(void) { return rulesControlled; }
void UpdateObjectTransforms(ObjNode* node) { (void)node; transforms++; }

Boolean DoSimpleBoxCollisionAgainstObject(float top, float bottom, float left, float right,
	float front, float back, ObjNode* node)
{
	assert(top == 3 && bottom == -3 && left == -2 && right == 2 && front == 4 && back == -4);
	return node->Coord.x == 0;
}
void Nanosaur2Script_RegisterObject(ObjNode* node, const char* type, const char* category)
{
	(void)type; (void)category;
	node->ScriptObjectID = 42; node->ScriptObjectGeneration = 7;
}
void JetpackOff(short player) { gPlayerInfo[player].jetpackActive = false; }
void StopAChannel(short* channel) { *channel = -1; stopped++; }
ObjNode* ShowWinLose(short player, Byte outcome) { if (outcome == 0) wonPlayer = player; return NULL; }
void Nanosaur2Script_OnObjectiveComplete(short player, int outcome) { (void)player; (void)outcome; }
void Nanosaur2Script_OnRaceFinish(short player, int place) { (void)player; (void)place; }
void StartLevelCompletion(float delay) { (void)delay; gLevelCompleted = true; }

int GetNeedState(int need, int player)
{
	if (player == 0 && need == kNeed_Fire) return KEYSTATE_DOWN;
	if (player == 1 && need == kNeed_Drop) return KEYSTATE_UP;
	return KEYSTATE_OFF;
}

void OGLVector3D_Transform(const OGLVector3D* vector, const OGLMatrix4x4* matrix, OGLVector3D* output)
{
	(void) matrix;
	*output = (OGLVector3D){vector->x, vector->y, vector->z * 2};
}

float GetTerrainY(float x, float z)
{
	gRecentTerrainNormal = (OGLVector3D){0, 1, 0};
	return x + z;
}

Boolean GetWaterY(float x, float z, float* height)
{
	(void) x;
	(void) z;
	*height = 25;
	return true;
}

static void TestEggOperations(void)
{
	ObjNode players[2] = {{.PlayerNum = 0, .CType = CTYPE_PLAYER1}, {.PlayerNum = 1, .CType = CTYPE_PLAYER2}};
	gPlayerInfo[0].objNode = &players[0];
	gPlayerInfo[1].objNode = &players[1];
	TerrainItemEntryType item = {.terrainY = 50};
	ObjNode* egg = CreateScriptTestEgg(&item);
	ObjNode* nest = egg->ChainHead;
	resolvedTarget = egg;
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_CARRY, .playerNum = 0,
		.hasTarget = true, .target = {42, 7}};
	PangeaScriptOperationResult result;
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(gPlayerInfo[0].carriedObj == egg && egg->PlayerNum == 0 && !nest->Flag[1]);
	players[0].Coord = (OGLPoint3D){90, 100, 110};
	Nanosaur2Script_AlignCarriedEgg(egg);
	assert(egg->Coord.x == 90 && egg->Coord.y == 100 && egg->Coord.z == 110);
	PangeaScriptPlayerRelations relations;
	assert(kNanosaur2ScriptGameplay.getRelations(0, &relations) && relations.carried.id == 42);
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	request.operation = PANGEA_SCRIPT_OP_HANDOVER;
	resolvedTarget = &players[1];
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(!gPlayerInfo[0].carriedObj && gPlayerInfo[1].carriedObj == egg && egg->PlayerNum == 1);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_RESET_CARRIED, .playerNum = 1};
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(!gPlayerInfo[1].carriedObj && nest->Flag[1] && egg->Coord.y == egg->InitCoord.y);
	resolvedTarget = egg;
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_CARRY, .hasTarget = true, .target = {42, 7}};
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_DROP, .hasVelocity = true, .velocity = {5, 6, 7}};
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(!gPlayerInfo[0].carriedObj && egg->Delta.x == 5 && egg->Delta.y == 6 && egg->Timer == 1);
	float previousDelta = gFramesPerSecondFrac;
	gFramesPerSecondFrac = 0;
	egg->Timer = 0;
	players[0].Coord = egg->Coord;
	egg->MoveCall(egg);
	assert(!gPlayerInfo[0].carriedObj);
	rulesControlled = false;
	egg->MoveCall(egg);
	assert(gPlayerInfo[0].carriedObj == egg);
	DropEgg_NoWormhole(0);
	rulesControlled = true;
	gFramesPerSecondFrac = previousDelta;
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_CARRY, .hasTarget = true, .target = {42, 7}};
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_DELIVER};
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	ObjNode wormhole = {0};
	eggDestination = &wormhole;
	gVSMode = VS_MODE_NONE;
	gNumEggsToSave[0] = 1;
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(gNumEggsSaved[0] == 1 && !gPlayerInfo[0].carriedObj && !nest->ChainNode);
	assert(item.flags & ITEM_FLAGS_USER1);
	assert(!gOpenPlayerWormhole && egg->CType == INVALID_NODE_FLAG && gEggObjs[0] == NULL);
	TerrainItemEntryType secondItem = {.terrainY = 80};
	resolvedTarget = egg = CreateScriptTestEgg(&secondItem);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_CARRY, .hasTarget = true, .target = {42, 7}};
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	ClearEggObjectReferences(&players[0]);
	assert(!gPlayerInfo[0].carriedObj && egg->Timer == 1);
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	DeleteObject(egg);
	assert(!gPlayerInfo[0].carriedObj && !egg->ChainHead && gEggObjs[1] == NULL);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_FIRE};
	gPlayerInfo[0].currentWeapon = WEAPON_TYPE_BLASTER;
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	gPlayerInfo[0].weaponQuantity[WEAPON_TYPE_BLASTER] = 1;
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK && firePress == 1);
	request.mode = "release";
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK && fireRelease == 1);
	request.mode = "invalid";
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	gPlayerInfo[0].weaponQuantity[WEAPON_TYPE_BLASTER] = 0;
	gPlayerInfo[0].weaponCharge = 1;
	gPlayerInfo[0].weaponChargeChannel = 3;
	request.mode = "release";
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(gPlayerInfo[0].weaponCharge == 0 && gPlayerInfo[0].weaponChargeChannel == -1 && fireRelease == 1);
	gPlayerInfo[0].currentWeapon = WEAPON_TYPE_NONE;
	gPlayerInfo[0].weaponCharge = .5f;
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK && gPlayerInfo[0].weaponCharge == 0);
	SkeletonObjDataType skeleton = {0};
	players[0].Skeleton = &skeleton;
	gPlayerIsDead[0] = true;
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_RESPAWN};
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_OK && respawns == 1 && !gPlayerIsDead[0]);
	request.operation = PANGEA_SCRIPT_OP_MOUNT;
	assert(kNanosaur2ScriptGameplay.runOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
}

int main(void)
{
	assert(Nanosaur2Script_UsesNativeRules());
	rulesControlled = true;
	assert(!Nanosaur2Script_UsesNativeRules());
	ObjNode actor = {0};
	gPlayerInfo[0].objNode = &actor;
	gPlayerInfo[0].analogControlX = -.4f;
	gPlayerInfo[0].analogControlZ = .8f;
	PangeaScriptInputSnapshot first, second;
	assert(kNanosaur2ScriptGameplay.getInput(0, &first));
	assert(kNanosaur2ScriptGameplay.getInput(1, &second));
	assert(!kNanosaur2ScriptGameplay.getInput(2, &second));
	assert(first.moveX == -.4f && first.moveY == .8f && first.hasAim && first.aim.z == -1);
	assert(!strcmp(first.actions[4].name, "fire") && first.actions[4].held && first.actions[4].pressed);
	assert(!second.actions[4].held && second.actions[5].released);
	float* mapRow = NULL;
	gMapYCoords = &mapRow;
	PangeaScriptTerrainSample terrain;
	assert(kNanosaur2ScriptGameplay.sampleTerrain(10, 20, &terrain));
	assert(terrain.height == 30 && terrain.normal.y == 1 && terrain.hasWater && terrain.waterHeight == 25);
	assert(gRecentTerrainNormal.x == 1 && gRecentTerrainNormal.y == 0);
	assert(!kNanosaur2ScriptGameplay.sampleTerrain(-1, 0, &terrain));
	assert(!kNanosaur2ScriptGameplay.sampleTerrain(1000, 0, &terrain));
	assert(!kNanosaur2ScriptGameplay.sampleTerrain(NAN, 0, &terrain));
	actor.CType = CTYPE_ENEMY;
	gFirstNodePtr = &actor;
	PangeaScriptBounds bounds = {{-2, -3, -4}, {2, 3, 4}};
	PangeaScriptObjectHandle results[2];
	int count;
	assert(kNanosaur2ScriptGameplay.overlapBox(&bounds, CTYPE_ENEMY, results, 2, &count));
	assert(count == 1 && results[0].id == 42 && results[0].generation == 7);
	actor.StatusBits = STATUS_BIT_NOCOLLISION;
	assert(kNanosaur2ScriptGameplay.overlapBox(&bounds, CTYPE_ENEMY, results, 2, &count) && count == 0);
	PangeaScriptStateValue value = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = .4};
	assert(kNanosaur2ScriptGameplay.setState(0, "fuel", &value) == PANGEA_SCRIPT_OK);
	assert(kNanosaur2ScriptGameplay.getState(0, "fuel", &value) && value.number > .39 && value.number < .41);
	value.number = NAN;
	assert(kNanosaur2ScriptGameplay.setState(0, "fuel", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = 1.5;
	assert(kNanosaur2ScriptGameplay.setState(0, "checkpoint", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = 2;
	assert(kNanosaur2ScriptGameplay.setState(0, "checkpoint", &value) == PANGEA_SCRIPT_OK);
	assert(gPlayerInfo[0].raceCheckpointTagged[2]);
	assert(kNanosaur2ScriptGameplay.setState(0, "activeWeapon", &value) == PANGEA_SCRIPT_OK && stopped == 1);
	value.number = 1;
	assert(kNanosaur2ScriptGameplay.setState(0, "placement", &value) == PANGEA_SCRIPT_OK && gPlayerInfo[0].place == 1);
	value.number = -1;
	assert(kNanosaur2ScriptGameplay.setState(0, "lap", &value) == PANGEA_SCRIPT_OK && gPlayerInfo[0].lapNum == -1);
	assert(kNanosaur2ScriptGameplay.setState(0, "eggsSaved", &value) == PANGEA_SCRIPT_CAPABILITY_UNSUPPORTED);
	gVSMode = VS_MODE_RACE;
	gPlayerInfo[0].lapNum = 2;
	gPlayerInfo[1].lapNum = 1;
	CalcPlayerPlaces();
	assert(gPlayerInfo[0].place == 1 && gPlayerInfo[1].place == 0);
	rulesControlled = false;
	CalcPlayerPlaces();
	assert(gPlayerInfo[0].place == 0 && gPlayerInfo[1].place == 1);
	rulesControlled = true;
	assert(kNanosaur2ScriptGameplay.complete(0, true) == PANGEA_SCRIPT_OK);
	assert(gLevelCompleted && gPlayerInfo[0].raceComplete && wonPlayer == 0);
	gLevelCompleted = false;
	gPlayerInfo[1].objNode = &actor;
	assert(kNanosaur2ScriptGameplay.complete(1, false) == PANGEA_SCRIPT_OK);
	assert(gLevelCompleted && wonPlayer == 0 && gPlayerInfo[0].raceComplete);
	actor.NumCollisionBoxes = 2;
	actor.CollisionBoxes[0] = (CollisionBoxType){.left = -1, .right = 1, .bottom = -2, .top = 2, .back = -3, .front = 3};
	actor.CollisionBoxes[1] = (CollisionBoxType){.left = 3, .right = 5, .bottom = 0, .top = 1, .back = -1, .front = 1};
	PangeaScriptBounds liveBounds;
	assert(Nanosaur2Script_ReadBounds(&actor, &liveBounds) && liveBounds.max.x == 5 && liveBounds.min.z == -3);
	PangeaScriptVector3 position = {10, 20, 30};
	assert(Nanosaur2Script_MoveGeometry(&actor, &position) && transforms == 1);
	assert(Nanosaur2Script_ReadBounds(&actor, &liveBounds) && liveBounds.min.x == 9 && liveBounds.max.x == 15 && liveBounds.min.y == 18);
	actor.Scale = (OGLVector3D){1, 1, 1};
	actor.BoundingSphereRadius = 3;
	assert(Nanosaur2Script_ScaleGeometry(&actor, 2));
	assert(actor.BoundingSphereRadius == 6);
	assert(Nanosaur2Script_ReadBounds(&actor, &liveBounds) && liveBounds.min.x == 8 && liveBounds.max.x == 20 && liveBounds.min.y == 16);
	actor.CollisionBoxes[1].top = NAN;
	assert(!Nanosaur2Script_ReadBounds(&actor, &liveBounds));
	CollisionBoxType before = actor.CollisionBoxes[0];
	PangeaScriptVector3 invalidMove = {11, 20, 30};
	assert(!Nanosaur2Script_MoveGeometry(&actor, &invalidMove));
	assert(actor.Coord.x == 10 && actor.CollisionBoxes[0].left == before.left);
	assert(!Nanosaur2Script_ScaleGeometry(&actor, 3));
	assert(actor.Scale.x == 2 && actor.CollisionBoxes[0].left == before.left);
	actor.CollisionBoxes[1].top = 22;
	assert(!Nanosaur2Script_ScaleGeometry(&actor, FLT_MAX));
	assert(actor.Scale.x == 2 && actor.BoundingSphereRadius == 6);
	actor.NumCollisionBoxes = 0;
	assert(!Nanosaur2Script_ReadBounds(&actor, &liveBounds));
	TestEggOperations();
	return 0;
}

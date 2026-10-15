#include "../Source/Scripting/ScriptBindings.c"
#include "../Source/Items/Triggers.c"
void CroMagNativePlayerCompletedRace(short playerNum);
#define PlayerCompletedRace CroMagNativePlayerCompletedRace
#include "../Source/Terrain/Checkpoints.c"
#undef PlayerCompletedRace
#include <assert.h>
#include <float.h>

PlayerInfoType gPlayerInfo[MAX_PLAYERS];
short gNumTotalPlayers = 2;
short gNumRealPlayers = 2;
short gNumPlayersEliminated;
Boolean gGamePaused;
Boolean gNetGameInProgress;
Boolean gTrackCompleted;
Boolean gIsSelfRunningDemo;
float gTrackCompletedCoolDownTimer;
int gGameMode;
short gCapturedFlagCount[2];
long gTerrainUnitWidth = 100;
long gTerrainUnitDepth = 100;
float** gMapYCoords;
OGLVector3D gRecentTerrainNormal;
TileAttribType** gTileAttribList;
uint16_t** gTileGrid;
ObjNode* gFirstNodePtr;
OGLBoundingBox gObjectGroupBBoxList[MAX_BG3D_GROUPS][MAX_OBJECTS_IN_GROUP];
static ObjNode* operationTarget;
static int powerupFired;
static bool networked;
static float lastTime;
static int frames;
static int completedPlayer = -1;
static int transformUpdates;
static bool rulesControlled;
static bool controllerOwned;
Boolean gIsNetworkClient;
static bool allowPickup;
static int pickupHooks;
static int winnerDisplays;
static int loserDisplays;
static int clearedResults;

void ClearWinLose(short playerNum) { assert(playerNum == 0); clearedResults++; }

void ShowWinLose(short playerNum, Byte mode, short winner)
{
	assert(winner == 0 && playerNum >= 0 && playerNum < gNumTotalPlayers);
	if (mode == 1) winnerDisplays++;
	else { assert(mode == 2); loserDisplays++; }
}

bool PangeaScript_RulesAreScriptControlled(void) { return rulesControlled; }
bool PangeaScript_ObjectIsScriptControlled(PangeaScriptObjectHandle handle)
{
	return handle.id == 1 && controllerOwned;
}

void UpdateObjectTransforms(ObjNode* node) { assert(node); transformUpdates++; }

Boolean IsRaceMode(void) { return gGameMode == GAME_MODE_MULTIPLAYERRACE; }
void PlayerCompletedRace(short playerNum) { completedPlayer = playerNum; }
void FindCoordOnJoint(ObjNode* node, long joint, const OGLPoint3D* local, OGLPoint3D* world)
{
	assert(joint == 0 && local->x == 0 && local->y == 0 && local->z == 0);
	*world = node->Coord;
}

bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle) { return handle.id != 0; }
bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** output)
{
	if (!operationTarget || handle.id != operationTarget->ScriptObjectID) return false;
	*output = operationTarget;
	return true;
}
Boolean PlayerFirePowerup(short playerNum, Boolean forward)
{
	assert(playerNum == 0 && !forward);
	powerupFired++;
	return true;
}
float RandomFloat2(void) { return 0; }
short PlayEffect_Parms(short effect, uint32_t left, uint32_t right, unsigned long rate)
{
	(void)effect; (void)left; (void)right; (void)rate;
	return -1;
}
PangeaScriptStatus PangeaScript_CallPickupHook(const PangeaScriptPickupContext* context, PangeaScriptPickupResult* result)
{
	assert(context->player.id == 1 && context->pickup.id == 2);
	pickupHooks++;
	result->hasConsumePickup = true;
	result->consumePickup = allowPickup;
	return PANGEA_SCRIPT_OK;
}
static int nativePickupCalls;
void PlayAnnouncerSound(short effect, Boolean override, float delay)
{
	(void)effect; (void)override; (void)delay;
	nativePickupCalls++;
}
short PlayEffect_Parms3D(short effect, const OGLPoint3D* point, uint32_t rate, float volume)
{
	(void)effect; (void)point; (void)rate; (void)volume;
	nativePickupCalls++;
	return -1;
}
void PangeaPickup_Collect(ObjNode* node) { (void)node; nativePickupCalls++; }
void CalcVehicleSuspension(short player) { (void)player; nativePickupCalls++; }
void SetStickyTires(short player) { (void)player; nativePickupCalls++; }
void SetSuspensionPOW(short player) { (void)player; nativePickupCalls++; }

int PangeaNetBridge_IsEnabled(void) { return 0; }
void PangeaScript_SetNetworkedMode(bool enabled) { networked = enabled; }
PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* context)
{
	lastTime = context->levelTimeSeconds;
	frames++;
	return PANGEA_SCRIPT_OK;
}
void PangeaScript_ExpireTriggerContacts(const PangeaScriptFrameContext* context) { (void)context; }
PangeaScriptStatus PangeaScript_ApplyDeferredActions(const PangeaScriptFrameContext* context)
{
	(void)context;
	return PANGEA_SCRIPT_OK;
}
const char* PangeaScript_GetLastError(void) { return ""; }
float GetTerrainY(float x, float z)
{
	gRecentTerrainNormal = (OGLVector3D){0, 1, 0};
	return x + z;
}
uint16_t GetTileAttribsAtRowCol(float x, float z) { (void)x; (void)z; return 0; }

int main(void)
{
	ObjNode car = {0};
	gPlayerInfo[0].objNode = &car;
	assert(CroMagScript_SetObjectActive(&car, false));
	assert(car.StatusBits & STATUS_BIT_NOMOVE);
	assert(car.StatusBits & STATUS_BIT_NOCOLLISION);
	bool active = true;
	assert(CroMagScript_GetObjectActive(&car, &active) && !active);
	PangeaScriptPlayerSnapshot snapshot;
	assert(GetScriptPlayer(0, &snapshot) && !snapshot.active);
	assert(CroMagScript_SetObjectActive(&car, true));
	assert(!(car.StatusBits & STATUS_BIT_NOMOVE));
	assert(CroMagScript_SetObjectHealth(&car, 0.6f));
	assert(gPlayerInfo[0].health == 0.6f);
	assert(!CroMagScript_SetObjectHealth(&car, NAN));
	PangeaScriptVector3 velocity = {3, 12, 4};
	assert(CroMagScript_SetObjectVelocity(&car, &velocity));
	assert(car.Speed2D == 5 && car.Speed3D == 13);
	velocity.x = NAN;
	assert(!CroMagScript_SetObjectVelocity(&car, &velocity));
	assert(car.Delta.x == 3 && car.Speed2D == 5);
	velocity = (PangeaScriptVector3){FLT_MAX, FLT_MAX, FLT_MAX};
	assert(!CroMagScript_SetObjectVelocity(&car, &velocity));
	assert(car.Delta.x == 3 && car.Speed3D == 13);
	PangeaScriptVector3 rotation = {0, INFINITY, 0};
	assert(!CroMagScript_SetObjectRotation(&car, &rotation));
	assert(!CroMagScript_SetObjectScale(&car, NAN));
	assert(!CroMagScript_SetObjectScale(&car, INFINITY));
	short extent = 0;
	assert(GetScriptCollisionExtent(0.25f, &extent) && extent == 1);
	assert(GetScriptCollisionExtent(50.25f, &extent) && extent == 51);
	assert(!GetScriptCollisionExtent(0, &extent));
	assert(!GetScriptCollisionExtent(-1, &extent));
	assert(!GetScriptCollisionExtent(NAN, &extent));
	assert(!GetScriptCollisionExtent(INFINITY, &extent));
	assert(!GetScriptCollisionExtent((float)SHRT_MAX + 1, &extent));
	car.ScriptObjectID = 1;
	car.ScriptObjectGeneration = 1;
	ObjNode pickup = {.ScriptObjectID = 2, .ScriptObjectGeneration = 1};
	gPlayerInfo[0].stickyTiresTimer = 2;
	gPlayerInfo[0].superSuspensionTimer = 3;
	gPlayerInfo[0].invisibilityTimer = 4;
	assert(!DoTrig_StickyTiresPOW(&pickup, &car, 0));
	assert(!DoTrig_SuspensionPOW(&pickup, &car, 0));
	assert(!DoTrig_InvisibilityPOW(&pickup, &car, 0));
	assert(!pickup.POWHidden && nativePickupCalls == 0);
	assert(gPlayerInfo[0].stickyTiresTimer == 2);
	assert(gPlayerInfo[0].superSuspensionTimer == 3);
	assert(gPlayerInfo[0].invisibilityTimer == 4);
	allowPickup = true;
	rulesControlled = true;
	assert(!CroMagScript_NativeRulesEnabled());
	assert(!DoTrig_StickyTiresPOW(&pickup, &car, 0));
	assert(!DoTrig_SuspensionPOW(&pickup, &car, 0));
	assert(!DoTrig_InvisibilityPOW(&pickup, &car, 0));
	assert(pickupHooks == 6 && nativePickupCalls == 0 && !pickup.POWHidden);
	assert(gPlayerInfo[0].invisibilityTimer == 4);
	assert(!CroMagScript_OnPickupCollected(NULL, &car, 0, 1, "missing"));
	rulesControlled = false;
	assert(CroMagScript_NativeRulesEnabled());
	assert(!DoTrig_InvisibilityPOW(&pickup, &car, 0));
	assert(pickupHooks == 7 && nativePickupCalls == 2 && pickup.POWHidden);
	assert(gPlayerInfo[0].invisibilityTimer == 15);
	assert(CroMagScript_OnPickupCollected(NULL, &car, 0, 1, "missing"));

	gPlayerInfo[0].controlBits = 1u << kControlBit_Forward;
	gPlayerInfo[0].controlBits_New = gPlayerInfo[0].controlBits;
	gPlayerInfo[0].analogSteering = (OGLVector2D){0.5f, -0.25f};
	CroMagScript_OnRaceFrame(0, 1, 0.1f, 1000);
	CroMagScript_OnRaceFrame(0, 2, 0.025f, 0);
	assert(fabsf(lastTime - 0.125f) < 0.00001f);
	PangeaScriptInputSnapshot input;
	assert(GetScriptInput(0, &input));
	assert(input.actions[kControlBit_Forward].held);
	assert(strcmp(input.actions[kControlBit_Forward].name, "throttle") == 0);
	assert(input.moveX == 0.5f && input.moveY == -0.25f);
	gPlayerInfo[0].controlBits = gPlayerInfo[0].controlBits_New = 0;
	CroMagScript_OnRaceFrame(0, 3, 0.025f, 0);
	assert(GetScriptInput(0, &input) && input.actions[kControlBit_Forward].released);
	assert(GetScriptInput(0, &input) && input.actions[kControlBit_Forward].released);
	gGamePaused = true;
	CroMagScript_OnRaceFrame(0, 4, 1.0f, 0);
	assert(frames == 3 && fabsf(lastTime - 0.15f) < 0.00001f);
	gGamePaused = false;
	gGameMode = GAME_MODE_MULTIPLAYERRACE;
	gPlayerInfo[0].lapNum = 0;
	gPlayerInfo[1].lapNum = 1;
	gPlayerInfo[0].place = 0;
	gPlayerInfo[1].place = 1;
	rulesControlled = true;
	CalcPlayerPlaces();
	assert(gPlayerInfo[0].place == 0 && gPlayerInfo[1].place == 1);
	rulesControlled = false;
	CalcPlayerPlaces();
	assert(gPlayerInfo[0].place == 1 && gPlayerInfo[1].place == 0);
	assert(!CroMagScript_IsNetworked());
	gNetGameInProgress = true;
	CroMagScript_OnRaceFrame(0, 5, 0.025f, 0);
	assert(networked);

	float* heightRow = NULL;
	gMapYCoords = &heightRow;
	gRecentTerrainNormal = (OGLVector3D){1, 0, 0};
	PangeaScriptTerrainSample terrain;
	assert(SampleScriptTerrain(3, 4, &terrain));
	assert(terrain.height == 7 && terrain.normal.y == 1);
	assert(gRecentTerrainNormal.x == 1 && gRecentTerrainNormal.y == 0);
	assert(!SampleScriptTerrain(-1, 0, &terrain));
	assert(!SampleScriptTerrain(NAN, 0, &terrain));
	CollisionBoxType boxes[2] = {
		{.left = 1, .right = 3, .bottom = 0, .top = 2, .back = 1, .front = 3},
		{.left = 1, .right = 3, .bottom = 0, .top = 2, .back = 1, .front = 3},
	};
	car.CType = CTYPE_PLAYER;
	car.CollisionBoxes = boxes;
	car.NumCollisionBoxes = 2;
	gFirstNodePtr = &car;
	PangeaScriptBounds bounds = {{0, 0, 0}, {2, 2, 2}};
	PangeaScriptObjectHandle hits[2];
	int count;
	assert(OverlapScriptBox(&bounds, CTYPE_PLAYER, hits, 2, &count) && count == 1);
	assert(hits[0].id == 1);
	assert(OverlapScriptBox(&bounds, CTYPE_TRIGGER, hits, 2, &count) && count == 0);
	car.StatusBits |= STATUS_BIT_NOCOLLISION;
	assert(OverlapScriptBox(&bounds, CTYPE_PLAYER, hits, 2, &count) && count == 0);
	car.StatusBits &= ~STATUS_BIT_NOCOLLISION;
	bounds.min.x = NAN;
	assert(!OverlapScriptBox(&bounds, CTYPE_PLAYER, hits, 2, &count));
	PangeaScriptStateValue state = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 8};
	assert(SetScriptState(0, "nitroTime", &state) == PANGEA_SCRIPT_OK);
	assert(GetScriptState(0, "nitroTime", &state) && state.number == 8);
	state.number = NAN;
	assert(SetScriptState(0, "nitroTime", &state) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(gPlayerInfo[0].nitroTimer == 8);
	state.number = 1;
	assert(SetScriptState(0, "lap", &state) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	rulesControlled = true;
	assert(SetScriptState(0, "lap", &state) == PANGEA_SCRIPT_OK);
	assert(gPlayerInfo[0].lapNum == 1);
	state.number = 1.5;
	assert(SetScriptState(0, "lap", &state) == PANGEA_SCRIPT_BAD_ARGUMENT);
	state.number = gNumTotalPlayers;
	assert(SetScriptState(0, "place", &state) == PANGEA_SCRIPT_BAD_ARGUMENT);
	state.number = 1;
	assert(SetScriptState(0, "place", &state) == PANGEA_SCRIPT_OK && gPlayerInfo[0].place == 1);
	state.number = 9;
	assert(SetScriptState(0, "captureScore", &state) == PANGEA_SCRIPT_OK && gCapturedFlagCount[0] == 9);
	assert(SetScriptState(0, "tagTime", &state) == PANGEA_SCRIPT_OK && gPlayerInfo[0].tagTimer == 9);
	rulesControlled = false;
	assert(!GetScriptState(0, "unknown", &state));
	assert(GetScriptState(-1, "gameMode", &state) && state.number == gGameMode);
	assert(GetScriptState(0, "raceComplete", &state) && state.type == PANGEA_SCRIPT_STATE_BOOLEAN);
	state = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 4};
	assert(SetScriptState(0, "invisibilityTime", &state) == PANGEA_SCRIPT_OK);
	assert(car.ColorFilter.a == 0.1f);
	state.number = 0;
	assert(SetScriptState(0, "invisibilityTime", &state) == PANGEA_SCRIPT_OK);
	assert(car.ColorFilter.a == 1);
	assert(CompleteScriptRace(0, true) == PANGEA_SCRIPT_OK && completedPlayer == 0);
	completedPlayer = -1;
	assert(CompleteScriptRace(0, false) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM && completedPlayer == -1);
	assert(CompleteScriptRace(-1, true) == PANGEA_SCRIPT_BAD_ARGUMENT);
	gGameMode = GAME_MODE_SURVIVAL;
	assert(CompleteScriptRace(0, true) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM && !gTrackCompleted);
	rulesControlled = true;
	assert(CompleteScriptRace(0, true) == PANGEA_SCRIPT_OK);
	assert(gTrackCompleted && gTrackCompletedCoolDownTimer == TRACK_COMPLETE_COOLDOWN_TIME);
	assert(winnerDisplays == 1 && loserDisplays == 1);
	assert(CompleteScriptRace(0, true) == PANGEA_SCRIPT_OK && winnerDisplays == 1);
	rulesControlled = false;
	SkeletonDefType skeletonDefinition = {.NumBones = 1};
	SkeletonObjDataType skeleton = {.skeletonDefinition = &skeletonDefinition};
	car.Skeleton = &skeleton;
	car.Coord = (OGLPoint3D){3, 4, 5};
	PangeaScriptVector3 joint;
	assert(GetScriptJointPosition(&car, 0, &joint) && joint.x == 3 && joint.y == 4 && joint.z == 5);
	assert(!GetScriptJointPosition(&car, 1, &joint));
	assert(!GetScriptJointPosition(&car, -1, &joint));
	car.Skeleton = NULL;
	PangeaScriptBounds objectBounds;
	boxes[1].right = 7;
	boxes[1].top = 8;
	assert(GetScriptObjectBounds(&car, &objectBounds));
	assert(objectBounds.min.x == 1 && objectBounds.max.x == 7 && objectBounds.max.y == 8);
	PangeaScriptVector3 destination = {13, 14, 15};
	assert(CroMagScript_SetObjectPosition(&car, &destination));
	assert(transformUpdates == 1 && gPlayerInfo[0].coord.x == 13);
	assert(car.ScriptContactStateInvalidated);
	assert(GetScriptPlayer(0, &snapshot));
	assert(!snapshot.hasGroundedState && !snapshot.hasWaterState && !snapshot.hasTerrainHeightState);
	assert(!snapshot.hasGroundPhysicsState && !snapshot.hasGroundSteeringState);
	car.ScriptContactStateInvalidated = false;
	assert(CroMagScript_SetObjectPosition(&car, &destination));
	assert(!car.ScriptContactStateInvalidated);
	assert(GetScriptPlayer(0, &snapshot) && snapshot.hasGroundedState && snapshot.hasWaterState);
	assert(GetScriptObjectBounds(&car, &objectBounds) && objectBounds.min.x == 11 && objectBounds.max.x == 17);
	bounds = (PangeaScriptBounds){{10, 10, 10}, {12, 12, 12}};
	assert(OverlapScriptBox(&bounds, CTYPE_PLAYER, hits, 2, &count) && count == 1);
	bounds = (PangeaScriptBounds){{0, 0, 0}, {2, 2, 2}};
	assert(OverlapScriptBox(&bounds, CTYPE_PLAYER, hits, 2, &count) && count == 0);
	boxes[1].right = boxes[1].left - 1;
	assert(!GetScriptObjectBounds(&car, &objectBounds));
	car.CollisionBoxes = NULL;
	assert(!GetScriptObjectBounds(&car, &objectBounds));
	gPlayerInfo[0].onThisMachine = true;
	state = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 0.5};
	assert(SetScriptState(0, "steering", &state) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	controllerOwned = true;
	assert(SetScriptState(0, "steering", &state) == PANGEA_SCRIPT_OK && gPlayerInfo[0].steering == 0.5);
	state.number = 2;
	assert(SetScriptState(0, "steering", &state) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(gPlayerInfo[0].steering == 0.5);
	state.number = -12;
	assert(SetScriptState(0, "angularVelocityY", &state) == PANGEA_SCRIPT_OK && car.DeltaRot.y == -12);
	state = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_BOOLEAN, .boolean = true};
	assert(SetScriptState(0, "braking", &state) == PANGEA_SCRIPT_OK && gPlayerInfo[0].braking);
	gGameMode = GAME_MODE_CAPTUREFLAG;
	ObjNode flag = {.Group = MODEL_GROUP_GLOBAL, .Kind = TRIGTYPE_TEAMTORCH, .ScriptObjectID = 9, .ScriptObjectGeneration = 1};
	flag.InitCoord = (OGLPoint3D){5, 6, 7};
	flag.Coord = (OGLPoint3D){50, 60, 70};
	assert(PlayerCaptureFlag(&car, &flag));
	assert(car.CapturedFlag == (Ptr)&flag && flag.Mode == 1 && flag.CType == 0);
	assert(!PlayerCaptureFlag(&car, &flag));
	PangeaScriptPlayerRelations relations;
	assert(GetScriptPlayerRelations(0, &relations) && relations.carried.id == 9);
	gIsNetworkClient = true;
	assert(!PlayerResetCarriedFlag(&car) && car.CapturedFlag == (Ptr)&flag);
	gIsNetworkClient = false;
	assert(PlayerResetCarriedFlag(&car));
	assert(!car.CapturedFlag && flag.Mode == 0 && flag.CType == CTYPE_TRIGGER);
	assert(flag.Coord.x == 5 && flag.Coord.y == 6 && flag.Coord.z == 7);
	assert(!PlayerResetCarriedFlag(&car));
	assert(GetScriptPlayerRelations(0, &relations) && relations.carried.id == 0);
	operationTarget = &flag;
	PangeaScriptOperationRequest operation = {.operation = PANGEA_SCRIPT_OP_CAPTURE, .playerNum = 0,
		.hasTarget = true, .target = {9, 1}};
	PangeaScriptOperationResult result;
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_OK && result.object.id == 9);
	operation.hasAim = true;
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	operation.hasAim = false;
	operation.mode = "unused";
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	operation.mode = NULL;
	ObjNode recipient = {.ScriptObjectID = 11, .ScriptObjectGeneration = 1};
	gPlayerInfo[1].objNode = &recipient;
	gPlayerInfo[1].onThisMachine = true;
	operationTarget = &recipient;
	operation = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_HANDOVER, .playerNum = 0,
		.hasTarget = true, .target = {11, 1}};
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_OK && result.object.id == 9);
	assert(!car.CapturedFlag && recipient.CapturedFlag == (Ptr)&flag && flag.PlayerNum == 1);
	assert(PlayerResetCarriedFlag(&recipient));
	assert(PlayerCaptureFlag(&car, &flag));
	CollisionBoxType flagBox = {.left = 0, .right = 1, .bottom = 0, .top = 1, .back = 0, .front = 1};
	flag.CollisionBoxes = &flagBox;
	flag.NumCollisionBoxes = 1;
	operation = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_DROP, .playerNum = 0,
		.hasPosition = true, .position = {FLT_MAX, 0, 0}};
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(car.CapturedFlag == (Ptr)&flag && flag.Mode == 1 && flagBox.left == 0);
	flag.CollisionBoxes = NULL;
	flag.NumCollisionBoxes = 0;
	operation = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_DROP, .playerNum = 0,
		.hasVelocity = true, .velocity = {2, 3, 4}};
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_OK && result.object.id == 9);
	assert(!car.CapturedFlag && flag.Mode == 0 && flag.Delta.x == 2 && flag.Delta.y == 3);
	flag.Mode = 0;
	assert(PlayerCaptureFlag(&car, &flag));
	gCapturedFlagCount[0] = 0;
	ObjNode base = {.Group = MODEL_GROUP_GLOBAL, .Kind = TRIGTYPE_TEAMBASE, .ScriptObjectID = 10};
	operationTarget = &base;
	operation = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_DELIVER, .playerNum = 0,
		.hasTarget = true, .target = {10, 1}};
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_OK);
	assert(!car.CapturedFlag && flag.Mode == 2 && gCapturedFlagCount[0] == 1);
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	operation = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_FIRE, .playerNum = 0, .mode = "backward"};
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_OK && powerupFired == 1);
	operation.mode = "garbage";
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && powerupFired == 1);
	operation = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_RESPAWN, .playerNum = 0,
		.hasPosition = true, .position = {100, 200, 300}};
	gPlayerInfo[0].isEliminated = true;
	gNumPlayersEliminated = 1;
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_OK && result.object.id == 1);
	assert(car.Coord.x == 100 && car.Delta.x == 0 && car.DeltaRot.y == 0 && !gPlayerInfo[0].isEliminated);
	assert(gNumPlayersEliminated == 0);
	assert(clearedResults == 1);
	operation.hasVelocity = true;
	operation.velocity = (PangeaScriptVector3){NAN, 0, 0};
	operation.position.x = 999;
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && car.Coord.x == 100);
	gPlayerInfo[0].onThisMachine = false;
	assert(RunScriptPlayerOperation(&operation, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	return 0;
}

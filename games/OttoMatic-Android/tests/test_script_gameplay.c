#include <assert.h>
#include "../src/Scripting/ScriptBindings.c"
#include "ScriptRules.h"

PlayerInfoType gPlayerInfo;
Boolean gLevelCompleted;
Boolean gPlayerIsDead;
Boolean gGameOver;
float gLevelCompletedCoolDownTimer;
short gBestCheckpointNum;
OGLPoint2D gBestCheckpointCoord;
float gBestCheckpointAim;
int gNumHumansInLevel;
int gNumHumansInTransit;
int gNumHumansInSaucer;
int gNumHumansRescuedTotal;
int gNumHumansRescuedOfType[NUM_HUMAN_TYPES];
TerrainItemEntryType** gMasterItemList;
int gNumTerrainItems;
uint32_t gScore;
OGLSetupOutputType* gGameViewInfoPtr;
OGLVector3D gRecentTerrainNormal;
float** gMapYCoords;
int gTerrainUnitWidth;
int gTerrainUnitDepth;
ObjNode* gFirstNodePtr;
OGLPoint3D gCoord;
OGLVector3D gDelta;
ObjNode* gPlayerSaucer;
ObjNode* gTargetPickup;
Boolean gPlayerEnterSaucer;
static ObjNode* mountedCar;
static int nativeActionCalls;
static PangeaScriptObjectHandle operationTarget;
static ObjNode* operationNativeTarget;
static int registeredContacts;
static bool heldNeeds[NUM_CONTROL_NEEDS];
static bool pressedNeeds[NUM_CONTROL_NEEDS];
static bool scriptControlledRules;

bool PangeaScript_RulesAreScriptControlled(void) { return scriptControlledRules; }

Boolean GetNeedState(int need) { return heldNeeds[need]; }
Boolean GetNewNeedState(int need) { return pressedNeeds[need]; }
float GetTerrainY(float x, float z)
{
	gRecentTerrainNormal = (OGLVector3D){0.0f, 1.0f, 0.0f};
	return x + z;
}
uint16_t GetTileAttribsAtRowCol(int row, int column) { return 7; }
Boolean GetWaterY(float x, float z, float* height) { *height = 12.0f; return true; }
Boolean DoSimpleBoxCollisionAgainstObject(float top, float bottom, float left,
	float right, float front, float back, const ObjNode* node)
{
	for (int i = 0; i < node->NumCollisionBoxes; i++)
	{
		const CollisionBoxType* box = &node->CollisionBoxes[i];
		if (box->left <= right && box->right >= left && box->bottom <= top &&
			box->top >= bottom && box->back <= front && box->front >= back) return true;
	}
	return false;
}
bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle)
{
	return handle.id != 0 && handle.generation == 1;
}
bool PangeaScript_ObjectIsScriptControlled(PangeaScriptObjectHandle handle)
{
	return PangeaScript_ObjectExists(handle);
}
PangeaScriptStatus PangeaScript_RegisterObject(const PangeaScriptObjectRegistration* registration,
	PangeaScriptObjectHandle* handle)
{
	*handle = (PangeaScriptObjectHandle){++registeredContacts, 1};
	return PANGEA_SCRIPT_OK;
}
void UpdateObjectTransforms(ObjNode* node) {}
void MorphToSkeletonAnim(SkeletonObjDataType* skeleton, long animation, float speed) {}
void SetSkeletonAnim(SkeletonObjDataType* skeleton, long animation) {}
void SetPlayerStandAnim(ObjNode* node, float speed) {}
const PangeaScriptCustomObjectDefinition* PangeaScript_GetCustomObjectDefinition(const char* id) { return NULL; }
const char* PangeaScript_GetLastError(void) { return ""; }
bool PangeaScript_UnregisterObject(PangeaScriptObjectHandle handle) { return true; }
PangeaScriptStatus PangeaScript_ApplyObjectLifecycle(PangeaScriptObjectHandle handle,
	const PangeaScriptFrameContext* frame, PangeaScriptObjectLifecycle lifecycle) { return PANGEA_SCRIPT_OK; }
void DeleteObject(ObjNode* node) {}
void FindCoordOnJoint(ObjNode* node, long joint, const OGLPoint3D* offset, OGLPoint3D* output)
{
	*output = node->Coord;
}
Boolean SelectPlayerWeapon(short weaponType)
{
	if (weaponType != WEAPON_TYPE_STUNPULSE) return false;
	gPlayerInfo.currentWeaponType = weaponType;
	return true;
}
bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** output)
{
	if (handle.id != operationTarget.id || handle.generation != operationTarget.generation) return false;
	*output = operationNativeTarget;
	return true;
}
bool OttoScript_CarryNativePickup(ObjNode* pickup) { nativeActionCalls++; gTargetPickup = pickup; return pickup != NULL; }
bool OttoScript_DropNativePickup(void) { nativeActionCalls++; gTargetPickup = NULL; return true; }
bool OttoScript_MountNativeCar(ObjNode* car) { nativeActionCalls++; mountedCar = car; return car != NULL; }
bool OttoScript_DismountNativeCar(void) { nativeActionCalls++; mountedCar = NULL; return true; }
ObjNode* OttoScript_GetNativeCar(void) { return mountedCar; }
bool OttoScript_EnterNativeSaucer(ObjNode* saucer) { return false; }
bool OttoScript_ExitNativeSaucer(void) { return false; }
bool OttoScript_CaptureNativeHuman(ObjNode* human) { nativeActionCalls++; return human != NULL; }
bool OttoScript_DeliverNativeHumans(void) { nativeActionCalls++; return true; }
bool OttoScript_DropNativeHumans(void) { nativeActionCalls++; return true; }
void OttoScript_CancelNativeDelivery(void) {}
void OttoScript_CancelNativeWeapon(void) {}
bool OttoScript_FireNativeWeapon(void) { nativeActionCalls++; return true; }
void ResetPlayerAtBestCheckpoint(void) { gPlayerIsDead = false; }
void MovePowerup(ObjNode* node) {}
static void OriginalPickupMove(ObjNode* node) {}
#include "../src/Scripting/ScriptPickupState.inc"

int main(void)
{
	ObjNode node = {0};
	ObjNode pickupState = {.MoveCall = OriginalPickupMove, .CType = CTYPE_POWERUP | CTYPE_MISC | CTYPE_BLOBPOW};
	SaveScriptPickup(&pickupState);
	pickupState.MoveCall = NULL;
	pickupState.CType = 0;
	OttoScript_RestoreNativePickup(&pickupState);
	assert(pickupState.MoveCall == OriginalPickupMove && (pickupState.CType & CTYPE_BLOBPOW));
	SaveScriptPickup(&pickupState);
	OttoScript_ForgetNativePickup(&pickupState);
	assert(gScriptPickup == NULL);
	node.StatusBits = STATUS_BIT_NOCOLLISION;
	assert(OttoObjectSetActive(&node, false));
	assert(!node.ScriptActive);
	assert(node.StatusBits & STATUS_BIT_HIDDEN);
	assert(OttoObjectSetActive(&node, false));
	assert(OttoObjectSetActive(&node, true));
	assert(!(node.StatusBits & STATUS_BIT_HIDDEN));
	assert(node.StatusBits & STATUS_BIT_NOCOLLISION);
	assert(OttoObjectSetActive(&node, false));
	assert(OttoObjectSetCollisionEnabled(&node, true));
	assert(node.StatusBits & STATUS_BIT_NOCOLLISION);
	assert(OttoObjectSetActive(&node, true));
	assert(!(node.StatusBits & STATUS_BIT_NOCOLLISION));
	assert(OttoObjectSetActive(&node, false));
	assert(OttoObjectSetCollisionEnabled(&node, false));
	assert(OttoObjectSetActive(&node, true));
	assert(node.StatusBits & STATUS_BIT_NOCOLLISION);
	assert(OttoObjectSetHealth(&node, 0.4f));
	assert(node.Health == 0.4f);
	assert(!OttoObjectSetHealth(&node, NAN));
	assert(OttoObjectSetDamage(&node, 0.2f));
	assert(!OttoObjectSetDamage(&node, -1.0f));
	gPlayerInfo.objNode = &node;
	ObjNode actionTarget = {.CType = CTYPE_HUMAN, .ScriptObjectID = 30, .ScriptObjectGeneration = 1};
	operationTarget = (PangeaScriptObjectHandle){30, 1};
	operationNativeTarget = &actionTarget;
	PangeaScriptOperationResult operationResult;
	PangeaScriptOperationRequest request = {.playerNum = 0, .operation = PANGEA_SCRIPT_OP_CAPTURE,
		.hasTarget = true, .target = {30, 2}};
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(nativeActionCalls == 0);
	request.target = operationTarget;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	assert(nativeActionCalls == 1 && operationResult.object.id == 30);
	request.operation = PANGEA_SCRIPT_OP_MOUNT;
	node.ScriptObjectID = 11;
	node.ScriptObjectGeneration = 1;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	assert(nativeActionCalls == 2);
	node.ScriptObjectGeneration = 2;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	PangeaScriptPlayerRelations relations;
	gNumHumansInSaucer = 2;
	gNumHumansInTransit = 1;
	assert(GetOttoNativeRelations(0, &relations));
	assert(relations.mounted.id == 30 && relations.capturedCount == 3);
	request.operation = PANGEA_SCRIPT_OP_DISMOUNT;
	request.hasTarget = false;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	assert(GetOttoNativeRelations(0, &relations) && relations.mounted.id == 0);
	request.operation = PANGEA_SCRIPT_OP_RESPAWN;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	gPlayerIsDead = true;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_OK && !gPlayerIsDead);
	node.ScriptObjectGeneration = 1;
	request.operation = PANGEA_SCRIPT_OP_CARRY;
	request.hasTarget = true;
	request.target = operationTarget;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	assert(OttoScript_IsNativeDelegated(&node));
	assert(!OttoScript_UsesControllerOverride(&node));
	PollOttoNativeDelegation(16.0f);
	assert(!OttoScript_IsNativeDelegated(&node) && OttoScript_UsesControllerOverride(&node));
	assert(gTargetPickup == NULL);
	request.hasWeapon = true;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.hasWeapon = false;
	request.operation = PANGEA_SCRIPT_OP_FIRE;
	request.hasTarget = false;
	assert(RunOttoNativeOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	assert(OttoScript_IsNativeDelegated(&node));
	node.ScriptObjectGeneration = 2;
	PollOttoNativeDelegation(0.1f);
	assert(!OttoScript_IsNativeDelegated(&node));
	assert(OttoObjectSetHealth(&node, 0.7f));
	assert(gPlayerInfo.health == 0.7f);
	assert(SetScriptPlayerInvulnerable(0, 2.5f) == PANGEA_SCRIPT_OK);
	node.InvincibleTimer = 0.0f;
	PangeaScriptPlayerSnapshot player;
	assert(GetScriptPlayer(0, &player));
	assert(player.invulnerable && player.invulnerabilityTimeRemaining == 2.5f);
	assert(SetScriptPlayerInvulnerable(0, 0.0f) == PANGEA_SCRIPT_OK);
	assert(GetScriptPlayer(0, &player) && !player.invulnerable);

	heldNeeds[kNeed_Jump] = pressedNeeds[kNeed_Jump] = true;
	gPlayerInfo.analogControlX = 0.5f;
	SampleScriptInput();
	assert(gScriptInput.actions[4].held && gScriptInput.actions[4].pressed);
	assert(gScriptInput.moveX == 0.5f);
	heldNeeds[kNeed_Jump] = pressedNeeds[kNeed_Jump] = false;
	SampleScriptInput();
	assert(gScriptInput.actions[4].released);
	PangeaScriptInputSnapshot input;
	assert(GetScriptInput(0, &input));
	assert(GetScriptInput(0, &input) && input.actions[4].released);
	assert(!GetScriptInput(1, &input));

	float* map = NULL;
	gMapYCoords = &map;
	gTerrainUnitWidth = gTerrainUnitDepth = 1000;
	gRecentTerrainNormal = (OGLVector3D){1.0f, 0.0f, 0.0f};
	PangeaScriptTerrainSample sample;
	assert(SampleScriptTerrain(10.0f, 20.0f, &sample));
	assert(sample.height == 30.0f && sample.normal.y == 1.0f);
	assert(sample.surfaceType == 7 && sample.hasWater && sample.waterHeight == 12.0f);
	assert(gRecentTerrainNormal.x == 1.0f && gRecentTerrainNormal.y == 0.0f);
	assert(!SampleScriptTerrain(-1.0f, 0.0f, &sample));
	assert(!SampleScriptTerrain(NAN, 0.0f, &sample));
	ObjNode contact = {.CType = CTYPE_ENEMY, .Coord = {2, 2, 2}, .NumCollisionBoxes = 1,
		.CollisionBoxes = {{.left = 1, .right = 3, .bottom = 1, .top = 3, .back = 1, .front = 3}}};
	ObjNode ignored = {.CType = CTYPE_ENEMY, .StatusBits = STATUS_BIT_NOCOLLISION,
		.Coord = {2, 2, 2}, .NextNode = &contact};
	gFirstNodePtr = &ignored;
	PangeaScriptBounds bounds = {.min = {0, 0, 0}, .max = {4, 4, 4}};
	PangeaScriptObjectHandle contacts[2];
	int count;
	assert(ScriptOverlapBox(&bounds, CTYPE_ENEMY, contacts, 2, &count));
	assert(count == 1 && contacts[0].id == contact.ScriptObjectID);
	assert(registeredContacts == 1);
	assert(ScriptOverlapBox(&bounds, CTYPE_ENEMY, contacts, 2, &count));
	assert(count == 1 && registeredContacts == 1);
	assert(ScriptOverlapBox(&bounds, CTYPE_PLAYER, contacts, 2, &count) && count == 0);
	assert(OttoScript_UsesControllerOverride(&contact));
	contact.ScriptDefinitionID[0] = 'x';
	assert(!OttoScript_UsesControllerOverride(&contact));
	contact.ScriptDefinitionID[0] = 0;
	contact.ScriptObjectGeneration = 2;
	assert(!OttoScript_UsesControllerOverride(&contact));
	contact.ScriptObjectGeneration = 1;
	contact.NumCollisionBoxes = 2;
	contact.CollisionBoxes[1] = (CollisionBoxType){.left = -2, .right = 4, .bottom = -5,
		.top = 6, .back = -3, .front = 8};
	PangeaScriptBounds actualBounds;
	assert(OttoObjectGetBounds(&contact, &actualBounds));
	assert(actualBounds.min.x == -2 && actualBounds.min.y == -5 && actualBounds.max.z == 8);
	PangeaScriptVector3 position = {12, 22, 32};
	assert(OttoObjectSetPosition(&contact, &position));
	assert(OttoObjectGetBounds(&contact, &actualBounds));
	assert(actualBounds.min.x == 8 && actualBounds.min.y == 15 && actualBounds.max.z == 38);
	assert(contact.CollisionBoxes[0].left == 11 && contact.CollisionBoxes[1].left == 8);
	PangeaScriptBounds movedQuery = {.min = {8, 15, 27}, .max = {9, 16, 28}};
	assert(ScriptOverlapBox(&movedQuery, CTYPE_ENEMY, contacts, 2, &count) && count == 1);
	position.x = NAN;
	assert(!OttoObjectSetPosition(&contact, &position) && contact.Coord.x == 12);
	contact.CollisionBoxes[1].right = NAN;
	assert(!OttoObjectGetBounds(&contact, &actualBounds));
	assert(ScriptOverlapBox(&bounds, CTYPE_ENEMY, contacts, 2, &count) && count == 0);
	contact.NumCollisionBoxes = 0;
	assert(!OttoObjectGetBounds(&contact, &actualBounds));
	bounds.max.z = NAN;
	assert(!ScriptOverlapBox(&bounds, CTYPE_ENEMY, contacts, 2, &count));
	PangeaScriptStateValue value = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 0.75};
	assert(SetOttoGameplayState(0, "fuel", &value) == PANGEA_SCRIPT_OK);
	assert(gPlayerInfo.fuel == 0.75f);
	assert(GetOttoGameplayState(0, "fuel", &value) && value.number == 0.75);
	value.number = NAN;
	assert(SetOttoGameplayState(0, "fuel", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = 2;
	assert(SetOttoGameplayState(0, "jumpJet", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(SetOttoGameplayState(0, "farmersRescued", &value) == PANGEA_SCRIPT_OK);
	assert(gNumHumansRescuedTotal == 2 && gNumHumansRescuedOfType[HUMAN_TYPE_FARMER] == 2);
	value.number = 1;
	assert(SetOttoGameplayState(0, "farmersRescued", &value) == PANGEA_SCRIPT_OK);
	assert(gNumHumansRescuedTotal == 1);
	assert(SetOttoGameplayState(0, "humansInTransit", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	TerrainItemEntryType checkpoint = {.type = MAP_ITEM_CHECKPOINT, .x = 120, .y = 240, .parm = {3, 2}};
	TerrainItemEntryType* items = &checkpoint;
	gMasterItemList = &items;
	gNumTerrainItems = 1;
	value.number = 2;
	assert(SetOttoGameplayState(0, "checkpoint", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = 3;
	assert(SetOttoGameplayState(0, "checkpoint", &value) == PANGEA_SCRIPT_OK);
	assert(gBestCheckpointNum == 3 && gBestCheckpointCoord.x == 120 && gBestCheckpointCoord.y == 240);
	assert(fabsf(gBestCheckpointAim - PI2 / 4) < 0.0001f);
	value.number = WEAPON_TYPE_STUNPULSE;
	assert(SetOttoGameplayState(0, "activeWeapon", &value) == PANGEA_SCRIPT_OK);
	gPlayerInfo.weaponInventory[0] = (WeaponInventoryType){WEAPON_TYPE_STUNPULSE, 5};
	assert(SetScriptPlayerWeaponQuantity(0, WEAPON_TYPE_STUNPULSE, 0) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(gPlayerInfo.weaponInventory[0].quantity == 5 && gPlayerInfo.currentWeaponType == WEAPON_TYPE_STUNPULSE);
	assert(SetScriptPlayerWeaponQuantity(0, WEAPON_TYPE_FIST, 0) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = WEAPON_TYPE_STUNPULSE + 0.5;
	assert(SetOttoGameplayState(0, "activeWeapon", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(CompleteOttoGameplay(1, true) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(CompleteOttoGameplay(0, true) == PANGEA_SCRIPT_OK && gLevelCompleted);
	assert(CompleteOttoGameplay(0, true) == PANGEA_SCRIPT_BAD_ARGUMENT);
	gLevelCompleted = false;
	assert(CompleteOttoGameplay(0, false) == PANGEA_SCRIPT_OK && gGameOver);
	gLevelCompleted = false;
	gLevelCompletedCoolDownTimer = 8;
	int previousRescues = gNumHumansRescuedTotal;
	scriptControlledRules = true;
	assert(!OttoScript_CompleteNativeLevel(4));
	assert(!gLevelCompleted && gLevelCompletedCoolDownTimer == 8);
	assert(!OttoScript_RecordNativeRescue(HUMAN_TYPE_FARMER));
	assert(gNumHumansRescuedTotal == previousRescues);
	value.number = 3;
	assert(SetOttoGameplayState(0, "farmersRescued", &value) == PANGEA_SCRIPT_OK);
	assert(gNumHumansRescuedOfType[HUMAN_TYPE_FARMER] == 3);
	gGameOver = false;
	assert(CompleteOttoGameplay(0, true) == PANGEA_SCRIPT_OK && gLevelCompleted);
	gLevelCompleted = false;
	scriptControlledRules = false;
	assert(OttoScript_CompleteNativeLevel(4));
	assert(gLevelCompleted && gLevelCompletedCoolDownTimer == 4);
	assert(OttoScript_RecordNativeRescue(HUMAN_TYPE_FARMER));
	assert(gNumHumansRescuedOfType[HUMAN_TYPE_FARMER] == 4);
	return 0;
}

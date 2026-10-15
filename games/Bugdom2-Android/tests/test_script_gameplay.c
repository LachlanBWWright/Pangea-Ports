#include <assert.h>
#include <float.h>
#include "../Source/Scripting/ScriptGameplay.c"
#include "../Source/Scripting/ScriptObjectQueries.c"
#include "../Source/Scripting/ScriptMechanics.c"
#include "../Source/System/GameplayRules.c"

Boolean gInGameNow = true, gLevelCompleted;
Boolean gPlayerIsDead;
ObjNode* gCurrentCarryingMoth;
PlayerInfoType gPlayerInfo;
int gNumMice, gNumDrowningMiceRescued, gNumDrowingMiceToRescue, gNumFreedBees;
int gTerrainUnitWidth = 100, gTerrainUnitDepth = 100;
OGLVector3D gRecentTerrainNormal;
ObjNode* gFirstNodePtr;
static int gCompletionCalls;
static int gJointCalls;
static int gTransformCalls;
static bool gScriptRules;
static bool gScriptController;
static int gHeldUpdates, gDrops, gInteractions;
Boolean IsTunnelLevel(void) { return false; }
float OGLPoint3D_Distance(const OGLPoint3D* a, const OGLPoint3D* b)
{ return sqrtf((a->x-b->x)*(a->x-b->x)+(a->y-b->y)*(a->y-b->y)+(a->z-b->z)*(a->z-b->z)); }
void UpdateHeldObject(ObjNode* player) { (void)player; gHeldUpdates++; }
void SetPlayerStandAnim(ObjNode* player, float speed) { (void)player; (void)speed; }
void ReleaseHeldObject(ObjNode* player, ObjNode* held)
{
	(void)player; gDrops++; held->StatusBits &= ~(STATUS_BIT_NOMOVE | STATUS_BIT_NOCOLLISION);
	gPlayerInfo.heldObject = NULL;
}
void PlayerDropObject(ObjNode* player, ObjNode* held) { ReleaseHeldObject(player, held); }
Boolean Bugdom2Script_MountBall(ObjNode* ball) { gPlayerInfo.ridingBall = ball; return ball != NULL; }
Boolean Bugdom2Script_SetPlayerForm(Boolean ball) { (void)ball; gPlayerInfo.ridingBall = NULL; return true; }
Boolean LaunchBuddyBug(void) { return false; }
ObjNode* Bugdom2Script_GetMountedPlane(void) { return NULL; }
Boolean Bugdom2Script_MountPlane(ObjNode* plane) { (void)plane; return false; }
Boolean Bugdom2Script_CreatePlaneMount(void) { return false; }
Boolean Bugdom2Script_DismountPlane(void) { return false; }
Boolean Bugdom2Script_FirePlane(Boolean bomb) { (void)bomb; return false; }
void Bugdom2Script_ClearPlaneRelationship(ObjNode* object) { (void)object; }
void Bugdom2Script_AlignMountedPlane(ObjNode* player) { (void)player; }
bool PangeaScript_ObjectIsScriptControlled(PangeaScriptObjectHandle handle)
{ (void)handle; return gScriptController; }
void KeepOldCollisionBoxes(ObjNode* object) { (void)object; }
void UpdateShadow(ObjNode* object) { (void)object; }
void CreateMyBuddy(OGLPoint3D* position) { (void)position; }
static ObjNode* gResolvedTarget;
bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** output)
{ if (handle.id != 1) return false; *output = gResolvedTarget; return true; }
static Boolean AcceptHeld(ObjNode* target, ObjNode* player, Byte sides)
{
	(void)target; (void)sides;
	assert(AllowNativeGameplayRules());
	gInteractions++; ReleaseHeldObject(player, gPlayerInfo.heldObject); return true;
}
bool PangeaScript_RulesAreScriptControlled(void) { return gScriptRules; }

void UpdateObjectTransforms(ObjNode* object) { (void)object; gTransformCalls++; }
void CalcObjectBoxFromNode(ObjNode* object) { (void)object; }

void FindCoordOnJoint(ObjNode* object, long joint, const OGLPoint3D* origin, OGLPoint3D* output)
{
	assert(origin->x == 0 && origin->y == 0 && origin->z == 0);
	gJointCalls++;
	*output = (OGLPoint3D){object->Coord.x + joint, object->Coord.y, object->Coord.z};
}

int GetNeedState(int need) { (void)need; return KEYSTATE_OFF; }
float GetNeedAxis1D(int negative, int positive) { (void)negative; (void)positive; return 0; }
float GetTerrainY(float x, float z)
{
	(void)x; (void)z;
	gRecentTerrainNormal = (OGLVector3D){0, 1, 0};
	return 10;
}
Boolean GetWaterY(float x, float z, float* height)
{
	(void)x; (void)z; *height = 20; return true;
}
Boolean DoSimpleBoxCollisionAgainstObject(float top, float bottom, float left,
	float right, float front, float back, ObjNode* object)
{
	(void)top; (void)bottom; (void)left; (void)right; (void)front; (void)back;
	return object->NumCollisionBoxes != 0;
}
void Bugdom2Script_RegisterObject(ObjNode* object, const char* id, const char* category)
{
	(void)id; (void)category;
	object->ScriptObjectID = 1; object->ScriptObjectGeneration = 1;
}
bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle) { return handle.id == 1; }
void StartLevelCompletion(float delay)
{
	assert(delay == 0); gCompletionCalls++; gLevelCompleted = true;
}

static void AssertUnusedOptionsRejected(void)
{
	const PangeaScriptPlayerOperation operations[] = {
		PANGEA_SCRIPT_OP_CARRY, PANGEA_SCRIPT_OP_DROP, PANGEA_SCRIPT_OP_HANDOVER,
		PANGEA_SCRIPT_OP_INTERACT, PANGEA_SCRIPT_OP_MOUNT, PANGEA_SCRIPT_OP_DISMOUNT,
		PANGEA_SCRIPT_OP_TRANSFORM, PANGEA_SCRIPT_OP_FIRE,
		PANGEA_SCRIPT_OP_COMMAND_BUDDY, PANGEA_SCRIPT_OP_RELEASE_BUDDY,
	};
	for (unsigned int i = 0; i < sizeof(operations) / sizeof(operations[0]); i++)
	{
		PangeaScriptOperationRequest request = {.operation = operations[i]};
		PangeaScriptOperationResult output;
		request.hasPosition = true;
		assert(Bugdom2Script_RunOperation(&request, &output) == PANGEA_SCRIPT_BAD_ARGUMENT);
		request.hasPosition = false; request.hasVelocity = true;
		assert(Bugdom2Script_RunOperation(&request, &output) == PANGEA_SCRIPT_BAD_ARGUMENT);
		request.hasVelocity = false; request.hasAim = true;
		assert(Bugdom2Script_RunOperation(&request, &output) == PANGEA_SCRIPT_BAD_ARGUMENT);
		request.hasAim = false; request.hasWeapon = true;
		assert(Bugdom2Script_RunOperation(&request, &output) == PANGEA_SCRIPT_BAD_ARGUMENT);
		request.hasWeapon = false; request.hasJoint = true;
		assert(Bugdom2Script_RunOperation(&request, &output) == PANGEA_SCRIPT_BAD_ARGUMENT);
		request.hasJoint = false; request.mode = "unsupported";
		assert(Bugdom2Script_RunOperation(&request, &output) == PANGEA_SCRIPT_BAD_ARGUMENT);
		request.mode = NULL; request.form = "unsupported";
		assert(Bugdom2Script_RunOperation(&request, &output) == PANGEA_SCRIPT_BAD_ARGUMENT);
	}
}

int main(void)
{
	ObjNode player = {0};
	gPlayerInfo.objNode = &player;
	PangeaScriptStateValue value = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 3};
	assert(SetGameplayState(0, "miceRescued", &value) == PANGEA_SCRIPT_OK);
	assert(gPlayerInfo.numMiceRescued == 3);
	value.number = 3.5;
	assert(SetGameplayState(0, "miceRescued", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = NAN;
	assert(SetGameplayState(0, "glidePower", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(SetGameplayState(0, "buddyCount", &value) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	value = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_BOOLEAN, .boolean = true};
	assert(SetGameplayState(0, "hasMap", &value) == PANGEA_SCRIPT_OK);
	assert(GetGameplayState(0, "hasMap", &value) && value.boolean);
	assert(!GetGameplayState(1, "hasMap", &value));
	assert(CompleteGameplay(0, false) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	assert(gCompletionCalls == 0);
	assert(CompleteGameplay(0, true) == PANGEA_SCRIPT_OK && gCompletionCalls == 1);
	gScriptRules = true;
	assert(!AllowNativeGameplayRules());
	StartAutomaticLevelCompletion(0);
	assert(gCompletionCalls == 1);
	assert(CompleteGameplay(0, true) == PANGEA_SCRIPT_OK && gCompletionCalls == 2);
	gScriptRules = false;
	assert(AllowNativeGameplayRules());
	StartAutomaticLevelCompletion(0);
	assert(gCompletionCalls == 3);
	PangeaScriptTerrainSample terrain;
	gRecentTerrainNormal = (OGLVector3D){1, 0, 0};
	assert(SampleGameplayTerrain(5, 5, &terrain) && terrain.height == 10);
	assert(terrain.hasWater && terrain.waterHeight == 20 && gRecentTerrainNormal.x == 1);
	assert(!SampleGameplayTerrain(-1, 5, &terrain));
	ObjNode disabled = {.CType = CTYPE_ENEMY, .StatusBits = STATUS_BIT_NOCOLLISION, .NumCollisionBoxes = 1};
	ObjNode enemy = {.CType = CTYPE_ENEMY, .NextNode = &disabled, .NumCollisionBoxes = 1};
	gFirstNodePtr = &enemy;
	PangeaScriptObjectHandle result[1];
	PangeaScriptBounds bounds = {.min = {0}, .max = {10, 10, 10}};
	int count;
	assert(OverlapGameplayBox(&bounds, CTYPE_ENEMY, result, 1, &count) && count == 1);
	assert(disabled.ScriptObjectID == 0);
	assert(OverlapGameplayBox(&bounds, CTYPE_PLAYER, result, 1, &count) && count == 0);
	ObjNode collider = {.Coord = {100, 200, 300}, .NumCollisionBoxes = 2};
	collider.CollisionBoxes[0] = (CollisionBoxType){.left = 90, .right = 110,
		.bottom = 190, .top = 210, .back = 290, .front = 310};
	collider.CollisionBoxes[1] = (CollisionBoxType){.left = 80, .right = 120,
		.bottom = 200, .top = 230, .back = 295, .front = 320};
	assert(Bugdom2Script_GetObjectBounds(&collider, &bounds));
	assert(bounds.min.x == 80 && bounds.max.x == 120);
	assert(bounds.min.y == 190 && bounds.max.y == 230);
	assert(bounds.min.z == 290 && bounds.max.z == 320);
	PangeaScriptVector3 position = {110, 220, 330};
	assert(Bugdom2Script_SetObjectPosition(&collider, &position) && gTransformCalls == 1);
	assert(Bugdom2Script_GetObjectBounds(&collider, &bounds));
	assert(bounds.min.x == 90 && bounds.max.x == 130);
	assert(bounds.min.y == 210 && bounds.max.y == 250);
	assert(bounds.min.z == 320 && bounds.max.z == 350);
	gPlayerInfo.heldObject = &enemy;
	assert(Bugdom2Script_SetObjectPosition(&player, &position));
	assert(gPlayerInfo.coord.x == 110 && gPlayerInfo.coord.y == 220 && gPlayerInfo.coord.z == 330);
	assert(gPlayerInfo.heldObject == &enemy);
	gPlayerInfo.ridingBall = &collider;
	Bugdom2Script_ClearPlayerRelationships(NULL);
	Bugdom2Script_ClearPlayerRelationships(&player);
	assert(gPlayerInfo.heldObject == NULL && gPlayerInfo.ridingBall == NULL);
	gPlayerInfo.heldObject = &enemy;
	gPlayerInfo.ridingBall = &collider;
	Bugdom2Script_ClearPlayerRelationships(&enemy);
	assert(gPlayerInfo.heldObject == NULL && gPlayerInfo.ridingBall == &collider);
	Bugdom2Script_ClearPlayerRelationships(&collider);
	assert(gPlayerInfo.ridingBall == NULL);
	collider.CollisionBoxes[1].left = NAN;
	assert(!Bugdom2Script_GetObjectBounds(&collider, &bounds));
	position = (PangeaScriptVector3){120, 230, 340};
	assert(!Bugdom2Script_SetObjectPosition(&collider, &position));
	assert(collider.Coord.x == 110 && collider.Coord.y == 220 && collider.Coord.z == 330);
	collider.NumCollisionBoxes = 0;
	assert(!Bugdom2Script_GetObjectBounds(&collider, &bounds));
	collider.Coord.x = -FLT_MAX;
	position.x = FLT_MAX;
	assert(!Bugdom2Script_SetObjectPosition(&collider, &position));
	assert(collider.Coord.x == -FLT_MAX);
	collider.Coord.x = 110;
	SkeletonDefType definition = {.NumBones = 2};
	SkeletonObjDataType skeleton = {.skeletonDefinition = &definition};
	collider.Skeleton = &skeleton;
	PangeaScriptVector3 jointPosition;
	assert(!Bugdom2Script_GetJointPosition(&collider, -1, &jointPosition));
	assert(!Bugdom2Script_GetJointPosition(&collider, 2, &jointPosition) && gJointCalls == 0);
	assert(Bugdom2Script_GetJointPosition(&collider, 1, &jointPosition));
	assert(jointPosition.x == 111 && jointPosition.y == 220 && jointPosition.z == 330);
	collider.CType = INVALID_NODE_FLAG;
	assert(!Bugdom2Script_GetJointPosition(&collider, 0, &jointPosition) && gJointCalls == 1);
	player.Skeleton = &skeleton;
	player.Coord = (OGLPoint3D){0};
	ObjNode pickup = {.CType = CTYPE_PICKUP, .Kind = PICKUP_KIND_ACORN};
	gResolvedTarget = &pickup;
	assert(Bugdom2Script_ObjectStillAlive(&pickup, (PangeaScriptObjectHandle){1, 1}));
	assert(!Bugdom2Script_ObjectStillAlive(&enemy, (PangeaScriptObjectHandle){1, 1}));
	ObjNode* previousFirst = gFirstNodePtr;
	gFirstNodePtr = &pickup;
	assert(Bugdom2Script_ObjectStillAlive(&pickup, (PangeaScriptObjectHandle){0}));
	Bugdom2Script_RegisterObject(&pickup, "bugdom2.test", "test");
	gFirstNodePtr = previousFirst;
	assert(!Bugdom2Script_ObjectStillAlive(&pickup, (PangeaScriptObjectHandle){0}));
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_CARRY,
		.hasTarget = true, .target = {1, 1}};
	PangeaScriptOperationResult operationResult;
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	assert(gPlayerInfo.heldObject == &pickup && gHeldUpdates == 1);
	Bugdom2Script_UpdatePlayerRelationships(&player);
	assert(gHeldUpdates == 2);
	assert((pickup.StatusBits & (STATUS_BIT_NOMOVE | STATUS_BIT_NOCOLLISION)) ==
		(STATUS_BIT_NOMOVE | STATUS_BIT_NOCOLLISION));
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	request.operation = PANGEA_SCRIPT_OP_DROP;
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	assert(!gPlayerInfo.heldObject && !(pickup.StatusBits & STATUS_BIT_NOCOLLISION));
	request.operation = PANGEA_SCRIPT_OP_CARRY;
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	ObjNode recipient = {.CType = CTYPE_TRIGGER, .TriggerCallback = AcceptHeld};
	gResolvedTarget = &recipient;
	request.operation = PANGEA_SCRIPT_OP_HANDOVER;
	gScriptRules = true;
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	assert(gInteractions == 1 && !gPlayerInfo.heldObject && !AllowNativeGameplayRules());
	request.operation = PANGEA_SCRIPT_OP_CARRY;
	gResolvedTarget = &pickup;
	pickup.Coord.x = 301;
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	request.target.id = 2;
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_TRANSFORM, .form = "bug"};
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_OK);
	request.form = "unrecognised";
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.form = "plane";
	assert(Bugdom2Script_RunOperation(&request, &operationResult) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	gPlayerInfo.numBuddyBugs = 2;
	gPlayerInfo.buddyBugs[0] = &pickup;
	gPlayerInfo.buddyBugs[1] = &recipient;
	gCurrentCarryingMoth = &pickup;
	Bugdom2Script_ClearPlayerRelationships(&pickup);
	assert(gPlayerInfo.numBuddyBugs == 1 && gPlayerInfo.buddyBugs[0] == &recipient);
	assert(!gPlayerInfo.buddyBugs[1] && !gCurrentCarryingMoth);
	ObjNode ball = {.CType = CTYPE_MISC, .Coord = {20, 30, 40}, .Scale = {1, 1, 1},
		.BBox = {.max = {.y = 50}}, .Rot = {.y = 1}, .Delta = {3, 4, 5}, .Speed2D = 100};
	gPlayerInfo.ridingBall = &ball;
	gScriptController = true;
	assert(Bugdom2Script_MountUsesScriptController(&ball));
	assert(!Bugdom2Script_MountUsesScriptController(&pickup));
	Bugdom2Script_UpdatePlayerRelationships(&player);
	assert(player.Coord.x == 20 && player.Coord.y == 80 && player.Coord.z == 40);
	assert(player.Rot.y == 1 && player.Delta.x == 3 && player.Skeleton->AnimSpeed == .4f);
	assert(gPlayerInfo.coord.y == -20);
	gScriptController = false;
	assert(!Bugdom2Script_MountUsesScriptController(&ball));
	gPlayerInfo.heldObject = &pickup;
	pickup.StatusBits |= STATUS_BIT_NOMOVE | STATUS_BIT_NOCOLLISION;
	gCurrentCarryingMoth = &recipient;
	Bugdom2Script_ReleasePlayerCarry();
	assert(!gPlayerInfo.heldObject && !gPlayerInfo.ridingBall && !gCurrentCarryingMoth);
	assert(!(pickup.StatusBits & (STATUS_BIT_NOMOVE | STATUS_BIT_NOCOLLISION)));
	AssertUnusedOptionsRejected();
	return 0;
}

#include "../src/Scripting/ScriptBindings.c"
#include <assert.h>

ObjNode* gPlayerObj;
ObjNode* gFirstNodePtr;
#include "../src/Scripting/ScriptObjectLifetime.inc"
TQ3Point3D gMyCoord;
float gMyHealth;
uint32_t gInfobarUpdateBits;
float gFuel, gTimeRemaining;
Byte gCurrentAttackMode, gMyStartAim;
long gMyStartX, gMyStartZ, gTerrainUnitWidth = 10000, gTerrainUnitDepth = 10000;
short gRecoveredEggs[NUM_EGG_SPECIES], gRequiredEggs[NUM_EGG_SPECIES];
short gNumLives, gWeaponInventory[NUM_ATTACK_MODES];
Boolean gWonGameFlag, gGameOverFlag;
Boolean gPlayerGotKilledFlag;
float gFramesPerSecondFrac;
static int exitAdvances;
static int deliveredDeletes;
float gAutoFireCounter;
Boolean FirePlayerWeapon(ObjNode* player) { (void)player; return false; }
void ResetPlayer(void) {}
void CalcObjectBoxFromNode(ObjNode* object) { (void)object; }
void MorphToSkeletonAnim(SkeletonObjDataType* skeleton, long animation, float speed)
{ (void)speed; skeleton->AnimNum = animation; }
void AddToScore(long points) { (void)points; }
short PlayEffect(short effect) { (void)effect; return -1; }
short PlayEffect_Parms(short effect, unsigned char volume, unsigned long frequency)
{ (void)effect; (void)volume; (void)frequency; return -1; }
void DeleteObject(ObjNode* object)
{
	assert(PangeaScript_ObjectIsScriptControlled((PangeaScriptObjectHandle){object->ScriptObjectID, object->ScriptObjectGeneration}));
	PangeaScript_UnregisterObject((PangeaScriptObjectHandle){object->ScriptObjectID, object->ScriptObjectGeneration});
	deliveredDeletes++;
}
void MakeObjectTransparent(ObjNode* object, float alpha) { (void)object; (void)alpha; }
ObjNode* MakeTimePortal(Byte kind, float x, float z) { (void)kind; (void)x; (void)z; return NULL; }
void AdvancePlayerExit(ObjNode* player) { exitAdvances++; player->ExitTimer += gFramesPerSecondFrac; }
void SetSkeletonAnim(SkeletonObjDataType* skeleton, long animation) { skeleton->AnimNum = animation; }
static int gTransformUpdates;

void UpdateObjectTransforms(ObjNode* node)
{
	(void)node;
	gTransformUpdates++;
}

static void TestNativeBounds(void)
{
	ObjNode object = {.NumCollisionBoxes = 1};
	object.CollisionBoxes[0] = (CollisionBoxType){.left = -10, .right = 10, .bottom = -5, .top = 5, .back = -8, .front = 8};
	PangeaScriptBounds bounds;
	assert(NanosaurScript_GetObjectBounds(&object, &bounds));
	assert(bounds.min.x == -10 && bounds.max.x == 10 && bounds.min.y == -5 && bounds.max.y == 5 && bounds.min.z == -8 && bounds.max.z == 8);
	PangeaScriptVector3 position = {100, 50, -50};
	gPlayerObj = &object;
	assert(NanosaurScript_SetObjectPosition(&object, &position));
	assert(gMyCoord.x == 100 && gMyCoord.y == 50 && gMyCoord.z == -50);
	assert(gTransformUpdates == 1);
	assert(NanosaurScript_GetObjectBounds(&object, &bounds));
	assert(bounds.min.x == 90 && bounds.max.x == 110 && bounds.min.y == 45 && bounds.max.y == 55 && bounds.min.z == -58 && bounds.max.z == -42);
	position.x = INFINITY;
	assert(!NanosaurScript_SetObjectPosition(&object, &position));
	assert(object.Coord.x == 100 && gTransformUpdates == 1);
	position.x = 100.75f;
	assert(NanosaurScript_SetObjectPosition(&object, &position));
	position.x = 101.5f;
	assert(NanosaurScript_SetObjectPosition(&object, &position));
	assert(NanosaurScript_GetObjectBounds(&object, &bounds) && bounds.min.x == 91 && bounds.max.x == 111);
	object.CollisionBoxes[0].left = object.CollisionBoxes[0].right + 1;
	assert(!NanosaurScript_GetObjectBounds(&object, &bounds));
	object.NumCollisionBoxes = 0;
	assert(!NanosaurScript_GetObjectBounds(&object, &bounds));
	object.CType = INVALID_NODE_FLAG;
	assert(!NanosaurScript_GetObjectBounds(&object, &bounds));
	gPlayerObj = NULL;
}

static void TestGameplayState(void)
{
	ObjNode player = {0};
	gPlayerObj = &player;
	PangeaScriptStateValue value = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 12.5};
	assert(SetScriptState(0, "fuel", &value) == PANGEA_SCRIPT_OK);
	assert(gFuel == 12.5f && (gInfobarUpdateBits & UPDATE_FUEL));
	assert(GetScriptState(0, "fuel", &value) && value.number == 12.5);
	value.number = NAN;
	assert(SetScriptState(0, "fuel", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = MAX_FUEL_CAPACITY + 1;
	assert(SetScriptState(0, "fuel", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = 2;
	assert(SetScriptState(1, "eggCount3", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(SetScriptState(0, "eggCount3", &value) == PANGEA_SCRIPT_OK);
	assert(gRecoveredEggs[2] == 2);
	assert(SetScriptState(0, "eggRequired3", &value) == PANGEA_SCRIPT_OK);
	assert(gRequiredEggs[2] == 2);
	value.number = 32767;
	assert(SetScriptState(0, "eggCount3", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(gRecoveredEggs[2] == 2);
	value.number = 1.5;
	assert(SetScriptState(0, "eggRequired3", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = 5;
	assert(SetScriptState(0, "activeWeapon", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = 10000;
	assert(SetScriptState(0, "spawnX", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.number = 120;
	assert(SetScriptState(0, "timeRemaining", &value) == PANGEA_SCRIPT_OK);
	assert(gTimeRemaining == 120 && (gInfobarUpdateBits & UPDATE_IMPACTTIME));
	assert(SetScriptState(0, "unknown", &value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(SetScriptPlayerHealth(0, 0.75f) == PANGEA_SCRIPT_OK);
	assert(gMyHealth == 0.75f && player.Health == 0.75f && (gInfobarUpdateBits & UPDATE_HEALTH));
	assert(SetScriptPlayerHealth(0, NAN) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(SetScriptPlayerHealth(0, -1) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(gMyHealth == 0.75f);
	assert(SetScriptPlayerLives(0, SHRT_MAX + 1) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(SetScriptPlayerLives(0, 3) == PANGEA_SCRIPT_OK);
	assert(gNumLives == 3 && (gInfobarUpdateBits & UPDATE_LIVES));
	assert(SetScriptPlayerWeaponQuantity(0, 1, 5) == PANGEA_SCRIPT_OK);
	assert(gWeaponInventory[1] == 5 && gCurrentAttackMode == 1 && (gInfobarUpdateBits & UPDATE_WEAPONICON));
	assert(SetScriptPlayerInvulnerable(0, INFINITY) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(SetScriptPlayerInvulnerable(0, 2.5f) == PANGEA_SCRIPT_OK && player.InvincibleTimer == 2.5f);
	assert(CompleteScriptMission(0, true) == PANGEA_SCRIPT_OK && gWonGameFlag && gGameOverFlag);
	assert(CompleteScriptMission(0, false) == PANGEA_SCRIPT_OK && !gWonGameFlag);
	gPlayerObj = NULL;
	assert(!GetScriptState(0, "fuel", &value));
	assert(CompleteScriptMission(0, true) == PANGEA_SCRIPT_BAD_ARGUMENT);
}

static void TestOwnedEggDelivery(void)
{
	SkeletonObjDataType skeleton = {0};
	ObjNode player = {.Skeleton = &skeleton};
	ObjNode egg = {.Genre = DISPLAY_GROUP_GENRE, .Group = LEVEL0_MGroupNum_Egg,
		.Type = LEVEL0_MObjType_Egg1, .CType = CTYPE_MISC | CTYPE_PICKUP};
	ObjNode portal = {.Genre = EVENT_GENRE, .Kind = PORTAL_TYPE_EGG, .CType = CTYPE_PORTAL};
	PangeaScriptObjectOps ops = {.getPosition = NanosaurScript_GetObjectPosition};
	PangeaScriptObjectRegistration registration = {.ops = &ops, .objectType = "test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL};
	PangeaScriptObjectHandle eggHandle, portalHandle;
	registration.nativeObject = &egg;
	assert(PangeaScript_RegisterObject(&registration, &eggHandle) == PANGEA_SCRIPT_OK);
	egg.ScriptObjectID = eggHandle.id;
	egg.ScriptObjectGeneration = eggHandle.generation;
	assert(PangeaScript_SetObjectScriptControlled(eggHandle, true));
	registration.nativeObject = &portal;
	assert(PangeaScript_RegisterObject(&registration, &portalHandle) == PANGEA_SCRIPT_OK);
	gPlayerObj = &player;
	gGameOverFlag = false;
	for (int i = 0; i < NUM_EGG_SPECIES; i++) { gRecoveredEggs[i] = 0; gRequiredEggs[i] = 1; }
	assert(CarryEgg(&player, &egg));
	assert(PangeaScript_ObjectIsScriptControlled(eggHandle));
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_DELIVER,
		.playerNum = 0, .hasTarget = true, .target = portalHandle};
	PangeaScriptOperationResult result;
	assert(RunScriptOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(deliveredDeletes == 1 && gRecoveredEggs[0] == 1 && !player.CarriedObj);
	assert(result.object.id == 0 && !PangeaScript_ObjectExists(eggHandle));
	assert(RunScriptOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(deliveredDeletes == 1 && gRecoveredEggs[0] == 1);
	PangeaScript_UnregisterObject(portalHandle);
	gPlayerObj = NULL;
	gTransformUpdates = 0;
}

static void TestNativeTransitionAndOptions(void)
{
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_CARRY};
	assert(ScriptOperationOptionsValid(&request));
	request.hasWeapon = true;
	assert(!ScriptOperationOptionsValid(&request));
	request.operation = PANGEA_SCRIPT_OP_FIRE;
	assert(ScriptOperationOptionsValid(&request));
	request.hasTarget = true;
	assert(!ScriptOperationOptionsValid(&request));
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_DROP, .hasVelocity = true, .velocity = {1, 2, 3}};
	assert(ScriptOperationOptionsValid(&request));
	request.velocity.y = NAN;
	assert(!ScriptOperationOptionsValid(&request));
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_INTERACT, .mode = "unknown"};
	assert(!ScriptOperationOptionsValid(&request));
	SkeletonObjDataType skeleton = {.AnimNum = PLAYER_ANIM_EXIT};
	ObjNode player = {.Skeleton = &skeleton, .ScriptObjectID = 90, .ScriptObjectGeneration = 1};
	PangeaScriptObjectOps ops = {.getPosition = NanosaurScript_GetObjectPosition};
	PangeaScriptObjectRegistration registration = {.nativeObject = &player, .ops = &ops,
		.objectType = "player", .capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	assert(ObjectRemainsLive(&player, handle));
	ObjNode other = {0};
	assert(!ObjectRemainsLive(&other, handle));
	gFirstNodePtr = &other;
	assert(ObjectRemainsLive(&other, (PangeaScriptObjectHandle){0}));
	gFirstNodePtr = NULL;
	assert(!ObjectRemainsLive(&other, (PangeaScriptObjectHandle){0}));
	player.ScriptObjectID = handle.id;
	player.ScriptObjectGeneration = handle.generation;
	assert(PangeaScript_SetObjectScriptControlled(handle, true));
	gPlayerObj = &player;
	gFramesPerSecondFrac = 1;
	BeginExitTransition(&player);
	assert(NanosaurScript_RunNativeTransition(&player) && exitAdvances == 1);
	assert(PangeaScript_ObjectIsScriptControlled(handle));
	gPlayerGotKilledFlag = true;
	assert(!NanosaurScript_RunNativeTransition(&player) && !gExitTransition.id);
	gPlayerGotKilledFlag = false;
	BeginExitTransition(&player);
	player.ScriptObjectGeneration++;
	assert(!NanosaurScript_RunNativeTransition(&player) && !gExitTransition.id);
	BeginExitTransition(&player);
	gFramesPerSecondFrac = 8;
	assert(NanosaurScript_RunNativeTransition(&player));
	assert(!gExitTransition.id && skeleton.AnimNum == PLAYER_ANIM_STAND);
	assert(PangeaScript_ObjectIsScriptControlled(handle));
	skeleton.AnimNum = PLAYER_ANIM_EXIT;
	BeginExitTransition(&player);
	ClearExitTransition();
	assert(!NanosaurScript_RunNativeTransition(&player));
	gFramesPerSecondFrac = 0;
	PangeaScript_UnregisterObject(handle);
	assert(!ObjectRemainsLive(&player, handle));
	gPlayerObj = NULL;
}

int main(void)
{
	TestNativeTransitionAndOptions();
	TestOwnedEggDelivery();
	TestGameplayState();
	TestNativeBounds();
	PangeaScriptBounds bounds = {.min = {-1, -1, -1}, .max = {1, 1, 1}};
	CollisionBoxType box = {.left = -2, .right = 2, .bottom = -2, .top = 2, .back = -2, .front = 2};
	assert(ScriptBoundsOverlapCollisionBox(&bounds, &box));
	box.back = 2;
	assert(!ScriptBoundsOverlapCollisionBox(&bounds, &box));
	box.back = 1;
	assert(ScriptBoundsOverlapCollisionBox(&bounds, &box));
	ObjNode object = {.StatusBits = STATUS_BIT_HIDDEN};
	assert(NanosaurScript_SetObjectActive(&object, true));
	assert(object.StatusBits == STATUS_BIT_HIDDEN);
	assert(NanosaurScript_SetObjectActive(&object, false));
	assert(object.StatusBits & STATUS_BIT_NOMOVE);
	assert(object.StatusBits & STATUS_BIT_NOCOLLISION);
	assert(NanosaurScript_SetObjectCollisionEnabled(&object, false));
	assert(NanosaurScript_SetObjectActive(&object, true));
	assert(object.StatusBits == (STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION));
	NanosaurScript_OnLevelLoad(0);
	NanosaurScript_OnFrame(0, 0, 0.25f, 0.0f);
	NanosaurScript_OnFrame(0, 1, 0.25f, 0.0f);
	assert(gScriptFrameContext.levelTimeSeconds == 0.5f);
	NanosaurScript_OnLevelLoad(0);
	NanosaurScript_OnFrame(0, 0, 0.125f, 0.0f);
	assert(gScriptFrameContext.levelTimeSeconds == 0.125f);
	return 0;
}

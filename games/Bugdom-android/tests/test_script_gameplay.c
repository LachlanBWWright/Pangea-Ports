#include "game.h"
#include "ScriptBindings.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

ObjNode* gPlayerObj;
ObjNode* gFirstNodePtr;
float gBallTimer, gShieldTimer, gCheckPointRot;
short gMoney, gNumGreenClovers, gNumBlueClovers, gNumGoldClovers;
short gNumLadyBugsThisArea, gNumLadyBugsOnThisLevel, gBestCheckPoint;
TQ3Point3D gMostRecentCheckPointCoord;
TQ3Point3D gMyCoord;
Boolean gAreaCompleted, gGameOverFlag;
u_long gInfobarUpdateBits;

void BugdomScript_SetMoney(short amount) { gMoney = amount; gInfobarUpdateBits |= UPDATE_HANDS; }
void UpdateObjectTransforms(ObjNode* object) { (void) object; }
float gFramesPerSecondFrac = 0.02f;
long gTerrainTileWidth = 2, gTerrainTileDepth = 2;
u_short** gFloorMap;
TQ3Vector3D gRecentTerrainNormal[2];
static bool gJumpHeld, gJumpPressed;
static float gMoveX;
static int gTerrainCalls;
static bool gScriptControlled;
static bool gFreeDuringCallback;
static bool gObjectAlive = true;
static ObjNode* gEventDeleteObject;
static int gAnimationEvents;
static bool gRulesControlled;
static int gRuleTriggers;

void BugdomTest_SetController(bool controlled) { gScriptControlled = controlled; }

bool PangeaScript_RulesAreScriptControlled(void) { return gRulesControlled; }
void BugdomScript_OnCustomTrigger(ObjNode* object, ObjNode* player, Byte sides)
{
	(void)object; (void)player; (void)sides;
	gRuleTriggers++;
}

void BugdomScript_ApplyObjectScripting(ObjNode* object)
{
	if (!gFreeDuringCallback) return;
	gObjectAlive = false;
	free(object);
}

void DeleteObject(ObjNode* object) { gObjectAlive = false; free(object); }

const PangeaScriptFrameContext* BugdomScript_GetFrameContext(void)
{
	static const PangeaScriptFrameContext context = {0};
	return &context;
}

PangeaScriptStatus PangeaScript_CallObjectEvent(PangeaScriptObjectHandle handle,
	const PangeaScriptFrameContext* context, const char* event)
{
	(void) handle; (void) context; (void) event;
	gAnimationEvents++;
	if (gEventDeleteObject)
	{
		gObjectAlive = false;
		free(gEventDeleteObject);
		gEventDeleteObject = NULL;
	}
	return PANGEA_SCRIPT_OK;
}

void BugdomScript_RegisterObject(ObjNode* node, const char* id, const char* category)
{
	(void) id; (void) category;
	node->ScriptObjectID = 1;
	node->ScriptObjectGeneration = 2;
}

bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle)
{
	return gObjectAlive && handle.id == 1 && handle.generation == 2;
}

Boolean DoSimpleBoxCollisionAgainstObject(float top, float bottom, float left, float right,
	float front, float back, ObjNode* target)
{
	(void) top; (void) bottom; (void) left; (void) right; (void) front; (void) back;
	return target->NumCollisionBoxes > 0;
}

bool PangeaScript_ObjectIsScriptControlled(PangeaScriptObjectHandle handle)
{
	(void) handle;
	return gScriptControlled;
}

Boolean GetKeyState(unsigned short key) { return key == kKey_Jump && gJumpHeld; }
Boolean GetNewKeyState(unsigned short key) { return key == kKey_Jump && gJumpPressed; }
TQ3Vector2D GetThumbStickVector(bool rightStick) { (void) rightStick; return (TQ3Vector2D){0}; }
void GetMouseDelta(float* dx, float* dy) { *dx = gMoveX * 1600 * gFramesPerSecondFrac; *dy = 0; }
float GetTerrainHeightAtCoord(float x, float z, long layer)
{
	(void) x; (void) z;
	gTerrainCalls++;
	gRecentTerrainNormal[layer] = (TQ3Vector3D){0, 1, 0};
	return 42;
}

int main(void)
{
	extern void TestBugdomBespoke(void);
	TestBugdomBespoke();
	assert(BugdomScript_NativeRulesEnabled());
	gRulesControlled = true;
	assert(!BugdomScript_NativeRulesEnabled());
	assert(!BugdomScript_CompleteNativeObjective() && !gAreaCompleted);
	gRulesControlled = false;
	assert(BugdomScript_NativeRulesEnabled());
	assert(BugdomScript_CompleteNativeObjective() && gAreaCompleted);
	gAreaCompleted = false;
	ObjNode ruleTrigger = {0};
	assert(!BugdomScript_OwnedRuleTrigger(&ruleTrigger, NULL, 0, "test.trigger"));
	assert(gRuleTriggers == 0);
	gRulesControlled = true;
	assert(BugdomScript_OwnedRuleTrigger(&ruleTrigger, NULL, 0, "test.trigger"));
	assert(gRuleTriggers == 1 && ruleTrigger.ScriptObjectID == 1);
	gRulesControlled = false;
	PangeaScriptInputSnapshot input;
	BugdomScript_ResetGameplayInput();
	gJumpHeld = gJumpPressed = true;
	gMoveX = 2;
	BugdomScript_CaptureGameplayInput();
	assert(kBugdomGameplayOps.getInput(0, &input));
	assert(input.actions[0].held && input.actions[0].pressed && !input.actions[0].released);
	assert(input.moveX == 1);
	assert(kBugdomGameplayOps.getInput(0, &input));
	assert(input.actions[0].pressed);
	gJumpHeld = gJumpPressed = false;
	BugdomScript_CaptureGameplayInput();
	assert(kBugdomGameplayOps.getInput(0, &input) && input.actions[0].released);
	BugdomScript_CaptureGameplayInput();
	assert(kBugdomGameplayOps.getInput(0, &input) && !input.actions[0].released);
	assert(!kBugdomGameplayOps.getInput(1, &input));
	assert(!kBugdomGameplayOps.getInput(0, NULL));

	u_short* floor = NULL;
	gFloorMap = &floor;
	gRecentTerrainNormal[FLOOR] = (TQ3Vector3D){1, 0, 0};
	PangeaScriptTerrainSample terrain;
	assert(kBugdomGameplayOps.sampleTerrain(20, 20, &terrain));
	assert(terrain.height == 42 && terrain.normal.y == 1);
	assert(gRecentTerrainNormal[FLOOR].x == 1 && gTerrainCalls == 1);
	assert(!kBugdomGameplayOps.sampleTerrain(-1, 20, &terrain));
	assert(!kBugdomGameplayOps.sampleTerrain(320, 20, &terrain));
	assert(gTerrainCalls == 1);
	gFloorMap = NULL;
	assert(!kBugdomGameplayOps.sampleTerrain(20, 20, &terrain));

	ObjNode collisionObject = {.CType = CTYPE_ENEMY, .CBits = CBITS_ALLSOLID, .NumCollisionBoxes = 1};
	gFirstNodePtr = &collisionObject;
	PangeaScriptBounds bounds = {.min = {-10, -10, -10}, .max = {10, 10, 10}};
	PangeaScriptObjectHandle hit;
	int count;
	assert(kBugdomGameplayOps.overlapBox(&bounds, CTYPE_ENEMY, &hit, 1, &count));
	assert(count == 1 && PangeaScript_ObjectExists(hit));
	assert(kBugdomGameplayOps.overlapBox(&bounds, CTYPE_PLAYER, &hit, 1, &count) && count == 0);
	collisionObject.StatusBits |= STATUS_BIT_NOCOLLISION;
	assert(kBugdomGameplayOps.overlapBox(&bounds, CTYPE_ENEMY, &hit, 1, &count) && count == 0);
	assert(kBugdomGameplayOps.overlapBox(&bounds, CTYPE_ENEMY, NULL, 0, &count) && count == 0);
	assert(!kBugdomGameplayOps.overlapBox(&bounds, CTYPE_ENEMY, NULL, 1, &count));
	gFirstNodePtr = NULL;

	CollisionBoxType boxes[] = {
		{.left = -2, .right = 3, .bottom = -4, .top = 5, .back = -6, .front = 7},
		{.left = -8, .right = 9, .bottom = -1, .top = 2, .back = -3, .front = 4},
	};
	ObjNode boundedObject = {.CollisionBoxes = boxes, .NumCollisionBoxes = 2};
	PangeaScriptBounds nativeBounds;
	assert(BugdomScript_GetObjectBounds(&boundedObject, &nativeBounds));
	assert(nativeBounds.min.x == -8 && nativeBounds.max.x == 9);
	assert(nativeBounds.min.y == -4 && nativeBounds.max.y == 5);
	assert(nativeBounds.min.z == -6 && nativeBounds.max.z == 7);
	PangeaScriptVector3 position = {10, 20, 30};
	assert(BugdomScript_SetObjectPosition(&boundedObject, &position));
	assert(BugdomScript_GetObjectBounds(&boundedObject, &nativeBounds));
	assert(nativeBounds.min.x == 2 && nativeBounds.max.x == 19);
	assert(nativeBounds.min.y == 16 && nativeBounds.max.y == 25);
	assert(nativeBounds.min.z == 24 && nativeBounds.max.z == 37);
	assert(boxes[0].left == 8 && boxes[1].left == 2);
	boxes[1].right = FLT_MAX;
	assert(!BugdomScript_SetObjectPosition(&boundedObject, &(PangeaScriptVector3){FLT_MAX, 20, 30}));
	assert(boundedObject.Coord.x == 10 && boxes[0].left == 8);
	boxes[1].front = NAN;
	assert(!BugdomScript_GetObjectBounds(&boundedObject, &nativeBounds));
	boundedObject.CollisionBoxes = NULL;
	assert(!BugdomScript_GetObjectBounds(&boundedObject, &nativeBounds));

	PangeaScriptStateValue state = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 0.75};
	assert(kBugdomGameplayOps.setState(0, "ballTime", &state) == PANGEA_SCRIPT_OK);
	assert(gBallTimer == 0.75 && (gInfobarUpdateBits & UPDATE_TIMER));
	assert(kBugdomGameplayOps.getState(0, "ballTime", &state) && state.number == 0.75);
	state.number = NAN;
	assert(kBugdomGameplayOps.setState(0, "ballTime", &state) == PANGEA_SCRIPT_BAD_ARGUMENT);
	state.number = 3.5;
	assert(kBugdomGameplayOps.setState(0, "money", &state) == PANGEA_SCRIPT_BAD_ARGUMENT);
	state.number = 3;
	assert(kBugdomGameplayOps.setState(0, "money", &state) == PANGEA_SCRIPT_OK && gMoney == 3);
	gNumLadyBugsOnThisLevel = 2;
	assert(kBugdomGameplayOps.setState(0, "rescuedLadybugs", &state) == PANGEA_SCRIPT_BAD_ARGUMENT);
	state.number = 2;
	assert(kBugdomGameplayOps.setState(0, "rescuedLadybugs", &state) == PANGEA_SCRIPT_OK);
	assert(gNumLadyBugsThisArea == 2 && (gInfobarUpdateBits & UPDATE_LADYBUGS));
	assert(kBugdomGameplayOps.setState(0, "bestCheckpoint", &state) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(!kBugdomGameplayOps.getState(1, "money", &state));
	assert(kBugdomGameplayOps.complete(0, true) == PANGEA_SCRIPT_BAD_ARGUMENT);
	ObjNode player = {0};
	gPlayerObj = &player;
	assert(BugdomScript_SetObjectPosition(&player, &position));
	assert(gMyCoord.x == 10 && gMyCoord.y == 20 && gMyCoord.z == 30);
	gRulesControlled = true;
	assert(kBugdomGameplayOps.complete(0, true) == PANGEA_SCRIPT_OK && gAreaCompleted);
	assert(kBugdomGameplayOps.getState(0, "completed", &state) && state.type == PANGEA_SCRIPT_STATE_BOOLEAN && state.boolean);
	assert(kBugdomGameplayOps.complete(0, false) == PANGEA_SCRIPT_BAD_ARGUMENT);
	gAreaCompleted = false;
	assert(kBugdomGameplayOps.complete(0, false) == PANGEA_SCRIPT_OK && gGameOverFlag);
	gRulesControlled = false;

	ObjNode object = {.StatusBits = STATUS_BIT_HIDDEN};
	assert(BugdomScript_ShouldRunNativeMovement(&object));
	assert(BugdomScript_SetObjectActive(&object, false));
	assert(!BugdomScript_ShouldRunNativeMovement(&object));
	assert(object.StatusBits & STATUS_BIT_NOCOLLISION);
	assert(BugdomScript_SetObjectActive(&object, false));
	assert(BugdomScript_SetObjectActive(&object, true));
	assert(object.StatusBits & STATUS_BIT_HIDDEN);
	assert(!(object.StatusBits & STATUS_BIT_NOCOLLISION));
	assert(BugdomScript_SetObjectActive(&object, false));
	assert(BugdomScript_SetObjectCollisionEnabled(&object, false));
	assert(BugdomScript_SetObjectActive(&object, true));
	assert(object.StatusBits & STATUS_BIT_NOCOLLISION);
	assert(BugdomScript_SetObjectActive(&object, false));
	assert(BugdomScript_SetObjectCollisionEnabled(&object, true));
	assert(object.StatusBits & STATUS_BIT_NOCOLLISION);
	assert(BugdomScript_SetObjectActive(&object, true));
	assert(!(object.StatusBits & STATUS_BIT_NOCOLLISION));
	assert(object.StatusBits & STATUS_BIT_HIDDEN);
	assert(BugdomScript_ShouldRunNativeMovement(&object));
	gScriptControlled = true;
	assert(!BugdomScript_ShouldRunNativeMovement(&object));
	object.CType = INVALID_NODE_FLAG;
	assert(!BugdomScript_SetObjectActive(&object, true));
	ObjNode* deletedInCallback = calloc(1, sizeof(*deletedInCallback));
	assert(deletedInCallback);
	deletedInCallback->ScriptObjectID = 1;
	deletedInCallback->ScriptObjectGeneration = 2;
	gFreeDuringCallback = true;
	BugdomScript_RunObjectFrame(deletedInCallback);
	assert(!gObjectAlive);
	gFreeDuringCallback = false;
	gObjectAlive = true;
	SkeletonObjDataType skeleton = {.AnimHasStopped = true};
	ObjNode inactive = {.ScriptObjectID = 1, .ScriptObjectGeneration = 2,
		.ScriptActiveStateInitialized = true, .ScriptActive = false, .Skeleton = &skeleton};
	BugdomScript_RunObjectFrame(&inactive);
	assert(gAnimationEvents == 0);
	inactive.ScriptActive = true;
	BugdomScript_RunObjectFrame(&inactive);
	assert(gAnimationEvents == 1 && inactive.ScriptAnimationCompletionSent);
	BugdomScript_RunObjectFrame(&inactive);
	assert(gAnimationEvents == 1);
	gEventDeleteObject = calloc(1, sizeof(*gEventDeleteObject));
	assert(gEventDeleteObject);
	gEventDeleteObject->ScriptObjectID = 1;
	gEventDeleteObject->ScriptObjectGeneration = 2;
	gEventDeleteObject->Skeleton = &skeleton;
	BugdomScript_RunObjectFrame(gEventDeleteObject);
	assert(!gObjectAlive && !gEventDeleteObject);
	return 0;
}

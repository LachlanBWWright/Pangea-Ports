#include "game.h"
#include "ScriptBindings.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

Byte gPlayerMode;
Boolean gPlayerGotKilledFlag;
ObjNode *gCurrentWaterBug, *gCurrentDragonFly, *gCurrentRope, *gPrevRope, *gMyBuddy;
short gCurrentRopeJoint;
static ObjNode* gTarget;
static bool gCanAfford;
static bool gRestoredController;
static int gMountCalls, gRespawnCalls;
static int gPlacementCalls;
void BugdomTest_SetController(bool controlled);

void FindJointFullMatrix(ObjNode* parent, long joint, TQ3Matrix4x4* matrix)
{
	assert(joint >= 0 && joint < parent->Skeleton->skeletonDefinition->NumBones);
	Q3Matrix4x4_SetTranslate(matrix, parent->Coord.x, parent->Coord.y, parent->Coord.z);
}

void CalcObjectBoxFromNode(ObjNode* node)
{
	assert(node == gPlayerObj);
	gPlacementCalls++;
}

bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** output)
{
	if (handle.id != 7 || handle.generation != 3)
		return false;
	*output = gTarget;
	return true;
}

bool PangeaScript_SetObjectScriptControlled(PangeaScriptObjectHandle handle, bool controlled)
{
	(void)handle;
	gRestoredController = controlled;
	return true;
}

void MorphToSkeletonAnim(SkeletonObjDataType* skeleton, long animation, float speed)
{
	(void)speed;
	skeleton->AnimNum = animation;
}

Boolean DoTrig_WaterBug(ObjNode* mount, ObjNode* player, Byte sides)
{
	(void)sides;
	gMountCalls++;
	if (!gCanAfford) return true;
	gCurrentWaterBug = mount;
	mount->Mode = WATERBUG_MODE_RIDE;
	player->Skeleton->AnimNum = PLAYER_ANIM_RIDEWATERBUG;
	return true;
}

Boolean DoTrig_DragonFly(ObjNode* mount, ObjNode* player, Byte sides)
{
	(void)sides;
	gMountCalls++;
	gCurrentDragonFly = mount;
	player->Skeleton->AnimNum = PLAYER_ANIM_RIDEDRAGONFLY;
	return true;
}

void PlayerOffDragonfly(void) { gCurrentDragonFly = NULL; }
void PlayerGrabRootSwing(ObjNode* root, int joint)
{
	gCurrentRope = gPrevRope = root;
	gCurrentRopeJoint = joint;
	gPlayerObj->Skeleton->AnimNum = PLAYER_ANIM_ROPESWING;
}

bool BugdomScript_CommandBuddy(ObjNode* buddy, bool attack)
{
	if (buddy->Type != SKELETON_TYPE_BUDDY) return false;
	gMyBuddy = attack ? NULL : buddy;
	return true;
}

PangeaScriptStatus BugdomScript_SetPlayerForm(int player, PangeaScriptPlayerForm form)
{
	assert(player == 0);
	gPlayerMode = form == PANGEA_SCRIPT_PLAYER_FORM_BALL ? PLAYER_MODE_BALL : PLAYER_MODE_BUG;
	return PANGEA_SCRIPT_OK;
}

void ResetPlayer(void)
{
	gRespawnCalls++;
	gPlayerGotKilledFlag = false;
}

static PangeaScriptStatus Run(PangeaScriptPlayerOperation operation, const char* mode, int joint)
{
	PangeaScriptOperationRequest request = {
		.operation = operation, .playerNum = 0,
		.hasTarget = operation == PANGEA_SCRIPT_OP_MOUNT || operation == PANGEA_SCRIPT_OP_GRAB
			|| operation == PANGEA_SCRIPT_OP_COMMAND_BUDDY,
		.target = {7, 3}, .mode = mode,
		.hasJoint = operation == PANGEA_SCRIPT_OP_GRAB, .joint = joint,
	};
	PangeaScriptOperationResult output;
	return kBugdomGameplayOps.runOperation(&request, &output);
}

void TestBugdomBespoke(void)
{
	SkeletonDefType definition = {.NumBones = 4};
	SkeletonObjDataType playerSkeleton = {.AnimNum = PLAYER_ANIM_STAND};
	SkeletonObjDataType mountSkeleton = {.skeletonDefinition = &definition};
	ObjNode shadow = {.StatusBits = STATUS_BIT_HIDDEN};
	ObjNode player = {.Skeleton = &playerSkeleton, .ShadowNode = &shadow, .Scale = {1, 1, 1}};
	TerrainItemEntryType item = {0};
	ObjNode mount = {.Genre = SKELETON_GENRE, .Type = SKELETON_TYPE_WATERBUG,
		.CType = CTYPE_TRIGGER, .Skeleton = &mountSkeleton, .TerrainItemPtr = &item,
		.Scale = {1, 1, 1}};
	player.NextNode = &mount;
	gFirstNodePtr = gPlayerObj = &player;
	gTarget = &mount;
	gPlayerMode = PLAYER_MODE_BUG;
	assert(BugdomScript_ObjectStillAlive(&mount, (PangeaScriptObjectHandle){7, 3}));
	assert(!BugdomScript_ObjectStillAlive(&mount, (PangeaScriptObjectHandle){7, 2}));
	assert(!BugdomScript_ObjectStillAlive(&player, (PangeaScriptObjectHandle){7, 3}));
	ObjNode* removed = calloc(1, sizeof(*removed));
	assert(removed);
	assert(!BugdomScript_ObjectStillAlive(removed, (PangeaScriptObjectHandle){0}));
	free(removed);
	assert(Run(PANGEA_SCRIPT_OP_MOUNT, NULL, 0) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(!gCurrentWaterBug && gMountCalls == 1);
	gCanAfford = true;
	assert(Run(PANGEA_SCRIPT_OP_MOUNT, NULL, 0) == PANGEA_SCRIPT_OK);
	BugdomScript_SyncRide();
	assert(gPlacementCalls == 0);
	BugdomTest_SetController(true);
	mount.Coord = (TQ3Point3D){100, 200, 300};
	BugdomScript_SyncRide();
	assert(gPlacementCalls == 1 && player.Coord.x == 100 && player.Coord.y == 230 && player.Coord.z == 340);
	BugdomTest_SetController(false);
	mount.ScriptActiveStateInitialized = true;
	mount.ScriptActive = false;
	BugdomScript_SyncRide();
	assert(gPlacementCalls == 2 && player.Coord.x == 100 && player.Coord.y == 230 && player.Coord.z == 340);
	mount.ScriptActive = true;
	PangeaScriptPlayerRelations relations;
	assert(kBugdomGameplayOps.getRelations(0, &relations));
	assert(relations.mounted.id == 1 && !strcmp(relations.form, "bug"));
	assert(Run(PANGEA_SCRIPT_OP_MOUNT, NULL, 0) == PANGEA_SCRIPT_BAD_ARGUMENT && gMountCalls == 2);
	BugdomScript_ClearRelationships(&mount);
	assert(!gCurrentWaterBug && mount.Mode == WATERBUG_MODE_COAST);
	assert(playerSkeleton.AnimNum == PLAYER_ANIM_FALL && !(shadow.StatusBits & STATUS_BIT_HIDDEN));
	assert(kBugdomGameplayOps.getRelations(0, &relations) && !relations.mounted.id);
	mount.Type = SKELETON_TYPE_ROOTSWING;
	assert(Run(PANGEA_SCRIPT_OP_GRAB, NULL, 4) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(Run(PANGEA_SCRIPT_OP_GRAB, NULL, 2) == PANGEA_SCRIPT_OK && gCurrentRopeJoint == 2);
	BugdomScript_ClearRelationships(&mount);
	assert(!gCurrentRope && !gPrevRope && playerSkeleton.AnimNum == PLAYER_ANIM_FALL);
	mount.Type = SKELETON_TYPE_DRAGONFLY;
	assert(Run(PANGEA_SCRIPT_OP_MOUNT, NULL, 0) == PANGEA_SCRIPT_OK);
	assert(Run(PANGEA_SCRIPT_OP_DISMOUNT, NULL, 0) == PANGEA_SCRIPT_OK && !gCurrentDragonFly);
	mount.Type = SKELETON_TYPE_BUDDY;
	assert(Run(PANGEA_SCRIPT_OP_COMMAND_BUDDY, "follow", 0) == PANGEA_SCRIPT_OK && gMyBuddy == &mount);
	assert(Run(PANGEA_SCRIPT_OP_COMMAND_BUDDY, "attack", 0) == PANGEA_SCRIPT_OK && !gMyBuddy);
	assert(Run(PANGEA_SCRIPT_OP_COMMAND_BUDDY, "unknown", 0) == PANGEA_SCRIPT_BAD_ARGUMENT);
	PangeaScriptOperationRequest stale = {.operation = PANGEA_SCRIPT_OP_MOUNT,
		.hasTarget = true, .target = {7, 2}};
	PangeaScriptOperationResult result;
	assert(kBugdomGameplayOps.runOperation(&stale, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	stale.target.generation = 3;
	stale.hasVelocity = true;
	assert(kBugdomGameplayOps.runOperation(&stale, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	stale.hasVelocity = false;
	stale.mode = "ignored";
	assert(kBugdomGameplayOps.runOperation(&stale, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	gPlayerGotKilledFlag = true;
	assert(Run(PANGEA_SCRIPT_OP_COMMAND_BUDDY, "follow", 0) == PANGEA_SCRIPT_BAD_ARGUMENT);
	u_short* row = NULL;
	gFloorMap = &row;
	PangeaScriptOperationRequest respawn = {.operation = PANGEA_SCRIPT_OP_RESPAWN};
	assert(kBugdomGameplayOps.runOperation(&respawn, &result) == PANGEA_SCRIPT_OK && gRespawnCalls == 1);
	assert(!gRestoredController);
	assert(kBugdomGameplayOps.runOperation(&respawn, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	gFloorMap = NULL;
	gPlayerObj = gFirstNodePtr = gTarget = NULL;
}

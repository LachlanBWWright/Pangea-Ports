#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"
#include <math.h>
#include <string.h>

static bool LiveObject(const ObjNode* object)
{
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
		if (node == object)
			return node->CType != INVALID_NODE_FLAG;
	return false;
}

static PangeaScriptObjectHandle ObjectHandle(ObjNode* object)
{
	if (!LiveObject(object))
		return (PangeaScriptObjectHandle){0};
	PangeaScriptObjectHandle previous = {object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
	if (PangeaScript_ObjectExists(previous))
		return previous;
	object->ScriptObjectID = object->ScriptObjectGeneration = 0;
	BugdomScript_RegisterObject(object, "bugdom.relationship", "relationship");
	return (PangeaScriptObjectHandle){object->ScriptObjectID, (uint32_t)object->ScriptObjectGeneration};
}

bool BugdomScript_GetRelations(int player, PangeaScriptPlayerRelations* output)
{
	if (player != 0 || !output || !LiveObject(gPlayerObj))
		return false;
	*output = (PangeaScriptPlayerRelations){
		.form = gPlayerMode == PLAYER_MODE_BALL ? "ball" : "bug",
		.buddy = ObjectHandle(gMyBuddy),
	};
	if (gPlayerMode != PLAYER_MODE_BUG || !gPlayerObj->Skeleton)
		return true;
	int animation = gPlayerObj->Skeleton->AnimNum;
	if (animation == PLAYER_ANIM_RIDEWATERBUG)
		output->mounted = ObjectHandle(gCurrentWaterBug);
	if (animation == PLAYER_ANIM_RIDEDRAGONFLY)
		output->mounted = ObjectHandle(gCurrentDragonFly);
	if (animation == PLAYER_ANIM_ROPESWING)
		output->grabbed = ObjectHandle(gCurrentRope);
	return true;
}

static ObjNode* Target(const PangeaScriptOperationRequest* request)
{
	void* native = NULL;
	if (!request->hasTarget || !PangeaScript_GetObjectNativeObject(request->target, &native)
		|| !LiveObject(native))
		return NULL;
	return native;
}

static bool Riding(void)
{
	if (gPlayerMode != PLAYER_MODE_BUG || !gPlayerObj->Skeleton)
		return false;
	int animation = gPlayerObj->Skeleton->AnimNum;
	return animation == PLAYER_ANIM_RIDEWATERBUG || animation == PLAYER_ANIM_RIDEDRAGONFLY
		|| animation == PLAYER_ANIM_ROPESWING;
}

static PangeaScriptStatus Mount(ObjNode* target)
{
	if (Riding() || gPlayerMode != PLAYER_MODE_BUG || !gPlayerObj->Skeleton
		|| !target || !target->Skeleton || target->Genre != SKELETON_GENRE
		|| !(target->CType & CTYPE_TRIGGER) || !isfinite(target->Scale.x) || target->Scale.x <= 0)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (target->Type == SKELETON_TYPE_WATERBUG && target->TerrainItemPtr)
	{
		(void)DoTrig_WaterBug(target, gPlayerObj, SIDE_BITS_TOP);
		return gCurrentWaterBug == target ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	if (target->Type == SKELETON_TYPE_DRAGONFLY)
	{
		(void)DoTrig_DragonFly(target, gPlayerObj, SIDE_BITS_TOP);
		return gCurrentDragonFly == target ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	return PANGEA_SCRIPT_BAD_ARGUMENT;
}

static PangeaScriptStatus Grab(ObjNode* target, int joint)
{
	if (Riding() || gPlayerMode != PLAYER_MODE_BUG || !gPlayerObj->Skeleton
		|| !target || target->Genre != SKELETON_GENRE || target->Type != SKELETON_TYPE_ROOTSWING
		|| !target->Skeleton || !target->Skeleton->skeletonDefinition
		|| joint < 0 || joint >= target->Skeleton->skeletonDefinition->NumBones)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	PlayerGrabRootSwing(target, joint);
	return PANGEA_SCRIPT_OK;
}

static bool OptionsValid(const PangeaScriptOperationRequest* request)
{
	if (request->hasPosition || request->hasVelocity || request->hasAim || request->hasWeapon)
		return false;
	if (request->hasJoint && request->operation != PANGEA_SCRIPT_OP_GRAB)
		return false;
	if (request->mode && request->operation != PANGEA_SCRIPT_OP_COMMAND_BUDDY)
		return false;
	if (request->form && request->operation != PANGEA_SCRIPT_OP_TRANSFORM)
		return false;
	if (!request->hasTarget)
		return true;
	return request->operation == PANGEA_SCRIPT_OP_MOUNT || request->operation == PANGEA_SCRIPT_OP_GRAB
		|| request->operation == PANGEA_SCRIPT_OP_COMMAND_BUDDY;
}

PangeaScriptStatus BugdomScript_RunOperation(const PangeaScriptOperationRequest* request,
	PangeaScriptOperationResult* output)
{
	if (!request || !output || request->playerNum != 0 || !LiveObject(gPlayerObj) || !OptionsValid(request))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	*output = (PangeaScriptOperationResult){0};
	if (request->operation == PANGEA_SCRIPT_OP_RESPAWN)
	{
		if (!gPlayerGotKilledFlag || !gFloorMap)
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		bool controlled = PangeaScript_ObjectIsScriptControlled(ObjectHandle(gPlayerObj));
		BugdomScript_ReleaseRide();
		ResetPlayer();
		(void)PangeaScript_SetObjectScriptControlled(ObjectHandle(gPlayerObj), controlled);
		output->object = ObjectHandle(gPlayerObj);
		return PANGEA_SCRIPT_OK;
	}
	if (gPlayerGotKilledFlag)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	ObjNode* target = Target(request);
	if (request->hasTarget && !target)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	PangeaScriptStatus status = PANGEA_SCRIPT_BAD_ARGUMENT;
	switch (request->operation)
	{
		case PANGEA_SCRIPT_OP_MOUNT: status = Mount(target); break;
		case PANGEA_SCRIPT_OP_GRAB: status = Grab(target, request->hasJoint ? request->joint : 0); break;
		case PANGEA_SCRIPT_OP_DISMOUNT:
		case PANGEA_SCRIPT_OP_DROP:
			if (!Riding()) break;
			BugdomScript_ReleaseRide();
			status = PANGEA_SCRIPT_OK;
			break;
		case PANGEA_SCRIPT_OP_TRANSFORM:
			if (!request->form) break;
			if (!strcmp(request->form, "ball")) status = BugdomScript_SetPlayerForm(0, PANGEA_SCRIPT_PLAYER_FORM_BALL);
			if (!strcmp(request->form, "bug")) status = BugdomScript_SetPlayerForm(0, PANGEA_SCRIPT_PLAYER_FORM_BUG);
			break;
		case PANGEA_SCRIPT_OP_COMMAND_BUDDY:
			if (!request->mode || (strcmp(request->mode, "follow") && strcmp(request->mode, "attack"))) break;
			if (!target) target = gMyBuddy;
			if (LiveObject(target) && BugdomScript_CommandBuddy(target, !strcmp(request->mode, "attack")))
				status = PANGEA_SCRIPT_OK;
			break;
		case PANGEA_SCRIPT_OP_RELEASE_BUDDY:
			if (LiveObject(gMyBuddy))
			{
				ObjNode* buddy = gMyBuddy;
				gMyBuddy = NULL;
				DeleteObject(buddy);
				status = PANGEA_SCRIPT_OK;
			}
			break;
		default: return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	if (status == PANGEA_SCRIPT_OK)
		output->object = ObjectHandle(target ? target : gPlayerObj);
	return status;
}

#endif

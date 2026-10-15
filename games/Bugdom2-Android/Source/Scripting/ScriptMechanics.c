#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "ScriptBindings.h"
#include <math.h>
#include <string.h>

static PangeaScriptObjectHandle RelationshipHandle(ObjNode* object)
{
	if (!object || object->CType == INVALID_NODE_FLAG) return (PangeaScriptObjectHandle){0};
	Bugdom2Script_RegisterObject(object, "bugdom2.relationship", "relationship");
	return (PangeaScriptObjectHandle){object->ScriptObjectID, object->ScriptObjectGeneration};
}

bool Bugdom2Script_MountUsesScriptController(ObjNode* object)
{
	ObjNode* player = gPlayerInfo.objNode;
	if (!object || !player || (object != gPlayerInfo.ridingBall && object != Bugdom2Script_GetMountedPlane()))
		return false;
	PangeaScriptObjectHandle handle = {player->ScriptObjectID, player->ScriptObjectGeneration};
	return PangeaScript_ObjectIsScriptControlled(handle);
}

void Bugdom2Script_ReleasePlayerCarry(void)
{
	ObjNode* player = gPlayerInfo.objNode;
	if (player && gPlayerInfo.heldObject) ReleaseHeldObject(player, gPlayerInfo.heldObject);
	gPlayerInfo.ridingBall = nil;
	gCurrentCarryingMoth = nil;
}

void Bugdom2Script_UpdatePlayerRelationships(ObjNode* player)
{
	if (player != gPlayerInfo.objNode || !player->Skeleton) return;
	ObjNode* ball = gPlayerInfo.ridingBall;
	if (ball && ball->CType != INVALID_NODE_FLAG)
	{
		player->Rot.y = ball->Rot.y;
		PangeaScriptVector3 position = {ball->Coord.x,
			ball->Coord.y + ball->BBox.max.y * ball->Scale.x - player->BBox.min.y, ball->Coord.z};
		KeepOldCollisionBoxes(player);
		if (Bugdom2Script_SetObjectPosition(player, &position))
		{
			player->Delta = ball->Delta;
			player->Skeleton->AnimSpeed = ball->Speed2D * .004f;
			player->StatusBits |= STATUS_BIT_ONGROUND;
			gPlayerInfo.coord.y -= 100;
			UpdateShadow(player);
		}
	}
	else if (Bugdom2Script_GetMountedPlane())
	{
		KeepOldCollisionBoxes(player);
		Bugdom2Script_AlignMountedPlane(player);
	}
	if (gPlayerInfo.heldObject) UpdateHeldObject(player);
}

bool Bugdom2Script_GetRelations(int player, PangeaScriptPlayerRelations* output)
{
	if (player != 0 || !output || !gInGameNow || !gPlayerInfo.objNode) return false;
	*output = (PangeaScriptPlayerRelations){
		.carried = RelationshipHandle(gPlayerInfo.heldObject),
		.mounted = RelationshipHandle(gPlayerInfo.ridingBall ? gPlayerInfo.ridingBall : Bugdom2Script_GetMountedPlane()),
		.buddy = RelationshipHandle(gPlayerInfo.numBuddyBugs ? gPlayerInfo.buddyBugs[0] : nil),
		.form = gPlayerInfo.ridingBall ? "ball" : Bugdom2Script_GetMountedPlane() ? "plane" : "bug",
	};
	return true;
}

static ObjNode* OperationTarget(const PangeaScriptOperationRequest* request)
{
	void* native = nil;
	if (!request->hasTarget || !PangeaScript_GetObjectNativeObject(request->target, &native)) return nil;
	ObjNode* object = native;
	return object && object->CType != INVALID_NODE_FLAG ? object : nil;
}

static Boolean Nearby(ObjNode* object)
{
	return object && OGLPoint3D_Distance(&gPlayerInfo.objNode->Coord, &object->Coord) <= 300.0f;
}

static PangeaScriptStatus CarryObject(ObjNode* object, PangeaScriptOperationResult* output)
{
	ObjNode* player = gPlayerInfo.objNode;
	if (!Nearby(object) || !(object->CType & CTYPE_PICKUP) ||
		object->StatusBits & (STATUS_BIT_HIDDEN | STATUS_BIT_DETACHED | STATUS_BIT_NOMOVE | STATUS_BIT_NOCOLLISION) ||
		object->Kind == PICKUP_KIND_POW || object->Kind == PICKUP_KIND_CANTAB ||
		gPlayerInfo.heldObject || gPlayerInfo.ridingBall || gCurrentCarryingMoth || Bugdom2Script_GetMountedPlane())
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	gPlayerInfo.heldObject = object;
	object->StatusBits |= STATUS_BIT_NOMOVE | STATUS_BIT_NOCOLLISION;
	UpdateHeldObject(player);
	SetPlayerStandAnim(player, 6);
	output->object = RelationshipHandle(object);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus DropObject(const PangeaScriptOperationRequest* request,
	PangeaScriptOperationResult* output)
{
	ObjNode* held = gPlayerInfo.heldObject;
	if (!held) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (request->hasTarget && OperationTarget(request) != held) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasPosition || request->hasVelocity) return PANGEA_SCRIPT_BAD_ARGUMENT;
	output->object = RelationshipHandle(held);
	PlayerDropObject(gPlayerInfo.objNode, held);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus Interact(ObjNode* target, Boolean handover)
{
	ObjNode* held = gPlayerInfo.heldObject;
	if (!Nearby(target) || (handover && !held)) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!RunExplicitGameplayInteraction(target, gPlayerInfo.objNode))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	return !handover || gPlayerInfo.heldObject != held ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
}

static PangeaScriptStatus CommandBuddy(const PangeaScriptOperationRequest* request,
	PangeaScriptOperationResult* output)
{
	if (!request->mode || strcmp(request->mode, "launch") == 0)
		return LaunchBuddyBug() ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (strcmp(request->mode, "recruit") != 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
	int before = gPlayerInfo.numBuddyBugs;
	if (before >= MAX_BUDDY_BUGS) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	CreateMyBuddy(&gPlayerInfo.objNode->Coord);
	if (gPlayerInfo.numBuddyBugs != before + 1) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	output->object = RelationshipHandle(gPlayerInfo.buddyBugs[before]);
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus Bugdom2Script_RunOperation(const PangeaScriptOperationRequest* request,
	PangeaScriptOperationResult* output)
{
	if (!request || !output || request->playerNum != 0 || !gInGameNow || gPlayerIsDead ||
		!gPlayerInfo.objNode || !gPlayerInfo.objNode->Skeleton || IsTunnelLevel() ||
		request->hasAim || request->hasWeapon || request->hasJoint ||
		request->hasPosition || request->hasVelocity)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	*output = (PangeaScriptOperationResult){0};
	ObjNode* target = OperationTarget(request);
	if (request->hasTarget && !target) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->mode && request->operation != PANGEA_SCRIPT_OP_COMMAND_BUDDY &&
		request->operation != PANGEA_SCRIPT_OP_FIRE)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->form && request->operation != PANGEA_SCRIPT_OP_TRANSFORM)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	switch (request->operation)
	{
		case PANGEA_SCRIPT_OP_CARRY: return CarryObject(target, output);
		case PANGEA_SCRIPT_OP_DROP: return DropObject(request, output);
		case PANGEA_SCRIPT_OP_HANDOVER: return Interact(target, true);
		case PANGEA_SCRIPT_OP_INTERACT: return Interact(target, false);
		case PANGEA_SCRIPT_OP_MOUNT:
			if (!Bugdom2Script_MountBall(target) && !Bugdom2Script_MountPlane(target))
				return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			output->object = RelationshipHandle(target);
			return PANGEA_SCRIPT_OK;
		case PANGEA_SCRIPT_OP_DISMOUNT:
			if (request->hasTarget) return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (Bugdom2Script_GetMountedPlane())
				return Bugdom2Script_DismountPlane() ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			if (!gPlayerInfo.ridingBall) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			return Bugdom2Script_SetPlayerForm(false) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		case PANGEA_SCRIPT_OP_TRANSFORM:
			if (!request->form || request->hasTarget) return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (!strcmp(request->form, "plane"))
			{
				if (!Bugdom2Script_GetMountedPlane() && !Bugdom2Script_CreatePlaneMount())
					return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
				output->object = RelationshipHandle(Bugdom2Script_GetMountedPlane());
				return PANGEA_SCRIPT_OK;
			}
			if (strcmp(request->form, "bug")) return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (Bugdom2Script_GetMountedPlane())
				return Bugdom2Script_DismountPlane() ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			return Bugdom2Script_SetPlayerForm(false) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		case PANGEA_SCRIPT_OP_FIRE:
			if (request->hasTarget || (request->mode && strcmp(request->mode, "bullet") &&
				strcmp(request->mode, "bomb"))) return PANGEA_SCRIPT_BAD_ARGUMENT;
			return Bugdom2Script_FirePlane(request->mode && !strcmp(request->mode, "bomb")) ?
				PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		case PANGEA_SCRIPT_OP_COMMAND_BUDDY:
			if (request->hasTarget) return PANGEA_SCRIPT_BAD_ARGUMENT;
			return CommandBuddy(request, output);
		case PANGEA_SCRIPT_OP_RELEASE_BUDDY:
			if (request->hasTarget) return PANGEA_SCRIPT_BAD_ARGUMENT;
			return LaunchBuddyBug() ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		default: return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
}
#endif

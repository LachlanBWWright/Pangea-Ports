#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "ScriptBindings.h"
#include <math.h>

static bool ValidBounds(const PangeaScriptBounds* bounds)
{
	return isfinite(bounds->min.x) && isfinite(bounds->min.y) && isfinite(bounds->min.z) &&
		isfinite(bounds->max.x) && isfinite(bounds->max.y) && isfinite(bounds->max.z) &&
		bounds->min.x <= bounds->max.x && bounds->min.y <= bounds->max.y && bounds->min.z <= bounds->max.z;
}

bool Bugdom2Script_ObjectStillAlive(ObjNode* object, PangeaScriptObjectHandle handle)
{
	if (!object) return false;
	if (handle.id)
	{
		void* native = nil;
		return PangeaScript_GetObjectNativeObject(handle, &native) && native == object;
	}
	for (ObjNode* live = gFirstNodePtr; live; live = live->NextNode)
		if (live == object) return true;
	return false;
}

void Bugdom2Script_ClearPlayerRelationships(ObjNode* object)
{
	if (!object) return;
	Bugdom2Script_ClearPlaneRelationship(object);
	if (object == gPlayerInfo.objNode)
	{
		if (gPlayerInfo.heldObject) ReleaseHeldObject(object, gPlayerInfo.heldObject);
		gPlayerInfo.ridingBall = nil;
		gCurrentCarryingMoth = nil;
	}
	if (gPlayerInfo.heldObject == object) gPlayerInfo.heldObject = nil;
	if (gPlayerInfo.ridingBall == object) gPlayerInfo.ridingBall = nil;
	if (gCurrentCarryingMoth == object) gCurrentCarryingMoth = nil;
	for (int i = 0; i < gPlayerInfo.numBuddyBugs; i++)
	{
		if (gPlayerInfo.buddyBugs[i] != object) continue;
		gPlayerInfo.numBuddyBugs--;
		gPlayerInfo.buddyBugs[i] = gPlayerInfo.buddyBugs[gPlayerInfo.numBuddyBugs];
		gPlayerInfo.buddyBugs[gPlayerInfo.numBuddyBugs] = nil;
		break;
	}
}

bool Bugdom2Script_GetObjectBounds(void* nativeObject, PangeaScriptBounds* output)
{
	ObjNode* object = nativeObject;
	if (!object || !output || object->CType == INVALID_NODE_FLAG ||
		object->NumCollisionBoxes == 0 || object->NumCollisionBoxes > MAX_COLLISION_BOXES)
		return false;
	PangeaScriptBounds bounds = {0};
	for (int i = 0; i < object->NumCollisionBoxes; i++)
	{
		const CollisionBoxType* box = &object->CollisionBoxes[i];
		PangeaScriptBounds current = {{box->left, box->bottom, box->back},
			{box->right, box->top, box->front}};
		if (!ValidBounds(&current)) return false;
		if (i == 0) bounds = current;
		else
		{
			bounds.min.x = fminf(bounds.min.x, current.min.x);
			bounds.min.y = fminf(bounds.min.y, current.min.y);
			bounds.min.z = fminf(bounds.min.z, current.min.z);
			bounds.max.x = fmaxf(bounds.max.x, current.max.x);
			bounds.max.y = fmaxf(bounds.max.y, current.max.y);
			bounds.max.z = fmaxf(bounds.max.z, current.max.z);
		}
	}
	*output = bounds;
	return true;
}

bool Bugdom2Script_GetJointPosition(void* nativeObject, int joint, PangeaScriptVector3* output)
{
	ObjNode* object = nativeObject;
	if (!object || !output || object->CType == INVALID_NODE_FLAG || !object->Skeleton ||
		!object->Skeleton->skeletonDefinition || joint < 0 ||
		joint >= object->Skeleton->skeletonDefinition->NumBones)
		return false;
	const OGLPoint3D origin = {0};
	OGLPoint3D position;
	FindCoordOnJoint(object, joint, &origin, &position);
	if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z)) return false;
	*output = (PangeaScriptVector3){position.x, position.y, position.z};
	return true;
}

bool Bugdom2Script_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* object = nativeObject;
	if (!object || !position || object->CType == INVALID_NODE_FLAG ||
		!isfinite(position->x) || !isfinite(position->y) || !isfinite(position->z) ||
		object->NumCollisionBoxes > MAX_COLLISION_BOXES)
		return false;
	OGLVector3D delta = {position->x - object->Coord.x,
		position->y - object->Coord.y, position->z - object->Coord.z};
	if (!isfinite(delta.x) || !isfinite(delta.y) || !isfinite(delta.z)) return false;
	for (int i = 0; i < object->NumCollisionBoxes; i++)
	{
		const CollisionBoxType* box = &object->CollisionBoxes[i];
		PangeaScriptBounds shifted = {{box->left + delta.x, box->bottom + delta.y, box->back + delta.z},
			{box->right + delta.x, box->top + delta.y, box->front + delta.z}};
		if (!ValidBounds(&shifted)) return false;
	}
	object->Coord = (OGLPoint3D){position->x, position->y, position->z};
	if (object == gPlayerInfo.objNode) gPlayerInfo.coord = object->Coord;
	UpdateObjectTransforms(object);
	if (object->NumCollisionBoxes <= 1) CalcObjectBoxFromNode(object);
	else
	{
		for (int i = 0; i < object->NumCollisionBoxes; i++)
		{
			CollisionBoxType* box = &object->CollisionBoxes[i];
			box->left += delta.x; box->right += delta.x;
			box->bottom += delta.y; box->top += delta.y;
			box->back += delta.z; box->front += delta.z;
		}
	}
	return true;
}
#endif

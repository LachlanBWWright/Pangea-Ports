#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"
#include <math.h>

static CollisionBoxType TranslateBox(CollisionBoxType box, TQ3Vector3D delta)
{
	box.left += delta.x;
	box.right += delta.x;
	box.bottom += delta.y;
	box.top += delta.y;
	box.back += delta.z;
	box.front += delta.z;
	return box;
}

static bool BoxIsValid(CollisionBoxType box)
{
	return isfinite(box.left) && isfinite(box.right) && isfinite(box.bottom)
		&& isfinite(box.top) && isfinite(box.back) && isfinite(box.front)
		&& box.left <= box.right && box.bottom <= box.top && box.back <= box.front;
}

bool BugdomScript_GetObjectBounds(void* nativeObject, PangeaScriptBounds* output)
{
	ObjNode* object = nativeObject;
	if (!object || !output || object->CType == INVALID_NODE_FLAG
		|| !object->CollisionBoxes || object->NumCollisionBoxes == 0)
		return false;
	PangeaScriptBounds bounds = {0};
	for (int i = 0; i < object->NumCollisionBoxes; i++)
	{
		CollisionBoxType box = object->CollisionBoxes[i];
		if (!BoxIsValid(box))
			return false;
		if (i == 0)
			bounds = (PangeaScriptBounds){{box.left, box.bottom, box.back}, {box.right, box.top, box.front}};
		else
		{
			bounds.min.x = fminf(bounds.min.x, box.left);
			bounds.min.y = fminf(bounds.min.y, box.bottom);
			bounds.min.z = fminf(bounds.min.z, box.back);
			bounds.max.x = fmaxf(bounds.max.x, box.right);
			bounds.max.y = fmaxf(bounds.max.y, box.top);
			bounds.max.z = fmaxf(bounds.max.z, box.front);
		}
	}
	*output = bounds;
	return true;
}

bool BugdomScript_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	ObjNode* object = nativeObject;
	if (!object || !position || object->CType == INVALID_NODE_FLAG
		|| !isfinite(position->x) || !isfinite(position->y) || !isfinite(position->z))
		return false;
	TQ3Vector3D delta = {position->x - object->Coord.x,
		position->y - object->Coord.y, position->z - object->Coord.z};
	if (!isfinite(delta.x) || !isfinite(delta.y) || !isfinite(delta.z))
		return false;
	for (int i = 0; object->CollisionBoxes && i < object->NumCollisionBoxes; i++)
		if (!BoxIsValid(TranslateBox(object->CollisionBoxes[i], delta)))
			return false;
	for (int i = 0; object->CollisionBoxes && i < object->NumCollisionBoxes; i++)
		object->CollisionBoxes[i] = TranslateBox(object->CollisionBoxes[i], delta);
	object->Coord = (TQ3Point3D){position->x, position->y, position->z};
	if (object == gPlayerObj) gMyCoord = object->Coord;
	UpdateObjectTransforms(object);
	return true;
}

bool BugdomScript_SetObjectCollisionEnabled(void* nativeObject, bool enabled)
{
	ObjNode* object = nativeObject;
	if (!object || object->CType == INVALID_NODE_FLAG)
		return false;
	if (enabled) object->ScriptInactiveStatusBits &= ~STATUS_BIT_NOCOLLISION;
	else object->ScriptInactiveStatusBits |= STATUS_BIT_NOCOLLISION;
	if (enabled && (!object->ScriptActiveStateInitialized || object->ScriptActive))
		object->StatusBits &= ~STATUS_BIT_NOCOLLISION;
	else object->StatusBits |= STATUS_BIT_NOCOLLISION;
	return true;
}

bool BugdomScript_SetObjectActive(void* nativeObject, bool active)
{
	ObjNode* obj = nativeObject;
	if (!obj || obj->CType == INVALID_NODE_FLAG) return false;
	if (obj->ScriptActiveStateInitialized && obj->ScriptActive == active)
		return true;
	if (!active || !obj->ScriptActiveStateInitialized)
		obj->ScriptInactiveStatusBits = obj->StatusBits & (STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION);
	obj->ScriptActiveStateInitialized = true;
	obj->ScriptActive = active;
	if (active)
	{
		obj->StatusBits &= ~(STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION);
		obj->StatusBits |= obj->ScriptInactiveStatusBits;
	}
	else obj->StatusBits |= STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION;
	return true;
}

Boolean BugdomScript_ShouldRunNativeMovement(const ObjNode* obj)
{
	if (obj->ScriptActiveStateInitialized && !obj->ScriptActive)
		return false;
	return !PangeaScript_ObjectIsScriptControlled(
		(PangeaScriptObjectHandle){obj->ScriptObjectID, obj->ScriptObjectGeneration});
}

#endif

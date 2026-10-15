#pragma once
#include "game.h"
#include "ScriptBindings.h"
#include <math.h>

static inline bool Nanosaur2Script_ReadBounds(void* nativeObject, PangeaScriptBounds* output)
{
	ObjNode* node = nativeObject;
	if (!node || !output || node->CType == INVALID_NODE_FLAG || node->NumCollisionBoxes <= 0 ||
		node->NumCollisionBoxes > MAX_COLLISION_BOXES) return false;
	for (int i = 0; i < node->NumCollisionBoxes; i++)
	{
		const CollisionBoxType* box = &node->CollisionBoxes[i];
		if (!isfinite(box->left) || !isfinite(box->right) || !isfinite(box->bottom) ||
			!isfinite(box->top) || !isfinite(box->back) || !isfinite(box->front) ||
			box->left > box->right || box->bottom > box->top || box->back > box->front) return false;
		if (i == 0) *output = (PangeaScriptBounds){{box->left, box->bottom, box->back}, {box->right, box->top, box->front}};
		else
		{
			output->min.x = fminf(output->min.x, box->left);
			output->min.y = fminf(output->min.y, box->bottom);
			output->min.z = fminf(output->min.z, box->back);
			output->max.x = fmaxf(output->max.x, box->right);
			output->max.y = fmaxf(output->max.y, box->top);
			output->max.z = fmaxf(output->max.z, box->front);
		}
	}
	return true;
}

static inline bool Nanosaur2Script_MoveGeometry(ObjNode* node, const PangeaScriptVector3* position)
{
	if (!node || !position || node->CType == INVALID_NODE_FLAG ||
		node->NumCollisionBoxes < 0 || node->NumCollisionBoxes > MAX_COLLISION_BOXES ||
		!isfinite(position->x) || !isfinite(position->y) || !isfinite(position->z)) return false;
	OGLVector3D delta = {position->x - node->Coord.x, position->y - node->Coord.y, position->z - node->Coord.z};
	if (!isfinite(delta.x) || !isfinite(delta.y) || !isfinite(delta.z)) return false;
	CollisionBoxType boxes[MAX_COLLISION_BOXES];
	for (int i = 0; i < node->NumCollisionBoxes; i++)
	{
		boxes[i] = node->CollisionBoxes[i];
		CollisionBoxType* box = &boxes[i];
		box->left += delta.x; box->right += delta.x;
		box->bottom += delta.y; box->top += delta.y;
		box->back += delta.z; box->front += delta.z;
		if (!isfinite(box->left) || !isfinite(box->right) || !isfinite(box->bottom) ||
			!isfinite(box->top) || !isfinite(box->back) || !isfinite(box->front) ||
			box->left > box->right || box->bottom > box->top || box->back > box->front) return false;
	}
	node->Coord = (OGLPoint3D){position->x, position->y, position->z};
	for (int i = 0; i < node->NumCollisionBoxes; i++) node->CollisionBoxes[i] = boxes[i];
	UpdateObjectTransforms(node);
	return true;
}

static inline bool Nanosaur2Script_ScaleGeometry(ObjNode* node, float scale)
{
	if (!node || node->CType == INVALID_NODE_FLAG || !isfinite(scale) || scale <= 0 ||
		node->NumCollisionBoxes < 0 || node->NumCollisionBoxes > MAX_COLLISION_BOXES ||
		!isfinite(node->Scale.x) || !isfinite(node->Scale.y) || !isfinite(node->Scale.z) ||
		node->Scale.x <= 0 || node->Scale.y <= 0 || node->Scale.z <= 0) return false;
	OGLVector3D ratio = {scale / node->Scale.x, scale / node->Scale.y, scale / node->Scale.z};
	if (!isfinite(ratio.x) || !isfinite(ratio.y) || !isfinite(ratio.z)) return false;
	CollisionBoxType boxes[MAX_COLLISION_BOXES];
	for (int i = 0; i < node->NumCollisionBoxes; i++)
	{
		boxes[i] = node->CollisionBoxes[i];
		CollisionBoxType* box = &boxes[i];
		box->left = node->Coord.x + (box->left - node->Coord.x) * ratio.x;
		box->right = node->Coord.x + (box->right - node->Coord.x) * ratio.x;
		box->bottom = node->Coord.y + (box->bottom - node->Coord.y) * ratio.y;
		box->top = node->Coord.y + (box->top - node->Coord.y) * ratio.y;
		box->back = node->Coord.z + (box->back - node->Coord.z) * ratio.z;
		box->front = node->Coord.z + (box->front - node->Coord.z) * ratio.z;
		if (!isfinite(box->left) || !isfinite(box->right) || !isfinite(box->bottom) ||
			!isfinite(box->top) || !isfinite(box->back) || !isfinite(box->front) ||
			box->left > box->right || box->bottom > box->top || box->back > box->front) return false;
	}
	if (!isfinite(node->LeftOff * ratio.x) || !isfinite(node->RightOff * ratio.x) ||
		!isfinite(node->BottomOff * ratio.y) || !isfinite(node->TopOff * ratio.y) ||
		!isfinite(node->BackOff * ratio.z) || !isfinite(node->FrontOff * ratio.z) ||
		!isfinite(node->BoundingSphereRadius * ratio.x)) return false;
	for (int i = 0; i < node->NumCollisionBoxes; i++) node->CollisionBoxes[i] = boxes[i];
	node->LeftOff *= ratio.x; node->RightOff *= ratio.x;
	node->BottomOff *= ratio.y; node->TopOff *= ratio.y;
	node->BackOff *= ratio.z; node->FrontOff *= ratio.z;
	node->BoundingSphereRadius *= ratio.x;
	node->Scale = (OGLVector3D){scale, scale, scale};
	UpdateObjectTransforms(node);
	return true;
}

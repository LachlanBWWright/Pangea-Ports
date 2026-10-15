#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"
#include <math.h>

static ObjNode* RideParent(int animation, int* joint, TQ3Point3D* offset)
{
	if (animation == PLAYER_ANIM_RIDEWATERBUG)
	{
		*joint = 0;
		*offset = (TQ3Point3D){0, 30, 40};
		return gCurrentWaterBug;
	}
	if (animation == PLAYER_ANIM_RIDEDRAGONFLY)
	{
		*joint = DRAGONFLY_JOINT_TAIL;
		*offset = (TQ3Point3D){0, 8, 55};
		return gCurrentDragonFly;
	}
	if (animation == PLAYER_ANIM_ROPESWING)
	{
		*joint = gCurrentRopeJoint;
		*offset = (TQ3Point3D){0, -100, 25};
		return gCurrentRope;
	}
	return NULL;
}

void BugdomScript_SyncRide(void)
{
	if (!gPlayerObj || gPlayerMode != PLAYER_MODE_BUG || !gPlayerObj->Skeleton || gPlayerGotKilledFlag)
		return;
	int joint = 0;
	TQ3Point3D offset;
	ObjNode* parent = RideParent(gPlayerObj->Skeleton->AnimNum, &joint, &offset);
	if (!parent || !parent->Skeleton || !parent->Skeleton->skeletonDefinition
		|| joint < 0 || joint >= parent->Skeleton->skeletonDefinition->NumBones
		|| !isfinite(parent->Scale.x) || parent->Scale.x <= 0)
		return;
	if (BugdomScript_ShouldRunNativeMovement(gPlayerObj) && BugdomScript_ShouldRunNativeMovement(parent))
		return;
	float scale = gPlayerObj->Scale.x / parent->Scale.x;
	TQ3Matrix4x4 matrix, jointMatrix, scaled;
	Q3Matrix4x4_SetScale(&matrix, scale, scale, scale);
	FindJointFullMatrix(parent, joint, &jointMatrix);
	Q3Matrix4x4_Multiply(&matrix, &jointMatrix, &scaled);
	if (gPlayerObj->Skeleton->AnimNum == PLAYER_ANIM_ROPESWING)
	{
		Q3Matrix4x4_SetRotate_Y(&matrix, gPlayerObj->Rot.y);
		Q3Matrix4x4_Multiply(&matrix, &scaled, &jointMatrix);
		scaled = jointMatrix;
	}
	Q3Matrix4x4_SetTranslate(&matrix, offset.x, offset.y, offset.z);
	Q3Matrix4x4_Multiply(&matrix, &scaled, &gPlayerObj->BaseTransformMatrix);
	const TQ3Point3D zero = {0};
	Q3Point3D_Transform(&zero, &gPlayerObj->BaseTransformMatrix, &gMyCoord);
	gPlayerObj->Coord = gMyCoord;
	if (gFramesPerSecondFrac > 0)
		gPlayerObj->Delta = (TQ3Vector3D){
			(gMyCoord.x - gPlayerObj->OldCoord.x) / gFramesPerSecondFrac,
			(gMyCoord.y - gPlayerObj->OldCoord.y) / gFramesPerSecondFrac,
			(gMyCoord.z - gPlayerObj->OldCoord.z) / gFramesPerSecondFrac,
		};
	CalcObjectBoxFromNode(gPlayerObj);
	if (gPlayerObj->ShadowNode)
		gPlayerObj->ShadowNode->StatusBits |= STATUS_BIT_HIDDEN;
}

#endif

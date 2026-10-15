#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

static void RestorePlayerAfterRide(void)
{
	if (!gPlayerObj || gPlayerMode != PLAYER_MODE_BUG || !gPlayerObj->Skeleton)
		return;
	int animation = gPlayerObj->Skeleton->AnimNum;
	if (!gPlayerGotKilledFlag && (animation == PLAYER_ANIM_RIDEWATERBUG
		|| animation == PLAYER_ANIM_RIDEDRAGONFLY || animation == PLAYER_ANIM_ROPESWING))
		MorphToSkeletonAnim(gPlayerObj->Skeleton, PLAYER_ANIM_FALL, 9);
	if (gPlayerObj->ShadowNode)
		gPlayerObj->ShadowNode->StatusBits &= ~STATUS_BIT_HIDDEN;
}

void BugdomScript_ReleaseRide(void)
{
	if (gCurrentDragonFly)
		PlayerOffDragonfly();
	if (gCurrentWaterBug)
	{
		gCurrentWaterBug->Mode = WATERBUG_MODE_COAST;
		MorphToSkeletonAnim(gCurrentWaterBug->Skeleton, WATERBUG_ANIM_OUTOFSERVICE, 5);
		gCurrentWaterBug = NULL;
	}
	gCurrentRope = NULL;
	RestorePlayerAfterRide();
}

void BugdomScript_ClearRelationships(ObjNode* object)
{
	if (!object)
		return;
	if (object == gPlayerObj)
		BugdomScript_ReleaseRide();
	if (object == gCurrentDragonFly || object == gCurrentWaterBug || object == gCurrentRope)
	{
		BugdomScript_ReleaseRide();
	}
	if (object == gPrevRope)
		gPrevRope = NULL;
	if (object == gMyBuddy)
		gMyBuddy = NULL;
}

#endif

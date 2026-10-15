static PangeaScriptObjectHandle gExitTransition;
static float gExitTransitionElapsed;

static void ClearExitTransition(void)
{
	gExitTransition = (PangeaScriptObjectHandle){0};
	gExitTransitionElapsed = 0;
}

static void BeginExitTransition(ObjNode* player)
{
	gExitTransition = (PangeaScriptObjectHandle){player->ScriptObjectID, player->ScriptObjectGeneration};
	gExitTransitionElapsed = 0;
}

bool NanosaurScript_RunNativeTransition(ObjNode* player)
{
	if (!gExitTransition.id || !player || player->ScriptObjectID != gExitTransition.id) return false;
	if (player != gPlayerObj || player->ScriptObjectGeneration != gExitTransition.generation
		|| player->CType == INVALID_NODE_FLAG || !player->Skeleton || gPlayerGotKilledFlag
		|| player->Skeleton->AnimNum != PLAYER_ANIM_EXIT || gGameOverFlag)
	{
		ClearExitTransition();
		return false;
	}
	if (isfinite(gFramesPerSecondFrac) && gFramesPerSecondFrac > 0) gExitTransitionElapsed += gFramesPerSecondFrac;
	if (gExitTransitionElapsed >= 8)
	{
		player->Flag[0] = false;
		player->ExitTimer = 0;
		SetSkeletonAnim(player->Skeleton, PLAYER_ANIM_STAND);
		ClearExitTransition();
		return true;
	}
	AdvancePlayerExit(player);
	if (player->ExitTimer > 5 || gGameOverFlag) ClearExitTransition();
	return true;
}

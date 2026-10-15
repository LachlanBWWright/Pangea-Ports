#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "ScriptBespoke.h"
#include <string.h>
#include <math.h>

static int ActorAnimation(const char* mode, int areaMode)
{
	if (!mode) return -1;
	if (strcmp(mode, "stand") == 0) return PLAYER_ANIM_STAND;
	if (strcmp(mode, "walk") == 0 && areaMode != BILLY_AREA_MODE_STAMPEDE) return PLAYER_ANIM_WALK;
	if (areaMode == BILLY_AREA_MODE_DUEL)
	{
		if (strcmp(mode, "draw") == 0) return PLAYER_ANIM_DRAWSHOOT;
		if (strcmp(mode, "drawLeft") == 0) return PLAYER_ANIM_DRAWSHOOT2;
		if (strcmp(mode, "drawThree") == 0) return PLAYER_ANIM_DRAWSHOOT3;
		if (strcmp(mode, "drawRight") == 0) return PLAYER_ANIM_DRAWSHOOT4;
	}
	if (areaMode == BILLY_AREA_MODE_STAMPEDE)
	{
		if (strcmp(mode, "run") == 0) return PLAYER_ANIM_RUN;
		if (strcmp(mode, "jump") == 0) return PLAYER_ANIM_STAMPEDEJUMP;
	}
	if (areaMode == BILLY_AREA_MODE_SHOOTOUT && strcmp(mode, "duck") == 0) return PLAYER_ANIM_DUCK;
	return -1;
}

PangeaScriptStatus BillyScript_SetActorMode(const char* mode)
{
	ObjNode* player = gPlayerInfo.objNode;
	int animation = ActorAnimation(mode, GetBillyAreaMode());
	if (!gGameViewInfoPtr || !player || !player->Skeleton || !player->Skeleton->skeletonDefinition || player->ScriptDeleteRequested
		|| gPlayerIsDead || gLevelCompleted || animation < 0
		|| animation >= player->Skeleton->skeletonDefinition->NumAnims) return PANGEA_SCRIPT_BAD_ARGUMENT;
	player->Flag[1] = player->Flag[2] = player->Flag[3] = false;
	MorphToSkeletonAnim(player->Skeleton, animation, 10);
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus BillyScript_RespawnActor(void)
{
	ObjNode* player = gPlayerInfo.objNode;
	if (!gGameViewInfoPtr || !player || !player->Skeleton || player->CType == INVALID_NODE_FLAG
		|| player->ScriptDeleteRequested || gLevelCompleted) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerIsDead = false;
	player->Health = 1;
	player->Delta = (OGLVector3D){0};
	player->Flag[1] = player->Flag[2] = player->Flag[3] = false;
	gPlayerInfo.invincibilityTimer = 2;
	gPlayerInfo.coord = player->Coord;
	SetSkeletonAnim(player->Skeleton, GetBillyAreaMode() == BILLY_AREA_MODE_STAMPEDE ? PLAYER_ANIM_RUN : PLAYER_ANIM_STAND);
	return PANGEA_SCRIPT_OK;
}

bool BillyScript_GetRelations(int playerNum, PangeaScriptPlayerRelations* output)
{
	if (playerNum != 0 || !output || !gGameViewInfoPtr) return false;
	*output = (PangeaScriptPlayerRelations){.form = "billy"};
	return true;
}

static bool NormalizeAim(const PangeaScriptVector3* input, PangeaScriptVector3* output)
{
	if (!isfinite(input->x) || !isfinite(input->y) || !isfinite(input->z)) return false;
	double length = sqrt((double)input->x * input->x + (double)input->y * input->y + (double)input->z * input->z);
	if (!isfinite(length) || length < 0.000001) return false;
	*output = (PangeaScriptVector3){input->x / length, input->y / length, input->z / length};
	return true;
}

PangeaScriptStatus BillyScript_RunOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output)
{
	ObjNode* player = gPlayerInfo.objNode;
	if (!request || !output || request->playerNum != 0 || !gGameViewInfoPtr
		|| (GetBillyAreaMode() != BILLY_AREA_MODE_TARGET_PRACTICE && !player)
		|| (player && (player->CType == INVALID_NODE_FLAG || player->ScriptDeleteRequested))
		|| request->hasTarget || request->hasPosition || request->hasVelocity || request->hasJoint) return PANGEA_SCRIPT_BAD_ARGUMENT;
	*output = (PangeaScriptOperationResult){0};
	if (request->operation != PANGEA_SCRIPT_OP_FIRE && (request->hasAim || request->hasWeapon)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->operation != PANGEA_SCRIPT_OP_FIRE && request->mode) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->operation != PANGEA_SCRIPT_OP_TRANSFORM && request->form) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->operation == PANGEA_SCRIPT_OP_TRANSFORM) return BillyScript_SetActorMode(request->form);
	if (request->operation == PANGEA_SCRIPT_OP_RESPAWN) return BillyScript_RespawnActor();
	if (gPlayerIsDead || gLevelCompleted) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->operation == PANGEA_SCRIPT_OP_RELOAD)
	{
		if (GetBillyAreaMode() != BILLY_AREA_MODE_SHOOTOUT || gPlayerInfo.ammoCount <= 0 || !gNeedToReloadNextAmmoClip) return PANGEA_SCRIPT_BAD_ARGUMENT;
		gNeedToReloadNextAmmoClip = false;
		PlayEffect(EFFECT_RELOAD);
		return PANGEA_SCRIPT_OK;
	}
	if (request->operation != PANGEA_SCRIPT_OP_FIRE) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (GetBillyAreaMode() == BILLY_AREA_MODE_DUEL)
	{
		if (request->hasAim) return PANGEA_SCRIPT_BAD_ARGUMENT;
		int side = request->hasWeapon ? request->weapon : 0;
		if (request->mode)
		{
			if (request->hasWeapon) return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (strcmp(request->mode, "forward") == 0) side = 0;
			else if (strcmp(request->mode, "left") == 0) side = 1;
			else if (strcmp(request->mode, "right") == 0) side = 2;
			else return PANGEA_SCRIPT_BAD_ARGUMENT;
		}
		return BillyScript_FireDuel(side) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (request->mode) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasWeapon && request->weapon != 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
	PangeaScriptVector3 aim;
	if (request->hasAim && !NormalizeAim(&request->aim, &aim)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	bool fired = GetBillyAreaMode() == BILLY_AREA_MODE_SHOOTOUT
		? BillyScript_FireShootout(request->hasAim ? &aim : NULL)
		: BillyScript_FirePractice(request->hasAim ? &aim : NULL);
	return fired ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_BAD_ARGUMENT;
}
#endif

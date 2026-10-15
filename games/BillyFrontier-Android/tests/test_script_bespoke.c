#include "game.h"
#include "ScriptBespoke.h"
#include <assert.h>
#include <math.h>

PlayerInfoType gPlayerInfo;
Boolean gPlayerIsDead, gLevelCompleted, gNeedToReloadNextAmmoClip;
OGLSetupOutputType* gGameViewInfoPtr;
static int areaMode, fireCount, duelSide;
static PangeaScriptVector3 lastAim;

int GetBillyAreaMode(void) { return areaMode; }
short PlayEffect(short effect) { assert(effect == EFFECT_RELOAD); return -1; }
void SetSkeletonAnim(SkeletonObjDataType* skeleton, long animation) { skeleton->AnimNum = animation; }
void MorphToSkeletonAnim(SkeletonObjDataType* skeleton, long animation, float speed)
{
	assert(speed > 0);
	SetSkeletonAnim(skeleton, animation);
}
bool BillyScript_FireShootout(const PangeaScriptVector3* aim)
{
	assert(areaMode == BILLY_AREA_MODE_SHOOTOUT);
	if (aim) lastAim = *aim;
	fireCount++;
	return true;
}
bool BillyScript_FirePractice(const PangeaScriptVector3* aim)
{
	if (areaMode != BILLY_AREA_MODE_TARGET_PRACTICE) return false;
	if (aim) lastAim = *aim;
	fireCount++;
	return true;
}
bool BillyScript_FireDuel(int side) { duelSide = side; return side >= 0 && side <= 2; }

int main(void)
{
	OGLSetupOutputType view = {0};
	SkeletonDefType definition = {.NumAnims = 14};
	SkeletonObjDataType skeleton = {.skeletonDefinition = &definition};
	ObjNode player = {.Skeleton = &skeleton, .CType = CTYPE_PLAYER, .Health = 0.3f};
	gGameViewInfoPtr = &view;
	gPlayerInfo.objNode = &player;
	gPlayerInfo.ammoCount = 20;
	PangeaScriptOperationResult result;
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_FIRE, .hasAim = true, .aim = {0, 3, 4}};
	areaMode = BILLY_AREA_MODE_SHOOTOUT;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(fireCount == 1 && fabsf(lastAim.y - 0.6f) < 0.0001f && fabsf(lastAim.z - 0.8f) < 0.0001f);
	request.aim = (PangeaScriptVector3){0};
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && fireCount == 1);
	request.hasAim = false;
	request.hasTarget = true;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.hasTarget = false;
	request.operation = PANGEA_SCRIPT_OP_TRANSFORM;
	request.form = "jump";
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	areaMode = BILLY_AREA_MODE_STAMPEDE;
	request.hasAim = true;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.hasAim = false;
	request.mode = "ignored";
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.mode = NULL;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK && skeleton.AnimNum == PLAYER_ANIM_STAMPEDEJUMP);
	gPlayerIsDead = true;
	request.operation = PANGEA_SCRIPT_OP_RESPAWN;
	player.Delta.z = 200;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && gPlayerIsDead);
	request.form = NULL;
	request.mode = "ignored";
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && gPlayerIsDead);
	request.mode = NULL;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(!gPlayerIsDead && player.Health == 1 && player.Delta.z == 0 && skeleton.AnimNum == PLAYER_ANIM_RUN);
	areaMode = BILLY_AREA_MODE_SHOOTOUT;
	request.operation = PANGEA_SCRIPT_OP_RELOAD;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	gNeedToReloadNextAmmoClip = true;
	request.hasAim = true;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && gNeedToReloadNextAmmoClip);
	request.hasAim = false;
	request.mode = "ignored";
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && gNeedToReloadNextAmmoClip);
	request.mode = NULL;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK && !gNeedToReloadNextAmmoClip);
	areaMode = BILLY_AREA_MODE_DUEL;
	request.operation = PANGEA_SCRIPT_OP_FIRE;
	request.hasWeapon = true;
	request.weapon = 2;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK && duelSide == 2);
	gLevelCompleted = true;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	PangeaScriptPlayerRelations relations;
	assert(BillyScript_GetRelations(0, &relations) && strcmp(relations.form, "billy") == 0);
	assert(!BillyScript_GetRelations(1, &relations));
	gLevelCompleted = false;
	areaMode = BILLY_AREA_MODE_TARGET_PRACTICE;
	gPlayerInfo.objNode = NULL;
	request.hasWeapon = false;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK && fireCount == 2);
	request.operation = PANGEA_SCRIPT_OP_RESPAWN;
	assert(BillyScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	return 0;
}

#include "game.h"
#include "ScriptBindings.h"
#include <assert.h>

Boolean gPlayerGotKilledFlag, gWonGameFlag;
uint32_t gInfobarUpdateBits;
static bool allowDamage;
static int stoppedJetpack, changedAnimation, appliedDamage;
static ObjNode* expectedSource;
static int expectedCause;

Boolean NanosaurScript_OnDamage(ObjNode* source, float damage, int cause, float* output)
{
	assert(source == expectedSource && cause == expectedCause);
	assert(stoppedJetpack == 0 && changedAnimation == 0);
	assert(gPlayerObj->InvincibleTimer == 0.0f && gPlayerObj->HurtTimer == 0.0f);
	*output = damage;
	return allowDamage;
}

void StopJetPack(ObjNode* node) { (void)node; stoppedJetpack++; }
void DropItem(ObjNode* node) { (void)node; }
void SetSkeletonAnim(SkeletonObjDataType* skeleton, long animation) { (void)skeleton; (void)animation; changedAnimation++; }
void MorphToSkeletonAnim(SkeletonObjDataType* skeleton, long animation, float speed) { (void)skeleton; (void)animation; (void)speed; }
void NanosaurScript_OnDamageApplied(float damage, int cause) { assert(damage == 0.25f && cause == expectedCause); appliedDamage++; }
void NanosaurScript_OnDeath(int value) { (void)value; }

int main(void)
{
	ObjNode player = {.StatusBits = STATUS_BIT_ONGROUND};
	ObjNode enemy = {0};
	gPlayerObj = &player;
	expectedSource = &enemy;
	expectedCause = PLAYER_DAMAGE_CONTACT;
	PlayerGotHurtFromSource(&player, &enemy, expectedCause, 0.25f, true, false);
	assert(gMyHealth == 1.0f && player.InvincibleTimer == 0.0f && player.HurtTimer == 0.0f);
	assert(stoppedJetpack == 0 && changedAnimation == 0 && appliedDamage == 0 && gInfobarUpdateBits == 0);
	allowDamage = true;
	PlayerGotHurtFromSource(&player, &enemy, expectedCause, 0.25f, true, false);
	assert(gMyHealth == 0.75f && player.InvincibleTimer > 0.0f && player.HurtTimer > 0.0f);
	assert(stoppedJetpack == 1 && changedAnimation == 1 && appliedDamage == 1);
	assert(gInfobarUpdateBits & UPDATE_HEALTH);
	return 0;
}

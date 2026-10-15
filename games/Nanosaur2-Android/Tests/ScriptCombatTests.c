#include "ScriptCombat.h"
#include <assert.h>

static Boolean allowHit = true;
static Boolean destroyHit;
static float adjustedDamage;
static int hookCalls, nativeCalls, deletions;
static bool controlled;
bool PangeaScript_ObjectIsScriptControlled(PangeaScriptObjectHandle handle) { return controlled && handle.id > 0; }

Boolean Nanosaur2Script_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* output, Boolean* destroy)
{
	assert(weapon && target && damage == weapon->Damage);
	hookCalls++;
	*output = adjustedDamage;
	*destroy = destroyHit;
	return allowHit;
}

void DeleteObject(ObjNode* object)
{
	deletions++;
	object->CType = INVALID_NODE_FLAG;
}

static Boolean NativeHit(ObjNode* weapon, ObjNode* target, OGLPoint3D* point, OGLVector3D* normal)
{
	(void) point;
	(void) normal;
	nativeCalls++;
	target->Health -= weapon->Damage;
	return true;
}

int main(void)
{
	ObjNode weapon = {.Damage = .5f};
	ObjNode target = {.Health = 1.0f, .ScriptObjectID = 1, .ScriptDefinitionID = "test"};
	adjustedDamage = .25f;
	DispatchProjectileHit(&weapon, &target, NULL, NULL);
	assert(target.Health == .75f && weapon.Damage == .5f && hookCalls == 1);
	allowHit = false;
	DispatchProjectileHit(&weapon, &target, NULL, NULL);
	assert(target.Health == .75f && deletions == 0);
	allowHit = true;
	target.HitByWeaponHandler = NativeHit;
	DispatchProjectileHit(&weapon, &target, NULL, NULL);
	assert(target.Health == .5f && nativeCalls == 1 && weapon.Damage == .5f);
	controlled = true;
	DispatchProjectileHit(&weapon, &target, NULL, NULL);
	assert(target.Health == .25f && nativeCalls == 1 && weapon.Damage == .5f);
	adjustedDamage = NAN;
	DispatchProjectileHit(&weapon, &target, NULL, NULL);
	assert(target.Health == .25f && nativeCalls == 1 && weapon.Damage == .5f);
	adjustedDamage = .25f;
	DispatchProjectileHit(NULL, &target, NULL, NULL);
	DispatchProjectileHit(&weapon, NULL, NULL, NULL);
	assert(target.Health == .25f && nativeCalls == 1);
	ObjNode nativeTarget = {.Health = 1, .HitByWeaponHandler = NativeHit};
	DispatchProjectileHit(&weapon, &nativeTarget, NULL, NULL);
	assert(nativeTarget.Health == .75f && nativeCalls == 2);
	destroyHit = true;
	DispatchProjectileHit(&weapon, &target, NULL, NULL);
	assert(deletions == 1 && nativeCalls == 2 && weapon.Damage == .5f);
	return 0;
}

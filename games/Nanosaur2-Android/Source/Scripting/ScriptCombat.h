#pragma once
#include "game.h"
#ifdef PANGEA_ENABLE_SCRIPTING
#include "ScriptBindings.h"
#endif
#include <math.h>

static inline void DispatchProjectileHit(ObjNode* weapon, ObjNode* target, OGLPoint3D* point, OGLVector3D* normal)
{
	if (!weapon || !target || weapon->CType == INVALID_NODE_FLAG || target->CType == INVALID_NODE_FLAG ||
		!isfinite(weapon->Damage) || weapon->Damage < 0) return;
	float originalDamage = weapon->Damage;
#ifdef PANGEA_ENABLE_SCRIPTING
	float damage = originalDamage;
	Boolean destroyTarget = false;
	if (!Nanosaur2Script_OnWeaponHit(weapon, target, damage, &damage, &destroyTarget)) return;
	if (target->CType == INVALID_NODE_FLAG || weapon->CType == INVALID_NODE_FLAG ||
		!isfinite(damage) || damage < 0) return;
	if (destroyTarget)
	{
		DeleteObject(target);
		return;
	}
	weapon->Damage = damage;
#endif
	Boolean nativeHit = true;
#ifdef PANGEA_ENABLE_SCRIPTING
	nativeHit = !PangeaScript_ObjectIsScriptControlled((PangeaScriptObjectHandle){target->ScriptObjectID, target->ScriptObjectGeneration});
#endif
	if (nativeHit && target->HitByWeaponHandler)
		target->HitByWeaponHandler(weapon, target, point, normal);
#ifdef PANGEA_ENABLE_SCRIPTING
	else if (target->ScriptObjectID && (!nativeHit || target->ScriptDefinitionID[0]))
		target->Health = fmaxf(0.0f, target->Health - weapon->Damage);
#endif
	weapon->Damage = originalDamage;
}

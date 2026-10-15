#include "game.h"
#ifdef PANGEA_ENABLE_SCRIPTING
#include "pangea_script.h"
#endif

static Boolean gExplicitInteraction;

Boolean AllowNativeGameplayRules(void)
{
#ifdef PANGEA_ENABLE_SCRIPTING
	return gExplicitInteraction || !PangeaScript_RulesAreScriptControlled();
#else
	return true;
#endif
}

Boolean RunExplicitGameplayInteraction(ObjNode* target, ObjNode* player)
{
	if (!target || !player || !target->TriggerCallback || !(target->CType & CTYPE_TRIGGER) ||
		target->CType == INVALID_NODE_FLAG || target->StatusBits & STATUS_BIT_NOCOLLISION)
		return false;
	Boolean previous = gExplicitInteraction;
	gExplicitInteraction = true;
	target->TriggerCallback(target, player, SIDE_BITS_BOTTOM);
	gExplicitInteraction = previous;
	return true;
}

void StartAutomaticLevelCompletion(float coolDownTimer)
{
	if (AllowNativeGameplayRules()) StartLevelCompletion(coolDownTimer);
}

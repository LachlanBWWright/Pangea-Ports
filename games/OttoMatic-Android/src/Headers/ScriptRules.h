#pragma once

#include "game.h"
#ifdef PANGEA_ENABLE_SCRIPTING
#include "pangea_script.h"
#endif

static inline bool OttoScript_UsesNativeRules(void)
{
#ifdef PANGEA_ENABLE_SCRIPTING
	return !PangeaScript_RulesAreScriptControlled();
#else
	return true;
#endif
}

static inline bool OttoScript_CompleteNativeLevel(float cooldown)
{
	if (!OttoScript_UsesNativeRules()) return false;
	gLevelCompleted = true;
	gLevelCompletedCoolDownTimer = cooldown;
	return true;
}

static inline bool OttoScript_RecordNativeRescue(int humanType)
{
	if (!OttoScript_UsesNativeRules() || humanType < 0 || humanType >= NUM_HUMAN_TYPES)
		return false;
	gNumHumansRescuedTotal++;
	gNumHumansRescuedOfType[humanType]++;
	return true;
}

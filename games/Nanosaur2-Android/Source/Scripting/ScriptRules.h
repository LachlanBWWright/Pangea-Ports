#pragma once
#include <stdbool.h>
#ifdef PANGEA_ENABLE_SCRIPTING
#include "pangea_script.h"
#endif

static inline bool Nanosaur2Script_UsesNativeRules(void)
{
#ifdef PANGEA_ENABLE_SCRIPTING
	return !PangeaScript_RulesAreScriptControlled();
#else
	return true;
#endif
}

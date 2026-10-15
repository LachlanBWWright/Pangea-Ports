#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING
#include "pangea_script.h"

PangeaScriptStatus MikeScript_RunOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output);
bool MikeScript_GetRelations(int playerNum, PangeaScriptPlayerRelations* output);
#endif

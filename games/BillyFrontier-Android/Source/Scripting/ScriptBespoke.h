#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING
#include "pangea_script.h"

bool BillyScript_FireShootout(const PangeaScriptVector3* aim);
bool BillyScript_FirePractice(const PangeaScriptVector3* aim);
bool BillyScript_FireDuel(int side);
PangeaScriptStatus BillyScript_SetActorMode(const char* mode);
PangeaScriptStatus BillyScript_RespawnActor(void);
PangeaScriptStatus BillyScript_RunOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output);
bool BillyScript_GetRelations(int playerNum, PangeaScriptPlayerRelations* output);
#endif

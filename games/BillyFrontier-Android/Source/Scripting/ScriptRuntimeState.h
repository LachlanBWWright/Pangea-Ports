#pragma once

#include "pangea_script.h"

float BillyScript_AdvanceClock(float elapsed, float delta);
int BillyScript_LogicalPlayerCount(bool areaActive, bool hasBody, bool targetPractice);
PangeaScriptInputAction BillyScript_SampleAction(const char* name, bool previous, bool held, bool pressed);
bool BillyScript_ValidBounds(const PangeaScriptBounds* bounds);
bool BillyScript_TranslateBounds(PangeaScriptBounds* bounds, const PangeaScriptVector3* delta);

int BillyScript_GetDuelStage(void);
float BillyScript_GetDuelDelay(void);
float BillyScript_GetDuelTimeLimit(void);
void BillyScript_SetDuelDelay(float value);
void BillyScript_SetDuelTimeLimit(float value);
bool BillyScript_GetDuelOutcome(void);
void BillyScript_SetDuelOutcome(bool success);

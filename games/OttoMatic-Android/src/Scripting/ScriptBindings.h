#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void OttoScript_Init(void);
void OttoScript_Shutdown(void);
void OttoScript_LoadLevelConfig(int levelNum);
void OttoScript_OnLevelLoad(int levelNum);
void OttoScript_OnLevelStart(int levelNum);
void OttoScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void OttoScript_OnLevelComplete(int levelNum);
void OttoScript_OnLevelUnload(int levelNum);
int OttoScript_RemapTerrainItemType(int levelNum, int itemType);
Boolean OttoScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean OttoScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);
void OttoScript_RegisterHuman(ObjNode* human);
void OttoScript_UnregisterHuman(ObjNode* human);
void OttoScript_RunHumanObjectFrame(ObjNode* human, Boolean usesGlobals);
void OttoScript_ApplyHumanVisualOffset(ObjNode* human);

#endif

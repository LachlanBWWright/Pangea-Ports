#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void NanosaurScript_Init(void);
void NanosaurScript_Shutdown(void);
void NanosaurScript_LoadLevelConfig(int levelNum);
void NanosaurScript_OnLevelLoad(int levelNum);
void NanosaurScript_OnLevelStart(int levelNum);
void NanosaurScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void NanosaurScript_OnLevelComplete(int levelNum);
void NanosaurScript_OnLevelUnload(int levelNum);
int NanosaurScript_RemapTerrainItemType(int levelNum, int itemType);
Boolean NanosaurScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);

#endif

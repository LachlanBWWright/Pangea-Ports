#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "pangea_script.h"
#include "structs.h"

void Bugdom2Script_Init(void);
void Bugdom2Script_Shutdown(void);
void Bugdom2Script_LoadLevelConfig(int levelNum);
void Bugdom2Script_OnLevelLoad(int levelNum);
void Bugdom2Script_OnLevelStart(int levelNum);
void Bugdom2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void Bugdom2Script_OnLevelComplete(int levelNum);
void Bugdom2Script_OnLevelUnload(int levelNum);
int Bugdom2Script_RemapTerrainItemType(int levelNum, int itemType);
Boolean Bugdom2Script_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);

#endif


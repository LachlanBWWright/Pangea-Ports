#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void CroMagScript_Init(void);
void CroMagScript_Shutdown(void);
void CroMagScript_LoadTrackConfig(int trackNum);
void CroMagScript_OnRaceLoad(int trackNum);
void CroMagScript_OnRaceStart(int trackNum);
void CroMagScript_OnRaceFrame(int trackNum, unsigned int frameNum, float deltaSeconds, float raceTimeSeconds);
void CroMagScript_OnRaceComplete(int trackNum);
void CroMagScript_OnRaceUnload(int trackNum);
int CroMagScript_RemapTerrainItemType(int trackNum, int itemType);
Boolean CroMagScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int trackNum, int playerNum, int originalType, int remappedType, float x, float z);

#endif

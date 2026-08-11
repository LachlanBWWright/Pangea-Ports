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
Boolean OttoScript_TryReplaceTerrainItem(int itemIndex, int nativeType, float x, float z);
Boolean OttoScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);
Boolean OttoScript_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex);
void OttoScript_RegisterHuman(ObjNode* human);
void OttoScript_UnregisterHuman(ObjNode* human);
void OttoScript_RunHumanObjectFrame(ObjNode* human, Boolean usesGlobals);
void OttoScript_ApplyHumanVisualOffset(ObjNode* human);

void OttoScript_RegisterObjectNode(ObjNode* theNode, const char* objectType, PangeaScriptCapabilityLevel capabilityLevel, const char* const* tags, int tagCount);
void OttoScript_UnregisterObjectNode(ObjNode* theNode);
void OttoScript_RunObjectFrame(ObjNode* theNode, Boolean usesGlobals);
void OttoScript_ApplyObjectVisualOffset(ObjNode* theNode);
void OttoScript_OnCustomTrigger(ObjNode* triggerNode, ObjNode* whoNode, Byte sideBits);
void OttoScript_OnAnimationEvent(ObjNode* node);

#endif

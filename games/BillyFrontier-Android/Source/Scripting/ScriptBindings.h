#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void BillyScript_Init(void);
void BillyScript_Shutdown(void);
void BillyScript_LoadAreaConfig(int areaNum);
void BillyScript_OnAreaLoad(int areaNum);
void BillyScript_OnAreaStart(int areaNum);
void BillyScript_OnAreaFrame(int areaNum, unsigned int frameNum, float deltaSeconds, float areaTimeSeconds);
void BillyScript_OnAreaComplete(int areaNum);
void BillyScript_OnAreaUnload(int areaNum);
int BillyScript_RemapTerrainItemType(int areaNum, int itemType);
Boolean BillyScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int areaNum, int originalType, int remappedType, float x, float z);
Boolean BillyScript_OnSplineItem(SplineItemType* itemPtr, int areaNum, int splineNum);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
void BillyScript_ResetObjectRegistry(void);
void BillyScript_RegisterPlayerObject(ObjNode* playerObj);
void BillyScript_UnregisterPlayerObject(ObjNode* playerObj);
void BillyScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void BillyScript_UnregisterObject(ObjNode* obj);
void BillyScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void BillyScript_ApplyObjectScripting(ObjNode* obj);
// Utility for MoveObjects loop
void BillyScript_RunObjectFrame(ObjNode* obj);

#endif

#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void BugdomScript_Init(void);
void BugdomScript_Shutdown(void);
void BugdomScript_LoadLevelConfig(int levelNum);
void BugdomScript_OnLevelLoad(int levelNum);
void BugdomScript_OnLevelStart(int levelNum);
void BugdomScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void BugdomScript_OnLevelComplete(int levelNum);
void BugdomScript_OnLevelUnload(int levelNum);
int BugdomScript_RemapTerrainItemType(int levelNum, int itemType);
Boolean BugdomScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean BugdomScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
void BugdomScript_ResetObjectRegistry(void);
void BugdomScript_RegisterPlayerObject(ObjNode* playerObj);
void BugdomScript_UnregisterPlayerObject(ObjNode* playerObj);
void BugdomScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void BugdomScript_ApplyObjectScripting(ObjNode* obj);
void BugdomScript_RunObjectFrame(ObjNode* obj);

#endif

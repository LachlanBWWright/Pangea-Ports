#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void BugdomScript_Init(void);
void BugdomScript_Shutdown(void);
void BugdomScript_LoadLevelConfig(int levelNum);
void BugdomScript_OnLevelLoad(int levelNum);
void BugdomScript_OnLevelStart(int levelNum);
void BugdomScript_OnCheckpointReset(void);
void BugdomScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void BugdomScript_OnLevelComplete(int levelNum);
void BugdomScript_OnLevelUnload(int levelNum);
int BugdomScript_RemapTerrainItemType(int levelNum, int itemType);
Boolean BugdomScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean BugdomScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);
Boolean BugdomScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z);
Boolean BugdomScript_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
Boolean BugdomScript_OnDamage(ObjNode* source, float damage, int cause, float* outDamage);
void BugdomScript_OnDamageApplied(float damage, int cause);
void BugdomScript_OnPlayerSpawn(ObjNode* playerObj);
void BugdomScript_OnPlayerRespawn(ObjNode* playerObj);
void BugdomScript_OnDeath(int eventValue);
void BugdomScript_ResetObjectRegistry(void);
void BugdomScript_RegisterPlayerObject(ObjNode* playerObj);
void BugdomScript_UnregisterPlayerObject(ObjNode* playerObj);
void BugdomScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void BugdomScript_UnregisterObject(ObjNode* obj);
void BugdomScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void BugdomScript_ApplyObjectScripting(ObjNode* obj);
void BugdomScript_RunObjectFrame(ObjNode* obj);
void BugdomScript_OnCustomTrigger(ObjNode* triggerNode, ObjNode* whoNode, Byte sideBits);
void BugdomScript_OnAnimationEvent(ObjNode* obj, int eventValue);
void BugdomScript_OnObjectDeleted(ObjNode* obj);

#endif

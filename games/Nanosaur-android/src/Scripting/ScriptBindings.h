#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void NanosaurScript_Init(void);
void NanosaurScript_Shutdown(void);
void NanosaurScript_LoadLevelConfig(int levelNum);
void NanosaurScript_OnLevelLoad(int levelNum);
void NanosaurScript_OnLevelStart(int levelNum);
void NanosaurScript_OnCheckpointReset(void);
void NanosaurScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void NanosaurScript_OnLevelComplete(int levelNum);
void NanosaurScript_OnLevelUnload(int levelNum);
int NanosaurScript_RemapTerrainItemType(int levelNum, int itemType);
Boolean NanosaurScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
void NanosaurScript_ResetObjectRegistry(void);
void NanosaurScript_RegisterPlayerObject(ObjNode* playerObj);
void NanosaurScript_UnregisterPlayerObject(ObjNode* playerObj);
void NanosaurScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void NanosaurScript_UnregisterObject(ObjNode* obj);
void NanosaurScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void NanosaurScript_ApplyObjectScripting(ObjNode* obj);
void NanosaurScript_RunObjectFrame(ObjNode* obj);
void NanosaurScript_OnObjectDeleted(ObjNode* obj);
void NanosaurScript_OnCustomTrigger(ObjNode* triggerNode, ObjNode* whoNode, Byte sideBits);
void NanosaurScript_OnAnimationEvent(ObjNode* obj, int eventValue);
Boolean NanosaurScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z);
Boolean NanosaurScript_OnDamage(ObjNode* source, float damage, int cause, float* outDamage);
void NanosaurScript_OnDamageApplied(float damage, int cause);
void NanosaurScript_OnPlayerSpawn(ObjNode* playerObj);
void NanosaurScript_OnPlayerRespawn(ObjNode* playerObj);
void NanosaurScript_OnDeath(int eventValue);

#endif

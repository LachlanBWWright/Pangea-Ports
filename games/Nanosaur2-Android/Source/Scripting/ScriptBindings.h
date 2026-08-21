#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void Nanosaur2Script_Init(void);
void Nanosaur2Script_Shutdown(void);
void Nanosaur2Script_LoadLevelConfig(int levelNum);
void Nanosaur2Script_OnLevelLoad(int levelNum);
void Nanosaur2Script_OnLevelStart(int levelNum);
void Nanosaur2Script_OnCheckpointReset(void);
void Nanosaur2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void Nanosaur2Script_OnLevelComplete(int levelNum);
void Nanosaur2Script_OnLevelUnload(int levelNum);
int Nanosaur2Script_RemapTerrainItemType(int levelNum, int itemType);
Boolean Nanosaur2Script_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean Nanosaur2Script_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
void Nanosaur2Script_ResetObjectRegistry(void);
void Nanosaur2Script_RegisterPlayerObject(ObjNode* playerObj);
void Nanosaur2Script_UnregisterPlayerObject(ObjNode* playerObj);
void Nanosaur2Script_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void Nanosaur2Script_UnregisterObject(ObjNode* obj);
void Nanosaur2Script_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void Nanosaur2Script_ApplyObjectScripting(ObjNode* obj);
void Nanosaur2Script_RunObjectFrame(ObjNode* obj);
void Nanosaur2Script_OnAnimationEvent(ObjNode* obj, int eventValue);
void Nanosaur2Script_OnObjectDeleted(ObjNode* obj);
Boolean Nanosaur2Script_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z);
Boolean Nanosaur2Script_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex);
Boolean Nanosaur2Script_OnDamage(short playerNum, ObjNode* source, float damage, int cause, float* outDamage);
void Nanosaur2Script_OnDamageApplied(short playerNum, float damage, int cause);
void Nanosaur2Script_OnPlayerSpawn(ObjNode* playerObj);
void Nanosaur2Script_OnPlayerRespawn(ObjNode* playerObj);
void Nanosaur2Script_OnDeath(short playerNum, int eventValue);

#endif

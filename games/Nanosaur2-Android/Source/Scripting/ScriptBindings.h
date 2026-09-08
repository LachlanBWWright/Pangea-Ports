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
int Nanosaur2Script_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z);
int Nanosaur2Script_ProbeSplineReplacementJS(int splineNum, int itemIndex, int nativeType, float placement);
int Nanosaur2Script_ProbeFirstSplineJS(void);
int Nanosaur2Script_ProbeFirstSplineReplacementJS(void);
int Nanosaur2Script_ProbeCheckpointResetJS(void);
int Nanosaur2Script_ProbeDeathRespawnJS(void);
int Nanosaur2Script_ProbeObjectiveCompletionJS(void);
int Nanosaur2Script_ProbeSaveLoadJS(int saveSlot);
int Nanosaur2Script_ProbePowerupPickupJS(int pickupKind);
Boolean Nanosaur2Script_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex);
Boolean Nanosaur2Script_OnDamage(short playerNum, ObjNode* source, float damage, int cause, float* outDamage);
Boolean Nanosaur2Script_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget);
Boolean Nanosaur2Script_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId);
void Nanosaur2Script_OnDamageApplied(short playerNum, float damage, int cause);
void Nanosaur2Script_OnPlayerSpawn(ObjNode* playerObj);
void Nanosaur2Script_OnPlayerRespawn(ObjNode* playerObj);
void Nanosaur2Script_OnDeath(short playerNum, int eventValue);
void Nanosaur2Script_OnCheckpointReached(short playerNum, short checkpointNum);
void Nanosaur2Script_OnLapComplete(short playerNum, short lapNum);
void Nanosaur2Script_OnRaceFinish(short playerNum, int placement);
void Nanosaur2Script_OnObjectiveComplete(short playerNum, int outcome);

#endif

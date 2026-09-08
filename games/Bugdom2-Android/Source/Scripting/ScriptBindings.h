#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "pangea_script.h"
#include "structs.h"

void Bugdom2Script_Init(void);
void Bugdom2Script_Shutdown(void);
void Bugdom2Script_LoadLevelConfig(int levelNum);
void Bugdom2Script_LoadLevelAssetDependencies(int levelNum);
void Bugdom2Script_OnLevelLoad(int levelNum);
void Bugdom2Script_OnLevelStart(int levelNum);
void Bugdom2Script_OnCheckpointReset(void);
void Bugdom2Script_RequestCheckpointResetProbe(void);
void Bugdom2Script_ProcessCheckpointResetProbe(void);
void Bugdom2Script_OnCheckpointReached(int checkpointNum);
void Bugdom2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void Bugdom2Script_OnLevelComplete(int levelNum);
void Bugdom2Script_OnLevelUnload(int levelNum);
int Bugdom2Script_RemapTerrainItemType(int levelNum, int itemType);
Boolean Bugdom2Script_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean Bugdom2Script_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
void Bugdom2Script_ResetObjectRegistry(void);
void Bugdom2Script_RegisterPlayerObject(ObjNode* playerObj);
void Bugdom2Script_UnregisterPlayerObject(ObjNode* playerObj);
void Bugdom2Script_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void Bugdom2Script_UnregisterObject(ObjNode* obj);
void Bugdom2Script_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void Bugdom2Script_ApplyObjectScripting(ObjNode* obj);
void Bugdom2Script_RunObjectFrame(ObjNode* obj);
void Bugdom2Script_OnAnimationEvent(ObjNode* obj, int eventValue);
Boolean Bugdom2Script_OnDamage(ObjNode* source, float damage, int cause, float* outDamage);
Boolean Bugdom2Script_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId);
void Bugdom2Script_OnMouseRescued(ObjNode* mouse, ObjNode* player, Boolean drowning);
void Bugdom2Script_OnObjectiveComplete(int playerNum, int outcome);
void Bugdom2Script_OnDamageApplied(float damage, int cause);
Boolean Bugdom2Script_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget);
void Bugdom2Script_OnDeath(int deathType);
void Bugdom2Script_OnPlayerSpawn(ObjNode* playerObj);
void Bugdom2Script_OnPlayerRespawn(ObjNode* playerObj);
void Bugdom2Script_OnObjectDeleted(ObjNode* obj);
Boolean Bugdom2Script_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z);
int Bugdom2Script_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z);
int Bugdom2Script_ProbeFirstSplineJS(void);
int Bugdom2Script_ProbeFirstSplineReplacementJS(void);
Boolean Bugdom2Script_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex);
int Bugdom2Script_ProbeSplineReplacementJS(int splineNum, int itemIndex, int nativeType, float placement);
int Bugdom2Script_SelectSplineItemForReplacementJS(void);
int Bugdom2Script_GetSelectedSplineItemFieldJS(int field);
float Bugdom2Script_GetSelectedSplinePlacementJS(void);
int Bugdom2Script_ProbeDamageJS(float damage);
int Bugdom2Script_ProbeBuddyLaunchJS(void);
int Bugdom2Script_ProbeSaveLoadJS(int saveSlot);

#endif

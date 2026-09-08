#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void OttoScript_Init(void);
void OttoScript_Shutdown(void);
void OttoScript_LoadLevelConfig(int levelNum);
void OttoScript_OnLevelLoad(int levelNum);
void OttoScript_OnLevelStart(int levelNum);
void OttoScript_OnCheckpointReset(void);
void OttoScript_OnCheckpointReached(int checkpointNum);
void OttoScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void OttoScript_OnLevelComplete(int levelNum);
void OttoScript_OnLevelUnload(int levelNum);
void OttoScript_OnSave(int saveSlot);
void OttoScript_OnLoad(int saveSlot);
int OttoScript_RemapTerrainItemType(int levelNum, int itemType);
Boolean OttoScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean OttoScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z);
int OttoScript_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z);
int OttoScript_ProbeSaveLoadJS(int saveSlot);
int OttoScript_ProbeWeaponHitJS(void);
int OttoScript_ProbeProjectileWeaponFamiliesJS(void);
int OttoScript_ProbeDartWeaponJS(void);
int OttoScript_ProbeSuperNovaWeaponJS(void);
int OttoScript_ProbePunchWeaponJS(void);
int OttoScript_GetLastWeaponHitScoreDelta(void);
Boolean OttoScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);
Boolean OttoScript_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex);
int OttoScript_ProbeSplineReplacementJS(int splineNum, int itemIndex, int nativeType, float placement);
int OttoScript_ProbeCheckpointResetJS(void);
void OttoScript_RegisterHuman(ObjNode* human);
void OttoScript_UnregisterHuman(ObjNode* human);
void OttoScript_RegisterPlayerObject(ObjNode* player);
void OttoScript_RunHumanObjectFrame(ObjNode* human, Boolean usesGlobals);
void OttoScript_ApplyHumanVisualOffset(ObjNode* human);

void OttoScript_RegisterObjectNode(ObjNode* theNode, const char* objectType, PangeaScriptCapabilityLevel capabilityLevel, const char* const* tags, int tagCount);
void OttoScript_UnregisterObjectNode(ObjNode* theNode);
void OttoScript_OnObjectDeleted(ObjNode* theNode);
void OttoScript_RunObjectFrame(ObjNode* theNode, Boolean usesGlobals);
void OttoScript_ApplyObjectVisualOffset(ObjNode* theNode);
void OttoScript_OnCustomTrigger(ObjNode* triggerNode, ObjNode* whoNode, Byte sideBits);
void OttoScript_OnAnimationEvent(ObjNode* node, int eventValue);
Boolean OttoScript_OnDamage(ObjNode* source, float damage, int cause, float* outDamage);
Boolean OttoScript_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget);
Boolean OttoScript_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId);
void OttoScript_OnDamageApplied(float damage, int cause);
void OttoScript_OnPlayerSpawn(ObjNode* player);
void OttoScript_OnPlayerRespawn(ObjNode* player);
void OttoScript_OnDeath(int eventValue);

#endif

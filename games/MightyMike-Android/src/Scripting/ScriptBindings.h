#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include <Pomme.h>
#include "structures.h"
#include "pangea_script.h"

void MikeScript_Init(void);
void MikeScript_Shutdown(void);
void MikeScript_LoadAreaConfig(int sceneNum, int areaNum);
void MikeScript_OnAreaLoad(int sceneNum, int areaNum);
void MikeScript_OnAreaStart(int sceneNum, int areaNum);
void MikeScript_OnAreaFrame(int sceneNum, int areaNum, unsigned int frameNum, float deltaSeconds);
void MikeScript_OnAreaComplete(int sceneNum, int areaNum);
void MikeScript_OnAreaUnload(int sceneNum, int areaNum);
// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
void MikeScript_ResetObjectRegistry(void);
void MikeScript_RegisterPlayerObject(ObjNode* playerObj);
void MikeScript_OnPlayerSpawn(ObjNode* playerObj);
void MikeScript_OnPlayerRespawn(ObjNode* playerObj);
void MikeScript_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, float amount, const char* pickupId);
Boolean MikeScript_IsInvulnerable(void);
Boolean MikeScript_OnDamage(float damage, float* outDamage);
Boolean MikeScript_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget);
void MikeScript_OnDamageApplied(float damage);
void MikeScript_OnDeath(ObjNode* playerObj, int eventValue);
void MikeScript_UnregisterPlayerObject(ObjNode* playerObj);
void MikeScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void MikeScript_UnregisterObject(ObjNode* obj);
void MikeScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void MikeScript_RunObjectFrame(ObjNode* obj);
void MikeScript_OnObjectDeleted(ObjNode* obj);
int MikeScript_RemapMapItemType(int sceneNum, int areaNum, int itemType);
Boolean MikeScript_OnMapItem(ObjectEntryType* itemPtr, int sceneNum, int areaNum, int itemType);
Boolean MikeScript_TryReplaceMapItem(ObjectEntryType* itemPtr, int itemIndex, int nativeType);
int MikeScript_SelectMapItemForReplacementJS(void);
int MikeScript_GetSelectedMapItemFieldJS(int field);
int MikeScript_ProbeSelectedMapReplacementJS(void);
int MikeScript_ProbeSaveLoadJS(int saveSlot);
void MikeScript_OnCustomTrigger(ObjNode* trigger, Byte sideBits);

#endif

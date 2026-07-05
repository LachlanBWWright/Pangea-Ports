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
void MikeScript_UnregisterPlayerObject(ObjNode* playerObj);
void MikeScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void MikeScript_UnregisterObject(ObjNode* obj);
void MikeScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void MikeScript_RunObjectFrame(ObjNode* obj);
int MikeScript_RemapMapItemType(int sceneNum, int areaNum, int itemType);
Boolean MikeScript_OnMapItem(ObjectEntryType* itemPtr, int sceneNum, int areaNum, int itemType);
Boolean MikeScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid);
Boolean MikeScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits);
Boolean MikeScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount);
Boolean MikeScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage);
Boolean MikeScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage);
Boolean MikeScript_OnPlayerDamage(float* ioDamage, const char* damageId, int damageType);

#endif

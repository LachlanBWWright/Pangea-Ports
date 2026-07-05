#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void NanosaurScript_Init(void);
void NanosaurScript_Shutdown(void);
void NanosaurScript_LoadLevelConfig(int levelNum);
void NanosaurScript_OnLevelLoad(int levelNum);
void NanosaurScript_OnLevelStart(int levelNum);
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
Boolean NanosaurScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount);
Boolean NanosaurScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float damage);
Boolean NanosaurScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage);
Boolean NanosaurScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid);
Boolean NanosaurScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits);
Boolean NanosaurScript_OnPlayerDamage(ObjNode* playerObj, float* ioDamage);
void NanosaurScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void NanosaurScript_ApplyObjectScripting(ObjNode* obj);
void NanosaurScript_RunObjectFrame(ObjNode* obj);

#endif

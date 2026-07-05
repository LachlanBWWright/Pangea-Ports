#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void BillyScript_Init(void);
void BillyScript_Shutdown(void);
void BillyScript_LoadAreaConfig(int areaNum);
void BillyScript_OnAreaLoad(int areaNum);
void BillyScript_OnAreaStart(int areaNum);
void BillyScript_OnAreaFrame(int areaNum, unsigned int frameNum, float deltaSeconds, float areaTimeSeconds);
void BillyScript_OnAreaComplete(int areaNum);
void BillyScript_OnAreaUnload(int areaNum);
int BillyScript_RemapTerrainItemType(int areaNum, int itemType);
Boolean BillyScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int areaNum, int originalType, int remappedType, float x, float z);
Boolean BillyScript_OnSplineItem(SplineItemType* itemPtr, int areaNum, int splineNum);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;

enum
{
	BILLY_SCRIPT_TRIGGER_GENERIC = 0,
	BILLY_SCRIPT_TRIGGER_PESO = 1,
	BILLY_SCRIPT_TRIGGER_FREE_LIFE = 2,
	BILLY_SCRIPT_TRIGGER_BOOST = 3,
	BILLY_SCRIPT_TRIGGER_EXPLOSIVE_ITEM = 4,
};
void BillyScript_ResetObjectRegistry(void);
void BillyScript_RegisterPlayerObject(ObjNode* playerObj);
void BillyScript_UnregisterPlayerObject(ObjNode* playerObj);
void BillyScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void BillyScript_UnregisterObject(ObjNode* obj);
void BillyScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void BillyScript_ApplyObjectScripting(ObjNode* obj);
// Utility for MoveObjects loop
void BillyScript_RunObjectFrame(ObjNode* obj);
Boolean BillyScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid);
Boolean BillyScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits);
Boolean BillyScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount);
Boolean BillyScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage);
Boolean BillyScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage);
Boolean BillyScript_OnPlayerDamage(ObjNode* sourceObj, float* ioDamage, const char* damageId, int damageType);

#endif

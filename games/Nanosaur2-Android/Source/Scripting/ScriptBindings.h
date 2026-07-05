#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

typedef struct ObjNode ObjNode;

enum
{
	NANOSAUR2_SCRIPT_TRIGGER_GENERIC = 0,
	NANOSAUR2_SCRIPT_TRIGGER_WEAPON_POW = 1,
	NANOSAUR2_SCRIPT_TRIGGER_HEALTH_POW = 2,
	NANOSAUR2_SCRIPT_TRIGGER_FUEL_POW = 3,
	NANOSAUR2_SCRIPT_TRIGGER_SHIELD_POW = 4,
	NANOSAUR2_SCRIPT_TRIGGER_FREE_LIFE_POW = 5,
	NANOSAUR2_SCRIPT_TRIGGER_EGG = 6,
	NANOSAUR2_SCRIPT_TRIGGER_MINE = 7,
	NANOSAUR2_SCRIPT_TRIGGER_ELECTRODE = 8,
	NANOSAUR2_SCRIPT_TRIGGER_DOOR_KEY = 9,
	NANOSAUR2_SCRIPT_TRIGGER_SMACKABLE = 10,
};

void Nanosaur2Script_Init(void);
void Nanosaur2Script_Shutdown(void);
void Nanosaur2Script_LoadLevelConfig(int levelNum);
void Nanosaur2Script_OnLevelLoad(int levelNum);
void Nanosaur2Script_OnLevelStart(int levelNum);
void Nanosaur2Script_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void Nanosaur2Script_OnLevelComplete(int levelNum);
void Nanosaur2Script_OnLevelUnload(int levelNum);
int Nanosaur2Script_RemapTerrainItemType(int levelNum, int itemType);
Boolean Nanosaur2Script_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean Nanosaur2Script_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);
Boolean Nanosaur2Script_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid);
Boolean Nanosaur2Script_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits);
Boolean Nanosaur2Script_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount);
Boolean Nanosaur2Script_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage);
Boolean Nanosaur2Script_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage);
Boolean Nanosaur2Script_OnPlayerDamage(short playerNum, float* ioDamage, Byte deathType, OGLPoint3D* where);

// --- Live-object scripting extension ---
void Nanosaur2Script_ResetObjectRegistry(void);
void Nanosaur2Script_RegisterPlayerObject(ObjNode* playerObj);
void Nanosaur2Script_UnregisterPlayerObject(ObjNode* playerObj);
void Nanosaur2Script_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void Nanosaur2Script_UnregisterObject(ObjNode* obj);
void Nanosaur2Script_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void Nanosaur2Script_ApplyObjectScripting(ObjNode* obj);
void Nanosaur2Script_RunObjectFrame(ObjNode* obj);

#endif

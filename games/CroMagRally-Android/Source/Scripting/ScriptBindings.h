#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void CroMagScript_Init(void);
void CroMagScript_Shutdown(void);
void CroMagScript_LoadTrackConfig(int trackNum);
void CroMagScript_OnRaceLoad(int trackNum);
void CroMagScript_OnRaceStart(int trackNum);
void CroMagScript_OnRaceFrame(int trackNum, unsigned int frameNum, float deltaSeconds, float raceTimeSeconds);
void CroMagScript_OnRaceComplete(int trackNum);
void CroMagScript_OnRaceUnload(int trackNum);
int CroMagScript_RemapTerrainItemType(int trackNum, int itemType);
Boolean CroMagScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int trackNum, int playerNum, int originalType, int remappedType, float x, float z);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
void CroMagScript_ResetObjectRegistry(void);
void CroMagScript_RegisterPlayerObject(ObjNode* playerObj);
void CroMagScript_UnregisterPlayerObject(ObjNode* playerObj);
void CroMagScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void CroMagScript_UnregisterObject(ObjNode* obj);
void CroMagScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void CroMagScript_ApplyObjectScripting(ObjNode* obj);
void CroMagScript_RunObjectFrame(ObjNode* obj);
Boolean CroMagScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid);
Boolean CroMagScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits);
Boolean CroMagScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount);
Boolean CroMagScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, short targetPlayerNum, const OGLPoint3D* position, float* ioDamage);
Boolean CroMagScript_OnPlayerDamage(short playerNum, float* ioDamage);


#endif

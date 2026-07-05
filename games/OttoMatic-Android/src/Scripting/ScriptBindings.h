#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void OttoScript_Init(void);
void OttoScript_Shutdown(void);
void OttoScript_LoadLevelConfig(int levelNum);
void OttoScript_OnLevelLoad(int levelNum);
void OttoScript_OnLevelStart(int levelNum);
void OttoScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void OttoScript_OnLevelComplete(int levelNum);
void OttoScript_OnLevelUnload(int levelNum);
int OttoScript_RemapTerrainItemType(int levelNum, int itemType);
Boolean OttoScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean OttoScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);
void OttoScript_RegisterHuman(ObjNode* human);
void OttoScript_UnregisterHuman(ObjNode* human);
void OttoScript_RunHumanObjectFrame(ObjNode* human, Boolean usesGlobals);
void OttoScript_ApplyHumanVisualOffset(ObjNode* human);

void OttoScript_RegisterObjectNode(ObjNode* theNode, PangeaScriptCapabilityLevel capabilityLevel, const char* const* tags, int tagCount);
void OttoScript_RegisterTaggedObjectNode(ObjNode* theNode, const char* nativeId, const char* category);
void OttoScript_UnregisterObjectNode(ObjNode* theNode);
Boolean OttoScript_OnTriggerEnter(ObjNode* triggerObj, ObjNode* otherObj, const char* triggerId, int triggerType, unsigned int sideBits, Boolean* outSolid);
Boolean OttoScript_OnObjectCollision(ObjNode* selfObj, ObjNode* otherObj, const char* collisionId, int collisionType, unsigned int sideBits);
Boolean OttoScript_OnPickupCollected(ObjNode* pickupObj, ObjNode* playerObj, const char* pickupId, int pickupType, int amount);
Boolean OttoScript_OnWeaponHit(ObjNode* weaponObj, ObjNode* targetObj, const char* weaponId, int weaponType, float* ioDamage);
Boolean OttoScript_OnObjectDamage(ObjNode* sourceObj, ObjNode* targetObj, const char* damageId, int damageType, float* ioDamage);
Boolean OttoScript_OnPlayerDamage(float* ioDamage, Byte deathType, ObjNode* sourceObj);
void OttoScript_RunObjectFrame(ObjNode* theNode, Boolean usesGlobals);
void OttoScript_ApplyObjectVisualOffset(ObjNode* theNode);

#endif

#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "pangea_script.h"

void BugdomScript_Init(void);
void BugdomScript_Shutdown(void);
void BugdomScript_LoadLevelConfig(int levelNum);
void BugdomScript_OnLevelLoad(int levelNum);
void BugdomScript_OnLevelStart(int levelNum);
void BugdomScript_OnCheckpointReset(void);
void BugdomScript_OnFrame(int levelNum, unsigned int frameNum, float deltaSeconds, float levelTimeSeconds);
void BugdomScript_OnLevelComplete(int levelNum);
void BugdomScript_OnObjectiveComplete(int playerNum, int outcome);
void BugdomScript_OnLevelUnload(int levelNum);
void BugdomScript_OnLevelCleanup(void);
int BugdomScript_RemapTerrainItemType(int levelNum, int itemType);
Boolean BugdomScript_OnTerrainItem(TerrainItemEntryType* itemPtr, int levelNum, int originalType, int remappedType, float x, float z);
Boolean BugdomScript_OnSplineItem(SplineItemType* itemPtr, int levelNum, int splineNum);
Boolean BugdomScript_TryReplaceTerrainItem(TerrainItemEntryType* itemPtr, int itemIndex, int nativeType, float x, float z);
int BugdomScript_ProbeTerrainReplacementJS(int itemIndex, int nativeType, float x, float z);
int BugdomScript_ProbeCheckpointResetJS(void);
int BugdomScript_ProbeSaveLoadJS(int saveSlot);
Boolean BugdomScript_TryReplaceSplineItem(SplineItemType* itemPtr, int splineNum, int itemIndex);

// --- Live-object scripting extension ---
typedef struct ObjNode ObjNode;
Boolean BugdomScript_OnDamage(ObjNode* source, float damage, int cause, float* outDamage);
Boolean BugdomScript_OnPickupCollected(ObjNode* pickup, ObjNode* player, int pickupType, int pickupVariant, float amount, const char* pickupId);
Boolean BugdomScript_OnWeaponHit(ObjNode* weapon, ObjNode* target, float damage, float* outDamage, Boolean* outDestroyTarget);
void BugdomScript_OnDamageApplied(float damage, int cause);
void BugdomScript_OnPlayerSpawn(ObjNode* playerObj);
void BugdomScript_OnPlayerRespawn(ObjNode* playerObj);
void BugdomScript_OnCheckpointReached(int checkpointNum);
void BugdomScript_OnDeath(int eventValue);
void BugdomScript_ResetObjectRegistry(void);
void BugdomScript_RegisterPlayerObject(ObjNode* playerObj);
void BugdomScript_UnregisterPlayerObject(ObjNode* playerObj);
void BugdomScript_RegisterObject(ObjNode* obj, const char* nativeId, const char* category);
void BugdomScript_UnregisterObject(ObjNode* obj);
void BugdomScript_CacheFrameContext(const PangeaScriptFrameContext* ctx);
void BugdomScript_ApplyObjectScripting(ObjNode* obj);
void BugdomScript_RunObjectFrame(ObjNode* obj);
bool BugdomScript_ObjectStillAlive(const ObjNode* object, PangeaScriptObjectHandle handle);
const PangeaScriptFrameContext* BugdomScript_GetFrameContext(void);
Boolean BugdomScript_ShouldRunNativeMovement(const ObjNode* obj);
bool BugdomScript_SetObjectActive(void* nativeObject, bool active);
bool BugdomScript_SetObjectPosition(void* nativeObject, const PangeaScriptVector3* position);
bool BugdomScript_SetObjectCollisionEnabled(void* nativeObject, bool enabled);
bool BugdomScript_GetObjectBounds(void* nativeObject, PangeaScriptBounds* output);
extern const PangeaScriptGameplayOps kBugdomGameplayOps;
extern const PangeaScriptStateField kBugdomStateFields[15];
extern const int kBugdomStateFieldCount;
bool BugdomScript_GetState(int player, const char* key, PangeaScriptStateValue* output);
PangeaScriptStatus BugdomScript_SetState(int player, const char* key, const PangeaScriptStateValue* value);
PangeaScriptStatus BugdomScript_Complete(int player, bool success);
void BugdomScript_CaptureGameplayInput(void);
void BugdomScript_ResetGameplayInput(void);
void BugdomScript_OnCustomTrigger(ObjNode* triggerNode, ObjNode* whoNode, Byte sideBits);
void BugdomScript_OnAnimationEvent(ObjNode* obj, int eventValue);
void BugdomScript_OnObjectDeleted(ObjNode* obj);
void BugdomScript_ReleaseRide(void);
void BugdomScript_ClearRelationships(ObjNode* object);
bool BugdomScript_CommandBuddy(ObjNode* buddy, bool attack);
PangeaScriptStatus BugdomScript_SetPlayerForm(int playerNum, PangeaScriptPlayerForm form);
PangeaScriptStatus BugdomScript_RunOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output);
bool BugdomScript_GetRelations(int player, PangeaScriptPlayerRelations* output);
void BugdomScript_SyncRide(void);

#endif

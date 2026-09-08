#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pangea_script_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PANGEA_SCRIPT_CONTRACT_VERSION 1
#define PANGEA_SCRIPT_API_VERSION 1
#define PANGEA_SCRIPT_COMMAND_ID_CAPACITY 64
#define PANGEA_SCRIPT_COMMAND_PHASE_CAPACITY 24
#define PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY 256
#define PANGEA_SCRIPT_LIFECYCLE_EVENT_ID_CAPACITY 32
#define PANGEA_SCRIPT_LIFECYCLE_TRACE_CAPACITY 4096
#define PANGEA_SCRIPT_DEFERRED_ACTION_CAPACITY 64

typedef enum PangeaScriptStatus
{
	PANGEA_SCRIPT_OK = 0,
	PANGEA_SCRIPT_NOT_ENABLED,
	PANGEA_SCRIPT_FILE_NOT_FOUND,
	PANGEA_SCRIPT_PARSE_ERROR,
	PANGEA_SCRIPT_RUNTIME_ERROR,
	PANGEA_SCRIPT_BAD_ARGUMENT,
	PANGEA_SCRIPT_BUDGET_EXCEEDED,
	PANGEA_SCRIPT_INCOMPATIBLE_ITEM,
	PANGEA_SCRIPT_CONFIG_ERROR,
} PangeaScriptStatus;

typedef enum PangeaScriptHook
{
	PANGEA_SCRIPT_HOOK_GAME_START = 0,
	PANGEA_SCRIPT_HOOK_LEVEL_LOAD,
	PANGEA_SCRIPT_HOOK_LEVEL_START,
	PANGEA_SCRIPT_HOOK_FRAME,
	PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE,
	PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD,
	PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN,
	PANGEA_SCRIPT_HOOK_TERRAIN_ITEM,
	PANGEA_SCRIPT_HOOK_SPLINE_ITEM,
	PANGEA_SCRIPT_HOOK_MAP_ITEM,
	PANGEA_SCRIPT_HOOK_OBJECT_FRAME,
	PANGEA_SCRIPT_HOOK_SAVE,
	PANGEA_SCRIPT_HOOK_LOAD,
} PangeaScriptHook;

typedef struct PangeaScriptVector3
{
	float x;
	float y;
	float z;
} PangeaScriptVector3;

typedef struct PangeaScriptObjectHandle
{
	int id;
	uint32_t generation;
} PangeaScriptObjectHandle;

#define PANGEA_SCRIPT_PLAYER_INVENTORY_CAPACITY 16
#define PANGEA_SCRIPT_PLAYER_KEY_CAPACITY 8
#define PANGEA_SCRIPT_PLAYER_EGG_CAPACITY 8

typedef struct PangeaScriptPlayerInventoryEntry
{
	int type;
	int quantity;
} PangeaScriptPlayerInventoryEntry;

typedef enum PangeaScriptPlayerForm
{
	PANGEA_SCRIPT_PLAYER_FORM_BUG = 0,
	PANGEA_SCRIPT_PLAYER_FORM_BALL = 1,
} PangeaScriptPlayerForm;

typedef struct PangeaScriptPlayerSnapshot
{
	PangeaScriptVector3 position;
	PangeaScriptVector3 velocity;
	bool hasVelocity;
	bool collisionEnabled;
	bool hasCollisionEnabled;
	PangeaScriptVector3 rotation;
	bool hasRotation;
	PangeaScriptVector3 aim;
	bool hasAimState;
	float health;
	bool hasHealth;
	float fuel;
	bool hasFuelState;
	int64_t score;
	bool hasScore;
	int coinCount;
	bool hasCoinState;
	int pesoCount;
	bool hasPesoState;
	int lives;
	bool hasLives;
	int activeWeapon;
	bool hasWeaponState;
	int weaponCount;
	PangeaScriptPlayerInventoryEntry weapons[PANGEA_SCRIPT_PLAYER_INVENTORY_CAPACITY];
	int keyCount;
	int keys[PANGEA_SCRIPT_PLAYER_KEY_CAPACITY];
	bool hasKeyState;
	int greenCloverCount;
	int blueCloverCount;
	int goldCloverCount;
	bool hasCollectibleState;
	int tokenCount;
	bool hasTokenState;
	bool shieldActive;
	bool hasShieldState;
	PangeaScriptPlayerForm form;
	bool hasForm;
	int miceRescued;
	bool hasMiceState;
	int miceTotal;
	int drowningMiceRescued;
	int drowningMiceRequired;
	int childObjectCount;
	bool hasChildObjectState;
	int eggCount;
	int eggs[PANGEA_SCRIPT_PLAYER_EGG_CAPACITY];
	int eggRequired[PANGEA_SCRIPT_PLAYER_EGG_CAPACITY];
	bool hasEggState;
	int lapNum;
	int checkpointNum;
	int placement;
	bool raceComplete;
	bool hasRaceState;
	int vehicleType;
	float vehicleMaxSpeed;
	float vehicleAcceleration;
	float vehicleTraction;
	float vehicleSuspension;
	bool hasVehicleState;
	int team;
	bool hasTeamState;
	bool carryingFlag;
	int captureScore;
	bool hasCaptureState;
	int sceneNum;
	int areaNum;
	bool areaComplete;
	bool hasLevelFlowState;
	PangeaScriptVector3 camera;
	bool hasCameraState;
	bool active;
} PangeaScriptPlayerSnapshot;

typedef struct PangeaScriptGameCapabilities
{
	bool terrainItems;
	bool splineItems;
	bool mapItems;
	bool pickupScoreEffects;
	bool objectCollision;
	bool playerScore;
	bool playerLives;
	bool playerInventory;
	bool weaponScoreEffects;
	bool playerForm;
} PangeaScriptGameCapabilities;

typedef struct PangeaScriptGameInfo
{
	const char* gameId;
	const char* gameName;
	PangeaScriptStatus (*spawnNative)(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle);
	PangeaScriptStatus (*spawnScripted)(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle);
	int (*getPlayerCount)(void);
	bool (*getPlayer)(int playerNum, PangeaScriptPlayerSnapshot* outPlayer);
	PangeaScriptStatus (*setPlayerHealth)(int playerNum, float health);
	PangeaScriptStatus (*setPlayerLives)(int playerNum, int lives);
	PangeaScriptStatus (*setPlayerScore)(int playerNum, int64_t score);
	PangeaScriptStatus (*setPlayerWeaponQuantity)(int playerNum, int weaponType, int quantity);
	PangeaScriptStatus (*setPlayerKey)(int playerNum, int keyId, bool enabled);
	PangeaScriptStatus (*setPlayerCloverCount)(int playerNum, int color, int count);
	PangeaScriptStatus (*setPlayerShieldActive)(int playerNum, bool active);
	PangeaScriptStatus (*setPlayerInvulnerable)(int playerNum, float durationSeconds);
	PangeaScriptStatus (*setPlayerPosition)(int playerNum, const PangeaScriptVector3* position);
	PangeaScriptStatus (*setPlayerVelocity)(int playerNum, const PangeaScriptVector3* velocity);
	PangeaScriptStatus (*setPlayerForm)(int playerNum, PangeaScriptPlayerForm form);
	PangeaScriptStatus (*loadPersistent)(const char* key, unsigned char* outData, int capacity, int* outSize);
	PangeaScriptStatus (*savePersistent)(const char* key, const unsigned char* data, int size);
	PangeaScriptGameCapabilities capabilities;
} PangeaScriptGameInfo;

typedef struct PangeaScriptLevelContext
{
	int levelNum;
	const char* levelName;
	const char* mode;
	bool networked;
	const char* trackName;
	const char* sceneName;
	const char* areaName;
	const char* playerMode;
	int modePhase;
	int modeWave;
	float modeTimer;
	bool hasModeState;
} PangeaScriptLevelContext;

typedef struct PangeaScriptFrameContext
{
	int levelNum;
	unsigned int frameNum;
	float deltaSeconds;
	float levelTimeSeconds;
	const char* mode;
	bool networked;
	const char* trackName;
	const char* sceneName;
	const char* areaName;
	const char* levelName;
	const char* playerMode;
	int modePhase;
	int modeWave;
	float modeTimer;
	bool hasModeState;
} PangeaScriptFrameContext;

typedef struct PangeaScriptTerrainItemContext
{
	int levelNum;
	int itemType;
	int remappedItemType;
	int playerNum;
	bool networked;
	float x;
	float z;
	const char* mode;
	const char* trackName;
	unsigned int flags;
	const unsigned char* params;
	int paramCount;
	bool handled;
	bool markInUse;
} PangeaScriptTerrainItemContext;

typedef struct PangeaScriptSplineItemContext
{
	int levelNum;
	int itemType;
	int splineNum;
	float placement;
	const char* mode;
	bool networked;
	const char* trackName;
	const unsigned char* params;
	int paramCount;
	bool handled;
	bool markInUse;
} PangeaScriptSplineItemContext;

typedef struct PangeaScriptMapItemContext
{
	int levelNum;
	int sceneNum;
	int areaNum;
	const char* sceneName;
	const char* areaName;
	int itemType;
	float x;
	float y;
	const unsigned char* params;
	int paramCount;
	bool handled;
	bool markInUse;
} PangeaScriptMapItemContext;

typedef struct PangeaScriptObjectOps
{
	bool (*getPosition)(void* nativeObject, PangeaScriptVector3* outPosition);
	bool (*getVelocity)(void* nativeObject, PangeaScriptVector3* outVelocity);
	bool (*getRotation)(void* nativeObject, PangeaScriptVector3* outRotation);
	bool (*getScale)(void* nativeObject, float* outScale);
	bool (*getAnimation)(void* nativeObject, int* outAnimation);
	bool (*getActive)(void* nativeObject, bool* outActive);
	bool (*getCollisionEnabled)(void* nativeObject, bool* outEnabled);
	bool (*setPosition)(void* nativeObject, const PangeaScriptVector3* position);
	bool (*setVelocity)(void* nativeObject, const PangeaScriptVector3* velocity);
	bool (*setRotation)(void* nativeObject, const PangeaScriptVector3* rotation);
	bool (*setScale)(void* nativeObject, float scale);
	bool (*setAnimation)(void* nativeObject, int animation, float speed, float blendSeconds);
	bool (*setAnimationNamed)(void* nativeObject, const char* animation, float speed, float blendSeconds);
	bool (*setCollisionEnabled)(void* nativeObject, bool enabled);
	bool (*setActive)(void* nativeObject, bool active);
	bool (*deleteObject)(void* nativeObject);
} PangeaScriptObjectOps;

typedef enum PangeaScriptCapabilityLevel
{
	PANGEA_SCRIPT_CAPABILITY_DEFAULT = 0,
	PANGEA_SCRIPT_CAPABILITY_UNSUPPORTED,
	PANGEA_SCRIPT_CAPABILITY_READ_ONLY,
	PANGEA_SCRIPT_CAPABILITY_BASE,
	PANGEA_SCRIPT_CAPABILITY_FULL
} PangeaScriptCapabilityLevel;

typedef struct PangeaScriptCommandDescriptor
{
	const char* id;
	const char* capability;
	const char* authority;
	const char* applicationPhase;
	const char* validation;
} PangeaScriptCommandDescriptor;

typedef struct PangeaScriptEventDescriptor
{
	const char* id;
	const char* applicationPhase;
	const char* payload;
	const char* result;
} PangeaScriptEventDescriptor;

typedef struct PangeaScriptObjectEventDescriptor
{
	const char* id;
	const char* handler;
	const char* applicationPhase;
	const char* cleanup;
	const char* statePolicy;
	bool invalidatesHandle;
} PangeaScriptObjectEventDescriptor;

typedef struct PangeaScriptCommandTrace
{
	uint32_t commandCount;
	uint32_t hash;
	uint32_t entryCount;
	bool overflow;
} PangeaScriptCommandTrace;

typedef struct PangeaScriptCommandTraceEntry
{
	char commandId[PANGEA_SCRIPT_COMMAND_ID_CAPACITY];
	char applicationPhase[PANGEA_SCRIPT_COMMAND_PHASE_CAPACITY];
	uint32_t order;
	PangeaScriptObjectHandle target;
	PangeaScriptStatus status;
} PangeaScriptCommandTraceEntry;

typedef struct PangeaScriptLifecycleTrace
{
	uint32_t eventCount;
	uint32_t entryCount;
	bool overflow;
} PangeaScriptLifecycleTrace;

typedef struct PangeaScriptLifecycleTraceEntry
{
	char eventId[PANGEA_SCRIPT_LIFECYCLE_EVENT_ID_CAPACITY];
	char applicationPhase[PANGEA_SCRIPT_COMMAND_PHASE_CAPACITY];
	uint32_t order;
	PangeaScriptObjectHandle target;
	PangeaScriptStatus status;
} PangeaScriptLifecycleTraceEntry;

typedef struct PangeaScriptCommandTraceComparison
{
	bool matches;
	uint32_t firstMismatchIndex;
	uint32_t expectedCommandCount;
	uint32_t actualCommandCount;
	uint32_t expectedHash;
	uint32_t actualHash;
	char expectedCommandId[PANGEA_SCRIPT_COMMAND_ID_CAPACITY];
	char actualCommandId[PANGEA_SCRIPT_COMMAND_ID_CAPACITY];
	PangeaScriptObjectHandle expectedTarget;
	PangeaScriptObjectHandle actualTarget;
	PangeaScriptStatus expectedStatus;
	PangeaScriptStatus actualStatus;
} PangeaScriptCommandTraceComparison;

typedef struct PangeaScriptLifecycleTraceComparison
{
	bool matches;
	uint32_t firstMismatchIndex;
	uint32_t expectedEventCount;
	uint32_t actualEventCount;
	char expectedEventId[PANGEA_SCRIPT_LIFECYCLE_EVENT_ID_CAPACITY];
	char actualEventId[PANGEA_SCRIPT_LIFECYCLE_EVENT_ID_CAPACITY];
	PangeaScriptObjectHandle expectedTarget;
	PangeaScriptObjectHandle actualTarget;
	PangeaScriptStatus expectedStatus;
	PangeaScriptStatus actualStatus;
} PangeaScriptLifecycleTraceComparison;

typedef struct PangeaScriptObjectRegistration
{
	void* nativeObject;
	const PangeaScriptObjectOps* ops;
	const char* objectType;
	const char* const* tags;
	int tagCount;
	PangeaScriptCapabilityLevel capabilityLevel;
} PangeaScriptObjectRegistration;

typedef enum PangeaScriptObjectSourceKind
{
	PANGEA_SCRIPT_SOURCE_NONE = 0,
	PANGEA_SCRIPT_SOURCE_TERRAIN,
	PANGEA_SCRIPT_SOURCE_SPLINE,
	PANGEA_SCRIPT_SOURCE_MAP
} PangeaScriptObjectSourceKind;

typedef struct PangeaScriptObjectSource
{
	PangeaScriptObjectSourceKind kind;
	int itemIndex;
	int nativeType;
	int splineNum;
	float x;
	float y;
	float z;
	float placement;
} PangeaScriptObjectSource;

typedef struct PangeaScriptObjectFrameContext
{
	int levelNum;
	unsigned int frameNum;
	float deltaSeconds;
	float levelTimeSeconds;
	PangeaScriptObjectHandle object;
	PangeaScriptVector3 position;
	const char* objectType;
	const char* const* tags;
	int tagCount;
	const char* event;
	bool hasEventValue;
	int eventValue;
	PangeaScriptObjectHandle other;
	bool hasOther;
	unsigned int sideBits;
} PangeaScriptObjectFrameContext;

typedef struct PangeaScriptObjectFrameResult
{
	bool hasPositionOffset;
	PangeaScriptVector3 positionOffset;
} PangeaScriptObjectFrameResult;

typedef enum PangeaScriptObjectLifecycle
{
	PANGEA_SCRIPT_OBJECT_ACTIVATE = 0,
	PANGEA_SCRIPT_OBJECT_DEACTIVATE,
	PANGEA_SCRIPT_OBJECT_STREAM_IN,
	PANGEA_SCRIPT_OBJECT_STREAM_OUT,
	PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET,
	PANGEA_SCRIPT_OBJECT_DESTROY
} PangeaScriptObjectLifecycle;

typedef struct PangeaScriptTriggerContext
{
	int levelNum;
	int playerNum;
	int triggerType;
	int otherType;
	unsigned int sideBits;
	unsigned int otherFlags;
	const char* triggerId;
	PangeaScriptObjectHandle self;
	PangeaScriptObjectHandle other;
	PangeaScriptVector3 position;
} PangeaScriptTriggerContext;

typedef struct PangeaScriptTriggerResult
{
	bool handled;
	bool hasSolid;
	bool solid;
	bool deleteSelf;
	bool deleteOther;
	float damagePlayer;
	float healthDelta;
	int scoreDelta;
} PangeaScriptTriggerResult;

typedef struct PangeaScriptPickupContext
{
	int levelNum;
	int playerNum;
	int pickupType;
	int pickupVariant;
	float amount;
	const char* pickupId;
	PangeaScriptObjectHandle pickup;
	PangeaScriptObjectHandle player;
	PangeaScriptVector3 position;
} PangeaScriptPickupContext;

typedef struct PangeaScriptPickupResult
{
	bool handled;
	bool hasConsumePickup;
	bool consumePickup;
	float healthDelta;
	int scoreDelta;
} PangeaScriptPickupResult;

typedef struct PangeaScriptWeaponHitContext
{
	int levelNum;
	int playerNum;
	int weaponType;
	int targetType;
	unsigned int targetFlags;
	float damage;
	const char* weaponId;
	PangeaScriptObjectHandle weapon;
	PangeaScriptObjectHandle target;
	PangeaScriptVector3 position;
} PangeaScriptWeaponHitContext;

typedef struct PangeaScriptWeaponHitResult
{
	bool handled;
	bool hasApplyDamage;
	bool applyDamage;
	bool destroyTarget;
	float damage;
	int scoreDelta;
} PangeaScriptWeaponHitResult;

typedef struct PangeaScriptDamageContext
{
	int levelNum;
	int playerNum;
	int cause;
	float damage;
	PangeaScriptObjectHandle source;
	PangeaScriptObjectHandle target;
	PangeaScriptVector3 position;
} PangeaScriptDamageContext;

typedef struct PangeaScriptDamageResult
{
	bool handled;
	bool hasApplyDamage;
	bool applyDamage;
	bool hasDamage;
	float damage;
} PangeaScriptDamageResult;

typedef struct PangeaScriptPlayerEventContext
{
	int levelNum;
	int playerNum;
	int eventValue;
	PangeaScriptObjectHandle player;
	PangeaScriptVector3 position;
} PangeaScriptPlayerEventContext;

typedef struct PangeaScriptNativeItem
{
	const char* id;
	int nativeType;
	const char* category;
	const char* dependencySummary;
} PangeaScriptNativeItem;

typedef struct PangeaScriptAssetDependency
{
	char kind[32];
	char id[96];
} PangeaScriptAssetDependency;

typedef enum PangeaScriptVisualKind
{
	PANGEA_SCRIPT_VISUAL_NONE = 0,
	PANGEA_SCRIPT_VISUAL_NATIVE_DISPLAY_GROUP,
	PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP,
	PANGEA_SCRIPT_VISUAL_NATIVE_SKELETON,
	PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON,
} PangeaScriptVisualKind;

typedef enum PangeaScriptCollisionPreset
{
	PANGEA_SCRIPT_COLLISION_NONE = 0,
	PANGEA_SCRIPT_COLLISION_SOLID_BOX,
	PANGEA_SCRIPT_COLLISION_TRIGGER_BOX,
	PANGEA_SCRIPT_COLLISION_PICKUP,
	PANGEA_SCRIPT_COLLISION_ENEMY,
	PANGEA_SCRIPT_COLLISION_PLATFORM,
} PangeaScriptCollisionPreset;

typedef struct PangeaScriptCustomObjectDefinition
{
	char id[96];
	PangeaScriptVisualKind visualKind;
	char modelPath[260];
	char skeletonPath[260];
	char nativeGroup[32];
	int modelObject;
	int skeletonType;
	int initialAnimation;
	char initialAnimationName[64];
	char animationNames[16][64];
	int animationIndices[16];
	int animationCount;
	float animationSpeed;
	float scale;
	int slot;
	PangeaScriptCollisionPreset collisionPreset;
	bool collisionBoundsSet;
	float collisionWidth;
	float collisionHeight;
	float collisionDepth;
} PangeaScriptCustomObjectDefinition;

typedef struct PangeaScriptTerrainReplacement
{
	int itemIndex;
	int nativeType;
	float x;
	float z;
	char customObjectId[96];
	bool strict;
} PangeaScriptTerrainReplacement;

typedef struct PangeaScriptMapReplacement
{
	int itemIndex;
	int nativeType;
	float x;
	float y;
	char customObjectId[96];
	bool strict;
} PangeaScriptMapReplacement;

typedef struct PangeaScriptSplineReplacement
{
	int splineNum;
	int itemIndex;
	int nativeType;
	float placement;
	char customObjectId[96];
	bool strict;
} PangeaScriptSplineReplacement;

PangeaScriptStatus PangeaScript_Init(const PangeaScriptGameInfo* gameInfo);
void PangeaScript_Shutdown(void);

bool PangeaScript_IsEnabled(void);
bool PangeaScript_HasRunnableModule(void);
void PangeaScript_SetNetworkedMode(bool networked);
bool PangeaScript_IsNetworkedMode(void);

PangeaScriptStatus PangeaScript_SetStartupScript(const char* path);
PangeaScriptStatus PangeaScript_SetConfigPath(const char* path);
PangeaScriptStatus PangeaScript_Reload(void);

const char* PangeaScript_GetLastError(void);
void PangeaScript_ClearLastError(void);
int PangeaScript_GetErrorCount(void);
PangeaScriptStatus PangeaScript_GetLastStatus(void);

PangeaScriptStatus PangeaScript_LoadLevelConfig(int levelNum);
int PangeaScript_RemapTerrainItemType(int levelNum, int itemType);
int PangeaScript_GetLevelAssetDependencyCount(void);
bool PangeaScript_GetLevelAssetDependency(int index, PangeaScriptAssetDependency* outDependency);
int PangeaScript_GetCustomObjectDefinitionCount(void);
const PangeaScriptCustomObjectDefinition* PangeaScript_GetCustomObjectDefinition(const char* id);
const PangeaScriptTerrainReplacement* PangeaScript_GetTerrainReplacement(int itemIndex, int nativeType, float x, float z);
const PangeaScriptMapReplacement* PangeaScript_GetMapReplacement(int itemIndex, int nativeType, float x, float y);
const PangeaScriptSplineReplacement* PangeaScript_GetSplineReplacement(int splineNum, int itemIndex, int nativeType, float placement);
bool PangeaScript_GetLevelFloatSetting(const char* key, float* outValue);
bool PangeaScript_GetLevelIntSetting(const char* key, int* outValue);
bool PangeaScript_GetLevelBoolSetting(const char* key, bool* outValue);
bool PangeaScript_GetLevelStringSetting(const char* key, char* outValue, int capacity);

PangeaScriptStatus PangeaScript_CallLevelHook(PangeaScriptHook hook, const PangeaScriptLevelContext* context);
PangeaScriptStatus PangeaScript_CallNativeSaveHook(int levelNum, int saveSlot, bool loading);
PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* context);
PangeaScriptStatus PangeaScript_CallTerrainItemHook(PangeaScriptTerrainItemContext* context);
PangeaScriptStatus PangeaScript_CallSplineItemHook(PangeaScriptSplineItemContext* context);
PangeaScriptStatus PangeaScript_CallMapItemHook(PangeaScriptMapItemContext* context);
PangeaScriptStatus PangeaScript_CallObjectFrame(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, PangeaScriptObjectFrameResult* outResult);
PangeaScriptStatus PangeaScript_CallObjectEvent(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, const char* event);
PangeaScriptStatus PangeaScript_CallObjectEventWithValue(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, const char* event, int eventValue);
PangeaScriptStatus PangeaScript_ApplyObjectLifecycle(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, PangeaScriptObjectLifecycle lifecycle);
PangeaScriptStatus PangeaScript_ApplyObjectLifecycleToAll(const PangeaScriptFrameContext* frameContext, PangeaScriptObjectLifecycle lifecycle);
bool PangeaScript_CallObjectTrigger(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, unsigned int sideBits, bool defaultSolid);
bool PangeaScript_CallObjectTriggerWithOther(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, unsigned int sideBits, bool defaultSolid, PangeaScriptObjectHandle other);
bool PangeaScript_CallObjectTriggerWithOtherAndPlayer(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, unsigned int sideBits, bool defaultSolid, PangeaScriptObjectHandle other, int playerNum);
void PangeaScript_ExpireTriggerContacts(const PangeaScriptFrameContext* frameContext);
PangeaScriptStatus PangeaScript_CallTriggerHook(const PangeaScriptTriggerContext* context, PangeaScriptTriggerResult* outResult);
PangeaScriptStatus PangeaScript_CallPickupHook(const PangeaScriptPickupContext* context, PangeaScriptPickupResult* outResult);
PangeaScriptStatus PangeaScript_CallWeaponHitHook(const PangeaScriptWeaponHitContext* context, PangeaScriptWeaponHitResult* outResult);
PangeaScriptStatus PangeaScript_CallDamageHook(const PangeaScriptDamageContext* context, PangeaScriptDamageResult* outResult);
PangeaScriptStatus PangeaScript_CallDamageAppliedHook(const PangeaScriptDamageContext* context);
PangeaScriptStatus PangeaScript_CallPlayerEvent(const PangeaScriptPlayerEventContext* context, const char* event);

void PangeaScript_ResetObjects(void);
PangeaScriptStatus PangeaScript_ApplyDeferredActions(const PangeaScriptFrameContext* frameContext);
PangeaScriptStatus PangeaScript_RegisterObject(const PangeaScriptObjectRegistration* registration, PangeaScriptObjectHandle* outHandle);
PangeaScriptStatus PangeaScript_RegisterScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle);
bool PangeaScript_UnregisterObject(PangeaScriptObjectHandle handle);
bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle);
int PangeaScript_GetScriptedObjectCount(void);
bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** outNativeObject);
PangeaScriptStatus PangeaScript_AssociateObjectSource(PangeaScriptObjectHandle handle, const PangeaScriptObjectSource* source);
bool PangeaScript_GetObjectSource(PangeaScriptObjectHandle handle, PangeaScriptObjectSource* outSource);
bool PangeaScript_FindObjectBySource(const PangeaScriptObjectSource* source, PangeaScriptObjectHandle* outHandle);
int PangeaScript_GetRegisteredObjectCount(void);
bool PangeaScript_GetRegisteredObjectHandle(int index, PangeaScriptObjectHandle* outHandle);
int PangeaScript_GetObjectTagCount(PangeaScriptObjectHandle handle);
const char* PangeaScript_GetObjectTag(PangeaScriptObjectHandle handle, int index);
bool PangeaScript_GetObjectPosition(PangeaScriptObjectHandle handle, PangeaScriptVector3* outPosition);
bool PangeaScript_GetObjectVelocity(PangeaScriptObjectHandle handle, PangeaScriptVector3* outVelocity);
bool PangeaScript_GetObjectRotation(PangeaScriptObjectHandle handle, PangeaScriptVector3* outRotation);
bool PangeaScript_GetObjectScale(PangeaScriptObjectHandle handle, float* outScale);
bool PangeaScript_GetObjectAnimation(PangeaScriptObjectHandle handle, int* outAnimation);
bool PangeaScript_GetObjectActive(PangeaScriptObjectHandle handle, bool* outActive);
bool PangeaScript_GetObjectCollisionEnabled(PangeaScriptObjectHandle handle, bool* outEnabled);
bool PangeaScript_SetObjectPosition(PangeaScriptObjectHandle handle, const PangeaScriptVector3* position);
bool PangeaScript_SetObjectPositionOffset(PangeaScriptObjectHandle handle, const PangeaScriptVector3* offset);
bool PangeaScript_SetObjectVelocity(PangeaScriptObjectHandle handle, const PangeaScriptVector3* velocity);
bool PangeaScript_SetObjectRotation(PangeaScriptObjectHandle handle, const PangeaScriptVector3* rotation);
bool PangeaScript_SetObjectScale(PangeaScriptObjectHandle handle, float scale);
bool PangeaScript_SetObjectAnimation(PangeaScriptObjectHandle handle, int animation, float speed, float blendSeconds);
bool PangeaScript_SetObjectAnimationNamed(PangeaScriptObjectHandle handle, const char* animation, float speed, float blendSeconds);
bool PangeaScript_SetObjectCollisionEnabled(PangeaScriptObjectHandle handle, bool enabled);
bool PangeaScript_SetObjectActive(PangeaScriptObjectHandle handle, bool active);
bool PangeaScript_DeleteObject(PangeaScriptObjectHandle handle);

PangeaScriptStatus PangeaScript_RegisterNativeItems(const PangeaScriptNativeItem* items, int count);
int PangeaScript_GetNativeItemCount(void);
const PangeaScriptNativeItem* PangeaScript_GetNativeItem(int index);
int PangeaScript_ResolveNativeItemType(const char* id);
PangeaScriptStatus PangeaScript_SpawnNative(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle);
int PangeaScript_GetCommandDescriptorCount(void);
const PangeaScriptCommandDescriptor* PangeaScript_GetCommandDescriptor(int index);
int PangeaScript_GetEventDescriptorCount(void);
const PangeaScriptEventDescriptor* PangeaScript_GetEventDescriptor(int index);
int PangeaScript_GetObjectEventDescriptorCount(void);
const PangeaScriptObjectEventDescriptor* PangeaScript_GetObjectEventDescriptor(int index);
void PangeaScript_ResetCommandTrace(void);
void PangeaScript_RecordCommand(const char* commandId, PangeaScriptObjectHandle target, PangeaScriptStatus status);
void PangeaScript_GetCommandTrace(PangeaScriptCommandTrace* outTrace);
bool PangeaScript_GetCommandTraceEntry(int index, PangeaScriptCommandTraceEntry* outEntry);
PangeaScriptStatus PangeaScript_CompareCommandTrace(
	const PangeaScriptCommandTrace* expectedTrace,
	const PangeaScriptCommandTraceEntry* expectedEntries,
	uint32_t expectedEntryCount,
	PangeaScriptCommandTraceComparison* outComparison);
void PangeaScript_ResetLifecycleTrace(void);
void PangeaScript_RecordLifecycleEvent(const char* eventId, PangeaScriptObjectHandle target, PangeaScriptStatus status);
void PangeaScript_GetLifecycleTrace(PangeaScriptLifecycleTrace* outTrace);
bool PangeaScript_GetLifecycleTraceEntry(int index, PangeaScriptLifecycleTraceEntry* outEntry);
PangeaScriptStatus PangeaScript_CompareLifecycleTrace(
	const PangeaScriptLifecycleTrace* expectedTrace,
	const PangeaScriptLifecycleTraceEntry* expectedEntries,
	uint32_t expectedEntryCount,
	PangeaScriptLifecycleTraceComparison* outComparison);
PangeaScriptStatus PangeaScript_CompareNormalizedLifecycleTrace(
	const PangeaScriptLifecycleTrace* expectedTrace,
	const PangeaScriptLifecycleTraceEntry* expectedEntries,
	uint32_t expectedEntryCount,
	PangeaScriptLifecycleTraceComparison* outComparison);

typedef enum PangeaScriptLogLevel
{
	PANGEA_LOG_INFO = 0,
	PANGEA_LOG_WARN,
	PANGEA_LOG_ERROR
} PangeaScriptLogLevel;

typedef struct PangeaScriptStatusInfo
{
	bool enabled;
	bool configLoaded;
	bool bundleLoaded;
	char activeScriptPath[260];
	char lastError[512];
	int errorCount;
	int budgetExceededCount;
	int hooksCalledCount;
	bool scriptsDisabled;
} PangeaScriptStatusInfo;

void PangeaScript_Log(PangeaScriptLogLevel level, const char* source, const char* message);
void PangeaScript_GetStatusInfo(PangeaScriptStatusInfo* outInfo);

bool PangeaScript_GetStatusEnabled(void);
bool PangeaScript_GetStatusConfigLoaded(void);
bool PangeaScript_GetStatusBundleLoaded(void);
const char* PangeaScript_GetStatusActiveScriptPath(void);
const char* PangeaScript_GetStatusLastError(void);
int PangeaScript_GetStatusErrorCount(void);
int PangeaScript_GetStatusBudgetExceededCount(void);
int PangeaScript_GetStatusHooksCalledCount(void);
bool PangeaScript_GetStatusScriptsDisabled(void);
uint32_t PangeaScript_GetRuntimeFingerprint(void);

#ifdef __cplusplus
}
#endif

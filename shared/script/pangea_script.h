#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

typedef enum PangeaScriptObjectInfoMask
{
	PANGEA_SCRIPT_OBJECT_INFO_NONE = 0,
	PANGEA_SCRIPT_OBJECT_INFO_TYPE = 1 << 0,
	PANGEA_SCRIPT_OBJECT_INFO_KIND = 1 << 1,
	PANGEA_SCRIPT_OBJECT_INFO_MODE = 1 << 2,
	PANGEA_SCRIPT_OBJECT_INFO_FLAGS = 1 << 3,
	PANGEA_SCRIPT_OBJECT_INFO_COLLISION = 1 << 4,
	PANGEA_SCRIPT_OBJECT_INFO_HEALTH = 1 << 5,
	PANGEA_SCRIPT_OBJECT_INFO_DAMAGE = 1 << 6,
	PANGEA_SCRIPT_OBJECT_INFO_VELOCITY = 1 << 7,
} PangeaScriptObjectInfoMask;

typedef struct PangeaScriptObjectInfo
{
	int type;
	int kind;
	int mode;
	unsigned int statusBits;
	unsigned int cType;
	unsigned int cBits;
	float health;
	float damage;
	PangeaScriptVector3 velocity;
	unsigned int validMask;
} PangeaScriptObjectInfo;

typedef struct PangeaScriptObjectBounds
{
	float left;
	float right;
	float front;
	float back;
	float top;
	float bottom;
} PangeaScriptObjectBounds;

#define PANGEA_SCRIPT_MAX_OBJECT_PARAMS 8

typedef struct PangeaScriptObjectParams
{
	int values[PANGEA_SCRIPT_MAX_OBJECT_PARAMS];
	int count;
} PangeaScriptObjectParams;

typedef enum PangeaScriptPlayerInfoMask
{
	PANGEA_SCRIPT_PLAYER_INFO_NONE = 0,
	PANGEA_SCRIPT_PLAYER_INFO_SCORE = 1 << 0,
	PANGEA_SCRIPT_PLAYER_INFO_HEALTH = 1 << 1,
	PANGEA_SCRIPT_PLAYER_INFO_LIVES = 1 << 2,
	PANGEA_SCRIPT_PLAYER_INFO_AMMO = 1 << 3,
	PANGEA_SCRIPT_PLAYER_INFO_FUEL = 1 << 4,
	PANGEA_SCRIPT_PLAYER_INFO_SHIELD = 1 << 5,
	PANGEA_SCRIPT_PLAYER_INFO_CURRENCY = 1 << 6,
	PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_TYPE = 1 << 7,
	PANGEA_SCRIPT_PLAYER_INFO_INVENTORY_QUANTITY = 1 << 8,
	PANGEA_SCRIPT_PLAYER_INFO_BOOST_TIMER = 1 << 9,
	PANGEA_SCRIPT_PLAYER_INFO_TRACTION_TIMER = 1 << 10,
	PANGEA_SCRIPT_PLAYER_INFO_INVISIBILITY_TIMER = 1 << 11,
	PANGEA_SCRIPT_PLAYER_INFO_HAZARD_TIMER = 1 << 12,
	PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_A = 1 << 13,
	PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_B = 1 << 14,
	PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_C = 1 << 15,
	PANGEA_SCRIPT_PLAYER_INFO_COLLECTIBLE_D = 1 << 16,
} PangeaScriptPlayerInfoMask;

typedef struct PangeaScriptPlayerInfo
{
	int playerNum;
	int score;
	float health;
	int lives;
	int ammo;
	float fuel;
	float shield;
	int currency;
	int inventoryType;
	int inventoryQuantity;
	float boostTimer;
	float tractionTimer;
	float invisibilityTimer;
	float hazardTimer;
	int collectibleA;
	int collectibleB;
	int collectibleC;
	int collectibleD;
	unsigned int validMask;
} PangeaScriptPlayerInfo;

typedef struct PangeaScriptSoundRequest
{
	int soundId;
	PangeaScriptVector3 position;
	float volume;
	float rate;
	bool hasPosition;
	bool hasVolume;
	bool hasRate;
} PangeaScriptSoundRequest;

typedef struct PangeaScriptEffectRequest
{
	int effectId;
	PangeaScriptVector3 position;
	PangeaScriptVector3 velocity;
	float scale;
	int quantity;
	bool hasPosition;
	bool hasVelocity;
	bool hasScale;
	bool hasQuantity;
} PangeaScriptEffectRequest;

typedef struct PangeaScriptGameInfo
{
	const char* gameId;
	const char* gameName;
	PangeaScriptStatus (*spawnNative)(const char* id, float x, float y, float z, int subtype, int amount, PangeaScriptObjectHandle* outHandle);
	bool (*getPlayerInfo)(int playerNum, PangeaScriptPlayerInfo* outInfo);
	bool (*setPlayerInfo)(int playerNum, const PangeaScriptPlayerInfo* info);
	bool (*playSound)(const PangeaScriptSoundRequest* request);
	bool (*spawnEffect)(const PangeaScriptEffectRequest* request);
} PangeaScriptGameInfo;

typedef struct PangeaScriptLevelContext
{
	int levelNum;
	const char* levelName;
} PangeaScriptLevelContext;

typedef struct PangeaScriptFrameContext
{
	int levelNum;
	unsigned int frameNum;
	float deltaSeconds;
	float levelTimeSeconds;
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
	int itemType;
	float x;
	float y;
	const unsigned char* params;
	int paramCount;
	bool handled;
	bool markInUse;
} PangeaScriptMapItemContext;

typedef struct PangeaScriptPickupContext
{
	int levelNum;
	int playerNum;
	const char* pickupId;
	int pickupType;
	int amount;
	PangeaScriptObjectHandle pickup;
	PangeaScriptObjectHandle player;
	PangeaScriptVector3 position;
	bool handled;
	bool consumePickup;
	int scoreDelta;
	float healthDelta;
} PangeaScriptPickupContext;

typedef struct PangeaScriptWeaponHitContext
{
	int levelNum;
	int playerNum;
	const char* weaponId;
	int weaponType;
	float damage;
	PangeaScriptObjectHandle weapon;
	PangeaScriptObjectHandle target;
	PangeaScriptVector3 position;
	int targetType;
	unsigned int targetFlags;
	bool handled;
	bool applyDamage;
	bool destroyTarget;
	int scoreDelta;
} PangeaScriptWeaponHitContext;

typedef struct PangeaScriptTriggerContext
{
	int levelNum;
	int playerNum;
	const char* triggerId;
	int triggerType;
	PangeaScriptObjectHandle self;
	PangeaScriptObjectHandle other;
	PangeaScriptVector3 position;
	unsigned int sideBits;
	int otherType;
	unsigned int otherFlags;
	bool handled;
	bool solid;
	bool deleteSelf;
	bool deleteOther;
	float damagePlayer;
	float healthDelta;
	int scoreDelta;
} PangeaScriptTriggerContext;

typedef struct PangeaScriptObjectCollisionContext
{
	int levelNum;
	int playerNum;
	const char* collisionId;
	int collisionType;
	PangeaScriptObjectHandle self;
	PangeaScriptObjectHandle other;
	PangeaScriptVector3 position;
	unsigned int sideBits;
	int selfType;
	unsigned int selfFlags;
	int otherType;
	unsigned int otherFlags;
	float damage;
	bool handled;
	bool suppressNative;
	bool deleteSelf;
	bool deleteOther;
	bool applyDamage;
	int scoreDelta;
	float healthDelta;
} PangeaScriptObjectCollisionContext;

typedef struct PangeaScriptPlayerDamageContext
{
	int levelNum;
	int playerNum;
	const char* damageId;
	int damageType;
	float damage;
	PangeaScriptObjectHandle source;
	PangeaScriptObjectHandle player;
	PangeaScriptVector3 position;
	bool handled;
	bool applyDamage;
	float healthDelta;
	int scoreDelta;
} PangeaScriptPlayerDamageContext;

typedef struct PangeaScriptObjectDamageContext
{
	int levelNum;
	int playerNum;
	const char* damageId;
	int damageType;
	float damage;
	PangeaScriptObjectHandle source;
	PangeaScriptObjectHandle target;
	PangeaScriptVector3 position;
	int targetType;
	unsigned int targetFlags;
	bool handled;
	bool applyDamage;
	bool destroyTarget;
	int scoreDelta;
} PangeaScriptObjectDamageContext;

typedef struct PangeaScriptObjectDeleteContext
{
	int levelNum;
	PangeaScriptObjectHandle object;
	PangeaScriptVector3 position;
	const char* const* tags;
	int tagCount;
} PangeaScriptObjectDeleteContext;

typedef struct PangeaScriptObjectOps
{
	bool (*getPosition)(void* nativeObject, PangeaScriptVector3* outPosition);
	bool (*setPosition)(void* nativeObject, const PangeaScriptVector3* position);
	bool (*setVelocity)(void* nativeObject, const PangeaScriptVector3* velocity);
	bool (*deleteObject)(void* nativeObject);
	bool (*getInfo)(void* nativeObject, PangeaScriptObjectInfo* outInfo);
	bool (*setInfo)(void* nativeObject, const PangeaScriptObjectInfo* info);
	bool (*getBounds)(void* nativeObject, PangeaScriptObjectBounds* outBounds);
	bool (*setBounds)(void* nativeObject, const PangeaScriptObjectBounds* bounds);
	bool (*getParams)(void* nativeObject, PangeaScriptObjectParams* outParams);
	bool (*setParams)(void* nativeObject, const PangeaScriptObjectParams* params);
} PangeaScriptObjectOps;

typedef enum PangeaScriptCapabilityLevel
{
	PANGEA_SCRIPT_CAPABILITY_DEFAULT = 0,
	PANGEA_SCRIPT_CAPABILITY_UNSUPPORTED,
	PANGEA_SCRIPT_CAPABILITY_READ_ONLY,
	PANGEA_SCRIPT_CAPABILITY_BASE,
	PANGEA_SCRIPT_CAPABILITY_FULL
} PangeaScriptCapabilityLevel;

typedef struct PangeaScriptObjectRegistration
{
	void* nativeObject;
	const PangeaScriptObjectOps* ops;
	const char* const* tags;
	int tagCount;
	PangeaScriptCapabilityLevel capabilityLevel;
} PangeaScriptObjectRegistration;

typedef struct PangeaScriptObjectFrameContext
{
	int levelNum;
	unsigned int frameNum;
	float deltaSeconds;
	float levelTimeSeconds;
	PangeaScriptObjectHandle object;
	PangeaScriptVector3 position;
	const char* const* tags;
	int tagCount;
} PangeaScriptObjectFrameContext;

typedef struct PangeaScriptObjectFrameResult
{
	bool hasPositionOffset;
	PangeaScriptVector3 positionOffset;
} PangeaScriptObjectFrameResult;

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

PangeaScriptStatus PangeaScript_Init(const PangeaScriptGameInfo* gameInfo);
void PangeaScript_Shutdown(void);

bool PangeaScript_IsEnabled(void);
bool PangeaScript_HasRunnableModule(void);

PangeaScriptStatus PangeaScript_SetStartupScript(const char* path);
PangeaScriptStatus PangeaScript_SetConfigPath(const char* path);
PangeaScriptStatus PangeaScript_Reload(void);

const char* PangeaScript_GetLastError(void);
int PangeaScript_GetErrorCount(void);
PangeaScriptStatus PangeaScript_GetLastStatus(void);

PangeaScriptStatus PangeaScript_LoadLevelConfig(int levelNum);
int PangeaScript_RemapTerrainItemType(int levelNum, int itemType);
int PangeaScript_GetLevelAssetDependencyCount(void);
bool PangeaScript_GetLevelAssetDependency(int index, PangeaScriptAssetDependency* outDependency);
bool PangeaScript_GetLevelFloatSetting(const char* key, float* outValue);
bool PangeaScript_GetLevelIntSetting(const char* key, int* outValue);
bool PangeaScript_GetLevelBoolSetting(const char* key, bool* outValue);
bool PangeaScript_GetLevelStringSetting(const char* key, char* outValue, int capacity);

PangeaScriptStatus PangeaScript_CallLevelHook(PangeaScriptHook hook, const PangeaScriptLevelContext* context);
PangeaScriptStatus PangeaScript_CallNamedLevelHook(const char* hookName, const PangeaScriptLevelContext* context);
PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* context);
PangeaScriptStatus PangeaScript_CallNamedFrameHook(const char* hookName, const PangeaScriptFrameContext* context);
PangeaScriptStatus PangeaScript_CallTerrainItemHook(PangeaScriptTerrainItemContext* context);
PangeaScriptStatus PangeaScript_CallSplineItemHook(PangeaScriptSplineItemContext* context);
PangeaScriptStatus PangeaScript_CallMapItemHook(PangeaScriptMapItemContext* context);
PangeaScriptStatus PangeaScript_CallPickupCollectedHook(PangeaScriptPickupContext* context);
PangeaScriptStatus PangeaScript_CallWeaponHitHook(PangeaScriptWeaponHitContext* context);
PangeaScriptStatus PangeaScript_CallTriggerEnterHook(PangeaScriptTriggerContext* context);
PangeaScriptStatus PangeaScript_CallObjectCollisionHook(PangeaScriptObjectCollisionContext* context);
PangeaScriptStatus PangeaScript_CallPlayerDamageHook(PangeaScriptPlayerDamageContext* context);
PangeaScriptStatus PangeaScript_CallObjectDamageHook(PangeaScriptObjectDamageContext* context);
PangeaScriptStatus PangeaScript_CallObjectDeleteHook(const PangeaScriptObjectDeleteContext* context);
PangeaScriptStatus PangeaScript_CallObjectFrame(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, PangeaScriptObjectFrameResult* outResult);

void PangeaScript_ResetObjects(void);
PangeaScriptStatus PangeaScript_RegisterObject(const PangeaScriptObjectRegistration* registration, PangeaScriptObjectHandle* outHandle);
PangeaScriptStatus PangeaScript_RegisterScriptedObject(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle);
bool PangeaScript_UnregisterObject(PangeaScriptObjectHandle handle);
bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle);
bool PangeaScript_GetObjectPosition(PangeaScriptObjectHandle handle, PangeaScriptVector3* outPosition);
bool PangeaScript_SetObjectPosition(PangeaScriptObjectHandle handle, const PangeaScriptVector3* position);
bool PangeaScript_SetObjectVelocity(PangeaScriptObjectHandle handle, const PangeaScriptVector3* velocity);
bool PangeaScript_DeleteObject(PangeaScriptObjectHandle handle);
bool PangeaScript_RequestDeleteObject(PangeaScriptObjectHandle handle);
bool PangeaScript_GetObjectInfo(PangeaScriptObjectHandle handle, PangeaScriptObjectInfo* outInfo);
bool PangeaScript_SetObjectInfo(PangeaScriptObjectHandle handle, const PangeaScriptObjectInfo* info);
bool PangeaScript_GetObjectBounds(PangeaScriptObjectHandle handle, PangeaScriptObjectBounds* outBounds);
bool PangeaScript_SetObjectBounds(PangeaScriptObjectHandle handle, const PangeaScriptObjectBounds* bounds);
bool PangeaScript_GetObjectParams(PangeaScriptObjectHandle handle, PangeaScriptObjectParams* outParams);
bool PangeaScript_SetObjectParams(PangeaScriptObjectHandle handle, const PangeaScriptObjectParams* params);
int PangeaScript_GetObjectTagCount(PangeaScriptObjectHandle handle);
const char* PangeaScript_GetObjectTag(PangeaScriptObjectHandle handle, int index);
bool PangeaScript_ObjectHasTag(PangeaScriptObjectHandle handle, const char* tag);
bool PangeaScript_AddObjectTag(PangeaScriptObjectHandle handle, const char* tag);
bool PangeaScript_RemoveObjectTag(PangeaScriptObjectHandle handle, const char* tag);

PangeaScriptStatus PangeaScript_RegisterNativeItems(const PangeaScriptNativeItem* items, int count);
PangeaScriptStatus PangeaScript_SpawnNative(const char* id, float x, float y, float z, int subtype, int amount, PangeaScriptObjectHandle* outHandle);
bool PangeaScript_GetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo);
bool PangeaScript_SetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info);
bool PangeaScript_PlaySound(const PangeaScriptSoundRequest* request);
bool PangeaScript_SpawnEffect(const PangeaScriptEffectRequest* request);

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

#ifdef __cplusplus
}
#endif

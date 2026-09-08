#include "../shared/script/pangea_script.h"
#include "../shared/script/pangea_script_backend.h"
#include "../shared/script/pangea_script_config.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static int gNativeSpawns;
typedef struct TestPersistentEntry
{
	unsigned char value[4096];
	int size;
	char key[64];
} TestPersistentEntry;

static TestPersistentEntry gPersistentEntries[8];

static TestPersistentEntry* find_persistent_entry(const char* key)
{
	if (!key) return NULL;
	for (int index = 0; index < (int)(sizeof(gPersistentEntries) / sizeof(gPersistentEntries[0])); index++)
		if (gPersistentEntries[index].size > 0 && strcmp(key, gPersistentEntries[index].key) == 0)
			return &gPersistentEntries[index];
	return NULL;
}

static PangeaScriptStatus load_persistent(const char* key, unsigned char* outData, int capacity, int* outSize)
{
	TestPersistentEntry* entry = find_persistent_entry(key);
	if (!entry || !outData || !outSize || capacity < entry->size)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	memcpy(outData, entry->value, (size_t)entry->size);
	*outSize = entry->size;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus save_persistent(const char* key, const unsigned char* data, int size)
{
	if (!key || size < 0 || size > (int)sizeof(gPersistentEntries[0].value))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	TestPersistentEntry* entry = find_persistent_entry(key);
	if (size == 0)
	{
		if (entry) *entry = (TestPersistentEntry){0};
		return PANGEA_SCRIPT_OK;
	}
	if (!data)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!entry)
	{
		for (int index = 0; index < (int)(sizeof(gPersistentEntries) / sizeof(gPersistentEntries[0])); index++)
		{
			if (gPersistentEntries[index].size == 0)
			{
				entry = &gPersistentEntries[index];
				break;
			}
		}
	}
	if (!entry)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	snprintf(entry->key, sizeof(entry->key), "%s", key);
	memcpy(entry->value, data, (size_t)size);
	entry->size = size;
	return PANGEA_SCRIPT_OK;
}

static bool test_object_position(void* nativeObject, PangeaScriptVector3* outPosition)
{
	if (!nativeObject || !outPosition) return false;
	*outPosition = *(PangeaScriptVector3*) nativeObject;
	return true;
}

static bool test_delete_object(void* nativeObject);

static const PangeaScriptObjectOps kTestObjectOps = {.getPosition = test_object_position};

static const PangeaScriptObjectOps kTestSpawnObjectOps =
{
	.getPosition = test_object_position,
	.deleteObject = test_delete_object,
};

static bool test_unreadable_object_position(void* nativeObject, PangeaScriptVector3* outPosition)
{
	(void)nativeObject;
	(void)outPosition;
	return false;
}

static bool test_delete_object(void* nativeObject)
{
	return nativeObject != NULL;
}

typedef struct TestVisualObject
{
	PangeaScriptVector3 position;
	PangeaScriptVector3 velocity;
	PangeaScriptVector3 rotation;
	float scale;
	int animation;
	bool collisionEnabled;
	char namedAnimation[32];
} TestVisualObject;

static bool test_visual_position(void* nativeObject, PangeaScriptVector3* outPosition)
{
	if (!nativeObject || !outPosition) return false;
	*outPosition = ((TestVisualObject*) nativeObject)->position;
	return true;
}

static bool test_visual_set_position(void* nativeObject, const PangeaScriptVector3* position)
{
	if (!nativeObject || !position) return false;
	((TestVisualObject*) nativeObject)->position = *position;
	return true;
}

static bool test_visual_set_rotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	if (!nativeObject || !rotation) return false;
	((TestVisualObject*) nativeObject)->rotation = *rotation;
	return true;
}

static bool test_visual_set_velocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	if (!nativeObject || !velocity) return false;
	((TestVisualObject*) nativeObject)->velocity = *velocity;
	return true;
}

static bool test_visual_set_scale(void* nativeObject, float scale)
{
	if (!nativeObject) return false;
	((TestVisualObject*) nativeObject)->scale = scale;
	return true;
}

static bool test_visual_set_animation(void* nativeObject, int animation, float speed, float blendSeconds)
{
	if (!nativeObject || animation < 0 || speed < 0.0f || blendSeconds < 0.0f) return false;
	((TestVisualObject*) nativeObject)->animation = animation;
	return true;
}

static bool test_visual_set_animation_named(void* nativeObject, const char* animation, float speed, float blendSeconds)
{
	if (!nativeObject || !animation || !animation[0] || speed < 0.0f || blendSeconds < 0.0f) return false;
	snprintf(((TestVisualObject*) nativeObject)->namedAnimation, sizeof(((TestVisualObject*) nativeObject)->namedAnimation), "%s", animation);
	return true;
}

static bool test_visual_set_collision_enabled(void* nativeObject, bool enabled)
{
	if (!nativeObject) return false;
	((TestVisualObject*) nativeObject)->collisionEnabled = enabled;
	return true;
}

static bool test_visual_delete(void* nativeObject)
{
	return nativeObject != NULL;
}

static const PangeaScriptObjectOps kTestVisualObjectOps =
{
	.getPosition = test_visual_position,
	.setPosition = test_visual_set_position,
	.setRotation = test_visual_set_rotation,
};

static const PangeaScriptObjectOps kTestFullVisualObjectOps =
{
	.getPosition = test_visual_position,
	.setPosition = test_visual_set_position,
	.setVelocity = test_visual_set_velocity,
	.setRotation = test_visual_set_rotation,
	.setScale = test_visual_set_scale,
	.setAnimation = test_visual_set_animation,
	.setAnimationNamed = test_visual_set_animation_named,
	.setCollisionEnabled = test_visual_set_collision_enabled,
	.deleteObject = test_visual_delete,
};

static bool nearly_equal(float actual, float expected)
{
	return fabsf(actual - expected) < 0.001f;
}

static int get_player_count(void) { return 1; }

static float gScriptPlayerHealth = 0.75f;
static int gScriptPlayerHealthMutationCalls = 0;
static int gScriptPlayerLives = 3;
static int64_t gScriptPlayerScore = 42;
static int gScriptPlayerInvulnerabilityMutationCalls = 0;
static int gScriptPlayerPositionMutationCalls = 0;
static int gScriptPlayerVelocityMutationCalls = 0;

static bool get_player(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (playerNum != 0 || !outPlayer) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {7, 8, 9}, .health = gScriptPlayerHealth, .hasHealth = true, .score = gScriptPlayerScore, .hasScore = true, .lives = gScriptPlayerLives, .hasLives = true, .lapNum = 2, .checkpointNum = 6, .placement = 1, .raceComplete = false, .hasRaceState = true, .active = true};
	outPlayer->vehicleType = 3;
	outPlayer->vehicleMaxSpeed = 120.0f;
	outPlayer->vehicleAcceleration = 8.5f;
	outPlayer->vehicleTraction = 4.0f;
	outPlayer->vehicleSuspension = 2.5f;
	outPlayer->hasVehicleState = true;
	outPlayer->team = 1;
	outPlayer->hasTeamState = true;
	outPlayer->carryingFlag = true;
	outPlayer->captureScore = 2;
	outPlayer->hasCaptureState = true;
	outPlayer->activeWeapon = 2;
	outPlayer->hasWeaponState = true;
	outPlayer->weaponCount = 2;
	outPlayer->weapons[0] = (PangeaScriptPlayerInventoryEntry){1, 99};
	outPlayer->weapons[1] = (PangeaScriptPlayerInventoryEntry){2, 4};
	outPlayer->eggCount = 3;
	outPlayer->eggs[0] = 2;
	outPlayer->eggs[1] = 0;
	outPlayer->eggs[2] = 5;
	outPlayer->eggRequired[0] = 5;
	outPlayer->eggRequired[1] = 5;
	outPlayer->eggRequired[2] = 5;
	outPlayer->hasEggState = true;
	return true;
}

static PangeaScriptStatus set_player_health(int playerNum, float health)
{
	gScriptPlayerHealthMutationCalls++;
	if (playerNum != 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gScriptPlayerHealth = health;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus set_player_lives(int playerNum, int lives)
{
	if (playerNum != 0 || lives < 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gScriptPlayerLives = lives;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus set_player_score(int playerNum, int64_t score)
{
	if (playerNum != 0 || score < 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gScriptPlayerScore = score;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus set_player_weapon_quantity(int playerNum, int weaponType, int quantity)
{
	if (playerNum != 0 || weaponType < 0 || weaponType >= 2 || quantity < 0 || quantity > 999) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gScriptPlayerScore = quantity;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus set_player_invulnerable(int playerNum, float durationSeconds)
{
	gScriptPlayerInvulnerabilityMutationCalls++;
	if (playerNum != 0 || durationSeconds < 0.0f) return PANGEA_SCRIPT_BAD_ARGUMENT;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus set_player_position(int playerNum, const PangeaScriptVector3* position)
{
	gScriptPlayerPositionMutationCalls++;
	if (playerNum != 0 || !position) return PANGEA_SCRIPT_BAD_ARGUMENT;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus set_player_velocity(int playerNum, const PangeaScriptVector3* velocity)
{
	gScriptPlayerVelocityMutationCalls++;
	if (playerNum != 0 || !velocity) return PANGEA_SCRIPT_BAD_ARGUMENT;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus spawn_native(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	assert(strcmp(id, "6") == 0);
	assert(x == 10 && y == 20 && z == 30 && params[0] == 2);
	gNativeSpawns++;
	if (outHandle) *outHandle = (PangeaScriptObjectHandle){0};
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus spawn_scripted_for_test(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
	static PangeaScriptVector3 position;
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestSpawnObjectOps,
		.objectType = id,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	position = (PangeaScriptVector3){x, y, z};
	PangeaScriptStatus status = PangeaScript_RegisterObject(&registration, &handle);
	if (status != PANGEA_SCRIPT_OK)
		return status;
	if (outHandle)
		*outHandle = handle;
	return PANGEA_SCRIPT_OK;
}

static void test_lifecycle_name(const char* gameId, const char* hookName)
{
	PangeaScriptGameInfo gameInfo = {.gameId = gameId, .gameName = gameId, .spawnNative = spawn_native};
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(&gameInfo);
	assert(backend != NULL);
	char source[256];
	snprintf(source, sizeof(source), "return { %s = function() pangea.spawn.native(6, {x=10,y=20,z=30}, {param0=2}) end }", hookName);
	char error[512] = {0};
	assert(PangeaScriptBackend_Load(backend, source, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptLevelContext level = {.levelNum = 1, .levelName = "test"};
	int previousSpawns = gNativeSpawns;
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_START, &level, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(gNativeSpawns == previousSpawns + 1);
	PangeaScriptBackend_Destroy(backend);
}

static void test_shutdown_lifecycle(void)
{
	PangeaScriptGameInfo gameInfo = {.gameId = "Test", .gameName = "Test"};
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(&gameInfo);
	assert(backend != NULL);
	char error[512] = {0};
	assert(PangeaScriptBackend_Load(backend, "return { onGameShutdown = function(ctx) assert(ctx.gameId == 'Test') end }", error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptLevelContext context = {0};
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN, &context, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptBackend_Destroy(backend);
}

static void test_native_save_load_hooks(void)
{
	memset(gPersistentEntries, 0, sizeof(gPersistentEntries));
	PangeaScriptGameInfo gameInfo = {
		.gameId = "OttoMatic-Android",
		.gameName = "Otto Matic",
		.loadPersistent = load_persistent,
		.savePersistent = save_persistent,
	};
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(&gameInfo);
	assert(backend != NULL);
	char error[512] = {0};
	const char* source =
		"return {"
		" onSave=function(ctx) assert(ctx.levelNum == 7 and ctx.levelName == 'slot-3'); assert(pangea.persistence.set('native-slot', 1, 'saved-value')) end,"
		" onLoad=function(ctx) assert(ctx.levelNum == 7 and ctx.levelName == 'slot-3'); assert(pangea.persistence.get('native-slot', 1) == 'saved-value') end"
		"}";
	assert(PangeaScriptBackend_Load(backend, source, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptLevelContext context = {.levelNum = 7, .levelName = "slot-3"};
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_SAVE, &context, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptBackend_Destroy(backend);
	backend = PangeaScriptBackend_Create(&gameInfo);
	assert(backend != NULL);
	assert(PangeaScriptBackend_Load(backend, source, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LOAD, &context, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptBackend_Destroy(backend);
}

static void test_gameplay_hooks(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"return {"
		" onTriggerEnter=function(ctx) assert(ctx.gameId=='Test' and ctx.position.x==1 and ctx.other.id==12 and ctx.other.generation==4 and ctx.playerNum==2); return {handled=true,solid=false,scoreDelta=3} end,"
		" onPickupCollected=function(ctx) assert(ctx.pickupType==4 and ctx.pickupVariant==6); return {handled=true,consumePickup=true,healthDelta=2} end,"
		" onWeaponHit=function(ctx) assert(ctx.damage==5); return {handled=true,applyDamage=true,damage=7,destroyTarget=true} end"
		"}";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptTriggerContext trigger = {.levelNum = 2, .playerNum = 2, .other = {12, 4}, .position = {1, 2, 3}};
	PangeaScriptTriggerResult triggerResult = {0};
	assert(PangeaScriptBackend_CallTriggerHook(backend, &trigger, &triggerResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(triggerResult.handled && triggerResult.hasSolid && !triggerResult.solid && triggerResult.scoreDelta == 3);
	PangeaScriptPickupContext pickup = {.levelNum = 2, .pickupType = 4, .pickupVariant = 6};
	PangeaScriptPickupResult pickupResult = {0};
	assert(PangeaScriptBackend_CallPickupHook(backend, &pickup, &pickupResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(pickupResult.handled && pickupResult.hasConsumePickup && pickupResult.consumePickup && pickupResult.healthDelta == 2);
	PangeaScriptWeaponHitContext hit = {.levelNum = 2, .damage = 5};
	PangeaScriptWeaponHitResult hitResult = {0};
	assert(PangeaScriptBackend_CallWeaponHitHook(backend, &hit, &hitResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(hitResult.handled && hitResult.hasApplyDamage && hitResult.applyDamage && hitResult.damage == 7 && hitResult.destroyTarget);
}

static void test_timers(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"local fired = false\n"
		"local cancelled = pangea.time.after(0, function() error('cancelled timer fired') end)\n"
		"assert(pangea.time.isActive(cancelled))\n"
		"assert(pangea.time.cancel(cancelled))\n"
		"assert(not pangea.time.isActive(cancelled))\n"
		"pangea.time.after(0.5, function() fired = true end)\n"
		"return { onFrame = function(ctx) if ctx.levelTimeSeconds >= 0.5 then assert(fired) end end }";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext early = {.levelNum = 1, .levelTimeSeconds = 0.25f};
	assert(PangeaScriptBackend_CallFrameHook(backend, &early, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext due = {.levelNum = 1, .levelTimeSeconds = 0.5f};
	assert(PangeaScriptBackend_CallFrameHook(backend, &due, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void test_repeating_timers_and_api_version(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"pangea.api.requireVersion(1)\n"
		"pangea.api.requireVersion(1, 1)\n"
		"local count = 0\n"
		"local timer\n"
		"timer = pangea.time.every(0.25, function() count = count + 1; if count == 2 then assert(pangea.time.cancel(timer)) end end)\n"
		"return { onFrame = function(ctx) if ctx.levelTimeSeconds >= 1 then assert(count == 2) end end }";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext first = {.levelNum = 1, .levelTimeSeconds = 0.25f};
	PangeaScriptFrameContext skipped = {.levelNum = 1, .levelTimeSeconds = 0.9f};
	PangeaScriptFrameContext final = {.levelNum = 1, .levelTimeSeconds = 1.0f};
	assert(PangeaScriptBackend_CallFrameHook(backend, &first, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &skipped, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &final, error, errorCapacity) == PANGEA_SCRIPT_OK);
	const char* incompatible = "pangea.api.requireVersion(2); return {}";
	assert(PangeaScriptBackend_Load(backend, incompatible, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "incompatible Pangea API version") != NULL);
}

static void test_context_immutability_and_finite_numbers(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* immutable =
		"return { onFrame = function(ctx) local ok, message = pcall(function() ctx.levelNum = 99 end); "
		"assert(not ok and string.find(message, 'read-only', 1, true)); "
		"local nestedOk = pcall(function() ctx.position = {x=1,y=2,z=3} end); assert(not nestedOk) end }";
	assert(PangeaScriptBackend_Load(backend, immutable, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 1};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_OK);
	const char* invalidTimer = "pangea.time.after(0/0, function() end); return {}";
	assert(PangeaScriptBackend_Load(backend, invalidTimer, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "delay must be finite") != NULL);
	const char* invalidNativeParam = "pangea.spawn.native(6, {x=1,y=2,z=3}, {param0=256}); return {}";
	assert(PangeaScriptBackend_Load(backend, invalidNativeParam, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "param0 must be an integer from 0 through 255") != NULL);
	const char* invalidVector = "pangea.spawn.native(6, {x=0/0,y=2,z=3}); return {}";
	assert(PangeaScriptBackend_Load(backend, invalidVector, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "position must be a Vector3") != NULL);
	const char* invalidScale = "local ok, message = pcall(function() pangea.object.setScale({id=1,generation=1}, 0/0) end); assert(not ok and string.find(message, 'scale must be finite', 1, true)); return {}";
	assert(PangeaScriptBackend_Load(backend, invalidScale, error, errorCapacity) == PANGEA_SCRIPT_OK);
	const char* invalidAnimation = "local ok, message = pcall(function() pangea.object.setAnimation({id=1,generation=1}, 1, -1) end); assert(not ok and string.find(message, 'non-negative', 1, true)); return {}";
	assert(PangeaScriptBackend_Load(backend, invalidAnimation, error, errorCapacity) == PANGEA_SCRIPT_OK);
	const char* invalidResult = "return { onWeaponHit = function() return { damage = 0/0 } end }";
	assert(PangeaScriptBackend_Load(backend, invalidResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptWeaponHitContext hit = {0};
	PangeaScriptWeaponHitResult result = {0};
	assert(PangeaScriptBackend_CallWeaponHitHook(backend, &hit, &result, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "finite number") != NULL);
	const char* negativeResult = "return { onWeaponHit = function() return { damage = -1 } end }";
	assert(PangeaScriptBackend_Load(backend, negativeResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallWeaponHitHook(backend, &hit, &result, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "non-negative") != NULL);
	const char* nestedImmutable =
		"return { onTerrainItem = function(ctx) local ok = pcall(function() ctx.position.x = 99 end); "
		"assert(not ok and #ctx.params == 4); local count = 0; for _ in pairs(ctx.params) do count = count + 1 end; assert(count == 4) end }";
	assert(PangeaScriptBackend_Load(backend, nestedImmutable, error, errorCapacity) == PANGEA_SCRIPT_OK);
	unsigned char params[4] = {1, 2, 3, 4};
	PangeaScriptTerrainItemContext item = {.params = params, .paramCount = 4};
	assert(PangeaScriptBackend_CallTerrainItemHook(backend, &item, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void test_coroutine_tasks(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"local phase = 0\n"
		"local cancelled = pangea.task.start(function() pangea.task.wait(0.1); error('cancelled task resumed') end)\n"
		"assert(pangea.task.isActive(cancelled))\n"
		"assert(pangea.task.cancel(cancelled))\n"
		"assert(not pangea.task.isActive(cancelled))\n"
		"assert(not pangea.task.cancel(999999))\n"
		"pangea.task.start(function() phase = 1; pangea.task.wait(0.25); phase = 2; pangea.task.wait(0); phase = 3 end)\n"
		"assert(pangea.api.diagnostics().activeTasks == 1)\n"
		"return { onFrame = function(ctx) if ctx.levelTimeSeconds < 0.25 then assert(phase == 1) elseif ctx.levelTimeSeconds < 0.5 then assert(phase == 2) else assert(phase == 3) end end }";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext early = {.levelNum = 1, .levelTimeSeconds = 0.1f};
	PangeaScriptFrameContext due = {.levelNum = 1, .levelTimeSeconds = 0.25f};
	PangeaScriptFrameContext next = {.levelNum = 1, .levelTimeSeconds = 0.5f};
	assert(PangeaScriptBackend_CallFrameHook(backend, &early, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &due, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &next, error, errorCapacity) == PANGEA_SCRIPT_OK);
	const char* invalidDelay = "pangea.task.start(function() pangea.task.wait(0/0) end); return {}";
	assert(PangeaScriptBackend_Load(backend, invalidDelay, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "task delay must be finite") != NULL);
	const char* capacity =
		"for _ = 1, 64 do pangea.task.start(function() pangea.task.wait(10) end) end\n"
		"local ok, message = pcall(function() pangea.task.start(function() end) end)\n"
		"assert(not ok and string.find(message, 'task capacity exceeded', 1, true)); return {}";
	assert(PangeaScriptBackend_Load(backend, capacity, error, errorCapacity) == PANGEA_SCRIPT_OK);
	const char* deferredError = "pangea.task.start(function() pangea.task.wait(0.1); error('deferred task failure') end); return {}";
	assert(PangeaScriptBackend_Load(backend, deferredError, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext errorFrame = {.levelTimeSeconds = 0.1f};
	assert(PangeaScriptBackend_CallFrameHook(backend, &errorFrame, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "deferred task failure") != NULL && strstr(error, "stack traceback") != NULL);
}

static void test_object_queries_and_events(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	PangeaScript_ResetObjects();
	PangeaScriptVector3 nearPosition = {1, 0, 0};
	PangeaScriptVector3 farPosition = {9, 0, 0};
	char nearEnemyTag[] = "enemy";
	char nearFlyingTag[] = "flying";
	const char* nearTags[] = {nearEnemyTag, nearFlyingTag};
	const char* farTags[] = {"enemy"};
	PangeaScriptObjectRegistration nearRegistration = {.nativeObject = &nearPosition, .ops = &kTestObjectOps, .objectType = "near", .tags = nearTags, .tagCount = 2};
	PangeaScriptObjectRegistration farRegistration = {.nativeObject = &farPosition, .ops = &kTestObjectOps, .objectType = "far", .tags = farTags, .tagCount = 1};
	PangeaScriptObjectHandle nearHandle;
	PangeaScriptObjectHandle farHandle;
	assert(PangeaScript_RegisterObject(&nearRegistration, &nearHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_RegisterObject(&farRegistration, &farHandle) == PANGEA_SCRIPT_OK);
	nearEnemyTag[0] = 'x';
	nearFlyingTag[0] = 'x';
	const char* source =
		"assert(#pangea.object.all() == 2 and #pangea.object.findByTag('enemy') == 2 and #pangea.object.findByTag('flying') == 1)\n"
		"assert(pangea.object.nearest({x=0,y=0,z=0}, 'enemy').id > 0)\n"
		"local received = 0\n"
		"local subscription = pangea.events.on('score', function(payload) local ok = pcall(function() payload.value = 99 end); assert(not ok); received = received + payload.value end)\n"
		"assert(pangea.events.emit('score', {value=3}) == 1 and received == 3)\n"
		"assert(pangea.events.off(subscription) and not pangea.events.off(subscription))\n"
		"assert(pangea.events.emit('score', {value=4}) == 0 and pangea.api.diagnostics().activeSubscriptions == 0)\n"
		"local onceCount = 0; pangea.events.once('once', function() onceCount = onceCount + 1; pangea.events.emit('once') end)\n"
		"assert(pangea.events.emit('once') == 1 and onceCount == 1 and pangea.events.emit('once') == 0)\n"
		"return {}";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ObjectExists(nearHandle) && PangeaScript_ObjectExists(farHandle));
}

static void test_object_command_results(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	PangeaScript_ResetObjects();
	TestVisualObject visual = {.position = {1, 2, 3}};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &visual,
		.ops = &kTestVisualObjectOps,
		.objectType = "command-result-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_BASE,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	char source[2048];
	snprintf(source, sizeof(source),
		"local handle={id=%d,generation=%u}\n"
		"local moved=pangea.object.setPositionResult(handle,{x=4,y=5,z=6})\n"
		"assert(moved.ok and moved.code==0 and moved.reason=='ok' and moved.primary.id==handle.id)\n"
		"local state=pangea.object.state(handle); state.health=10; state.nested={value=3}; assert(pangea.object.captureCheckpoint(handle), 'capture')\n"
		"state.health=20; state.nested.value=9; assert(pangea.object.restoreCheckpoint(handle), 'restore'); assert(state.health==10 and state.nested.value==3, 'state')\n"
		"local unsupported=pangea.object.setVelocityResult(handle,{x=1,y=2,z=3})\n"
		"assert(not unsupported.ok and unsupported.reason=='runtime-error' and string.find(unsupported.message,'velocity',1,true))\n"
		"local stale=pangea.object.deleteResult({id=999,generation=1})\n"
		"assert(not stale.ok and stale.reason=='bad-argument' and stale.primary.id==999)\n"
		"local invalid=pangea.object.setPositionResult(handle,{x=0/0,y=1,z=2})\n"
		"assert(not invalid.ok and invalid.reason=='bad-argument' and invalid.primary == nil)\n"
		"local diagnostics=pangea.api.diagnostics()\n"
		"assert(diagnostics.commandCount==3 and diagnostics.commandHash>0 and not diagnostics.commandTraceOverflow)\n"
		"assert(#diagnostics.commandTrace==3 and diagnostics.commandTrace[1].id=='pangea.object.setPosition' and diagnostics.commandTrace[1].objectId==handle.id)\n"
		"return {}\n",
		handle.id, handle.generation);
	PangeaScriptStatus commandStatus = PangeaScriptBackend_Load(backend, source, error, errorCapacity);
	if (commandStatus != PANGEA_SCRIPT_OK) fprintf(stderr, "checkpoint fixture failed: %s\n", error);
	assert(commandStatus == PANGEA_SCRIPT_OK);
	assert(nearly_equal(visual.position.x, 4) && nearly_equal(visual.position.y, 5) && nearly_equal(visual.position.z, 6));
	PangeaScriptCommandTrace firstTrace;
	PangeaScript_GetCommandTrace(&firstTrace);
	PangeaScriptCommandTraceEntry firstEntries[3];
	for (int index = 0; index < 3; index++)
		assert(PangeaScript_GetCommandTraceEntry(index, &firstEntries[index]));
	PangeaScriptCommandTraceComparison comparison;
	assert(PangeaScript_CompareCommandTrace(&firstTrace, firstEntries, firstTrace.entryCount, &comparison) == PANGEA_SCRIPT_OK);
	assert(comparison.matches && comparison.firstMismatchIndex == UINT32_MAX);
	firstEntries[1].commandId[0] = 'x';
	assert(PangeaScript_CompareCommandTrace(&firstTrace, firstEntries, firstTrace.entryCount, &comparison) == PANGEA_SCRIPT_OK);
	assert(!comparison.matches && comparison.firstMismatchIndex == 1);
	assert(strcmp(comparison.expectedCommandId, "xangea.object.setVelocity") == 0);
	assert(strcmp(comparison.actualCommandId, "pangea.object.setVelocity") == 0);
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptCommandTrace secondTrace;
	PangeaScript_GetCommandTrace(&secondTrace);
	assert(firstTrace.commandCount == secondTrace.commandCount && firstTrace.hash == secondTrace.hash && !secondTrace.overflow);
	PangeaScript_ResetCommandTrace();
	for (int index = 0; index < PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY + 1; index++)
	{
		PangeaScriptVector3 position = {(float)index, 0, 0};
		assert(PangeaScript_SetObjectPosition(handle, &position));
	}
	PangeaScriptCommandTrace boundedTrace;
	PangeaScript_GetCommandTrace(&boundedTrace);
	assert(boundedTrace.commandCount == PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY + 1);
	assert(boundedTrace.entryCount == PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY && boundedTrace.overflow);
	PangeaScriptCommandTraceEntry finalEntry;
	assert(PangeaScript_GetCommandTraceEntry(PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY - 1, &finalEntry));
	assert(strcmp(finalEntry.commandId, "pangea.object.setPosition") == 0);
	assert(!PangeaScript_GetCommandTraceEntry(PANGEA_SCRIPT_COMMAND_TRACE_CAPACITY, &finalEntry));
}

static void test_full_object_command_surface(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	PangeaScript_ResetObjects();
	TestVisualObject visual = {.position = {1, 2, 3}, .scale = 1.0f, .collisionEnabled = true};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &visual,
		.ops = &kTestFullVisualObjectOps,
		.objectType = "full-command-surface-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	char source[4096];
	snprintf(source, sizeof(source),
		"local handle={id=%d,generation=%u}\n"
		"assert(pangea.object.setPositionResult(handle,{x=4,y=5,z=6}).ok)\n"
		"assert(pangea.object.setVelocityResult(handle,{x=1,y=2,z=3}).ok)\n"
		"assert(pangea.object.setRotationResult(handle,{x=0.1,y=0.2,z=0.3}).ok)\n"
		"assert(pangea.object.setScaleResult(handle,2).ok)\n"
		"assert(pangea.object.setAnimationResult(handle,3,1.5,0.25).ok)\n"
		"assert(pangea.object.setAnimationResult(handle,'run',1,0).ok)\n"
		"assert(pangea.object.setCollisionEnabled(handle,false))\n"
		"assert(pangea.object.setCollisionEnabledResult(handle,true).ok)\n"
		"assert(pangea.object.setActiveResult(handle,false).ok)\n"
		"assert(pangea.object.setActiveResult(handle,true).ok)\n"
		"local invalid=pangea.object.setScaleResult(handle,0/0)\n"
		"assert(not invalid.ok and invalid.reason=='bad-argument')\n"
		"assert(pangea.object.deleteResult(handle).ok)\n"
		"local stale=pangea.object.setVelocityResult(handle,{x=0,y=0,z=0})\n"
		"assert(not stale.ok and stale.reason=='bad-argument' and not pangea.object.exists(handle))\n"
		"return {}\n",
		handle.id, handle.generation);
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(nearly_equal(visual.position.x, 4) && nearly_equal(visual.position.y, 5) && nearly_equal(visual.position.z, 6));
	assert(nearly_equal(visual.velocity.x, 1) && nearly_equal(visual.velocity.y, 2) && nearly_equal(visual.velocity.z, 3));
	assert(nearly_equal(visual.rotation.x, 0.1f) && nearly_equal(visual.rotation.y, 0.2f) && nearly_equal(visual.rotation.z, 0.3f));
	assert(nearly_equal(visual.scale, 2.0f) && visual.animation == 3 && strcmp(visual.namedAnimation, "run") == 0 && visual.collisionEnabled);
	assert(PangeaScript_GetRegisteredObjectCount() == 0);
}

static void test_object_owned_resource_cleanup(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	PangeaScript_ResetObjects();
	PangeaScriptVector3 position = {2, 0, 0};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "owned-resource-test",
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	const char* source =
		"return {"
		" onObjectFrame=function(ctx)"
		"   pangea.time.after(10, function() error('owned timer survived') end);"
		"   pangea.task.start(function() pangea.task.wait(10); error('owned task survived') end);"
		"   pangea.events.on('owned', function() error('owned subscription survived') end)"
		" end,"
		" onFrame=function()"
		"   local diagnostics=pangea.api.diagnostics();"
		"   assert(diagnostics.activeTimers==0 and diagnostics.activeTasks==0 and diagnostics.activeSubscriptions==0)"
		" end"
		"}";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptObjectFrameContext context =
	{
		.levelNum = 1,
		.object = handle,
		.position = position,
		.objectType = "owned-resource-test",
		.event = "spawn",
	};
	PangeaScriptObjectFrameResult result = {0};
	assert(PangeaScriptBackend_CallObjectFrameHook(backend, &context, &result, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptBackend_ClearObjectState(backend, handle);
	PangeaScriptFrameContext frame = {.levelNum = 1, .levelTimeSeconds = 0.0f};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void test_public_handle_generation_reset(void)
{
	PangeaScriptVector3 position = {2, 0, 0};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "generation-reset-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle oldHandle = {0};
	PangeaScriptObjectHandle newHandle = {0};

	PangeaScript_ResetObjects();
	PangeaScriptObjectRegistration invalidRegistration = registration;
	invalidRegistration.tagCount = 1;
	assert(PangeaScript_RegisterObject(&invalidRegistration, &oldHandle) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_RegisterObject(&registration, &oldHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ObjectExists(oldHandle));
	PangeaScript_ResetObjects();
	assert(!PangeaScript_ObjectExists(oldHandle));
	assert(PangeaScript_RegisterObject(&registration, &newHandle) == PANGEA_SCRIPT_OK);
	assert(newHandle.id == oldHandle.id);
	assert(newHandle.generation != oldHandle.generation);
	assert(!PangeaScript_ObjectExists(oldHandle));
	assert(PangeaScript_ObjectExists(newHandle));
}

static void test_config_reference_validation(void)
{
	static PangeaConfig config;
	char error[512] = {0};
	const char* unknownReplacement =
		"{\"version\":1,\"levels\":{\"1\":{"
		"\"customObjects\":[{\"id\":\"custom.valid\",\"visual\":{\"kind\":\"customDisplayGroup\",\"modelPath\":\"Data/Scripts/assets/models/test.bg3d\",\"modelObject\":0}}],"
		"\"terrainReplacements\":[{\"itemIndex\":1,\"nativeType\":2,\"x\":0,\"z\":0,\"customObjectId\":\"custom.missing\"}]}}}";
	assert(PangeaScript_ParseConfig(unknownReplacement, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(error, "unknown custom object") != NULL);

	memset(error, 0, sizeof(error));
	const char* trailingData = "{\"version\":1,\"levels\":{}} trailing";
	assert(PangeaScript_ParseConfig(trailingData, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(error, "trailing data") != NULL);

	memset(error, 0, sizeof(error));
	const char* validMapReplacement =
		"{\"version\":1,\"levels\":{\"1\":{"
		"\"customObjects\":[{\"id\":\"custom.valid\",\"visual\":{\"kind\":\"customDisplayGroup\",\"modelPath\":\"Data/Scripts/assets/models/test.bg3d\",\"modelObject\":0}}],"
		"\"mapReplacements\":[{\"itemIndex\":4,\"nativeType\":9,\"x\":12,\"y\":24,\"customObjectId\":\"custom.valid\",\"strict\":true}]}}}";
	assert(PangeaScript_ParseConfig(validMapReplacement, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(config.level.mapReplacementCount == 1);
	assert(config.level.mapReplacements[0].itemIndex == 4);
	assert(config.level.mapReplacements[0].nativeType == 9);
	assert(config.level.mapReplacements[0].x == 12.0f && config.level.mapReplacements[0].y == 24.0f);
	assert(config.level.mapReplacements[0].strict);

	const char* invalidAssetPaths[] = {
		"Data/Scripts/assets/models//test.bg3d",
		"Data/Scripts/assets/models/./test.bg3d",
		"Data/Scripts/assets/models/../test.bg3d",
		"Data/Scripts/assets/modelz/test.bg3d",
	};
	for (int pathIndex = 0; pathIndex < (int)(sizeof(invalidAssetPaths) / sizeof(invalidAssetPaths[0])); pathIndex++)
	{
		char invalidAssetPath[1024];
		snprintf(invalidAssetPath, sizeof(invalidAssetPath),
			"{\"version\":1,\"levels\":{\"1\":{\"customObjects\":[{\"id\":\"custom.invalid\",\"visual\":{\"kind\":\"customDisplayGroup\",\"modelPath\":\"%s\",\"modelObject\":0}}]}}}",
			invalidAssetPaths[pathIndex]);
		memset(error, 0, sizeof(error));
		assert(PangeaScript_ParseConfig(invalidAssetPath, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_CONFIG_ERROR);
	}

	memset(error, 0, sizeof(error));
	const char* invalidScale =
		"{\"version\":1,\"levels\":{\"1\":{"
		"\"customObjects\":[{\"id\":\"custom.invalid\",\"visual\":{\"kind\":\"customDisplayGroup\",\"modelPath\":\"Data/Scripts/assets/models/test.bg3d\",\"modelObject\":0,\"scale\":1e999}}]}}}";
	assert(PangeaScript_ParseConfig(invalidScale, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(error, "finite") != NULL);

	memset(error, 0, sizeof(error));
	const char* duplicateTerrain =
		"{\"version\":1,\"levels\":{\"1\":{"
		"\"customObjects\":[{\"id\":\"custom.valid\",\"visual\":{\"kind\":\"customDisplayGroup\",\"modelPath\":\"Data/Scripts/assets/models/test.bg3d\",\"modelObject\":0}}],"
		"\"terrainReplacements\":["
		"{\"itemIndex\":1,\"nativeType\":2,\"x\":0,\"z\":0,\"customObjectId\":\"custom.valid\"},"
		"{\"itemIndex\":1,\"nativeType\":2,\"x\":0.25,\"z\":0,\"customObjectId\":\"custom.valid\"}]}}}";
	assert(PangeaScript_ParseConfig(duplicateTerrain, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(error, "Duplicate terrain replacement") != NULL);

	memset(error, 0, sizeof(error));
	const char* invalidPlacement =
		"{\"version\":1,\"levels\":{\"1\":{"
		"\"customObjects\":[{\"id\":\"custom.valid\",\"visual\":{\"kind\":\"customDisplayGroup\",\"modelPath\":\"Data/Scripts/assets/models/test.bg3d\",\"modelObject\":0}}],"
		"\"splineReplacements\":[{\"splineNum\":1,\"itemIndex\":2,\"nativeType\":3,\"placement\":2,\"customObjectId\":\"custom.valid\"}]}}}";
	assert(PangeaScript_ParseConfig(invalidPlacement, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(error, "between 0 and 1") != NULL);

	memset(error, 0, sizeof(error));
	const char* invalidInteger =
		"{\"version\":1,\"levels\":{\"1\":{"
		"\"customObjects\":[{\"id\":\"custom.invalid\",\"visual\":{\"kind\":\"customDisplayGroup\",\"modelPath\":\"Data/Scripts/assets/models/test.bg3d\",\"modelObject\":1.5}}]}}}";
	assert(PangeaScript_ParseConfig(invalidInteger, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(error, "32-bit integer") != NULL);

	memset(error, 0, sizeof(error));
	const char* invalidCollisionBounds =
		"{\"version\":1,\"levels\":{\"1\":{"
		"\"customObjects\":[{\"id\":\"custom.invalid\",\"visual\":{\"kind\":\"customDisplayGroup\",\"modelPath\":\"Data/Scripts/assets/models/test.bg3d\",\"modelObject\":0},"
		"\"collision\":{\"kind\":\"preset\",\"preset\":\"solidBox\",\"bounds\":{\"width\":0,\"height\":2,\"depth\":2}}}]}}}";
	assert(PangeaScript_ParseConfig(invalidCollisionBounds, 1, &config, error, sizeof(error)) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(error, "collision bounds") != NULL);
}

static void test_otto_humans_jump_sample(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"local HUMAN_TAG = 'ottomatic.human'\n"
		"local SCIENTIST_TAG = 'ottomatic.human.scientist'\n"
		"local function hasTag(tags, tag) for _, value in ipairs(tags) do if value == tag then return true end end return false end\n"
		"local function getBobHeight(tags) if hasTag(tags, SCIENTIST_TAG) then return 56 else return 32 end end\n"
		"return { onObjectFrame = function(ctx)\n"
		"  if not hasTag(ctx.tags, HUMAN_TAG) then return end\n"
		"  return\n"
		"end }";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	const char* scientistTags[] = {"ottomatic.human", "ottomatic.human.scientist"};
	PangeaScriptObjectFrameContext scientist =
	{
		.levelNum = 3,
		.levelTimeSeconds = 0.19634954f,
		.objectType = "ottomatic.human.scientist",
		.tags = scientistTags,
		.tagCount = 2,
	};
	PangeaScriptObjectFrameResult result = {0};
	assert(PangeaScriptBackend_CallObjectFrameHook(backend, &scientist, &result, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(!result.hasPositionOffset);

	const char* unrelatedTags[] = {"ottomatic.enemy"};
	PangeaScriptObjectFrameContext unrelated = scientist;
	unrelated.tags = unrelatedTags;
	unrelated.tagCount = 1;
	result = (PangeaScriptObjectFrameResult){0};
	assert(PangeaScriptBackend_CallObjectFrameHook(backend, &unrelated, &result, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(!result.hasPositionOffset);

	const char* legacySource =
		"return { onObjectFrame = function() return { positionOffset = { x=0, y=1, z=0 } } end }";
	assert(PangeaScriptBackend_Load(backend, legacySource, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallObjectFrameHook(backend, &scientist, &result, error, errorCapacity) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(strstr(error, "must return nil") != NULL);
}

static void test_hover_beacon_sample(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	PangeaScript_ResetObjects();
	TestVisualObject beacon = {.position = {10, 100, 30}};
	const char* tags[] = {"editor.custom.hoverBeacon"};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &beacon,
		.ops = &kTestVisualObjectOps,
		.objectType = "sample.hoverBeacon",
		.tags = tags,
		.tagCount = 1,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	const char* source =
		"local origins = {}\n"
		"local hoverBeacon = {}\n"
		"function hoverBeacon.onUpdate(self, ctx)\n"
		"  local current = pangea.object.position(self.handle); if not current then return end\n"
		"  local origin = origins[self.handle.id]; if not origin then origin = current; origins[self.handle.id] = origin end\n"
		"  local wave = math.sin(ctx.levelTimeSeconds * 4) * 16\n"
		"  pangea.object.setPosition(self.handle, {x=origin.x, y=origin.y+wave, z=origin.z})\n"
		"  pangea.object.setRotation(self.handle, {x=0, y=ctx.levelTimeSeconds*2, z=0})\n"
		"end\n"
		"return { onObjectFrame = function(ctx) if ctx.objectType == 'sample.hoverBeacon' then hoverBeacon.onUpdate({handle=ctx.object}, ctx) end end }";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptObjectFrameContext context =
	{
		.levelNum = 3,
		.levelTimeSeconds = 0.39269908f,
		.object = handle,
		.position = beacon.position,
		.objectType = "sample.hoverBeacon",
		.tags = tags,
		.tagCount = 1,
		.event = "update",
	};
	PangeaScriptObjectFrameResult result = {0};
	assert(PangeaScriptBackend_CallObjectFrameHook(backend, &context, &result, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(nearly_equal(beacon.position.x, 10));
	assert(nearly_equal(beacon.position.y, 116));
	assert(nearly_equal(beacon.position.z, 30));
	assert(nearly_equal(beacon.rotation.x, 0));
	assert(nearly_equal(beacon.rotation.y, 0.78539816f));
	assert(nearly_equal(beacon.rotation.z, 0));
	assert(PangeaScript_UnregisterObject(handle));
}

static void test_timer_reset_and_diagnostics(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"local fired = false\n"
		"pangea.time.after(0.1, function() fired = true end)\n"
		"pangea.task.start(function() pangea.task.wait(10) end)\n"
		"pangea.events.on('level-owned', function() end)\n"
		"local diagnostics = pangea.api.diagnostics()\n"
		"assert(diagnostics.activeTimers == 1 and diagnostics.activeTasks == 1 and diagnostics.activeSubscriptions == 1 and diagnostics.memoryUsedBytes > 0)\n"
		"return { onLevelLoad = function() local reset = pangea.api.diagnostics(); assert(reset.activeTimers == 0 and reset.activeTasks == 0 and reset.activeSubscriptions == 0 and not fired) end, onFrame = function() assert(not fired) end }";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptLevelContext level = {.levelNum = 2, .levelName = "reset"};
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_LOAD, &level, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 2, .levelTimeSeconds = 1.0f};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void test_timer_capacity(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"for _ = 1, 128 do pangea.time.after(10, function() end) end\n"
		"local ok, message = pcall(function() pangea.time.after(10, function() end) end)\n"
		"assert(not ok and string.find(message, 'timer capacity exceeded', 1, true))\n"
		"assert(not pangea.time.cancel(999999))\n"
		"return {}";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void test_reload_isolation(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* firstSource = "leakedGlobal = 42; return {}";
	const char* secondSource = "assert(leakedGlobal == nil); return {}";
	assert(PangeaScriptBackend_Load(backend, firstSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_Load(backend, secondSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void prepare_script_test_directories(void)
{
#ifdef __EMSCRIPTEN__
	EM_ASM({ FS.mkdirTree("Data/Scripts/config"); FS.mkdirTree("Data/Scripts/dist"); FS.mkdirTree("Data/Scripts/persistence"); });
#else
	system("mkdir -p Data/Scripts/config Data/Scripts/dist Data/Scripts/persistence");
#endif
}

static void write_script(const char* path, const char* source)
{
	prepare_script_test_directories();
	FILE* file = fopen(path, "w");
	assert(file != NULL);
	assert(fputs(source, file) >= 0);
	assert(fclose(file) == 0);
}

static void test_public_host_failure_recovery(void)
{
	const char* path = "Data/Scripts/dist/host-test.lua";
	write_script(path, "return { onFrame = function() error('host failure') end }");
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {0};
	for (int i = 0; i < 5; i++)
		assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(PangeaScript_GetStatusScriptsDisabled());
	assert(PangeaScript_GetStatusErrorCount() >= 5);
	assert(strstr(PangeaScript_GetStatusLastError(), "host failure") != NULL);
	write_script(path, "return { onFrame = function() end }");
	assert(PangeaScript_Reload() == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_GetStatusScriptsDisabled());
	assert(PangeaScript_GetStatusLastError()[0] == '\0');
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	assert(remove(path) == 0);
}

static void test_public_object_lifecycle(void)
{
	const char* path = "Data/Scripts/dist/object-lifecycle-test.lua";
	write_script(path,
		"local checkpointCount = 0; local streamInCount = 0; local completionCount = 0; return { onObjectFrame = function(ctx)"
		" local state = pangea.object.state(ctx.object);"
		" if ctx.event == 'spawn' then state.value = 7; state.nested = {score=3, labels={'baseline'}}; pangea.time.after(100, function() end); pangea.events.on('checkpoint-test', function() end) end;"
		" if ctx.event == 'animationEvent' then assert(ctx.eventValue == 37) end;"
		" if ctx.event == 'animationComplete' then completionCount = completionCount + 1; assert(ctx.eventValue == nil) end;"
		" if ctx.event == 'checkpointReset' then checkpointCount = checkpointCount + 1; local diagnostics = pangea.api.diagnostics(); if checkpointCount == 1 then assert(diagnostics.activeTimers == 1 and diagnostics.activeSubscriptions == 1) else assert(diagnostics.activeTimers == 0 and diagnostics.activeSubscriptions == 0) end; assert(state.value == 7 and state.nested.score == 3 and state.nested.labels[1] == 'baseline'); if checkpointCount == 1 then state.value = 9; state.nested.score = 9 else state.value = 11; state.nested.score = 11 end end;"
		" if ctx.event == 'streamIn' then streamInCount = streamInCount + 1; local diagnostics = pangea.api.diagnostics(); assert(diagnostics.activeTimers == 0 and diagnostics.activeSubscriptions == 0); local source = pangea.object.source(ctx.object); assert(source.kind == 'terrain' and source.itemIndex == 12 and source.nativeType == 7 and source.x == 10 and source.y == 2 and source.z == 30); assert(streamInCount <= 3); if streamInCount == 1 then assert(state.value == 11) else assert(state.value == nil) end end;"
		" if ctx.event == 'activate' then assert(checkpointCount == 2 and streamInCount == 1 and state.value == 11); state.value = 8 end;"
		" end }"
	);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptVector3 position = {1, 2, 3};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "lifecycle-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	void* nativeObject = NULL;
	assert(PangeaScript_GetObjectNativeObject(handle, &nativeObject));
	assert(nativeObject == &position);
	PangeaScriptObjectSource source = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN,
		.itemIndex = 12,
		.nativeType = 7,
		.x = 10.0f,
		.y = 2.0f,
		.z = 30.0f,
	};
	assert(PangeaScript_AssociateObjectSource(handle, &source) == PANGEA_SCRIPT_OK);
	PangeaScriptObjectSource storedSource = {0};
	assert(PangeaScript_GetObjectSource(handle, &storedSource));
	assert(storedSource.kind == PANGEA_SCRIPT_SOURCE_TERRAIN);
	assert(storedSource.itemIndex == 12 && storedSource.nativeType == 7);
	assert(storedSource.x == 10.0f && storedSource.y == 2.0f && storedSource.z == 30.0f);
	PangeaScriptObjectHandle foundHandle = {0};
	assert(PangeaScript_FindObjectBySource(&source, &foundHandle));
	assert(foundHandle.id == handle.id && foundHandle.generation == handle.generation);
	PangeaScriptObjectSource invalidSource = source;
	invalidSource.itemIndex = -1;
	assert(!PangeaScript_FindObjectBySource(&invalidSource, &foundHandle));
	invalidSource = source;
	invalidSource.placement = 0.5f;
	assert(PangeaScript_AssociateObjectSource(handle, &invalidSource) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(!PangeaScript_FindObjectBySource(&invalidSource, &foundHandle));
	PangeaScriptObjectSource mapSource = source;
	mapSource.kind = PANGEA_SCRIPT_SOURCE_MAP;
	mapSource.y = 24.0f;
	mapSource.z = 0.0f;
	assert(PangeaScript_AssociateObjectSource(handle, &mapSource) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetObjectSource(handle, &storedSource));
	assert(storedSource.kind == PANGEA_SCRIPT_SOURCE_MAP);
	assert(storedSource.itemIndex == 12 && storedSource.nativeType == 7);
	assert(storedSource.x == 10.0f && storedSource.y == 24.0f && storedSource.z == 0.0f);
	assert(PangeaScript_FindObjectBySource(&mapSource, &foundHandle));
	assert(foundHandle.id == handle.id && foundHandle.generation == handle.generation);
	assert(PangeaScript_AssociateObjectSource(handle, &source) == PANGEA_SCRIPT_OK);
	PangeaScriptObjectHandle refreshedHandle;
	assert(PangeaScript_RegisterObject(&registration, &refreshedHandle) == PANGEA_SCRIPT_OK);
	assert(refreshedHandle.id == handle.id && refreshedHandle.generation == handle.generation);
	assert(PangeaScript_GetObjectSource(refreshedHandle, &storedSource));
	assert(storedSource.kind == PANGEA_SCRIPT_SOURCE_TERRAIN && storedSource.itemIndex == 12);
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	PangeaScript_ResetLifecycleTrace();
	assert(PangeaScript_CallObjectEvent(handle, &frame, "spawn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectEventWithValue(handle, &frame, "animationEvent", 37) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectEvent(handle, &frame, "animationComplete") == PANGEA_SCRIPT_OK);
	PangeaScriptLifecycleTrace lifecycleTrace;
	PangeaScript_GetLifecycleTrace(&lifecycleTrace);
	assert(lifecycleTrace.eventCount == 3 && lifecycleTrace.entryCount == 3 && !lifecycleTrace.overflow);
	PangeaScriptLifecycleTraceEntry lifecycleEntry;
	assert(PangeaScript_GetLifecycleTraceEntry(0, &lifecycleEntry));
	assert(strcmp(lifecycleEntry.eventId, "spawn") == 0);
	assert(strcmp(lifecycleEntry.applicationPhase, "callback") == 0 && lifecycleEntry.order == 0);
	assert(lifecycleEntry.target.id == handle.id && lifecycleEntry.target.generation == handle.generation);
	assert(lifecycleEntry.status == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetLifecycleTraceEntry(1, &lifecycleEntry));
	assert(strcmp(lifecycleEntry.eventId, "animationEvent") == 0);
	assert(strcmp(lifecycleEntry.applicationPhase, "callback") == 0 && lifecycleEntry.order == 1);
	assert(PangeaScript_GetLifecycleTraceEntry(2, &lifecycleEntry));
	assert(strcmp(lifecycleEntry.eventId, "animationComplete") == 0);
	assert(strcmp(lifecycleEntry.applicationPhase, "callback") == 0 && lifecycleEntry.order == 2);
	assert(!PangeaScript_GetLifecycleTraceEntry(3, &lifecycleEntry));
	PangeaScriptLifecycleTraceEntry expectedLifecycleEntries[3];
	for (int index = 0; index < 3; index++)
		assert(PangeaScript_GetLifecycleTraceEntry(index, &expectedLifecycleEntries[index]));
	PangeaScriptLifecycleTraceComparison lifecycleComparison;
	assert(PangeaScript_CompareLifecycleTrace(&lifecycleTrace, expectedLifecycleEntries, 3, &lifecycleComparison) == PANGEA_SCRIPT_OK);
	assert(lifecycleComparison.matches);
	assert(lifecycleComparison.firstMismatchIndex == UINT32_MAX);
	for (int index = 0; index < 3; index++)
		expectedLifecycleEntries[index].target = (PangeaScriptObjectHandle){99, 20};
	assert(PangeaScript_CompareNormalizedLifecycleTrace(&lifecycleTrace, expectedLifecycleEntries, 3, &lifecycleComparison) == PANGEA_SCRIPT_OK);
	assert(lifecycleComparison.matches);
	assert(lifecycleComparison.firstMismatchIndex == UINT32_MAX);
	for (int index = 0; index < 3; index++)
		expectedLifecycleEntries[index].target = lifecycleEntry.target;
	expectedLifecycleEntries[1].status = PANGEA_SCRIPT_BAD_ARGUMENT;
	assert(PangeaScript_CompareLifecycleTrace(&lifecycleTrace, expectedLifecycleEntries, 3, &lifecycleComparison) == PANGEA_SCRIPT_OK);
	assert(!lifecycleComparison.matches);
	assert(lifecycleComparison.firstMismatchIndex == 1);
	assert(strcmp(lifecycleComparison.expectedEventId, "animationEvent") == 0);
	assert(strcmp(lifecycleComparison.actualEventId, "animationEvent") == 0);
	expectedLifecycleEntries[1].status = PANGEA_SCRIPT_OK;
	PangeaScriptLifecycleTrace overflowLifecycleTrace = lifecycleTrace;
	overflowLifecycleTrace.overflow = true;
	assert(PangeaScript_CompareLifecycleTrace(&overflowLifecycleTrace, expectedLifecycleEntries, 3, &lifecycleComparison) == PANGEA_SCRIPT_OK);
	assert(!lifecycleComparison.matches && lifecycleComparison.firstMismatchIndex == 3);
	assert(PangeaScript_CompareLifecycleTrace(NULL, expectedLifecycleEntries, 3, &lifecycleComparison) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CompareLifecycleTrace(&lifecycleTrace, expectedLifecycleEntries, 2, &lifecycleComparison) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CompareLifecycleTrace(&lifecycleTrace, expectedLifecycleEntries, 3, NULL) == PANGEA_SCRIPT_BAD_ARGUMENT);
	PangeaScript_ResetLifecycleTrace();
	PangeaScriptFrameContext adventureFrame = {.levelNum = 1, .frameNum = 1, .mode = "adventure"};
	PangeaScriptFrameContext raceFrame = adventureFrame;
	raceFrame.frameNum = 2;
	raceFrame.mode = "race";
	assert(PangeaScript_CallFrameHook(&adventureFrame) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallFrameHook(&raceFrame) == PANGEA_SCRIPT_OK);
	PangeaScript_GetLifecycleTrace(&lifecycleTrace);
	assert(lifecycleTrace.eventCount == 1 && lifecycleTrace.entryCount == 1 && !lifecycleTrace.overflow);
	assert(PangeaScript_GetLifecycleTraceEntry(0, &lifecycleEntry));
	assert(strcmp(lifecycleEntry.eventId, "modeTransition") == 0);
	assert(strcmp(lifecycleEntry.applicationPhase, "mode") == 0);
	PangeaScriptLifecycleTraceEntry expectedModeEntry = lifecycleEntry;
	assert(PangeaScript_CompareNormalizedLifecycleTrace(&lifecycleTrace, &expectedModeEntry, 1, &lifecycleComparison) == PANGEA_SCRIPT_OK);
	assert(lifecycleComparison.matches && lifecycleComparison.firstMismatchIndex == UINT32_MAX);
	PangeaScriptStatus firstCheckpointStatus = PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET);
	if (firstCheckpointStatus != PANGEA_SCRIPT_OK)
		fprintf(stderr, "first checkpoint failed: %s\n", PangeaScript_GetLastError());
	assert(firstCheckpointStatus == PANGEA_SCRIPT_OK);
	PangeaScriptStatus secondCheckpointStatus = PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET);
	if (secondCheckpointStatus != PANGEA_SCRIPT_OK)
		fprintf(stderr, "second checkpoint failed: %s\n", PangeaScript_GetLastError());
	assert(secondCheckpointStatus == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_STREAM_IN) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_ACTIVATE) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ObjectExists(handle));
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_DEACTIVATE) == PANGEA_SCRIPT_OK);
	PangeaScriptObjectFrameResult inactiveResult;
	assert(PangeaScript_CallObjectFrame(handle, &frame, &inactiveResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_STREAM_IN) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectFrame(handle, &frame, &inactiveResult) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectEvent(handle, &frame, "not-a-contract-event") == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_STREAM_OUT) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(handle));
	assert(!PangeaScript_GetObjectSource(handle, &storedSource));
	assert(PangeaScript_CallObjectEvent(handle, &frame, "streamIn") == PANGEA_SCRIPT_BAD_ARGUMENT);
	PangeaScriptVector3 recreatedPosition = {4, 5, 6};
	registration.nativeObject = &recreatedPosition;
	PangeaScriptObjectHandle recreatedHandle;
	assert(PangeaScript_RegisterObject(&registration, &recreatedHandle) == PANGEA_SCRIPT_OK);
	assert(recreatedHandle.id == handle.id);
	assert(recreatedHandle.generation != handle.generation);
	assert(PangeaScript_AssociateObjectSource(recreatedHandle, &source) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectEvent(recreatedHandle, &frame, "streamIn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ObjectExists(recreatedHandle));
	PangeaScriptObjectHandle unloadHandle;
	assert(PangeaScript_RegisterObject(&registration, &unloadHandle) == PANGEA_SCRIPT_OK);
	PangeaScriptLevelContext unloadContext = {.levelNum = 1, .levelName = "lifecycle"};
	assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, &unloadContext) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(handle));
	assert(!PangeaScript_ObjectExists(unloadHandle));
	assert(PangeaScript_CallObjectEvent(handle, &frame, "streamIn") == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_DESTROY) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(remove(path) == 0);
}

static void test_atomic_checkpoint_failure(void)
{
	const char* path = "Data/Scripts/dist/checkpoint-atomic-test.lua";
	write_script(path,
		"local resetCount = 0; return { onObjectFrame = function(ctx)"
		" local state = pangea.object.state(ctx.object);"
		" if ctx.event == 'spawn' then state.value = 7; state.unsupported = function() end; state.cycle = {}; state.cycle.self = state.cycle end;"
		" if ctx.event == 'checkpointReset' then resetCount = resetCount + 1;"
		"   if resetCount == 1 then assert(state.value == 7 and state.unsupported ~= nil and state.cycle.self == state.cycle); state.unsupported = nil; state.cycle = nil; state.value = 9"
		"   else assert(state.value == 9) end"
		" end end }"
	);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptVector3 position = {0, 0, 0};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "checkpoint-atomic-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {0};
	assert(PangeaScript_CallObjectEvent(handle, &frame, "spawn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_CHECKPOINT_RESET) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_UnregisterObject(handle));
	assert(remove(path) == 0);
}

static void test_public_owned_child_cleanup(void)
{
	const char* path = "Data/Scripts/dist/owned-child-lifecycle-test.lua";
	write_script(path,
		"local destroyed = 0; local frameChecks = 0; return {"
		" onObjectFrame=function(ctx)"
		"   if ctx.event == 'spawn' and ctx.objectType == 'owned-parent' then"
		"     local child = pangea.spawn.scripted('owned-child', {x=4,y=5,z=6}); assert(child)"
		"   elseif ctx.event == 'destroy' and ctx.objectType == 'owned-child' then"
		"     destroyed = destroyed + 1"
		"   end"
		" end,"
		" onFrame=function() frameChecks = frameChecks + 1; assert(destroyed == frameChecks) end"
		" }");
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScript_ResetObjects();
	PangeaScriptVector3 position = {1, 2, 3};
	const PangeaScriptObjectOps deleteOps =
	{
		.getPosition = test_object_position,
		.deleteObject = test_delete_object,
	};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &deleteOps,
		.objectType = "owned-parent",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle parentHandle;
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	assert(PangeaScript_RegisterObject(&registration, &parentHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectEvent(parentHandle, &frame, "spawn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetRegisteredObjectCount() == 2);
	assert(PangeaScript_ApplyObjectLifecycle(parentHandle, &frame, PANGEA_SCRIPT_OBJECT_DESTROY) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetRegisteredObjectCount() == 0);
	assert(!PangeaScript_ObjectExists(parentHandle));
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	PangeaScriptObjectHandle directDeleteParent;
	assert(PangeaScript_RegisterObject(&registration, &directDeleteParent) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectEvent(directDeleteParent, &frame, "spawn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetRegisteredObjectCount() == 2);
	assert(PangeaScript_DeleteObject(directDeleteParent));
	assert(PangeaScript_GetRegisteredObjectCount() == 0);
	assert(!PangeaScript_ObjectExists(directDeleteParent));
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	assert(remove(path) == 0);
}

static void test_active_command_lifecycle(void)
{
	const char* path = "Data/Scripts/dist/active-command-lifecycle-test.lua";
	write_script(path,
		"local activated = 0; local deactivated = 0; local failDeactivate = true; return {"
		" onObjectFrame=function(ctx)"
		"   if ctx.event == 'spawn' then"
		"     pangea.time.after(100, function() error('owned timer survived deactivation') end)"
		"   elseif ctx.event == 'deactivate' then"
		"     if failDeactivate then failDeactivate = false; error('deactivation rejected') end"
		"     deactivated = deactivated + 1; assert(pangea.api.diagnostics().activeTimers == 1)"
		"   elseif ctx.event == 'activate' then"
		"     activated = activated + 1; assert(pangea.api.diagnostics().activeTimers == 0)"
		"   elseif ctx.event == 'update' and activated == 0 then"
		"     local rejected = pcall(function() assert(pangea.object.setActive(ctx.object, false)) end); assert(not rejected)"
		"     assert(pangea.object.setActive(ctx.object, false))"
		"     assert(pangea.object.setActive(ctx.object, true))"
		"     assert(activated == 1 and deactivated == 1)"
		"   end"
		" end"
		" }"
	);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScript_ResetObjects();
	PangeaScriptVector3 position = {1, 2, 3};
	PangeaScriptObjectRegistration registration = {
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "active-command-lifecycle-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectEvent(handle, &frame, "spawn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectFrame(handle, &frame, &(PangeaScriptObjectFrameResult){0}) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ObjectExists(handle));
	assert(PangeaScript_CallObjectFrame(handle, &frame, &(PangeaScriptObjectFrameResult){0}) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_UnregisterObject(handle));
	assert(remove(path) == 0);
}

static void test_public_custom_pickup_trigger(void)
{
	const char* scriptPath = "Data/Scripts/dist/custom-pickup.lua";
	const char* configPath = "Data/Scripts/config/custom-pickup.json";
	prepare_script_test_directories();
	write_script(scriptPath,
		"return {"
		" onTriggerEnter = function(ctx)"
		"   assert(ctx.triggerId == 'custom.pickup' and ctx.position.x == 12 and ctx.playerNum == 3)"
		"   return {handled=true, solid=false}"
		" end,"
		" onPickupCollected = function(ctx)"
		"   assert(ctx.pickupId == 'custom.pickup' and ctx.pickup.id > 0 and ctx.pickup.generation > 0)"
		"   assert(ctx.player.id > 0 and ctx.player.generation > 0 and ctx.playerNum == 3)"
		"   assert(ctx.position.x == 12 and ctx.position.y == 4 and ctx.position.z == 8)"
		"   return {handled=true, consumePickup=true, scoreDelta=4}"
		" end"
		"}");
	write_script(configPath,
		"{\"version\":1,\"levels\":{\"1\":{"
		"\"script\":\"Data/Scripts/dist/custom-pickup.lua\","
		"\"customObjects\":[{\"id\":\"custom.pickup\","
		"\"collision\":{\"preset\":\"pickup\",\"bounds\":{\"width\":2,\"height\":3,\"depth\":4}}}],"
		"\"mapReplacements\":[{\"itemIndex\":4,\"nativeType\":9,\"x\":12,\"y\":8,\"customObjectId\":\"custom.pickup\"}]"
		"}}}");

	assert(PangeaScript_SetConfigPath(configPath) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_OK);
	const PangeaScriptCustomObjectDefinition* definition =
		PangeaScript_GetCustomObjectDefinition("custom.pickup");
	assert(definition != NULL && definition->collisionPreset == PANGEA_SCRIPT_COLLISION_PICKUP);
	const PangeaScriptMapReplacement* mapReplacement =
		PangeaScript_GetMapReplacement(4, 9, 12.0f, 8.0f);
	assert(mapReplacement != NULL && strcmp(mapReplacement->customObjectId, "custom.pickup") == 0);
	assert(PangeaScript_GetMapReplacement(4, 9, 12.0f, 9.0f) == NULL);

	PangeaScriptVector3 position = {12, 4, 8};
	const PangeaScriptObjectOps pickupOps = {
		.getPosition = test_object_position,
		.deleteObject = test_delete_object,
	};
	const PangeaScriptObjectRegistration registration = {
		.nativeObject = &position,
		.ops = &pickupOps,
		.objectType = "custom.pickup",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	PangeaScriptVector3 playerPosition = {2, 3, 4};
	PangeaScriptObjectRegistration playerRegistration = registration;
	playerRegistration.nativeObject = &playerPosition;
	playerRegistration.objectType = "test.player";
	PangeaScriptObjectHandle playerHandle;
	assert(PangeaScript_RegisterObject(&playerRegistration, &playerHandle) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 4};
	assert(!PangeaScript_CallObjectTriggerWithOtherAndPlayer(handle, &frame, 0, true, playerHandle, 3));
	assert(PangeaScript_ObjectExists(handle));
	assert(PangeaScript_DeleteObject(handle));
	PangeaScriptObjectHandle replacementHandle;
	assert(PangeaScript_RegisterObject(&registration, &replacementHandle) == PANGEA_SCRIPT_OK);
	assert(replacementHandle.id == handle.id && replacementHandle.generation != handle.generation);
	assert(PangeaScript_ApplyDeferredActions(&frame) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(handle));
	assert(PangeaScript_ObjectExists(replacementHandle));
	PangeaScriptPickupContext stalePickup = {
		.levelNum = 1,
		.playerNum = 3,
		.pickupId = "custom.pickup",
		.pickup = handle,
		.player = playerHandle,
		.position = position,
	};
	PangeaScriptPickupResult stalePickupResult;
	assert(PangeaScript_CallPickupHook(&stalePickup, &stalePickupResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_DeleteObject(replacementHandle));
	PangeaScriptWeaponHitContext staleHit = {
		.levelNum = 1,
		.target = handle,
		.position = position,
	};
	PangeaScriptWeaponHitResult staleHitResult;
	assert(PangeaScript_CallWeaponHitHook(&staleHit, &staleHitResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallObjectTrigger(handle, &frame, 0, true));

	assert(remove(scriptPath) == 0);
	assert(remove(configPath) == 0);
	printf("Configured custom pickup trigger tests passed!\n");
}

static void test_public_trigger_contact_lifecycle(void)
{
	const char* path = "Data/Scripts/dist/trigger-contact-lifecycle.lua";
	write_script(path,
		"local enters = 0; local stays = 0; local exits = 0; return {"
		" onObjectFrame=function(ctx)"
		"   if ctx.event == 'triggerEnter' then enters = enters + 1 end"
		"   if ctx.event == 'triggerStay' then stays = stays + 1 end"
		"   if ctx.event == 'triggerExit' then exits = exits + 1 end"
		"   if ctx.event == 'triggerEnter' and ctx.other then assert(ctx.other.id > 0 and ctx.other.generation > 0 and ctx.sideBits == 0) end"
		" end,"
		" onTriggerEnter=function(ctx) return {deleteOther = ctx.other ~= nil} end,"
		" onFrame=function() assert(enters == 4 and stays == 2 and exits == 2) end"
		" }");
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScript_ResetObjects();
	PangeaScriptVector3 position = {1, 2, 3};
	PangeaScriptObjectRegistration registration = {
		.nativeObject = &position,
		.ops = &kTestSpawnObjectOps,
		.objectType = "trigger-contact-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	assert(PangeaScript_CallObjectTrigger(handle, &frame, 0, true));
	assert(PangeaScript_CallObjectTrigger(handle, &frame, 0, true));
	frame.frameNum = 2;
	assert(PangeaScript_CallObjectTrigger(handle, &frame, 0, true));
	frame.frameNum = 4;
	PangeaScript_ExpireTriggerContacts(&frame);
		assert(PangeaScript_CallObjectTrigger(handle, &frame, 0, true));
	frame.frameNum = 8;
	PangeaScript_ExpireTriggerContacts(&frame);
		assert(PangeaScript_CallObjectTrigger(handle, &frame, 0, true));
	PangeaScriptVector3 otherPosition = {4, 5, 6};
	registration.nativeObject = &otherPosition;
	PangeaScriptObjectHandle otherHandle;
	assert(PangeaScript_RegisterObject(&registration, &otherHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallObjectTriggerWithOther(handle, &frame, 0, true, otherHandle));
	assert(!PangeaScript_ObjectExists(otherHandle));
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_UnregisterObject(handle));
	assert(remove(path) == 0);
}

static void test_level_load_delivers_destroy_before_reset(void)
{
	const char* path = "Data/Scripts/dist/level-load-cleanup-test.lua";
	write_script(path,
		"local destroyed = 0; return {"
		" onObjectFrame=function(ctx) if ctx.event == 'destroy' then destroyed = destroyed + 1 end end,"
		" onLevelLoad=function() assert(destroyed == 1) end"
		" }");
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptVector3 position = {1, 2, 3};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "level-load-cleanup-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	PangeaScriptLevelContext level = {.levelNum = 2, .levelName = "next-level"};
	assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, &level) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(handle));
	assert(PangeaScript_CallObjectEvent(handle, &(PangeaScriptFrameContext){.levelNum = 2}, "destroy") == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(remove(path) == 0);
}

static void test_public_player_gameplay_events(void)
{
	const char* path = "Data/Scripts/dist/player-events-test.lua";
	char source[2048];
	PangeaScriptVector3 position = {4, 5, 6};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "player-events-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	PangeaScriptDamageContext damageContext;
	PangeaScriptDamageResult damageResult;
	PangeaScriptPlayerEventContext playerContext;

	write_script(path, "return {}");
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	snprintf(source, sizeof(source),
		"return {"
		" onDamage=function(ctx) assert(ctx.target.id==%d and ctx.target.generation==%u and ctx.source==nil and ctx.cause==4 and ctx.damage==2.0); return {handled=true,applyDamage=false,damage=.25} end,"
		" onDamageApplied=function(ctx) assert(ctx.target.id==%d and ctx.cause==4 and ctx.damage==.25) end,"
		" onDeath=function(ctx) assert(ctx.player.id==%d and ctx.eventValue==7) end,"
		" onPlayerSpawn=function(ctx) assert(ctx.player.id==%d and ctx.position.x==4) end,"
		" onPlayerRespawn=function(ctx) assert(ctx.player.id==%d and ctx.position.z==6) end,"
		" onCheckpointReached=function(ctx) local results=pangea.player.checkpointResults(); assert(ctx.player.id==%d and ctx.eventValue==3 and ctx.position.y==5 and #results==1 and results[1].levelNum==3 and results[1].playerNum==0 and results[1].checkpoint==3) end,"
		" onLapComplete=function(ctx) assert(ctx.player.id==%d and ctx.eventValue==2 and ctx.position.z==6) end,"
		" onRaceFinish=function(ctx) assert(ctx.player.id==%d and ctx.eventValue==1 and ctx.position.x==4) end,"
		" onObjectiveComplete=function(ctx) local results=pangea.player.objectiveResults(); assert(ctx.player.id==%d and ctx.eventValue==0 and ctx.position.y==5 and #results==1 and results[1].levelNum==3 and results[1].playerNum==0 and results[1].outcome==0) end"
		" }",
		handle.id, handle.generation, handle.id, handle.id, handle.id, handle.id, handle.id, handle.id, handle.id, handle.id);
	write_script(path, source);
	assert(PangeaScript_Reload() == PANGEA_SCRIPT_OK);
	PangeaScript_ResetLifecycleTrace();
	damageContext = (PangeaScriptDamageContext)
	{
		.levelNum = 3,
		.playerNum = 0,
		.cause = 4,
		.damage = 2.0f,
		.target = handle,
		.position = position,
	};
	assert(PangeaScript_CallDamageHook(&damageContext, &damageResult) == PANGEA_SCRIPT_OK);
	assert(damageResult.handled && damageResult.hasApplyDamage && !damageResult.applyDamage && damageResult.hasDamage && nearly_equal(damageResult.damage, .25f));
	damageContext.damage = damageResult.damage;
	assert(PangeaScript_CallDamageAppliedHook(&damageContext) == PANGEA_SCRIPT_OK);
	playerContext = (PangeaScriptPlayerEventContext){.levelNum = 3, .playerNum = 0, .eventValue = 7, .player = handle, .position = position};
	assert(PangeaScript_CallPlayerEvent(&playerContext, "onPlayerSpawn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallPlayerEvent(&playerContext, "onPlayerRespawn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallPlayerEvent(&playerContext, "onDeath") == PANGEA_SCRIPT_OK);
	playerContext.eventValue = 3;
	PangeaScriptStatus checkpointStatus = PangeaScript_CallPlayerEvent(&playerContext, "onCheckpointReached");
	assert(checkpointStatus == PANGEA_SCRIPT_OK);
	playerContext.eventValue = 2;
	PangeaScriptStatus lapStatus = PangeaScript_CallPlayerEvent(&playerContext, "onLapComplete");
	assert(lapStatus == PANGEA_SCRIPT_OK);
	playerContext.eventValue = 1;
	PangeaScriptStatus finishStatus = PangeaScript_CallPlayerEvent(&playerContext, "onRaceFinish");
	assert(finishStatus == PANGEA_SCRIPT_OK);
	playerContext.eventValue = 0;
	PangeaScriptStatus objectiveStatus = PangeaScript_CallPlayerEvent(&playerContext, "onObjectiveComplete");
	assert(objectiveStatus == PANGEA_SCRIPT_OK);
	playerContext.eventValue = 3;
	assert(PangeaScript_CallPlayerEvent(&playerContext, "onObjectiveComplete") == PANGEA_SCRIPT_BAD_ARGUMENT);
	playerContext.eventValue = -1;
	assert(PangeaScript_CallPlayerEvent(&playerContext, "onLapComplete") == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallPlayerEvent(&playerContext, "onUnsupportedPlayerEvent") == PANGEA_SCRIPT_BAD_ARGUMENT);
	write_script(path, "return { onDamage=function() return {damage=-1} end }");
	assert(PangeaScript_Reload() == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallDamageHook(&damageContext, &damageResult) == PANGEA_SCRIPT_RUNTIME_ERROR);
	PangeaScriptLifecycleTrace trace;
	PangeaScript_GetLifecycleTrace(&trace);
	assert(trace.eventCount == 11 && trace.entryCount == 11 && !trace.overflow);
	const char* expectedEvents[] = {
		"damage", "damageApplied", "onPlayerSpawn", "onPlayerRespawn", "onDeath",
		"onCheckpointReached", "onLapComplete", "onRaceFinish", "onObjectiveComplete",
		"onUnsupportedPlayerEvent", "damage",
	};
	for (int index = 0; index < 11; index++)
	{
		PangeaScriptLifecycleTraceEntry entry;
		assert(PangeaScript_GetLifecycleTraceEntry(index, &entry));
		assert(strcmp(entry.eventId, expectedEvents[index]) == 0);
		assert(strcmp(entry.applicationPhase, index < 2 || index == 10 ? "damage" : "callback") == 0);
		assert(entry.order == (uint32_t)index);
	}
	assert(PangeaScript_UnregisterObject(handle));
	assert(remove(path) == 0);
}

static void test_public_lifecycle_self_removal(void)
{
	const char* path = "Data/Scripts/dist/self-removing-lifecycle-test.lua";
	write_script(path,
		"return { onObjectFrame = function(ctx)"
		" if ctx.event == 'destroy' or ctx.event == 'streamOut' then"
		"   pangea.object.delete(ctx.object);"
		" end"
		" end }"
	);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptVector3 position = {0, 0, 0};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "self-removing-lifecycle-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle streamHandle;
	assert(PangeaScript_RegisterObject(&registration, &streamHandle) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	assert(PangeaScript_ApplyObjectLifecycle(streamHandle, &frame, PANGEA_SCRIPT_OBJECT_STREAM_OUT) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(streamHandle));

	PangeaScriptObjectHandle destroyHandle;
	assert(PangeaScript_RegisterObject(&registration, &destroyHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(destroyHandle, &frame, PANGEA_SCRIPT_OBJECT_DESTROY) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(destroyHandle));

	PangeaScriptObjectHandle broadcastFirst;
	PangeaScriptObjectHandle broadcastSecond;
	assert(PangeaScript_RegisterObject(&registration, &broadcastFirst) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_RegisterObject(&registration, &broadcastSecond) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycleToAll(&frame, PANGEA_SCRIPT_OBJECT_DESTROY) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(broadcastFirst));
	assert(!PangeaScript_ObjectExists(broadcastSecond));
	assert(remove(path) == 0);
}

static void test_direct_delete_delivers_destroy(void)
{
	const char* path = "Data/Scripts/dist/direct-delete-test.lua";
	write_script(path,
		"local destroyed = 0; local frameDeleted = false; return { onObjectFrame = function(ctx)"
		" if ctx.event == 'destroy' then destroyed = destroyed + 1 end;"
		" if ctx.event == 'update' then assert(pangea.object.delete(ctx.object)); assert(destroyed == 1) end;"
		" end, onFrame = function()"
		" if not frameDeleted then local objects = pangea.object.all(); assert(#objects == 1);"
		" assert(pangea.object.delete(objects[1])); assert(destroyed == 2); frameDeleted = true end;"
		" end }"
	);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptVector3 position = {0, 0, 0};
	const PangeaScriptObjectOps deleteOps =
	{
		.getPosition = test_object_position,
		.deleteObject = test_delete_object,
	};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &deleteOps,
		.objectType = "direct-delete-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	PangeaScriptObjectFrameResult result;
	assert(PangeaScript_CallObjectFrame(handle, &frame, &result) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(handle));
	PangeaScriptVector3 framePosition = {1, 2, 3};
	registration.nativeObject = &framePosition;
	PangeaScriptObjectHandle frameHandle;
	assert(PangeaScript_RegisterObject(&registration, &frameHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(frameHandle));
	assert(remove(path) == 0);
}

static void test_public_lifecycle_failure_cleanup(void)
{
	const char* path = "Data/Scripts/dist/lifecycle-failure-test.lua";
	write_script(path,
		"return { onObjectFrame = function(ctx)"
		" if ctx.event == 'destroy' or ctx.event == 'streamOut' or ctx.event == 'streamIn' then error('lifecycle failure') end"
		" end }"
	);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptVector3 position = {0, 0, 0};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "failed-lifecycle-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	PangeaScriptObjectHandle streamInHandle;
	assert(PangeaScript_RegisterObject(&registration, &streamInHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(streamInHandle, &frame, PANGEA_SCRIPT_OBJECT_STREAM_IN) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(!PangeaScript_ObjectExists(streamInHandle));

	PangeaScriptObjectHandle streamHandle;
	assert(PangeaScript_RegisterObject(&registration, &streamHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(streamHandle, &frame, PANGEA_SCRIPT_OBJECT_STREAM_OUT) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(!PangeaScript_ObjectExists(streamHandle));

	PangeaScriptObjectHandle destroyHandle;
	assert(PangeaScript_RegisterObject(&registration, &destroyHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(destroyHandle, &frame, PANGEA_SCRIPT_OBJECT_DESTROY) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(!PangeaScript_ObjectExists(destroyHandle));

	PangeaScriptObjectHandle broadcastFirst;
	PangeaScriptObjectHandle broadcastSecond;
	assert(PangeaScript_RegisterObject(&registration, &broadcastFirst) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_RegisterObject(&registration, &broadcastSecond) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycleToAll(&frame, PANGEA_SCRIPT_OBJECT_DESTROY) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(!PangeaScript_ObjectExists(broadcastFirst));
	assert(!PangeaScript_ObjectExists(broadcastSecond));
	assert(remove(path) == 0);
}

static void test_public_spawn_failure_cleanup(void)
{
	const char* path = "Data/Scripts/dist/spawn-failure-test.lua";
	write_script(path,
		"return { onObjectFrame = function(ctx)"
		" if ctx.event == 'spawn' then"
		"   pangea.time.after(10, function() error('failed-spawn timer survived') end)"
		"   pangea.task.start(function() pangea.task.wait(10); error('failed-spawn task survived') end)"
		"   pangea.events.on('failed-spawn', function() error('failed-spawn subscription survived') end)"
		"   error('spawn failure')"
		" end"
		" end,"
		" onFrame = function()"
		"   local diagnostics = pangea.api.diagnostics()"
		"   assert(diagnostics.activeTimers == 0 and diagnostics.activeTasks == 0 and diagnostics.activeSubscriptions == 0)"
		" end }"
	);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_Reload() == PANGEA_SCRIPT_OK);
	PangeaScript_ResetObjects();
	PangeaScriptObjectHandle handle = {0};
	assert(PangeaScript_RegisterScriptedObject("spawn-failure-test", 1, 2, 3, &handle) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(handle.id == 0 && handle.generation == 0);
	assert(PangeaScript_GetRegisteredObjectCount() == 0);
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	write_script(path,
		"return { onObjectFrame = function(ctx)"
		" if ctx.event == 'spawn' then pangea.object.delete(ctx.object) end"
		" end }"
	);
	assert(PangeaScript_Reload() == PANGEA_SCRIPT_OK);
	PangeaScript_ResetObjects();
	handle = (PangeaScriptObjectHandle){0};
	assert(PangeaScript_RegisterScriptedObject("spawn-self-remove-test", 4, 5, 6, &handle) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(handle.id == 0 && handle.generation == 0);
	assert(PangeaScript_GetRegisteredObjectCount() == 0);
	assert(PangeaScript_RegisterScriptedObject("spawn-self-remove-no-output-test", 7, 8, 9, NULL) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(PangeaScript_GetRegisteredObjectCount() == 0);
	assert(remove(path) == 0);
}

static void test_scripted_spawn_options_cleanup(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"return { onLevelStart = function()"
		" local invalid = pcall(function() pangea.spawn.scripted('invalid-scale', {x=1,y=2,z=3}, {scale=0}) end)"
		" assert(not invalid and #pangea.object.all() == 0)"
		" local unsupported = pangea.spawn.scripted('unsupported-scale', {x=4,y=5,z=6}, {scale=2})"
		" assert(unsupported == nil and #pangea.object.all() == 0)"
		"end }";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScript_ResetObjects();
	PangeaScriptLevelContext level = {.levelNum = 1, .levelName = "spawn-options"};
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_START, &level, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void test_lifecycle_cleanup_without_position(void)
{
	const char* path = "Data/Scripts/dist/lifecycle-position-fallback-test.lua";
	write_script(path,
		"return { onObjectFrame = function(ctx)"
		" if ctx.event == 'destroy' then assert(ctx.position.x == 0 and ctx.position.y == 0 and ctx.position.z == 0) end"
		" end }"
	);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	int nativeObject = 1;
	const PangeaScriptObjectOps ops = {.getPosition = test_unreadable_object_position};
	const PangeaScriptObjectRegistration registration = {
		.nativeObject = &nativeObject,
		.ops = &ops,
		.objectType = "unreadable-position-object",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(handle, &frame, PANGEA_SCRIPT_OBJECT_DESTROY) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(handle));
	assert(remove(path) == 0);
}

static void test_public_fallback_spawn_callback(void)
{
	const char* path = "Data/Scripts/dist/fallback-spawn-test.lua";
	write_script(path,
		"return {"
		" onObjectFrame=function(ctx)"
		"   if ctx.event == 'spawn' then pangea.object.state(ctx.object).spawned = true end"
		"   if ctx.event == 'update' then assert(pangea.object.setVelocityResult(ctx.object, {x=1,y=2,z=3}).ok); assert(pangea.object.setPositionOffsetResult(ctx.object, {x=0,y=4,z=0}).ok) end"
		" end,"
		" onFrame=function()"
		"   local objects = pangea.object.all(); assert(#objects == 1)"
		"   assert(pangea.object.state(objects[1]).spawned == true)"
		"   assert(pangea.object.setVelocityResult(objects[1], {x=1,y=2,z=3}).ok)"
		"   assert(pangea.object.setRotationResult(objects[1], {x=.1,y=.2,z=.3}).ok)"
		"   assert(pangea.object.setScaleResult(objects[1], 2).ok)"
		"   assert(pangea.object.setAnimationResult(objects[1], 3, 1.5, .25).ok)"
		"   assert(pangea.object.setAnimationResult(objects[1], 'run', 1, 0).ok)"
		" end"
		" }");
	const PangeaScriptGameInfo fallbackGameInfo = {
		.gameId = "FallbackTest",
		.gameName = "Fallback Test",
	};
	assert(PangeaScript_Init(&fallbackGameInfo) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptObjectHandle handle = {0};
	assert(PangeaScript_RegisterScriptedObject("fallback-scripted", 1, 2, 3, &handle) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 1, .frameNum = 1};
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	frame.deltaSeconds = 1.0f;
	PangeaScriptObjectFrameResult objectFrameResult = {0};
	assert(PangeaScript_CallObjectFrame(handle, &frame, &objectFrameResult) == PANGEA_SCRIPT_OK);
	assert(objectFrameResult.hasPositionOffset && nearly_equal(objectFrameResult.positionOffset.y, 4.0f));
	PangeaScriptVector3 movedPosition = {0};
	assert(PangeaScript_GetObjectPosition(handle, &movedPosition));
	assert(nearly_equal(movedPosition.x, 2.0f) && nearly_equal(movedPosition.y, 4.0f) && nearly_equal(movedPosition.z, 6.0f));
	assert(PangeaScript_DeleteObject(handle));
	assert(!PangeaScript_ObjectExists(handle));
	assert(remove(path) == 0);
	PangeaScript_Shutdown();
}

static void test_public_network_gate(void)
{
	const char* path = "Data/Scripts/dist/network-gate-test.lua";
	write_script(path, "return { onFrame = function() end }");
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {0};
	PangeaScript_SetNetworkedMode(true);
	assert(PangeaScript_IsNetworkedMode());
	assert(!PangeaScript_HasRunnableModule());
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_RUNTIME_ERROR);
	PangeaScriptObjectHandle blockedHandle = {0};
	assert(PangeaScript_RegisterScriptedObject("network-blocked", 1, 2, 3, &blockedHandle) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(blockedHandle.id == 0 && blockedHandle.generation == 0);
	PangeaScript_SetNetworkedMode(false);
	assert(!PangeaScript_IsNetworkedMode());
	assert(PangeaScript_HasRunnableModule());
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	assert(remove(path) == 0);
}

static void test_startup_script_path_validation(void)
{
	assert(PangeaScript_SetStartupScript(NULL) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_SetStartupScript("") == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_SetStartupScript("/tmp/main.lua") == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(PangeaScript_SetStartupScript("Data/Scripts/../main.lua") == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(PangeaScript_SetStartupScript("Data/Scripts\\dist\\main.lua") == PANGEA_SCRIPT_CONFIG_ERROR);
}

static void test_command_descriptors(void)
{
	static const char* expectedIds[] = {
		"pangea.player.setHealth",
		"pangea.player.setLives",
		"pangea.player.setScore",
		"pangea.player.setWeaponQuantity",
		"pangea.player.setKey",
		"pangea.player.setCloverCount",
		"pangea.player.setShieldActive",
		"pangea.player.heal",
		"pangea.player.setInvulnerable",
		"pangea.player.setPosition",
		"pangea.player.setVelocity",
		"pangea.player.setForm",
		"pangea.object.setPosition",
		"pangea.object.setPositionOffset",
		"pangea.object.setVelocity",
		"pangea.object.setRotation",
		"pangea.object.setScale",
		"pangea.object.setAnimation",
		"pangea.object.setCollisionEnabled",
		"pangea.object.setActive",
		"pangea.object.delete",
	};
	static const char* expectedCapabilities[] = {
		"player-health",
		"player-lives",
		"player-score",
		"player-inventory",
		"player-keys",
		"player-collectibles",
		"player-shield",
		"player-heal",
		"player-invulnerability",
		"player-position",
		"player-velocity",
		"player-form",
		"object-position",
		"object-position-offset",
		"object-velocity",
		"object-rotation",
		"object-scale",
		"object-animation",
		"object-collision",
		"object-activation",
		"object-delete",
	};
	assert(PangeaScript_GetCommandDescriptorCount() == 21);
	for (int index = 0; index < PangeaScript_GetCommandDescriptorCount(); index++)
	{
		const PangeaScriptCommandDescriptor* descriptor = PangeaScript_GetCommandDescriptor(index);
		assert(descriptor != NULL);
		assert(strcmp(descriptor->id, expectedIds[index]) == 0);
		assert(strcmp(descriptor->capability, expectedCapabilities[index]) == 0);
		assert(descriptor->id != NULL && descriptor->id[0] != '\0');
		assert(descriptor->capability != NULL && descriptor->capability[0] != '\0');
		assert(strcmp(descriptor->authority, "disabled-network") == 0);
		assert(strcmp(descriptor->applicationPhase, "callback") == 0);
		assert(descriptor->validation != NULL && descriptor->validation[0] != '\0');
	}
	assert(PangeaScript_GetCommandDescriptor(-1) == NULL);
	assert(PangeaScript_GetCommandDescriptor(21) == NULL);
	static const char* expectedEventIds[] = {
		"onTriggerEnter", "onPickupCollected", "onWeaponHit", "onDamage", "onDamageApplied", "onDeath", "onPlayerSpawn", "onPlayerRespawn", "onCheckpointReached", "onLapComplete", "onRaceFinish", "onObjectiveComplete", "animationComplete", "destroy"
	};
	assert(PangeaScript_GetEventDescriptorCount() == 14);
	for (int index = 0; index < PangeaScript_GetEventDescriptorCount(); index++)
	{
		const PangeaScriptEventDescriptor* descriptor = PangeaScript_GetEventDescriptor(index);
		assert(descriptor != NULL);
		assert(strcmp(descriptor->id, expectedEventIds[index]) == 0);
		assert(descriptor->applicationPhase != NULL && descriptor->applicationPhase[0] != '\0');
		assert(descriptor->payload != NULL && descriptor->payload[0] != '\0');
		assert(descriptor->result != NULL && descriptor->result[0] != '\0');
	}
	assert(PangeaScript_GetEventDescriptor(-1) == NULL);
	assert(PangeaScript_GetEventDescriptor(14) == NULL);
	assert(PangeaScript_GetObjectEventDescriptorCount() == 13);
	const PangeaScriptObjectEventDescriptor* streamOut = PangeaScript_GetObjectEventDescriptor(10);
	assert(streamOut != NULL);
	assert(strcmp(streamOut->id, "streamOut") == 0);
	assert(strcmp(streamOut->cleanup, "owner-resources") == 0);
	assert(strcmp(streamOut->statePolicy, "clear") == 0);
	assert(streamOut->invalidatesHandle);
	const PangeaScriptObjectEventDescriptor* checkpointReset = PangeaScript_GetObjectEventDescriptor(11);
	assert(checkpointReset != NULL);
	assert(strcmp(checkpointReset->id, "checkpointReset") == 0);
	assert(strcmp(checkpointReset->statePolicy, "preserve") == 0);
	assert(PangeaScript_GetObjectEventDescriptor(-1) == NULL);
	assert(PangeaScript_GetObjectEventDescriptor(13) == NULL);
}

static void test_public_boundary_errors(const PangeaScriptGameInfo* gameInfo)
{
	PangeaScriptTriggerResult triggerResult = {0};
	PangeaScriptPickupResult pickupResult = {0};
	PangeaScriptWeaponHitResult weaponResult = {0};
	PangeaScriptDamageResult damageResult = {0};

	assert(PangeaScript_CallFrameHook(NULL) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_GetLastStatus() == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(strstr(PangeaScript_GetLastError(), "Frame hook context") != NULL);
	assert(PangeaScript_CallTerrainItemHook(NULL) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_GetLastStatus() == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallSplineItemHook(NULL) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallMapItemHook(NULL) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallTriggerHook(NULL, &triggerResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallPickupHook(NULL, &pickupResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallWeaponHitHook(NULL, &weaponResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallDamageHook(NULL, &damageResult) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_CallPlayerEvent(NULL, "onDeath") == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_Init(gameInfo) == PANGEA_SCRIPT_OK);
}

static void test_transactional_level_config_load(void)
{
	const char* path = "Data/Scripts/config/transactional-levels.json";
	write_script(path, "{\"version\":1,\"levels\":{\"1\":{}}}");
	assert(PangeaScript_SetConfigPath(path) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_OK);
	PangeaScriptVector3 position = {1, 2, 3};
	PangeaScriptObjectRegistration registration = {
		.nativeObject = &position,
		.ops = &kTestObjectOps,
		.objectType = "transactional-object",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ObjectExists(handle));

	write_script(path, "{\"version\":1,\"levels\":{\"1\":{\"script\":\"../outside.lua\"}}}");
	assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(PangeaScript_ObjectExists(handle));
	assert(PangeaScript_UnregisterObject(handle));
	assert(remove(path) == 0);
}

static void test_native_registration_metadata(void)
{
	char mutableId[] = "test.mutable";
	char mutableCategory[] = "pickup";
	char mutableDependency[] = "mutable dependency";
	PangeaScriptNativeItem mutable = {
		.id = mutableId,
		.nativeType = 4,
		.category = mutableCategory,
		.dependencySummary = mutableDependency,
	};
	assert(PangeaScript_RegisterNativeItems(&mutable, 1) == PANGEA_SCRIPT_OK);
	mutableId[0] = 'x';
	mutableCategory[0] = 'x';
	mutableDependency[0] = 'x';
	const PangeaScriptNativeItem* stable = PangeaScript_GetNativeItem(0);
	assert(stable != NULL);
	assert(strcmp(stable->id, "test.mutable") == 0);
	assert(strcmp(stable->category, "pickup") == 0);
	assert(strcmp(stable->dependencySummary, "mutable dependency") == 0);

	static const PangeaScriptNativeItem items[] = {
		{.id = "test.pickup", .nativeType = 5, .category = "pickup", .dependencySummary = "level assets and pickup state"},
		{.id = "test.trigger", .nativeType = 9, .category = "trigger", .dependencySummary = "level transition state"},
	};
	PangeaScriptNativeItem invalid = items[0];
	invalid.category = "";
	assert(PangeaScript_RegisterNativeItems(&invalid, 1) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_RegisterNativeItems(items, 2) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetNativeItemCount() == 2);
	assert(PangeaScript_GetNativeItem(-1) == NULL);
	assert(PangeaScript_GetNativeItem(2) == NULL);
	const PangeaScriptNativeItem* pickup = PangeaScript_GetNativeItem(0);
	assert(pickup != NULL && strcmp(pickup->id, "test.pickup") == 0 && pickup->nativeType == 5);
	assert(strcmp(pickup->category, "pickup") == 0);
	assert(PangeaScript_ResolveNativeItemType("test.trigger") == 9);
	assert(PangeaScript_ResolveNativeItemType("2147483648") == -1);
	assert(PangeaScript_ResolveNativeItemType("-1") == -1);
	PangeaScriptNativeItem duplicate = items[1];
	duplicate.nativeType = items[0].nativeType;
	assert(PangeaScript_RegisterNativeItems(&duplicate, 1) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetNativeItemCount() == 1);
	assert(PangeaScript_RegisterNativeItems(items, 2) == PANGEA_SCRIPT_OK);
	PangeaScriptNativeItem invalidBatch[] = {items[0], items[0]};
	assert(PangeaScript_RegisterNativeItems(invalidBatch, 2) == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(PangeaScript_GetNativeItemCount() == 2);
	assert(PangeaScript_RegisterNativeItems(NULL, 0) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetNativeItemCount() == 0);
	assert(PangeaScript_RegisterNativeItems(items, 2) == PANGEA_SCRIPT_OK);
}

static void test_persistence_api(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	memset(gPersistentEntries, 0, sizeof(gPersistentEntries));
	const char* source =
		"assert(pangea.api.capabilities().persistence)\n"
		"assert(pangea.persistence.get('score', 1) == nil)\n"
		"assert(pangea.persistence.set('score', 1, 42))\n"
		"assert(pangea.persistence.get('score', 1) == 42)\n"
		"assert(pangea.persistence.get('score', 2) == nil)\n"
		"assert(pangea.persistence.get('score', 65536) == nil and not pangea.persistence.set('score', 65536, 1))\n"
		"assert(pangea.persistence.set('score', 2, 'migrated'))\n"
		"assert(pangea.persistence.get('score', 2) == 'migrated')\n"
		"assert(pangea.persistence.set('featureA', 1, true))\n"
		"assert(pangea.persistence.set('featureB', 1, false))\n"
		"assert(pangea.persistence.get('featureA', 1) == true and pangea.persistence.get('featureB', 1) == false)\n"
		"local diagnostics=pangea.api.diagnostics()\n"
		"assert(diagnostics.persistentBytes > 8 and diagnostics.persistentEntries == 3)\n"
		"assert(not pangea.persistence.set('bad/key', 1, true))\n"
		"assert(not pangea.persistence.set('large', 1, string.rep('x', 4089)))\n"
		"assert(pangea.persistence.set('slot1', 1, string.rep('a', 4070)))\n"
		"assert(pangea.persistence.set('slot2', 1, string.rep('b', 4070)))\n"
		"assert(pangea.persistence.set('slot3', 1, string.rep('c', 4070)))\n"
		"assert(pangea.persistence.set('slot4', 1, string.rep('d', 4070)))\n"
		"return {}\n";
	PangeaScriptStatus status = PangeaScriptBackend_Load(backend, source, error, errorCapacity);
	if (status != PANGEA_SCRIPT_OK) fprintf(stderr, "persistence fixture failed: %s\n", error);
	assert(status == PANGEA_SCRIPT_OK);
	TestPersistentEntry* slot1 = find_persistent_entry("slot1");
	assert(slot1 != NULL && slot1->size == 4078);
	TestPersistentEntry* external = &gPersistentEntries[7];
	snprintf(external->key, sizeof(external->key), "%s", "external");
	memcpy(external->value, slot1->value, (size_t)slot1->size);
	external->size = slot1->size;
	const char* boundedLoadSource =
		"assert(pangea.persistence.get('slot1', 1) ~= nil)\n"
		"assert(pangea.persistence.get('slot2', 1) ~= nil)\n"
		"assert(pangea.persistence.get('slot3', 1) ~= nil)\n"
		"assert(pangea.persistence.get('slot4', 1) ~= nil)\n"
		"assert(pangea.persistence.get('external', 1) == nil)\n"
		"return {}\n";
	assert(PangeaScriptBackend_Load(backend, boundedLoadSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
	TestPersistentEntry* featureA = find_persistent_entry("featureA");
	assert(featureA != NULL && featureA->size == 9);
	featureA->value[0] = 0;
	const char* reloadSource =
		"assert(pangea.persistence.get('score', 2) == 'migrated')\n"
		"assert(pangea.persistence.get('featureA', 1) == nil)\n"
		"assert(pangea.persistence.get('featureB', 1) == false)\n"
		"return {}\n";
	assert(PangeaScriptBackend_Load(backend, reloadSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
	const char* deleteSource =
		"assert(pangea.persistence.delete('score'))\n"
		"assert(pangea.persistence.get('score', 2) == nil and pangea.persistence.get('featureB', 1) == false)\n"
		"return {}\n";
	assert(PangeaScriptBackend_Load(backend, deleteSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void test_default_persistence_fallback(const PangeaScriptGameInfo* sourceGameInfo, char* error, int errorCapacity)
{
	PangeaScriptGameInfo fallbackGameInfo = *sourceGameInfo;
	fallbackGameInfo.loadPersistent = NULL;
	fallbackGameInfo.savePersistent = NULL;
	assert(PangeaScript_Init(&fallbackGameInfo) == PANGEA_SCRIPT_OK);
	write_script("Data/Scripts/dist/default-persistence.lua", "local pangea = require('pangea')\nassert(pangea.persistence.set('fallback', 3, 'native-file'))\nreturn {}\n");
	PangeaScriptStatus startupStatus = PangeaScript_SetStartupScript("Data/Scripts/dist/default-persistence.lua");
	assert(startupStatus == PANGEA_SCRIPT_OK);
	write_script("Data/Scripts/dist/default-persistence.lua", "local pangea = require('pangea')\nassert(pangea.persistence.get('fallback', 3) == 'native-file')\nassert(pangea.persistence.delete('fallback'))\nreturn {}\n");
	assert(PangeaScript_Reload() == PANGEA_SCRIPT_OK);
	(void)error;
	(void)errorCapacity;
	assert(PangeaScript_Init(sourceGameInfo) == PANGEA_SCRIPT_OK);
	assert(remove("Data/Scripts/dist/default-persistence.lua") == 0);
#ifdef __EMSCRIPTEN__
	(void)remove("Data/Scripts/persistence/.pangea-persistence-fallback.bin");
#else
	(void)remove("Data/Scripts/.pangea-persistence-fallback.bin");
#endif
}

int main(void)
{
	const PangeaScriptGameInfo gameInfo = {
		.gameId = "Test",
		.gameName = "Lua Test",
		.spawnNative = spawn_native,
		.spawnScripted = spawn_scripted_for_test,
		.getPlayerCount = get_player_count,
		.getPlayer = get_player,
		.setPlayerHealth = set_player_health,
		.setPlayerLives = set_player_lives,
		.setPlayerScore = set_player_score,
		.setPlayerWeaponQuantity = set_player_weapon_quantity,
		.setPlayerInvulnerable = set_player_invulnerable,
		.setPlayerPosition = set_player_position,
		.setPlayerVelocity = set_player_velocity,
		.loadPersistent = load_persistent,
		.savePersistent = save_persistent,
		.capabilities = {.terrainItems = true, .splineItems = true, .mapItems = true},
	};
	write_script("Data/Scripts/dist/preinit-startup.lua", "return {}\n");
	assert(PangeaScript_SetStartupScript("Data/Scripts/dist/preinit-startup.lua") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_Init(&gameInfo) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_HasRunnableModule());
	assert(remove("Data/Scripts/dist/preinit-startup.lua") == 0);
	test_command_descriptors();
	test_public_boundary_errors(&gameInfo);
	test_transactional_level_config_load();
	test_native_registration_metadata();
	const char* source =
		"local pangea = require('pangea')\n"
		"assert(os == nil and io == nil and debug == nil and math.random == nil)\n"
		"local capabilities = pangea.api.capabilities()\n"
		"assert(capabilities.contractVersion == 1 and capabilities.apiVersion == 1 and capabilities.objectMutation and capabilities.events and capabilities.timers)\n"
		"assert(capabilities.terrainItems and capabilities.splineItems and capabilities.mapItems)\n"
		"assert(not capabilities.pickupScoreEffects)\n"
		"assert(not capabilities.weaponScoreEffects)\n"
		"assert(not capabilities.objectCollision and capabilities.playerCommands and capabilities.playerInvulnerability)\n"
		"assert(pangea.object.setPositionResult and pangea.object.setPositionOffset and pangea.object.setPositionOffsetResult and pangea.object.setVelocityResult and pangea.object.setRotationResult and pangea.object.setScaleResult and pangea.object.setAnimationResult and pangea.object.setActiveResult and pangea.object.deleteResult)\n"
		"assert(pangea.player.count() == 1)\n"
		"assert(pangea.player.get('zero') == nil)\n"
		"local player = pangea.player.get(0)\n"
		"assert(player.playerNum == 0 and player.position.x == 7 and player.health == 0.75)\n"
		"assert(player.lives == 3)\n"
		"assert(player.score == 42)\n"
		"assert(player.lapNum == 2 and player.checkpointNum == 6 and player.placement == 1 and not player.raceComplete)\n"
		"assert(player.vehicleType == 3 and player.vehicleMaxSpeed == 120 and player.vehicleAcceleration == 8.5 and player.vehicleTraction == 4 and player.vehicleSuspension == 2.5)\n"
		"assert(player.team == 1 and player.carryingFlag and player.captureScore == 2)\n"
		"assert(player.activeWeapon == 2 and #player.weapons == 2 and player.weapons[1].type == 1 and player.weapons[2].quantity == 4)\n"
		"assert(#player.eggs == 3 and player.eggs[1].recovered == 2 and player.eggs[1].required == 5 and player.eggs[2].recovered == 0 and player.eggs[3].recovered == 5)\n"
		"local raceResults = pangea.player.raceResults()\n"
		"assert(#raceResults == 1 and raceResults[1].playerNum == 0 and raceResults[1].placement == 1 and not raceResults[1].raceComplete)\n"
		"assert(pangea.player.setHealth(0, 0.5) == true and pangea.player.get(0).health == 0.5)\n"
		"assert(pangea.player.setHealth(0, 1.5) == false and pangea.player.get(0).health == 0.5)\n"
		"assert(pangea.player.heal(0, 0.25) == true and pangea.player.get(0).health == 0.75)\n"
		"assert(pangea.player.heal(0, 0.5) == true and pangea.player.get(0).health == 1.0)\n"
		"assert(pangea.player.heal(0, -0.1) == false and pangea.player.get(0).health == 1.0)\n"
		"assert(pangea.player.setInvulnerable(0, 2.5) == true)\n"
		"assert(pangea.player.setInvulnerable(0, -1) == false)\n"
		"assert(pangea.player.setHealth(1, 0.2) == false)\n"
		"assert(pangea.player.setInvulnerable(1, 1) == false)\n"
		"assert(pangea.player.setPosition(0, {x=10,y=20,z=30}) == true)\n"
		"assert(pangea.player.setPosition(0, {x=0/0,y=20,z=30}) == false)\n"
		"assert(pangea.player.setPosition(1, {x=10,y=20,z=30}) == false)\n"
		"assert(pangea.player.setVelocity(0, {x=1,y=2,z=3}) == true)\n"
		"assert(pangea.player.setVelocity(0, {x=0/0,y=2,z=3}) == false)\n"
		"local commandDiagnostics = pangea.api.diagnostics()\n"
		"assert(commandDiagnostics.commandCount == 14 and not commandDiagnostics.commandTraceOverflow)\n"
		"assert(commandDiagnostics.commandTrace[1].id == 'pangea.player.setHealth' and commandDiagnostics.commandTrace[13].id == 'pangea.player.setVelocity')\n"
		"assert(pangea.player.setLives(0, 5) == true and pangea.player.get(0).lives == 5)\n"
		"assert(pangea.player.setLives(0, -1) == false and pangea.player.get(0).lives == 5)\n"
		"local livesResult = pangea.player.setLivesResult(0, 7)\n"
		"assert(livesResult.ok and pangea.player.get(0).lives == 7)\n"
		"assert(pangea.player.setScore(0, 100000) == true and pangea.player.get(0).score == 100000)\n"
		"assert(pangea.player.setScore(0, -1) == false and pangea.player.get(0).score == 100000)\n"
		"local scoreResult = pangea.player.setScoreResult(0, 200000)\n"
		"assert(scoreResult.ok and pangea.player.get(0).score == 200000)\n"
		"assert(pangea.player.setWeaponQuantity(0, 1, 12) == true)\n"
		"assert(pangea.player.setWeaponQuantity(0, -1, 12) == false)\n"
		"local weaponQuantityResult = pangea.player.setWeaponQuantityResult(0, 1, 24)\n"
		"assert(weaponQuantityResult.ok and weaponQuantityResult.playerNum == 0)\n"
		"local healthResult = pangea.player.setHealthResult(0, 0.25)\n"
		"assert(healthResult.ok and healthResult.reason == 'ok' and healthResult.playerNum == 0)\n"
		"local healResult = pangea.player.healResult(0, 0.25)\n"
		"assert(healResult.ok and healResult.reason == 'ok')\n"
		"local invulnerabilityResult = pangea.player.setInvulnerableResult(0, 1)\n"
		"assert(invulnerabilityResult.ok and invulnerabilityResult.reason == 'ok')\n"
		"local positionResult = pangea.player.setPositionResult(0, {x=11,y=21,z=31})\n"
		"assert(positionResult.ok and positionResult.reason == 'ok')\n"
		"local invalidResult = pangea.player.setHealthResult(0, 2)\n"
		"assert(not invalidResult.ok and invalidResult.reason == 'bad-argument' and invalidResult.playerNum == 0)\n"
		"local invalidPlayerType = pangea.player.setHealthResult('zero', 0.5)\n"
		"assert(not invalidPlayerType.ok and invalidPlayerType.reason == 'bad-argument' and invalidPlayerType.playerNum == -1)\n"
		"local invalidValueType = pangea.player.healResult(0, 'half')\n"
		"assert(not invalidValueType.ok and invalidValueType.reason == 'bad-argument' and invalidValueType.playerNum == 0)\n"
		"local invalidPositionType = pangea.player.setPositionResult(0, 'origin')\n"
		"assert(not invalidPositionType.ok and invalidPositionType.reason == 'bad-argument')\n"
		"local inactiveResult = pangea.player.setPositionResult(1, {x=1,y=2,z=3})\n"
		"assert(not inactiveResult.ok and inactiveResult.reason == 'bad-argument' and string.find(inactiveResult.message, 'inactive', 1, true))\n"
		"pangea.random.seed(42)\n"
		"local firstRandom = pangea.random.integer(1, 100)\n"
		"pangea.random.seed(42)\n"
		"assert(firstRandom == pangea.random.integer(1, 100))\n"
		"local entry = {}\n"
		"function entry.onLevelStart(ctx)\n"
		"  assert(ctx.gameId == 'Test' and ctx.gameName == 'Lua Test')\n"
		"  assert(pangea.level.current() == 1)\n"
		"  pangea.spawn.native(6, {x=10,y=20,z=30}, {param0=2})\n"
		"end\n"
		"function entry.onTerrainItem(ctx)\n"
		"  assert(ctx.position.x == 4 and ctx.position.z == 6)\n"
		"  return {handled=true, markInUse=true, remappedItemType=9}\n"
		"end\n"
		"return entry\n";
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(&gameInfo);
	assert(backend != NULL);
	char error[512] = {0};
	assert(PangeaScriptBackend_Load(backend, source, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(gScriptPlayerHealthMutationCalls == 5);
	assert(gScriptPlayerInvulnerabilityMutationCalls == 2);
	assert(gScriptPlayerPositionMutationCalls == 2);
	assert(gScriptPlayerVelocityMutationCalls == 1);
	PangeaScriptLevelContext level = {.levelNum = 1, .levelName = "test"};
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_START, &level, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(gNativeSpawns == 1);
	unsigned char params[4] = {0};
	PangeaScriptTerrainItemContext item = {.levelNum = 1, .itemType = 2, .x = 4, .z = 6, .params = params, .paramCount = 4};
	assert(PangeaScriptBackend_CallTerrainItemHook(backend, &item, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(item.handled && item.markInUse && item.remappedItemType == 9);
	test_gameplay_hooks(backend, error, sizeof(error));
	test_timers(backend, error, sizeof(error));
	test_repeating_timers_and_api_version(backend, error, sizeof(error));
	test_context_immutability_and_finite_numbers(backend, error, sizeof(error));
	test_coroutine_tasks(backend, error, sizeof(error));
	test_object_queries_and_events(backend, error, sizeof(error));
	test_object_command_results(backend, error, sizeof(error));
	test_full_object_command_surface(backend, error, sizeof(error));
	test_persistence_api(backend, error, sizeof(error));
	test_object_owned_resource_cleanup(backend, error, sizeof(error));
	test_active_command_lifecycle();
	test_public_handle_generation_reset();
	test_config_reference_validation();
	test_otto_humans_jump_sample(backend, error, sizeof(error));
	test_hover_beacon_sample(backend, error, sizeof(error));
	test_timer_reset_and_diagnostics(backend, error, sizeof(error));
	test_timer_capacity(backend, error, sizeof(error));
	test_reload_isolation(backend, error, sizeof(error));
	test_scripted_spawn_options_cleanup(backend, error, sizeof(error));
	const char* invalidResultSource = "return { onTerrainItem = function() return 'invalid' end }";
	assert(PangeaScriptBackend_Load(backend, invalidResultSource, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallTerrainItemHook(backend, &item, error, sizeof(error)) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "onTerrainItem must return a table or nil") != NULL);
	const char* invalidFieldSource = "return { onTerrainItem = function() return { handled = 'yes' } end }";
	assert(PangeaScriptBackend_Load(backend, invalidFieldSource, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallTerrainItemHook(backend, &item, error, sizeof(error)) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "result field 'handled' must be boolean or nil") != NULL);
	const char* tracebackSource = "return { onFrame = function() local function broken() error('trace-test') end broken() end }";
	assert(PangeaScriptBackend_Load(backend, tracebackSource, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {0};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error)) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "stack traceback") != NULL && strstr(error, "broken") != NULL);
	const char* memorySource = "return { onFrame = function() return string.rep('x', 32 * 1024 * 1024) end }";
	assert(PangeaScriptBackend_Load(backend, memorySource, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error)) == PANGEA_SCRIPT_BUDGET_EXCEEDED);
	const char* budgetSource = "return { onFrame = function() while true do end end }";
	assert(PangeaScriptBackend_Load(backend, budgetSource, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error)) == PANGEA_SCRIPT_BUDGET_EXCEEDED);
	PangeaScriptBackend_Destroy(backend);
	test_default_persistence_fallback(&gameInfo, error, sizeof(error));
	test_lifecycle_name("CroMagRally", "onRaceStart");
	test_lifecycle_name("BillyFrontier", "onAreaStart");
	test_lifecycle_name("MightyMike", "onAreaStart");
	test_shutdown_lifecycle();
	test_public_host_failure_recovery();
	test_public_network_gate();
	test_public_object_lifecycle();
	test_atomic_checkpoint_failure();
	test_public_owned_child_cleanup();
	test_level_load_delivers_destroy_before_reset();
	test_native_save_load_hooks();
	test_public_custom_pickup_trigger();
	test_public_trigger_contact_lifecycle();
	test_public_player_gameplay_events();
	test_public_lifecycle_self_removal();
	test_direct_delete_delivers_destroy();
	test_public_lifecycle_failure_cleanup();
	test_public_spawn_failure_cleanup();
	test_lifecycle_cleanup_without_position();
	test_public_fallback_spawn_callback();
	test_startup_script_path_validation();
	puts("Lua backend integration tests passed");
	return 0;
}

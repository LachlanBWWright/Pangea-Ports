#include "../shared/script/pangea_script.h"
#include "../shared/script/pangea_script_backend.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int gNativeSpawns;

static bool test_object_position(void* nativeObject, PangeaScriptVector3* outPosition)
{
	if (!nativeObject || !outPosition) return false;
	*outPosition = *(PangeaScriptVector3*) nativeObject;
	return true;
}

static const PangeaScriptObjectOps kTestObjectOps = {.getPosition = test_object_position};

typedef struct TestVisualObject
{
	PangeaScriptVector3 position;
	PangeaScriptVector3 rotation;
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

static const PangeaScriptObjectOps kTestVisualObjectOps =
{
	.getPosition = test_visual_position,
	.setPosition = test_visual_set_position,
	.setRotation = test_visual_set_rotation,
};

static bool nearly_equal(float actual, float expected)
{
	return fabsf(actual - expected) < 0.001f;
}

static int get_player_count(void) { return 1; }

static bool get_player(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (playerNum != 0 || !outPlayer) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {7, 8, 9}, .health = 0.75f, .hasHealth = true, .active = true};
	return true;
}

static PangeaScriptStatus spawn_native(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	assert(strcmp(id, "6") == 0);
	assert(x == 10 && y == 20 && z == 30 && params[0] == 2);
	gNativeSpawns++;
	if (outHandle) *outHandle = (PangeaScriptObjectHandle){0};
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

static void test_gameplay_hooks(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"return {"
		" onTriggerEnter=function(ctx) assert(ctx.gameId=='Test' and ctx.position.x==1); return {handled=true,solid=false,scoreDelta=3} end,"
		" onPickupCollected=function(ctx) assert(ctx.pickupType==4); return {handled=true,consumePickup=true,healthDelta=2} end,"
		" onWeaponHit=function(ctx) assert(ctx.damage==5); return {handled=true,applyDamage=true,damage=7,destroyTarget=true} end"
		"}";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptTriggerContext trigger = {.levelNum = 2, .position = {1, 2, 3}};
	PangeaScriptTriggerResult triggerResult = {0};
	assert(PangeaScriptBackend_CallTriggerHook(backend, &trigger, &triggerResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(triggerResult.handled && triggerResult.hasSolid && !triggerResult.solid && triggerResult.scoreDelta == 3);
	PangeaScriptPickupContext pickup = {.levelNum = 2, .pickupType = 4};
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
	const char* invalidResult = "return { onWeaponHit = function() return { damage = 0/0 } end }";
	assert(PangeaScriptBackend_Load(backend, invalidResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptWeaponHitContext hit = {0};
	PangeaScriptWeaponHitResult result = {0};
	assert(PangeaScriptBackend_CallWeaponHitHook(backend, &hit, &result, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "finite number") != NULL);
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
	const char* nearTags[] = {"enemy", "flying"};
	const char* farTags[] = {"enemy"};
	PangeaScriptObjectRegistration nearRegistration = {.nativeObject = &nearPosition, .ops = &kTestObjectOps, .objectType = "near", .tags = nearTags, .tagCount = 2};
	PangeaScriptObjectRegistration farRegistration = {.nativeObject = &farPosition, .ops = &kTestObjectOps, .objectType = "far", .tags = farTags, .tagCount = 1};
	PangeaScriptObjectHandle nearHandle;
	PangeaScriptObjectHandle farHandle;
	assert(PangeaScript_RegisterObject(&nearRegistration, &nearHandle) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_RegisterObject(&farRegistration, &farHandle) == PANGEA_SCRIPT_OK);
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

static void test_otto_humans_jump_sample(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"local HUMAN_TAG = 'ottomatic.human'\n"
		"local SCIENTIST_TAG = 'ottomatic.human.scientist'\n"
		"local function hasTag(tags, tag) for _, value in ipairs(tags) do if value == tag then return true end end return false end\n"
		"local function getBobHeight(tags) if hasTag(tags, SCIENTIST_TAG) then return 56 else return 32 end end\n"
		"return { onObjectFrame = function(ctx)\n"
		"  if not hasTag(ctx.tags, HUMAN_TAG) then return end\n"
		"  return { positionOffset = { x=0, y=math.sin(ctx.levelTimeSeconds * 8) * getBobHeight(ctx.tags), z=0 } }\n"
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
	assert(result.hasPositionOffset);
	assert(nearly_equal(result.positionOffset.x, 0));
	assert(nearly_equal(result.positionOffset.y, 56));
	assert(nearly_equal(result.positionOffset.z, 0));

	const char* unrelatedTags[] = {"ottomatic.enemy"};
	PangeaScriptObjectFrameContext unrelated = scientist;
	unrelated.tags = unrelatedTags;
	unrelated.tagCount = 1;
	result = (PangeaScriptObjectFrameResult){0};
	assert(PangeaScriptBackend_CallObjectFrameHook(backend, &unrelated, &result, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(!result.hasPositionOffset);
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
}

static void test_timer_reset_and_diagnostics(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"local fired = false\n"
		"pangea.time.after(0.1, function() fired = true end)\n"
		"local diagnostics = pangea.api.diagnostics()\n"
		"assert(diagnostics.activeTimers == 1 and diagnostics.memoryUsedBytes > 0)\n"
		"return { onLevelLoad = function() assert(not fired) end, onFrame = function() assert(not fired) end }";
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

static void write_script(const char* path, const char* source)
{
	FILE* file = fopen(path, "w");
	assert(file != NULL);
	assert(fputs(source, file) >= 0);
	assert(fclose(file) == 0);
}

static void test_public_host_failure_recovery(void)
{
	const char* path = "/tmp/pangea-script-host-test.lua";
	write_script(path, "return { onFrame = function() error('host failure') end }");
	assert(PangeaScript_SetStartupScript(path) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {0};
	for (int i = 0; i < 5; i++)
		assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(PangeaScript_GetStatusScriptsDisabled());
	assert(PangeaScript_GetStatusErrorCount() >= 5);
	write_script(path, "return { onFrame = function() end }");
	assert(PangeaScript_Reload() == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_GetStatusScriptsDisabled());
	assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
	assert(remove(path) == 0);
}

int main(void)
{
	const PangeaScriptGameInfo gameInfo = {.gameId = "Test", .gameName = "Lua Test", .spawnNative = spawn_native, .getPlayerCount = get_player_count, .getPlayer = get_player};
	assert(PangeaScript_Init(&gameInfo) == PANGEA_SCRIPT_OK);
	const char* source =
		"local pangea = require('pangea')\n"
		"assert(os == nil and io == nil and debug == nil and math.random == nil)\n"
		"assert(pangea.player.count() == 1)\n"
		"local player = pangea.player.get(0)\n"
		"assert(player.playerNum == 0 and player.position.x == 7 and player.health == 0.75)\n"
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
	test_otto_humans_jump_sample(backend, error, sizeof(error));
	test_hover_beacon_sample(backend, error, sizeof(error));
	test_timer_reset_and_diagnostics(backend, error, sizeof(error));
	test_timer_capacity(backend, error, sizeof(error));
	test_reload_isolation(backend, error, sizeof(error));
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
	test_lifecycle_name("CroMagRally", "onRaceStart");
	test_lifecycle_name("BillyFrontier", "onAreaStart");
	test_lifecycle_name("MightyMike", "onAreaStart");
	test_shutdown_lifecycle();
	test_public_host_failure_recovery();
	PangeaScript_Shutdown();
	puts("Lua backend integration tests passed");
	return 0;
}

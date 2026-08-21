#include "../shared/script/pangea_script.h"
#include "../shared/script/pangea_script_backend.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct GameSampleCase
{
	const char* argument;
	const char* gameId;
	const char* gameName;
	const char* loadHook;
	const char* startHook;
	const char* frameHook;
	const char* completeHook;
	const char* unloadHook;
	const char* sampleTag;
	const char* specializedTag;
	float frequency;
	float amplitude;
	float specializedAmplitude;
	bool terrainItems;
	bool splineItems;
	bool mapItems;
	bool weaponHitEvents;
	bool damageEvents;
	bool playerSpawn;
	bool playerRespawn;
	bool playerDeath;
} GameSampleCase;

typedef struct TestVisualObject
{
	PangeaScriptVector3 position;
	PangeaScriptVector3 rotation;
} TestVisualObject;

static const GameSampleCase kGameCases[] =
{
	{"ottomatic", "OttoMatic-Android", "Otto Matic", "onLevelLoad", "onLevelStart", "onFrame", "onLevelComplete", "onLevelUnload", "ottomatic.human", "ottomatic.human.scientist", 8, 32, 56, true, true, false, false, true, true, true, true},
	{"bugdom", "Bugdom-android", "Bugdom", "onLevelLoad", "onLevelStart", "onFrame", "onLevelComplete", "onLevelUnload", "bugdom.buddy", NULL, 6, 20, 20, true, true, false, false, true, true, true, true},
	{"bugdom2", "Bugdom2-Android", "Bugdom 2", "onLevelLoad", "onLevelStart", "onFrame", "onLevelComplete", "onLevelUnload", "bugdom2.collectible", NULL, 5, 15, 15, true, true, false, true, true, true, true, true},
	{"cromag", "CroMagRally-Android", "Cro-Mag Rally", "onRaceLoad", "onRaceStart", "onRaceFrame", "onRaceComplete", "onRaceUnload", "cromag.pickup", NULL, 7, 25, 25, true, false, false, false, true, false, false, true},
	{"nanosaur", "Nanosaur-android", "Nanosaur", "onLevelLoad", "onLevelStart", "onFrame", "onLevelComplete", "onLevelUnload", "nanosaur.egg", NULL, 4, 12, 12, true, false, false, false, true, true, true, true},
	{"nanosaur2", "Nanosaur2-Android", "Nanosaur 2", "onLevelLoad", "onLevelStart", "onFrame", "onLevelComplete", "onLevelUnload", "nanosaur2.powerup", NULL, 6, 18, 18, true, true, false, false, true, true, true, true},
	{"billy", "BillyFrontier-Android", "Billy Frontier", "onAreaLoad", "onAreaStart", "onAreaFrame", "onAreaComplete", "onAreaUnload", "billy.cacti", NULL, 5, 14, 14, true, true, false, false, true, false, false, true},
	{"mightymike", "MightyMike-Android", "Mighty Mike", "onAreaLoad", "onAreaStart", "onAreaFrame", "onAreaComplete", "onAreaUnload", "mightymike.box", NULL, 5, 16, 16, false, false, true, false, false, false, false, false},
};

static int gNativeSpawnCount;

static int get_player_count(void) { return 1; }

static bool get_player(int playerNum, PangeaScriptPlayerSnapshot* outPlayer)
{
	if (playerNum != 0 || !outPlayer) return false;
	*outPlayer = (PangeaScriptPlayerSnapshot){.position = {7, 8, 9}, .health = 0.75f, .hasHealth = true, .active = true};
	return true;
}

static PangeaScriptStatus spawn_native(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	if (!id || strcmp(id, "test.native") != 0 || x != 1 || y != 2 || z != 3 || params[0] != 4)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	gNativeSpawnCount++;
	if (outHandle) *outHandle = (PangeaScriptObjectHandle){0};
	return PANGEA_SCRIPT_OK;
}

static bool get_position(void* nativeObject, PangeaScriptVector3* outPosition)
{
	if (!nativeObject || !outPosition) return false;
	*outPosition = ((TestVisualObject*) nativeObject)->position;
	return true;
}

static bool set_position(void* nativeObject, const PangeaScriptVector3* position)
{
	if (!nativeObject || !position) return false;
	((TestVisualObject*) nativeObject)->position = *position;
	return true;
}

static bool set_rotation(void* nativeObject, const PangeaScriptVector3* rotation)
{
	if (!nativeObject || !rotation) return false;
	((TestVisualObject*) nativeObject)->rotation = *rotation;
	return true;
}

static const PangeaScriptObjectOps kVisualOps =
{
	.getPosition = get_position,
	.setPosition = set_position,
	.setRotation = set_rotation,
};

static bool nearly_equal(float actual, float expected)
{
	return fabsf(actual - expected) < 0.001f;
}

static void write_text_file(const char* path, const char* source)
{
	FILE* file = fopen(path, "w");
	assert(file != NULL);
	assert(fputs(source, file) >= 0);
	assert(fclose(file) == 0);
}

static const GameSampleCase* find_game_case(const char* argument)
{
	for (int i = 0; i < (int)(sizeof(kGameCases) / sizeof(kGameCases[0])); i++)
	{
		if (strcmp(kGameCases[i].argument, argument) == 0)
			return &kGameCases[i];
	}
	return NULL;
}

static void test_game_lifecycle(const GameSampleCase* game, PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	char source[4096];
	snprintf(source, sizeof(source),
		"local calls = {}\n"
		"local function check(ctx, name) assert(ctx.gameId == '%s' and ctx.gameName == '%s'); calls[name] = (calls[name] or 0) + 1 end\n"
		"return {\n"
		" onGameStart=function(ctx) check(ctx,'gameStart') end,\n"
		" %s=function(ctx) check(ctx,'load'); assert(ctx.levelNum==4) end,\n"
		" %s=function(ctx) check(ctx,'start'); assert(ctx.levelNum==4 and calls.load==1) end,\n"
		" %s=function(ctx) check(ctx,'frame'); assert(ctx.frameNum==12 and ctx.deltaSeconds>0 and ctx.levelTimeSeconds==2) end,\n"
		" %s=function(ctx) check(ctx,'complete'); assert(calls.frame==1) end,\n"
		" %s=function(ctx) check(ctx,'unload'); assert(calls.complete==1) end,\n"
		" onGameShutdown=function(ctx) check(ctx,'shutdown'); assert(calls.unload==1) end\n"
		"}",
		game->gameId,
		game->gameName,
		game->loadHook,
		game->startHook,
		game->frameHook,
		game->completeHook,
		game->unloadHook);
	PangeaScriptStatus loadStatus = PangeaScriptBackend_Load(backend, source, error, errorCapacity);
	if (loadStatus != PANGEA_SCRIPT_OK) fprintf(stderr, "runtime API fixture failed to load: %s\n", error);
	assert(loadStatus == PANGEA_SCRIPT_OK);
	PangeaScriptLevelContext context = {.levelNum = 4, .levelName = "conformance"};
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_GAME_START, &context, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_LOAD, &context, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_START, &context, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 4, .frameNum = 12, .deltaSeconds = 0.016f, .levelTimeSeconds = 2};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, &context, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, &context, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN, &context, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

static void test_game_runtime_apis(const GameSampleCase* game, PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	char source[4096];
	snprintf(source, sizeof(source),
		"assert(os==nil and io==nil and debug==nil and math.random==nil)\n"
		"pangea.api.requireVersion(1)\n"
		"assert(pangea.player.count()==1)\n"
		"local player=pangea.player.get(0); assert(player and player.playerNum==0 and player.position.x==7 and player.health==0.75)\n"
		"assert(not pangea.api.capabilities().persistence)\n"
		"assert(pangea.api.capabilities().terrainItems == %s and pangea.api.capabilities().splineItems == %s and pangea.api.capabilities().mapItems == %s)\n"
		"assert(pangea.persistence.get('unsupported',1)==nil and not pangea.persistence.set('unsupported',1,true))\n"
		"pangea.random.seed(123); local random=pangea.random.integer(1,100); pangea.random.seed(123); assert(random==pangea.random.integer(1,100))\n"
		"local eventValue=0; local subscription=pangea.events.on('conformance',function(payload) eventValue=payload.value end)\n"
		"assert(pangea.events.emit('conformance',{value=9})==1 and eventValue==9); assert(pangea.events.off(subscription))\n"
		"local timerFired=false; pangea.time.after(0.25,function() timerFired=true end)\n"
		"local taskFinished=false; pangea.task.start(function() pangea.task.wait(0.25); taskFinished=true end)\n"
		"return { %s=function(ctx)\n"
		" assert(pangea.level.current()==4 and ctx.frameNum==20 and timerFired and taskFinished)\n"
		" local diagnostics=pangea.api.diagnostics(); assert(diagnostics.activeSubscriptions==0)\n"
		" pangea.spawn.native('test.native',{x=1,y=2,z=3},{param0=4})\n"
		" local scripted=pangea.spawn.scripted('test.scripted',{x=4,y=5,z=6}); assert(scripted and pangea.object.position(scripted).y==5)\n"
		" local moved=pangea.object.setPositionResult(scripted,{x=8,y=9,z=10}); assert(moved.ok and moved.reason=='ok' and moved.primary.id==scripted.id); assert(pangea.object.position(scripted).x==8)\n"
		" local deleted=pangea.object.deleteResult(scripted); assert(deleted.ok and deleted.reason=='ok' and not pangea.object.exists(scripted))\n"
		"end }",
		game->terrainItems ? "true" : "false",
		game->splineItems ? "true" : "false",
		game->mapItems ? "true" : "false",
		game->frameHook);
	PangeaScriptStatus loadStatus = PangeaScriptBackend_Load(backend, source, error, errorCapacity);
	if (loadStatus != PANGEA_SCRIPT_OK) fprintf(stderr, "runtime API fixture failed to load: %s\n", error);
	assert(loadStatus == PANGEA_SCRIPT_OK);
	PangeaScriptLevelContext level = {.levelNum = 4};
	assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_START, &level, error, errorCapacity) == PANGEA_SCRIPT_OK);
	gNativeSpawnCount = 0;
	PangeaScriptFrameContext frame = {.levelNum = 4, .frameNum = 20, .deltaSeconds = 0.25f, .levelTimeSeconds = 0.25f};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(gNativeSpawnCount == 1);
	PangeaScriptCommandTrace firstTrace;
	PangeaScript_GetCommandTrace(&firstTrace);
	assert(firstTrace.commandCount > 0 && !firstTrace.overflow);
	PangeaScript_ResetObjects();
	TestVisualObject traceVisual = {.position = {1, 2, 3}};
	PangeaScriptObjectRegistration traceRegistration =
	{
		.nativeObject = &traceVisual,
		.ops = &kVisualOps,
		.objectType = "trace-test",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle traceHandle;
	assert(PangeaScript_RegisterObject(&traceRegistration, &traceHandle) == PANGEA_SCRIPT_OK);
	char traceSource[512];
	snprintf(traceSource, sizeof(traceSource),
		"return { %s=function() local result=pangea.object.setPositionResult({id=%d,generation=%u},{x=4,y=5,z=6}); assert(result.ok) end }",
		game->frameHook, traceHandle.id, traceHandle.generation);
	assert(PangeaScriptBackend_Load(backend, traceSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptCommandTrace secondTrace;
	PangeaScript_GetCommandTrace(&secondTrace);
	assert(secondTrace.commandCount == 1 && !secondTrace.overflow);
	assert(secondTrace.entryCount == 1 && secondTrace.hash == 0xd3237928u);
	PangeaScriptCommandTraceEntry secondEntry;
	assert(PangeaScript_GetCommandTraceEntry(0, &secondEntry));
	assert(strcmp(secondEntry.commandId, "pangea.object.setPosition") == 0);
	assert(secondEntry.target.id == traceHandle.id && secondEntry.target.generation == traceHandle.generation);
	assert(secondEntry.status == PANGEA_SCRIPT_OK);
	uint32_t firstDeterministicHash = secondTrace.hash;
	assert(PangeaScriptBackend_Load(backend, traceSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptCommandTrace replayTrace;
	PangeaScript_GetCommandTrace(&replayTrace);
	assert(replayTrace.commandCount == secondTrace.commandCount);
	assert(replayTrace.hash == firstDeterministicHash);
	assert(!replayTrace.overflow);
}

static void test_game_object_sample(const GameSampleCase* game, PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	char source[2048];
	snprintf(source, sizeof(source),
		"local TARGET_TAG = '%s'\n"
		"local SPECIAL_TAG = %s\n"
		"local function hasTag(tags, target) if not target then return false end; for _, tag in ipairs(tags) do if tag == target then return true end end return false end\n"
		"return { onObjectFrame = function(ctx)\n"
		" assert(ctx.gameId == '%s' and ctx.gameName == '%s')\n"
		" if not hasTag(ctx.tags, TARGET_TAG) then return end\n"
		" local amplitude = hasTag(ctx.tags, SPECIAL_TAG) and %.9g or %.9g\n"
		" return {positionOffset={x=0,y=math.sin(ctx.levelTimeSeconds*%.9g)*amplitude,z=0}}\n"
		"end }",
		game->sampleTag,
		game->specializedTag ? "'ottomatic.human.scientist'" : "nil",
		game->gameId,
		game->gameName,
		game->specializedAmplitude,
		game->amplitude,
		game->frequency);
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);

	const char* matchingTags[] = {game->sampleTag};
	PangeaScriptObjectFrameContext context =
	{
		.levelNum = 4,
		.levelTimeSeconds = 1.57079632679f / game->frequency,
		.objectType = game->sampleTag,
		.tags = matchingTags,
		.tagCount = 1,
	};
	PangeaScriptObjectFrameResult result = {0};
	assert(PangeaScriptBackend_CallObjectFrameHook(backend, &context, &result, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(result.hasPositionOffset);
	assert(nearly_equal(result.positionOffset.x, 0));
	assert(nearly_equal(result.positionOffset.y, game->amplitude));
	assert(nearly_equal(result.positionOffset.z, 0));
	if (game->specializedTag)
	{
		const char* specializedTags[] = {game->sampleTag, game->specializedTag};
		context.tags = specializedTags;
		context.tagCount = 2;
		result = (PangeaScriptObjectFrameResult){0};
		assert(PangeaScriptBackend_CallObjectFrameHook(backend, &context, &result, error, errorCapacity) == PANGEA_SCRIPT_OK);
		assert(result.hasPositionOffset);
		assert(nearly_equal(result.positionOffset.y, game->specializedAmplitude));
	}

	const char* unrelatedTags[] = {"unrelated.object"};
	context.tags = unrelatedTags;
	context.tagCount = 1;
	result = (PangeaScriptObjectFrameResult){0};
	assert(PangeaScriptBackend_CallObjectFrameHook(backend, &context, &result, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(!result.hasPositionOffset);
}

static void test_custom_object_sample(const GameSampleCase* game, PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	PangeaScript_ResetObjects();
	TestVisualObject beacon = {.position = {10, 100, 30}};
	const char* tags[] = {"editor.custom.hoverBeacon"};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &beacon,
		.ops = &kVisualOps,
		.objectType = "sample.hoverBeacon",
		.tags = tags,
		.tagCount = 1,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	char source[2048];
	snprintf(source, sizeof(source),
		"local origins = {}\n"
		"return { onObjectFrame = function(ctx)\n"
		" assert(ctx.gameId == '%s' and ctx.gameName == '%s')\n"
		" if ctx.objectType ~= 'sample.hoverBeacon' then return end\n"
		" local current = pangea.object.position(ctx.object); if not current then return end\n"
		" local origin = origins[ctx.object.id]; if not origin then origin=current; origins[ctx.object.id]=origin end\n"
		" pangea.object.setPosition(ctx.object,{x=origin.x,y=origin.y+math.sin(ctx.levelTimeSeconds*4)*16,z=origin.z})\n"
		" pangea.object.setRotation(ctx.object,{x=0,y=ctx.levelTimeSeconds*2,z=0})\n"
		"end }",
		game->gameId,
		game->gameName);
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptObjectFrameContext context =
	{
		.levelNum = 4,
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
	assert(nearly_equal(beacon.rotation.y, 0.78539816f));
}

static void test_public_object_lifecycle(const GameSampleCase* game)
{
	const char* scriptPath = "pangea-sample-lifecycle.lua";
	const char* source =
		"return { onObjectFrame = function(ctx)"
		" local source = pangea.object.source(ctx.object)"
		" if ctx.event == 'spawn' then assert(source and source.kind == 'terrain' and source.itemIndex == 12) end"
		" if ctx.event == 'streamOut' then assert(source and source.nativeType == 7) end"
		" if ctx.event == 'streamIn' then assert(source and source.x == 10 and source.z == 30) end"
		" end }";
	write_text_file(scriptPath, source);
	assert(PangeaScript_SetStartupScript(scriptPath) == PANGEA_SCRIPT_OK);

	PangeaScript_ResetObjects();
	TestVisualObject firstObject = {.position = {10, 2, 30}};
	const PangeaScriptObjectRegistration registration = {
		.nativeObject = &firstObject,
		.ops = &kVisualOps,
		.objectType = "sample.lifecycle",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle firstHandle;
	assert(PangeaScript_RegisterObject(&registration, &firstHandle) == PANGEA_SCRIPT_OK);
	const PangeaScriptObjectSource sourceIdentity = {
		.kind = PANGEA_SCRIPT_SOURCE_TERRAIN,
		.itemIndex = 12,
		.nativeType = 7,
		.x = 10,
		.y = 2,
		.z = 30,
	};
	assert(PangeaScript_AssociateObjectSource(firstHandle, &sourceIdentity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 4, .frameNum = 1};
	assert(PangeaScript_CallObjectEvent(firstHandle, &frame, "spawn") == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(firstHandle, &frame, PANGEA_SCRIPT_OBJECT_STREAM_OUT) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(firstHandle));

	TestVisualObject recreatedObject = {.position = {11, 3, 31}};
	PangeaScriptObjectRegistration recreatedRegistration = registration;
	recreatedRegistration.nativeObject = &recreatedObject;
	PangeaScriptObjectHandle recreatedHandle;
	assert(PangeaScript_RegisterObject(&recreatedRegistration, &recreatedHandle) == PANGEA_SCRIPT_OK);
	assert(recreatedHandle.id == firstHandle.id && recreatedHandle.generation != firstHandle.generation);
	assert(PangeaScript_AssociateObjectSource(recreatedHandle, &sourceIdentity) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ApplyObjectLifecycle(recreatedHandle, &frame, PANGEA_SCRIPT_OBJECT_STREAM_IN) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_ObjectExists(recreatedHandle));
	PangeaScriptLevelContext unload = {.levelNum = 4, .levelName = game->gameName};
	assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, &unload) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_ObjectExists(recreatedHandle));
	assert(remove(scriptPath) == 0);
}

static void test_supported_item_hooks(const GameSampleCase* game, PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"return {\n"
		" onTerrainItem=function(ctx) assert(ctx.itemType==2 and ctx.position.x==10 and ctx.params[1]==1); return {handled=true,markInUse=true,remappedItemType=7} end,\n"
		" onSplineItem=function(ctx) assert(ctx.itemType==3 and ctx.splineNum==4 and ctx.placement==0.5); return {handled=true,markInUse=true} end,\n"
		" onMapItem=function(ctx) assert(ctx.itemType==5 and ctx.scene==6 and ctx.area==7); return {handled=true,markInUse=true} end\n"
		"}";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	unsigned char params[4] = {1, 2, 3, 4};
	if (game->terrainItems)
	{
		PangeaScriptTerrainItemContext terrain = {.levelNum = 4, .itemType = 2, .x = 10, .z = 20, .params = params, .paramCount = 4};
		assert(PangeaScriptBackend_CallTerrainItemHook(backend, &terrain, error, errorCapacity) == PANGEA_SCRIPT_OK);
		assert(terrain.handled && terrain.markInUse && terrain.remappedItemType == 7);
	}
	else
	{
		PangeaScriptTerrainItemContext terrain = {.levelNum = 4, .itemType = 2, .x = 10, .z = 20, .params = params, .paramCount = 4};
		assert(PangeaScript_CallTerrainItemHook(&terrain) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	}
	if (game->splineItems)
	{
		PangeaScriptSplineItemContext spline = {.levelNum = 4, .splineNum = 4, .itemType = 3, .placement = 0.5f, .params = params, .paramCount = 4};
		assert(PangeaScriptBackend_CallSplineItemHook(backend, &spline, error, errorCapacity) == PANGEA_SCRIPT_OK);
		assert(spline.handled && spline.markInUse);
	}
	else
	{
		PangeaScriptSplineItemContext spline = {.levelNum = 4, .splineNum = 4, .itemType = 3, .placement = 0.5f, .params = params, .paramCount = 4};
		assert(PangeaScript_CallSplineItemHook(&spline) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	}
	if (game->mapItems)
	{
		PangeaScriptMapItemContext map = {.sceneNum = 6, .areaNum = 7, .itemType = 5, .x = 10, .y = 20, .params = params, .paramCount = 4};
		assert(PangeaScriptBackend_CallMapItemHook(backend, &map, error, errorCapacity) == PANGEA_SCRIPT_OK);
		assert(map.handled && map.markInUse);
	}
	else
	{
		PangeaScriptMapItemContext map = {.sceneNum = 6, .areaNum = 7, .itemType = 5, .x = 10, .y = 20, .params = params, .paramCount = 4};
		assert(PangeaScript_CallMapItemHook(&map) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	}
}

static void test_structured_gameplay_hooks(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	const char* source =
		"return {\n"
		" onTriggerEnter=function(ctx) assert(ctx.triggerId=='gate' and ctx.sideBits==3); return {handled=true,solid=false,deleteSelf=true,damagePlayer=2,scoreDelta=5} end,\n"
		" onPickupCollected=function(ctx) assert(ctx.pickupType==4); return {handled=true,consumePickup=true,healthDelta=3,scoreDelta=6} end,\n"
		" onWeaponHit=function(ctx) assert(ctx.weaponType==8 and ctx.damage==9); return {handled=true,applyDamage=true,destroyTarget=true,damage=10,scoreDelta=7} end\n"
		"}";
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptTriggerContext trigger = {.levelNum = 4, .triggerId = "gate", .sideBits = 3};
	PangeaScriptTriggerResult triggerResult = {0};
	assert(PangeaScriptBackend_CallTriggerHook(backend, &trigger, &triggerResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(triggerResult.handled && triggerResult.hasSolid && !triggerResult.solid && triggerResult.deleteSelf && triggerResult.damagePlayer == 2 && triggerResult.scoreDelta == 5);
	PangeaScriptPickupContext pickup = {.levelNum = 4, .pickupType = 4};
	PangeaScriptPickupResult pickupResult = {0};
	assert(PangeaScriptBackend_CallPickupHook(backend, &pickup, &pickupResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(pickupResult.handled && pickupResult.consumePickup && pickupResult.healthDelta == 3 && pickupResult.scoreDelta == 6);
	PangeaScriptWeaponHitContext hit = {.levelNum = 4, .weaponType = 8, .damage = 9};
	PangeaScriptWeaponHitResult hitResult = {0};
	assert(PangeaScriptBackend_CallWeaponHitHook(backend, &hit, &hitResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(hitResult.handled && hitResult.applyDamage && hitResult.destroyTarget && hitResult.damage == 10 && hitResult.scoreDelta == 7);
}

static void test_player_event_sample(const GameSampleCase* game, PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	TestVisualObject player = {.position = {4, 5, 6}};
	PangeaScriptObjectRegistration registration =
	{
		.nativeObject = &player,
		.ops = &kVisualOps,
		.objectType = "sample.player",
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	char source[2048];
	PangeaScriptDamageContext damageContext =
	{
		.levelNum = 4,
		.playerNum = 0,
		.cause = 3,
		.damage = 2,
		.target = {1, 1},
		.position = player.position,
	};
	PangeaScriptDamageResult damageResult = {0};
	PangeaScriptWeaponHitContext weaponContext =
	{
		.levelNum = 4,
		.playerNum = 0,
		.weaponType = 1,
		.targetType = 2,
		.damage = 3,
		.weaponId = "bugdom2.projectile",
		.targetFlags = 0,
		.position = player.position,
	};
	PangeaScriptWeaponHitResult weaponResult = {0};
	PangeaScriptPlayerEventContext playerContext =
	{
		.levelNum = 4,
		.playerNum = 0,
		.eventValue = 7,
		.player = {1, 1},
		.position = player.position,
	};

	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	damageContext.target = handle;
	playerContext.player = handle;
	snprintf(source, sizeof(source),
		"return {"
		" onWeaponHit=function(ctx) assert(ctx.weaponId=='bugdom2.projectile' and ctx.damage==3); return {handled=true,damage=1,applyDamage=true} end,"
		" onDamage=function(ctx) assert(ctx.target.id==%d and ctx.damage==2 and ctx.cause==3); return {handled=true,damage=.5,applyDamage=true} end,"
		" onDamageApplied=function(ctx) assert(ctx.target.id==%d and ctx.damage==.5 and ctx.cause==3) end,"
		" onPlayerSpawn=function(ctx) assert(ctx.player.id==%d and ctx.position.x==4) end,"
		" onPlayerRespawn=function(ctx) assert(ctx.player.id==%d and ctx.position.z==6) end,"
		" onDeath=function(ctx) assert(ctx.player.id==%d and ctx.eventValue==7) end"
		"}",
		handle.id, handle.id, handle.id, handle.id, handle.id);
	assert(PangeaScriptBackend_Load(backend, source, error, errorCapacity) == PANGEA_SCRIPT_OK);
	if (game->weaponHitEvents)
	{
		assert(PangeaScriptBackend_CallWeaponHitHook(backend, &weaponContext, &weaponResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
		assert(weaponResult.handled && weaponResult.hasApplyDamage && weaponResult.applyDamage && nearly_equal(weaponResult.damage, 1));
	}
	if (game->damageEvents)
	{
		assert(PangeaScriptBackend_CallDamageHook(backend, &damageContext, &damageResult, error, errorCapacity) == PANGEA_SCRIPT_OK);
		assert(damageResult.handled && damageResult.hasDamage && nearly_equal(damageResult.damage, .5f));
		damageContext.damage = damageResult.damage;
		assert(PangeaScriptBackend_CallDamageAppliedHook(backend, &damageContext, error, errorCapacity) == PANGEA_SCRIPT_OK);
	}
	if (game->playerSpawn)
		assert(PangeaScriptBackend_CallPlayerEvent(backend, &playerContext, "onPlayerSpawn", error, errorCapacity) == PANGEA_SCRIPT_OK);
	if (game->playerRespawn)
		assert(PangeaScriptBackend_CallPlayerEvent(backend, &playerContext, "onPlayerRespawn", error, errorCapacity) == PANGEA_SCRIPT_OK);
	if (game->playerDeath)
		assert(PangeaScriptBackend_CallPlayerEvent(backend, &playerContext, "onDeath", error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_UnregisterObject(handle));
}

static void test_error_isolation(const GameSampleCase* game, PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	char failingSource[256];
	snprintf(failingSource, sizeof(failingSource), "return {%s=function() error('per-game failure') end}", game->frameHook);
	assert(PangeaScriptBackend_Load(backend, failingSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 4};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "per-game failure") != NULL && strstr(error, "stack traceback") != NULL);
	char recoveredSource[128];
	snprintf(recoveredSource, sizeof(recoveredSource), "return {%s=function() end}", game->frameHook);
	assert(PangeaScriptBackend_Load(backend, recoveredSource, error, errorCapacity) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, errorCapacity) == PANGEA_SCRIPT_OK);
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s GAME\n", argv[0]);
		return 2;
	}
	const GameSampleCase* game = find_game_case(argv[1]);
	if (!game)
	{
		fprintf(stderr, "unknown game case: %s\n", argv[1]);
		return 2;
	}
	PangeaScriptGameInfo gameInfo =
	{
		.gameId = game->gameId,
		.gameName = game->gameName,
		.spawnNative = spawn_native,
		.getPlayerCount = get_player_count,
		.getPlayer = get_player,
		.capabilities = {
			.terrainItems = game->terrainItems,
			.splineItems = game->splineItems,
			.mapItems = game->mapItems,
		},
	};
	assert(PangeaScript_Init(&gameInfo) == PANGEA_SCRIPT_OK);
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(&gameInfo);
	assert(backend != NULL);
	char error[1024] = {0};
	test_game_lifecycle(game, backend, error, sizeof(error));
	test_game_runtime_apis(game, backend, error, sizeof(error));
	test_supported_item_hooks(game, backend, error, sizeof(error));
	test_structured_gameplay_hooks(backend, error, sizeof(error));
	test_player_event_sample(game, backend, error, sizeof(error));
	test_game_object_sample(game, backend, error, sizeof(error));
	test_custom_object_sample(game, backend, error, sizeof(error));
	test_public_object_lifecycle(game);
	test_error_isolation(game, backend, error, sizeof(error));
	PangeaScriptBackend_Destroy(backend);
	PangeaScript_Shutdown();
	printf("%s scripting sample conformance passed\n", game->gameName);
	return 0;
}

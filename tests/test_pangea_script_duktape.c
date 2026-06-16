#include "../shared/script/pangea_script.h"
#include "../shared/script/pangea_script_backend.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Dummy object structures for testing operations
typedef struct DummyObject {
	PangeaScriptVector3 position;
	PangeaScriptVector3 velocity;
	bool deleted;
} DummyObject;

static bool DummyGetPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	*outPosition = obj->position;
	return true;
}

static bool DummySetPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	obj->position = *position;
	return true;
}

static bool DummySetVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	obj->velocity = *velocity;
	return true;
}

static bool DummyDeleteObject(void* nativeObject)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	obj->deleted = true;
	return true;
}

static const PangeaScriptObjectOps kDummyOps = {
	.getPosition = DummyGetPosition,
	.setPosition = DummySetPosition,
	.setVelocity = DummySetVelocity,
	.deleteObject = DummyDeleteObject,
};

static DummyObject gSpawnedNativeDummy;

static PangeaScriptStatus TestSpawnNativeCallback(const char* id, float x, float y, float z, int subtype, int amount, PangeaScriptObjectHandle* outHandle)
{
	(void) subtype;
	(void) amount;
	if (strcmp(id, "test.native") == 0)
	{
		gSpawnedNativeDummy.position.x = x;
		gSpawnedNativeDummy.position.y = y;
		gSpawnedNativeDummy.position.z = z;
		gSpawnedNativeDummy.deleted = false;

		static const char* tags[] = { "pickup", "test" };
		PangeaScriptObjectRegistration reg = {
			.nativeObject = &gSpawnedNativeDummy,
			.ops = &kDummyOps,
			.tags = tags,
			.tagCount = 2,
			.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL
		};
		return PangeaScript_RegisterObject(&reg, outHandle);
	}
	return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
}

static void write_temp_file(const char* path, const char* content)
{
	FILE* f = fopen(path, "w");
	assert(f != NULL);
	fputs(content, f);
	fclose(f);
}

void test_duktape_integration(void)
{
	printf("Running Duktape backend integration tests...\n");

	PangeaScriptGameInfo gameInfo = {
		.gameId = "TestGame",
		.gameName = "Test Game",
		.spawnNative = TestSpawnNativeCallback
	};
	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	assert(status == PANGEA_SCRIPT_OK);

	// Temp directory setup
	system("mkdir -p Data/Scripts/config");
	system("mkdir -p Data/Scripts/dist");

	const char* test_script =
		"var nativeHandle = null;\n"
		"var scriptedHandle = null;\n"
		"\n"
		"onLevelLoad = function(ctx) {\n"
		"  pangea.log.info('onLevelLoad triggered in Duktape');\n"
		"  nativeHandle = pangea.spawn.native('test.native', {x: 100, y: 200, z: 300}, {subtype: 3, amount: 1});\n"
		"  scriptedHandle = pangea.experimental.spawn.scripted('my.scripted', {x: 10, y: 20, z: 30});\n"
		"};\n"
		"\n"
		"onLevelStart = function(ctx) {\n"
		"  pangea.log.info('onLevelStart triggered');\n"
		"  if (nativeHandle) {\n"
		"    pangea.object.setPosition(nativeHandle, {x: 105, y: 205, z: 305});\n"
		"    pangea.object.setVelocity(nativeHandle, {x: 1, y: 2, z: 3});\n"
		"  }\n"
		"  if (scriptedHandle) {\n"
		"    var pos = pangea.object.position(scriptedHandle);\n"
		"    pangea.object.setPosition(scriptedHandle, {x: pos.x + 5, y: pos.y + 5, z: pos.z + 5});\n"
		"  }\n"
		"};\n"
		"\n"
		"onTerrainItem = function(item) {\n"
		"  if (item.itemType === 42) {\n"
		"    return { handled: true, markInUse: true };\n"
		"  }\n"
		"  return { handled: false };\n"
		"};\n"
		"\n"
		"onSplineItem = function(item) {\n"
		"  if (item.itemType === 100) {\n"
		"    return { handled: true, markInUse: true };\n"
		"  }\n"
		"  return { handled: false };\n"
		"};\n"
		"\n"
		"onMapItem = function(item) {\n"
		"  if (item.sceneNum === 5) {\n"
		"    return { handled: true, markInUse: true };\n"
		"  }\n"
		"  return { handled: false };\n"
		"};\n"
		"\n"
		"onObjectFrame = function(ctx) {\n"
		"  var pos = pangea.object.position(ctx.object);\n"
		"  if (pos) {\n"
		"    pangea.object.setPosition(ctx.object, {x: pos.x + 1, y: pos.y + 1, z: pos.z + 1});\n"
		"  }\n"
		"  return { positionOffset: { x: 0.1, y: 0.2, z: 0.3 } };\n"
		"};\n";

	write_temp_file("Data/Scripts/dist/main.js", test_script);

	// Load startup script
	status = PangeaScript_SetStartupScript("Data/Scripts/dist/main.js");
	assert(status == PANGEA_SCRIPT_OK);
	assert(PangeaScript_HasRunnableModule());

	// Call level load hook
	PangeaScriptLevelContext lvlCtx = { 1, "Level 1" };
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, &lvlCtx);
	assert(status == PANGEA_SCRIPT_OK);

	// Verify native spawn occurred and stored initial coordinate
	assert(gSpawnedNativeDummy.position.x == 100.0f);
	assert(gSpawnedNativeDummy.position.y == 200.0f);
	assert(gSpawnedNativeDummy.position.z == 300.0f);

	// Call level start hook
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &lvlCtx);
	assert(status == PANGEA_SCRIPT_OK);

	// Verify native spawn's coordinate was mutated by level start hook
	assert(gSpawnedNativeDummy.position.x == 105.0f);
	assert(gSpawnedNativeDummy.position.y == 205.0f);
	assert(gSpawnedNativeDummy.position.z == 305.0f);
	assert(gSpawnedNativeDummy.velocity.x == 1.0f);
	assert(gSpawnedNativeDummy.velocity.y == 2.0f);
	assert(gSpawnedNativeDummy.velocity.z == 3.0f);

	// Call terrain item hook
	unsigned char dummyParams[2] = { 10, 20 };
	PangeaScriptTerrainItemContext terrainCtx = {
		.levelNum = 1,
		.itemType = 42,
		.remappedItemType = 42,
		.playerNum = 0,
		.networked = false,
		.x = 12.0f,
		.z = 34.0f,
		.flags = 0,
		.params = dummyParams,
		.paramCount = 2,
		.handled = false,
		.markInUse = false
	};
	status = PangeaScript_CallTerrainItemHook(&terrainCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(terrainCtx.handled == true);
	assert(terrainCtx.markInUse == true);

	terrainCtx.itemType = 43;
	terrainCtx.handled = false;
	terrainCtx.markInUse = false;
	status = PangeaScript_CallTerrainItemHook(&terrainCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(terrainCtx.handled == false);

	// Call spline item hook
	PangeaScriptSplineItemContext splineCtx = {
		.levelNum = 1,
		.itemType = 100,
		.splineNum = 2,
		.placement = 0.5f,
		.params = dummyParams,
		.paramCount = 2,
		.handled = false,
		.markInUse = false
	};
	status = PangeaScript_CallSplineItemHook(&splineCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(splineCtx.handled == true);
	assert(splineCtx.markInUse == true);

	// Call map item hook
	PangeaScriptMapItemContext mapCtx = {
		.levelNum = 1,
		.sceneNum = 5,
		.areaNum = 1,
		.itemType = 50,
		.x = 15.0f,
		.y = 25.0f,
		.params = dummyParams,
		.paramCount = 2,
		.handled = false,
		.markInUse = false
	};
	status = PangeaScript_CallMapItemHook(&mapCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(mapCtx.handled == true);
	assert(mapCtx.markInUse == true);

	// Test object frame hooks and position offset returning
	// Let's call object frame hook on the native spawned object (ID = 1, gen = 1)
	PangeaScriptObjectHandle nativeHandle = { 1, 1 };
	PangeaScriptFrameContext frameCtx = {
		.levelNum = 1,
		.frameNum = 1,
		.deltaSeconds = 0.016f,
		.levelTimeSeconds = 0.016f
	};
	PangeaScriptObjectFrameResult frameResult;
	status = PangeaScript_CallObjectFrame(nativeHandle, &frameCtx, &frameResult);
	assert(status == PANGEA_SCRIPT_OK);
	assert(frameResult.hasPositionOffset == true);
	assert(frameResult.positionOffset.x == 0.1f);
	assert(frameResult.positionOffset.y == 0.2f);
	assert(frameResult.positionOffset.z == 0.3f);

	// Verify position was updated to pos + (1, 1, 1) -> 106, 206, 306
	assert(gSpawnedNativeDummy.position.x == 106.0f);
	assert(gSpawnedNativeDummy.position.y == 206.0f);
	assert(gSpawnedNativeDummy.position.z == 306.0f);

	// Clean up files
	system("rm -rf Data");

	PangeaScript_Shutdown();
	printf("Duktape backend integration tests passed successfully!\n");
}

int main(void)
{
	printf("==================================================\n");
	printf(" Running Real Duktape Backend Integration Tests   \n");
	printf("==================================================\n");

	test_duktape_integration();

	printf("==================================================\n");
	printf(" All Duktape Backend Integration Tests Passed!    \n");
	printf("==================================================\n");
	return 0;
}

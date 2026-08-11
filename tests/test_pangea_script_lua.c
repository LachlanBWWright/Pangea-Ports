#include "../shared/script/pangea_script.h"
#include "../shared/script/pangea_script_backend.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int gNativeSpawns;

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

int main(void)
{
	const PangeaScriptGameInfo gameInfo = {.gameId = "Test", .gameName = "Lua Test", .spawnNative = spawn_native};
	assert(PangeaScript_Init(&gameInfo) == PANGEA_SCRIPT_OK);
	const char* source =
		"local pangea = require('pangea')\n"
		"assert(os == nil and io == nil and debug == nil)\n"
		"local entry = {}\n"
		"function entry.onLevelStart(ctx)\n"
		"  pangea.spawn.native(6, {x=10,y=20,z=30}, {param0=2})\n"
		"end\n"
		"function entry.onTerrainItem(ctx)\n"
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
	PangeaScriptTerrainItemContext item = {.levelNum = 1, .itemType = 2, .params = params, .paramCount = 4};
	assert(PangeaScriptBackend_CallTerrainItemHook(backend, &item, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(item.handled && item.markInUse && item.remappedItemType == 9);
	const char* budgetSource = "return { onFrame = function() while true do end end }";
	assert(PangeaScriptBackend_Load(backend, budgetSource, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {0};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error)) == PANGEA_SCRIPT_BUDGET_EXCEEDED);
	PangeaScriptBackend_Destroy(backend);
	test_lifecycle_name("CroMagRally", "onRaceStart");
	test_lifecycle_name("BillyFrontier", "onAreaStart");
	test_lifecycle_name("MightyMike", "onAreaStart");
	PangeaScript_Shutdown();
	puts("Lua backend integration tests passed");
	return 0;
}

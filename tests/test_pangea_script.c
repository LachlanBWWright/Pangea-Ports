#include "../shared/script/pangea_script.h"
#include "../shared/script/pangea_script_backend.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock implementation of backend functions to allow full control in tests
struct PangeaScriptBackend {
	int dummy;
};

static PangeaScriptStatus g_mock_load_status = PANGEA_SCRIPT_OK;
static PangeaScriptStatus g_mock_hook_status = PANGEA_SCRIPT_OK;
static int g_mock_scripted_spawn_count;
static int g_mock_native_spawn_count;
static bool g_mock_executed_hook = true;
static PangeaScriptGameInfo g_mock_effective_info;
bool PangeaScriptBackend_DidExecuteHook(PangeaScriptBackend* backend)
{ (void) backend; return g_mock_executed_hook; }

static PangeaScriptStatus mock_spawn_native(const char* id, float x, float y, float z, const int params[4], PangeaScriptObjectHandle* outHandle)
{
	(void) x; (void) y; (void) z; (void) outHandle;
	assert(strcmp(id, "6") == 0);
	assert(params[0] == 2);
	g_mock_native_spawn_count++;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus mock_spawn_scripted(
	const char* id,
	float x,
	float y,
	float z,
	PangeaScriptObjectHandle* outHandle);

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo)
{
	static PangeaScriptBackend backend;
	g_mock_effective_info = *gameInfo;
	return &backend;
}

void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend)
{
	(void) backend;
}

PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity)
{
	(void) backend;
	(void) source;
	(void) error;
	(void) errorCapacity;
	return g_mock_load_status;
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) hook;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) result;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallTriggerHook(PangeaScriptBackend* backend, const PangeaScriptTriggerContext* context, PangeaScriptTriggerResult* result, char* error, int errorCapacity)
{ (void) backend; (void) context; (void) result; (void) error; (void) errorCapacity; return g_mock_hook_status; }

PangeaScriptStatus PangeaScriptBackend_CallPickupHook(PangeaScriptBackend* backend, const PangeaScriptPickupContext* context, PangeaScriptPickupResult* result, char* error, int errorCapacity)
{ (void) backend; (void) context; (void) result; (void) error; (void) errorCapacity; return g_mock_hook_status; }

PangeaScriptStatus PangeaScriptBackend_CallWeaponHitHook(PangeaScriptBackend* backend, const PangeaScriptWeaponHitContext* context, PangeaScriptWeaponHitResult* result, char* error, int errorCapacity)
{ (void) backend; (void) context; (void) result; (void) error; (void) errorCapacity; return g_mock_hook_status; }

PangeaScriptStatus PangeaScriptBackend_CallDamageHook(PangeaScriptBackend* backend, const PangeaScriptDamageContext* context, PangeaScriptDamageResult* result, char* error, int errorCapacity)
{ (void) backend; (void) context; (void) result; (void) error; (void) errorCapacity; return g_mock_hook_status; }

PangeaScriptStatus PangeaScriptBackend_CallDamageAppliedHook(PangeaScriptBackend* backend, const PangeaScriptDamageContext* context, char* error, int errorCapacity)
{ (void) backend; (void) context; (void) error; (void) errorCapacity; return g_mock_hook_status; }

PangeaScriptStatus PangeaScriptBackend_CallPlayerEvent(PangeaScriptBackend* backend, const PangeaScriptPlayerEventContext* context, const char* event, char* error, int errorCapacity)
{ (void) backend; (void) context; (void) event; (void) error; (void) errorCapacity; return g_mock_hook_status; }

void PangeaScriptBackend_ClearObjectState(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle)
{ (void) backend; (void) handle; }

void PangeaScriptBackend_ClearObjectResources(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle)
{ (void) backend; (void) handle; }

void PangeaScriptBackend_CaptureObjectCheckpointState(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle)
{ (void) backend; (void) handle; }

bool PangeaScriptBackend_RestoreObjectCheckpointState(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle)
{ (void) backend; (void) handle; return false; }

void PangeaScriptBackend_ResetObjectStates(PangeaScriptBackend* backend)
{ (void) backend; }

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

static PangeaScriptStatus mock_spawn_scripted(const char* id, float x, float y, float z, PangeaScriptObjectHandle* outHandle)
{
 assert(strcmp(id, "custom.test") == 0 && x == 10 && y == 20 && z == 30);
 g_mock_scripted_spawn_count++;
 static DummyObject object;
 object.position = (PangeaScriptVector3){x,y,z};
 PangeaScriptObjectRegistration registration = {
  .nativeObject = &object, .ops = &kDummyOps, .objectType = id,
  .capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
 };
 return PangeaScript_RegisterObject(&registration, outHandle);
}

static void write_temp_file(const char* path, const char* content)
{
	FILE* f = fopen(path, "w");
	assert(f != NULL);
	fputs(content, f);
	fclose(f);
}

void test_capability_gates(void)
{
	printf("Testing object capability level gates...\n");

	PangeaScriptGameInfo gameInfo = {.gameId = "TestGame", .gameName = "Test Game"};
	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	assert(status == PANGEA_SCRIPT_OK);

	DummyObject dummy = { {1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, false };
	PangeaScriptObjectHandle handle = {0};

	// 1. UNSUPPORTED capability level
	PangeaScriptObjectRegistration regUnsup = {
		.nativeObject = &dummy,
		.ops = &kDummyOps,
		.tags = NULL,
		.tagCount = 0,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_UNSUPPORTED
	};
	status = PangeaScript_RegisterObject(&regUnsup, &handle);
	assert(status == PANGEA_SCRIPT_OK);
	assert(handle.id > 0);

	PangeaScriptVector3 pos;
	assert(!PangeaScript_GetObjectPosition(handle, &pos));
	assert(!PangeaScript_SetObjectPosition(handle, &pos));
	assert(!PangeaScript_SetObjectVelocity(handle, &pos));
	assert(!PangeaScript_DeleteObject(handle));
	assert(!dummy.deleted);

	// 2. READ_ONLY capability level
	PangeaScriptObjectRegistration regReadOnly = {
		.nativeObject = &dummy,
		.ops = &kDummyOps,
		.tags = NULL,
		.tagCount = 0,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_READ_ONLY
	};
	status = PangeaScript_RegisterObject(&regReadOnly, &handle);
	assert(status == PANGEA_SCRIPT_OK);

	assert(PangeaScript_GetObjectPosition(handle, &pos));
	assert(pos.x == 1.0f && pos.y == 2.0f && pos.z == 3.0f);
	assert(!PangeaScript_SetObjectPosition(handle, &pos));
	assert(!PangeaScript_SetObjectVelocity(handle, &pos));
	assert(!PangeaScript_DeleteObject(handle));

	// 3. BASE capability level (transforms/positions allowed, no velocity/delete)
	PangeaScriptObjectRegistration regBase = {
		.nativeObject = &dummy,
		.ops = &kDummyOps,
		.tags = NULL,
		.tagCount = 0,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_BASE
	};
	status = PangeaScript_RegisterObject(&regBase, &handle);
	assert(status == PANGEA_SCRIPT_OK);

	PangeaScriptVector3 newPos = { 10.0f, 20.0f, 30.0f };
	assert(PangeaScript_GetObjectPosition(handle, &pos));
	assert(PangeaScript_SetObjectPosition(handle, &newPos));
	assert(dummy.position.x == 10.0f && dummy.position.y == 20.0f && dummy.position.z == 30.0f);
	assert(!PangeaScript_SetObjectVelocity(handle, &newPos));
	assert(!PangeaScript_DeleteObject(handle));

	// 4. FULL capability level (everything allowed)
	PangeaScriptObjectRegistration regFull = {
		.nativeObject = &dummy,
		.ops = &kDummyOps,
		.tags = NULL,
		.tagCount = 0,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL
	};
	status = PangeaScript_RegisterObject(&regFull, &handle);
	assert(status == PANGEA_SCRIPT_OK);

	PangeaScriptVector3 vel = { 5.0f, 5.0f, 5.0f };
	assert(PangeaScript_SetObjectVelocity(handle, &vel));
	assert(dummy.velocity.x == 5.0f && dummy.velocity.y == 5.0f && dummy.velocity.z == 5.0f);
	assert(PangeaScript_DeleteObject(handle));
	assert(dummy.deleted);

	PangeaScript_Shutdown();
	printf("Capability gates tests passed!\n");
}

void test_config_parsing_and_sandbox(void)
{
	printf("Testing config parsing and path traversal protections...\n");

	PangeaScriptGameInfo gameInfo = {.gameId = "TestGame", .gameName = "Test Game"};
	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	assert(status == PANGEA_SCRIPT_OK);

	// Temp directory setup
	system("mkdir -p Data/Scripts/config");
	system("mkdir -p Data/Scripts/dist");

	// Write dummy script files
	write_temp_file("Data/Scripts/dist/main.lua", "return {}");
	write_temp_file("Data/Scripts/dist/level1.lua", "return {}");

	// 1. Valid config
	const char* valid_config = 
		"{\n"
		"  \"version\": 1,\n"
		"  \"levels\": {\n"
		"    \"1\": {\n"
		"      \"script\": \"Data/Scripts/dist/level1.lua\",\n"
		"      \"customObjects\": [{\n"
		"        \"id\": \"custom.test-model\",\n"
		"        \"visual\": {\"kind\": \"customDisplayGroup\", \"modelPath\": \"Data/Scripts/assets/models/test.bg3d\", \"modelObject\": 2, \"scale\": 1.5, \"slot\": 400},\n"
		"        \"collision\": {\"kind\": \"preset\", \"preset\": \"solidBox\", \"bounds\": {\"width\": 2, \"height\": 3, \"depth\": 4}}\n"
		"      }, {\n"
		"        \"id\": \"custom.test-skeleton\",\n"
		"        \"visual\": {\"kind\": \"customSkeleton\", \"modelPath\": \"Data/Scripts/assets/skeletons/test.bg3d\", \"skeletonPath\": \"Data/Scripts/assets/skeletons/test.skeleton\", \"animations\": {\"idle\": 0, \"walk\": 2}, \"initialAnimation\": \"idle\", \"animationSpeed\": 1, \"scale\": 1, \"slot\": 450},\n"
		"        \"collision\": {\"kind\": \"none\"}\n"
		"      }],\n"
		"      \"terrainReplacements\": [{\"id\": \"replace-3\", \"itemIndex\": 3, \"nativeType\": 12, \"x\": 100, \"z\": 200, \"customObjectId\": \"custom.test-model\", \"strict\": false}],\n"
		"      \"mapReplacements\": [{\"id\": \"replace-map-5\", \"itemIndex\": 5, \"nativeType\": 8, \"x\": 12, \"y\": 24, \"customObjectId\": \"custom.test-model\", \"strict\": true}],\n"
		"      \"splineReplacements\": [{\"id\": \"replace-spline\", \"splineNum\": 2, \"itemIndex\": 4, \"nativeType\": 7, \"placement\": 0.25, \"customObjectId\": \"custom.test-model\", \"strict\": false}]\n"
		"    }\n"
		"  }\n"
		"}\n";
	write_temp_file("Data/Scripts/config/levels.json", valid_config);
	PangeaScript_SetConfigPath("Data/Scripts/config/levels.json");
	status = PangeaScript_LoadLevelConfig(1);
	assert(status == PANGEA_SCRIPT_OK);
	assert(PangeaScript_HasRunnableModule());
	assert(PangeaScript_GetCustomObjectDefinitionCount() == 2);
	const PangeaScriptCustomObjectDefinition* customDefinition =
		PangeaScript_GetCustomObjectDefinition("custom.test-model");
	assert(customDefinition != NULL);
	assert(customDefinition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP);
	assert(strcmp(customDefinition->modelPath, "Data/Scripts/assets/models/test.bg3d") == 0);
	assert(customDefinition->modelObject == 2);
	assert(customDefinition->scale == 1.5f);
	assert(customDefinition->slot == 400);
	assert(customDefinition->collisionPreset == PANGEA_SCRIPT_COLLISION_SOLID_BOX);
	assert(customDefinition->collisionBoundsSet);
	assert(customDefinition->collisionWidth == 2.0f);
	assert(customDefinition->collisionHeight == 3.0f);
	assert(customDefinition->collisionDepth == 4.0f);
	const PangeaScriptCustomObjectDefinition* skeletonDefinition =
		PangeaScript_GetCustomObjectDefinition("custom.test-skeleton");
	assert(skeletonDefinition != NULL);
	assert(skeletonDefinition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON);
	assert(strcmp(skeletonDefinition->initialAnimationName, "idle") == 0);
	assert(skeletonDefinition->animationCount == 2);
	assert(strcmp(skeletonDefinition->animationNames[1], "walk") == 0);
	assert(skeletonDefinition->animationIndices[1] == 2);
	const PangeaScriptTerrainReplacement* replacement =
		PangeaScript_GetTerrainReplacement(3, 12, 100.0f, 200.0f);
	assert(replacement != NULL);
	assert(strcmp(replacement->customObjectId, "custom.test-model") == 0);
	assert(!replacement->strict);
	assert(PangeaScript_GetTerrainReplacement(4, 12, 100.0f, 200.0f) == NULL);
	const PangeaScriptMapReplacement* mapReplacement =
		PangeaScript_GetMapReplacement(5, 8, 12.0f, 24.0f);
	assert(mapReplacement != NULL);
	assert(strcmp(mapReplacement->customObjectId, "custom.test-model") == 0);
	assert(mapReplacement->strict);
	assert(PangeaScript_GetMapReplacement(5, 8, 12.0f, 25.0f) == NULL);
	const PangeaScriptSplineReplacement* splineReplacement =
		PangeaScript_GetSplineReplacement(2, 4, 7, 0.25f);
	assert(splineReplacement != NULL);
	assert(strcmp(splineReplacement->customObjectId, "custom.test-model") == 0);

	// 2. Traversal path rejection: using ".."
	const char* traversal_config = 
		"{\n"
		"  \"version\": 1,\n"
		"  \"levels\": {\n"
		"    \"1\": {\n"
		"      \"script\": \"Data/Scripts/dist/../../evil.lua\"\n"
		"    }\n"
		"  }\n"
		"}\n";
	write_temp_file("Data/Scripts/config/levels.json", traversal_config);
	status = PangeaScript_LoadLevelConfig(1);
	assert(status == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(PangeaScript_GetLastError(), "path traversal") != NULL);

	// 3. Invalid script path: outside Data/Scripts/
	const char* outside_config = 
		"{\n"
		"  \"version\": 1,\n"
		"  \"levels\": {\n"
		"    \"1\": {\n"
		"      \"script\": \"Data/evil.lua\"\n"
		"    }\n"
		"  }\n"
		"}\n";
	write_temp_file("Data/Scripts/config/levels.json", outside_config);
	status = PangeaScript_LoadLevelConfig(1);
	assert(status == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(PangeaScript_GetLastError(), "Invalid script path") != NULL);

	// 4. Missing required field 'version'
	const char* missing_version_config = 
		"{\n"
		"  \"levels\": {\n"
		"    \"1\": {\n"
		"      \"script\": \"Data/Scripts/dist/level1.lua\"\n"
		"    }\n"
		"  }\n"
		"}\n";
	write_temp_file("Data/Scripts/config/levels.json", missing_version_config);
	status = PangeaScript_LoadLevelConfig(1);
	assert(status == PANGEA_SCRIPT_CONFIG_ERROR);
	assert(strstr(PangeaScript_GetLastError(), "version") != NULL);

	// Clean up files
	system("rm -rf Data");

	PangeaScript_Shutdown();
	printf("Config parsing and sandbox tests passed!\n");
}

void test_level_settings_accessors(void)
{
	printf("Testing level settings accessors...\n");

	PangeaScriptGameInfo gameInfo = {.gameId = "TestGame", .gameName = "Test Game"};
	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	assert(status == PANGEA_SCRIPT_OK);

	system("mkdir -p Data/Scripts/config");
	system("mkdir -p Data/Scripts/dist");
	write_temp_file("Data/Scripts/dist/main.lua", "return {}");

	const char* valid_config =
		"{\n"
		"  \"version\": 1,\n"
		"  \"levels\": {\n"
		"    \"1\": {\n"
		"      \"script\": \"Data/Scripts/dist/main.lua\",\n"
		"      \"levelSettings\": {\n"
		"        \"gravity\": 3900,\n"
		"        \"debugSplineFlatY\": 500.5,\n"
		"        \"skipIntro\": true,\n"
		"        \"song\": \"slimeBoss\",\n"
		"        \"assetDependencies\": [\n"
		"          { \"kind\": \"skeleton\", \"id\": \"moth\" },\n"
		"          { \"kind\": \"spriteGroup\", \"id\": \"Level6_Closet\" }\n"
		"        ],\n"
		"        \"ignoredObject\": { \"nested\": true },\n"
		"        \"ignoredArray\": [1, 2, 3]\n"
		"      }\n"
		"    },\n"
		"    \"2\": {\n"
		"      \"levelSettings\": {\n"
		"        \"gravity\": 1200\n"
		"      }\n"
		"    }\n"
		"  }\n"
		"}\n";
	write_temp_file("Data/Scripts/config/levels.json", valid_config);
	PangeaScript_SetConfigPath("Data/Scripts/config/levels.json");

	status = PangeaScript_LoadLevelConfig(1);
	assert(status == PANGEA_SCRIPT_OK);

	float floatValue = 0.0f;
	int intValue = 0;
	bool boolValue = false;
	char stringValue[32];
	assert(PangeaScript_GetLevelFloatSetting("gravity", &floatValue));
	assert(floatValue == 3900.0f);
	assert(PangeaScript_GetLevelIntSetting("gravity", &intValue));
	assert(intValue == 3900);
	assert(PangeaScript_GetLevelFloatSetting("debugSplineFlatY", &floatValue));
	assert(floatValue == 500.5f);
	assert(!PangeaScript_GetLevelIntSetting("debugSplineFlatY", &intValue));
	assert(PangeaScript_GetLevelBoolSetting("skipIntro", &boolValue));
	assert(boolValue);
	assert(PangeaScript_GetLevelStringSetting("song", stringValue, (int)sizeof(stringValue)));
	assert(strcmp(stringValue, "slimeBoss") == 0);
	assert(!PangeaScript_GetLevelStringSetting("gravity", stringValue, (int)sizeof(stringValue)));
	assert(!PangeaScript_GetLevelBoolSetting("missing", &boolValue));
	assert(!PangeaScript_GetLevelBoolSetting("ignoredObject", &boolValue));
	assert(!PangeaScript_GetLevelIntSetting("ignoredArray", &intValue));
	assert(PangeaScript_GetLevelAssetDependencyCount() == 2);
	PangeaScriptAssetDependency dependency;
	assert(PangeaScript_GetLevelAssetDependency(0, &dependency));
	assert(strcmp(dependency.kind, "skeleton") == 0);
	assert(strcmp(dependency.id, "moth") == 0);
	assert(PangeaScript_GetLevelAssetDependency(1, &dependency));
	assert(strcmp(dependency.kind, "spriteGroup") == 0);
	assert(strcmp(dependency.id, "Level6_Closet") == 0);
	assert(!PangeaScript_GetLevelAssetDependency(2, &dependency));

	status = PangeaScript_LoadLevelConfig(2);
	assert(status == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetLevelIntSetting("gravity", &intValue));
	assert(intValue == 1200);
	assert(!PangeaScript_GetLevelStringSetting("song", stringValue, (int)sizeof(stringValue)));
	assert(PangeaScript_GetLevelAssetDependencyCount() == 0);

	const char* invalid_config =
		"{\n"
		"  \"version\": 1,\n"
		"  \"levels\": {\n"
		"    \"2\": {\n"
		"      \"levelSettings\": \"wrong\"\n"
		"    }\n"
		"  }\n"
		"}\n";
	write_temp_file("Data/Scripts/config/levels.json", invalid_config);
	status = PangeaScript_LoadLevelConfig(2);
	assert(status == PANGEA_SCRIPT_CONFIG_ERROR);
	/* A rejected configuration preserves the last valid level settings. */
	assert(PangeaScript_GetLevelIntSetting("gravity", &intValue));
	assert(intValue == 1200);
	assert(PangeaScript_GetLevelAssetDependencyCount() == 0);

	system("rm -rf Data");
	PangeaScript_Shutdown();
	printf("Level settings accessor tests passed!\n");
}

void test_consecutive_failures(void)
{
	printf("Testing host shutdown on consecutive hook failures...\n");

	PangeaScriptGameInfo gameInfo = {.gameId = "TestGame", .gameName = "Test Game"};
	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	assert(status == PANGEA_SCRIPT_OK);

	// Mock script loaded status so call hooks will execute mock logic
	// In the mock, PangeaScriptBackend_CallLevelHook returns g_mock_hook_status
	g_mock_load_status = PANGEA_SCRIPT_OK;
	// Create a dummy startup script path and simulate successful load
	system("mkdir -p Data/Scripts/dist");
	write_temp_file("Data/Scripts/dist/main.lua", "return {}");
	status = PangeaScript_SetStartupScript("Data/Scripts/dist/main.lua");
	assert(status == PANGEA_SCRIPT_OK);
	assert(PangeaScript_HasRunnableModule());

	PangeaScriptLevelContext ctx = {.levelNum = 1, .levelName = "Level 1"};

	// Call with OK status, should be fine
	g_mock_hook_status = PANGEA_SCRIPT_OK;
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_GetStatusScriptsDisabled());
	assert(PangeaScript_GetStatusErrorCount() == 0);

	// Bad host arguments are diagnostics, not failed script callbacks.
 for (int i = 0; i < 8; i++)
  assert(PangeaScript_CallFrameHook(NULL) == PANGEA_SCRIPT_BAD_ARGUMENT);
 assert(!PangeaScript_GetStatusScriptsDisabled());
 int initialErrors = PangeaScript_GetStatusErrorCount();
 // A real successful callback resets the consecutive failure streak.
 g_mock_hook_status = PANGEA_SCRIPT_RUNTIME_ERROR;
 for (int i = 0; i < 4; i++)
  assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx) == PANGEA_SCRIPT_RUNTIME_ERROR);
 g_mock_hook_status = PANGEA_SCRIPT_OK;
 assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx) == PANGEA_SCRIPT_OK);
 initialErrors += 4;
 // Fail the hook calls repeatedly
 g_mock_hook_status = PANGEA_SCRIPT_RUNTIME_ERROR;

	for (int i = 0; i < 4; i++) {
		status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx);
		assert(status == PANGEA_SCRIPT_RUNTIME_ERROR);
		assert(!PangeaScript_GetStatusScriptsDisabled());
		assert(PangeaScript_GetStatusErrorCount() == initialErrors + i + 1);
	}

	// An empty deferred queue and absent optional hook do not reset the streak.
 PangeaScriptFrameContext emptyFrame = {0};
 assert(PangeaScript_ApplyDeferredActions(&emptyFrame) == PANGEA_SCRIPT_OK);
 g_mock_executed_hook = false; g_mock_hook_status = PANGEA_SCRIPT_OK;
 assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, &ctx) == PANGEA_SCRIPT_OK);
 g_mock_executed_hook = true; g_mock_hook_status = PANGEA_SCRIPT_RUNTIME_ERROR;
 // 5th failure should trigger host shutdown
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx);
	assert(status == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(PangeaScript_GetStatusScriptsDisabled());
	assert(PangeaScript_GetStatusErrorCount() == initialErrors + 5);

	// Subsequent calls should return PANGEA_SCRIPT_RUNTIME_ERROR immediately and not call backend
	g_mock_hook_status = PANGEA_SCRIPT_OK; // Even if backend works now, host should remain disabled
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx);
	assert(status == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(PangeaScript_GetStatusScriptsDisabled());

	system("rm -rf Data");
	PangeaScript_Shutdown();
	printf("Consecutive failures tests passed!\n");
}

static void test_scripted_spawn_adapter(void)
{
	printf("Testing game-owned scripted spawn adapter...\n");
	const PangeaScriptGameInfo gameInfo =
	{
		.gameId = "test-game",
		.gameName = "Test Game",
		.spawnScripted = mock_spawn_scripted,
	};
	PangeaScriptObjectHandle handle = {0};
	g_mock_scripted_spawn_count = 0;
	assert(PangeaScript_Init(&gameInfo) == PANGEA_SCRIPT_OK);
	system("mkdir -p Data/Scripts/dist");
 write_temp_file("Data/Scripts/dist/main.lua", "return {}");
 assert(PangeaScript_SetStartupScript("Data/Scripts/dist/main.lua") == PANGEA_SCRIPT_OK);
 assert(PangeaScript_RegisterScriptedObject("custom.test", 10.0f, 20.0f, 30.0f, &handle) == PANGEA_SCRIPT_OK);
	assert(g_mock_scripted_spawn_count == 1);
	assert(handle.id > 0 && handle.generation > 0);
	assert(PangeaScript_ObjectExists(handle));
	PangeaScript_Shutdown();
	printf("Game-owned scripted spawn adapter tests passed!\n");
}

static void test_scripted_object_reuse(void)
{
	const PangeaScriptGameInfo gameInfo = {
		.gameId = "test-game",
		.gameName = "Test Game",
	};
	PangeaScriptObjectHandle previous = {0};
	assert(PangeaScript_Init(&gameInfo) == PANGEA_SCRIPT_OK);
	for (int iteration = 0; iteration < 4096; iteration++)
	{
		PangeaScriptObjectHandle handle = {0};
		assert(PangeaScript_RegisterScriptedObject("custom.reusable", 1.0f, 2.0f, 3.0f, &handle) == PANGEA_SCRIPT_OK);
		assert(PangeaScript_ObjectExists(handle));
		if (previous.id == handle.id)
			assert(previous.generation != handle.generation);
		assert(PangeaScript_UnregisterObject(handle));
		assert(!PangeaScript_ObjectExists(handle));
		previous = handle;
	}
	PangeaScript_Shutdown();
	printf("Scripted object reuse tests passed!\n");
}

static void test_native_spawn_validation(void)
{
	const PangeaScriptGameInfo gameInfo = { .gameId = "TestGame", .gameName = "Test Game", .spawnNative = mock_spawn_native };
	PangeaScriptObjectHandle handle = {0};
	const int validParams[4] = {2, 0, 0, 0};
	const int invalidParams[4] = {256, 0, 0, 0};
	g_mock_native_spawn_count = 0;
	assert(PangeaScript_Init(&gameInfo) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_SpawnNative("6", 1, 2, 3, validParams, &handle) == PANGEA_SCRIPT_OK);
	assert(g_mock_native_spawn_count == 1);
	assert(PangeaScript_SpawnNative("6", 1, 2, 3, invalidParams, &handle) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(g_mock_native_spawn_count == 1);
	assert(strstr(PangeaScript_GetLastError(), "0-255") != NULL);
	PangeaScript_Shutdown();
}

static void test_frame_snapshot_and_persistence_boundary(void)
{
 const PangeaScriptGameInfo game = {.gameId="test", .gameName="Test"};
 assert(PangeaScript_Init(&game)==PANGEA_SCRIPT_OK);
 system("mkdir -p Data/Scripts/dist");
 write_temp_file("Data/Scripts/dist/main.lua","return {}");
 assert(PangeaScript_SetStartupScript("Data/Scripts/dist/main.lua")==PANGEA_SCRIPT_OK);
 unsigned char value[4]={0,1,2,3}, restored[4]={0}; int size=0;
 assert(g_mock_effective_info.savePersistent("exact-capacity",value,4)==PANGEA_SCRIPT_OK);
 assert(g_mock_effective_info.loadPersistent("exact-capacity",restored,4,&size)==PANGEA_SCRIPT_OK);
 assert(size==4 && memcmp(value,restored,4)==0);
 assert(g_mock_effective_info.loadPersistent("exact-capacity",restored,3,&size)==PANGEA_SCRIPT_RUNTIME_ERROR);
 char mode[16]="race";
 PangeaScriptFrameContext frame={.mode=mode};
 PangeaScript_ResetLifecycleTrace();
 assert(PangeaScript_CallFrameHook(&frame)==PANGEA_SCRIPT_OK);
 strcpy(mode,"finished");
 assert(PangeaScript_CallFrameHook(&frame)==PANGEA_SCRIPT_OK);
 int transitions=0;
 for(int i=0;;i++) {
  PangeaScriptLifecycleTraceEntry entry;
  if(!PangeaScript_GetLifecycleTraceEntry(i,&entry)) break;
  if(strcmp(entry.eventId,"modeTransition")==0) transitions++;
 }
 assert(transitions==1);
 PangeaScript_Shutdown();
}

static bool g_mock_invalid_asset_size;
static int g_mock_asset_reads;
static char* mock_packaged_text(const char* path, long* outSize)
{
 const char* source = NULL;
 if (strcmp(path,"Data/Scripts/dist/packaged-only.lua")==0) source="return {}";
 if (strcmp(path,"Data/Scripts/config/packaged-only.json")==0)
  source="{\"version\":1,\"levels\":{\"7\":{\"script\":\"Data/Scripts/dist/packaged-only.lua\",\"levelSettings\":{\"gravity\":321}}}}";
 if (!source) return NULL;
 size_t size=strlen(source);
 char* result=malloc(size+1); assert(result);
 memcpy(result,source,size+1);
 *outSize=g_mock_invalid_asset_size ? -1 : (long)size;
 g_mock_asset_reads++;
 return result;
}

static void test_packaged_script_transport(void)
{
 const PangeaScriptGameInfo game={.gameId="test",.gameName="Test",.readTextFile=mock_packaged_text};
 assert(PangeaScript_Init(&game)==PANGEA_SCRIPT_OK);
 g_mock_asset_reads=0; g_mock_invalid_asset_size=false;
 assert(PangeaScript_SetStartupScript("Data/Scripts/dist/packaged-only.lua")==PANGEA_SCRIPT_OK);
 assert(PangeaScript_HasRunnableModule() && g_mock_asset_reads==1);
 assert(PangeaScript_SetConfigPath("Data/Scripts/config/packaged-only.json")==PANGEA_SCRIPT_OK);
 assert(PangeaScript_LoadLevelConfig(7)==PANGEA_SCRIPT_OK);
 int gravity=0;
 assert(PangeaScript_GetLevelIntSetting("gravity",&gravity) && gravity==321);
 assert(g_mock_asset_reads>=3);
 g_mock_invalid_asset_size=true;
 assert(PangeaScript_Reload()==PANGEA_SCRIPT_FILE_NOT_FOUND);
 assert(!PangeaScript_HasRunnableModule());
 PangeaScript_Shutdown();
 g_mock_invalid_asset_size=false;
}

static void test_large_native_item_registry(void)
{
	const PangeaScriptGameInfo game = {.gameId = "registry", .gameName = "Registry"};
	PangeaScriptNativeItem items[256] = {0};
	char ids[256][32];
	assert(PangeaScript_Init(&game) == PANGEA_SCRIPT_OK);
	for (int i = 0; i < 256; i++)
	{
		snprintf(ids[i], sizeof(ids[i]), "registry.item.%d", i);
		items[i] = (PangeaScriptNativeItem){
			.id = ids[i], .nativeType = i,
			.category = "terrain", .dependencySummary = "registry test assets",
		};
	}
	/* Otto's production registry exceeds the former 128-item capacity. */
	assert(PangeaScript_RegisterNativeItems(items, 256) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetNativeItemCount() == 256);
	for (int i = 0; i < 256; i++)
	{
		const PangeaScriptNativeItem* item = PangeaScript_GetNativeItem(i);
		assert(item && item->nativeType == i && strcmp(item->id, ids[i]) == 0);
	}
	assert(PangeaScript_RegisterNativeItems(items, 257) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_GetNativeItemCount() == 256);
	PangeaScript_Shutdown();
}

int main(void)
{
	printf("========================================\n");
	printf(" Running Native PangeaScript Unit Tests \n");
	printf("========================================\n");

	test_large_native_item_registry();
	test_capability_gates();
	test_config_parsing_and_sandbox();
	test_level_settings_accessors();
	test_consecutive_failures();
	test_scripted_spawn_adapter();
	test_scripted_object_reuse();
	test_native_spawn_validation();
	test_frame_snapshot_and_persistence_boundary();
	test_packaged_script_transport();

	printf("========================================\n");
	printf(" All Native Unit Tests Passed!          \n");
	printf("========================================\n");
	return 0;
}

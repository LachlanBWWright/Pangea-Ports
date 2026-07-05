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

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo)
{
	static PangeaScriptBackend backend;
	(void) gameInfo;
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

PangeaScriptStatus PangeaScriptBackend_CallNamedLevelHook(PangeaScriptBackend* backend, const char* hookName, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) hookName;
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

PangeaScriptStatus PangeaScriptBackend_CallNamedFrameHook(PangeaScriptBackend* backend, const char* hookName, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) hookName;
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

PangeaScriptStatus PangeaScriptBackend_CallPickupCollectedHook(PangeaScriptBackend* backend, PangeaScriptPickupContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallWeaponHitHook(PangeaScriptBackend* backend, PangeaScriptWeaponHitContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallTriggerEnterHook(PangeaScriptBackend* backend, PangeaScriptTriggerContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectCollisionHook(PangeaScriptBackend* backend, PangeaScriptObjectCollisionContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallPlayerDamageHook(PangeaScriptBackend* backend, PangeaScriptPlayerDamageContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) error;
	(void) errorCapacity;
	return g_mock_hook_status;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectDeleteHook(PangeaScriptBackend* backend, const PangeaScriptObjectDeleteContext* context, char* error, int errorCapacity)
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

	PangeaScriptGameInfo gameInfo = { "TestGame", "Test Game" };
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
	assert(!PangeaScript_AddObjectTag(handle, "read-only-script-tag"));
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
	static const char* nativeTags[] = { "native", "item" };
	PangeaScriptObjectRegistration regFull = {
		.nativeObject = &dummy,
		.ops = &kDummyOps,
		.tags = nativeTags,
		.tagCount = 2,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL
	};
	status = PangeaScript_RegisterObject(&regFull, &handle);
	assert(status == PANGEA_SCRIPT_OK);
	assert(PangeaScript_GetObjectTagCount(handle) == 2);
	assert(PangeaScript_ObjectHasTag(handle, "native"));
	assert(PangeaScript_AddObjectTag(handle, "scripted-item"));
	assert(PangeaScript_AddObjectTag(handle, "scripted-item"));
	assert(PangeaScript_GetObjectTagCount(handle) == 3);
	assert(PangeaScript_ObjectHasTag(handle, "scripted-item"));
	assert(PangeaScript_RemoveObjectTag(handle, "scripted-item"));
	assert(!PangeaScript_ObjectHasTag(handle, "scripted-item"));
	assert(PangeaScript_GetObjectTagCount(handle) == 2);
	assert(!PangeaScript_RemoveObjectTag(handle, "native"));

	PangeaScriptVector3 vel = { 5.0f, 5.0f, 5.0f };
	assert(PangeaScript_SetObjectVelocity(handle, &vel));
	assert(dummy.velocity.x == 5.0f && dummy.velocity.y == 5.0f && dummy.velocity.z == 5.0f);
	assert(PangeaScript_DeleteObject(handle));
	assert(dummy.deleted);
	assert(!PangeaScript_ObjectExists(handle));

	PangeaScript_Shutdown();
	printf("Capability gates tests passed!\n");
}

void test_config_parsing_and_sandbox(void)
{
	printf("Testing config parsing and path traversal protections...\n");

	PangeaScriptGameInfo gameInfo = { "TestGame", "Test Game" };
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
			"      \"script\": \"Data/Scripts/dist/level1.lua\"\n"
		"    }\n"
		"  }\n"
		"}\n";
	write_temp_file("Data/Scripts/config/levels.json", valid_config);
	PangeaScript_SetConfigPath("Data/Scripts/config/levels.json");
	status = PangeaScript_LoadLevelConfig(1);
	assert(status == PANGEA_SCRIPT_OK);
	assert(PangeaScript_HasRunnableModule());

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
	assert(strstr(PangeaScript_GetLastError(), "must be within Data/Scripts/") != NULL);

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

	PangeaScriptGameInfo gameInfo = { "TestGame", "Test Game" };
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
	assert(!PangeaScript_GetLevelIntSetting("gravity", &intValue));
	assert(PangeaScript_GetLevelAssetDependencyCount() == 0);

	system("rm -rf Data");
	PangeaScript_Shutdown();
	printf("Level settings accessor tests passed!\n");
}

void test_consecutive_failures(void)
{
	printf("Testing host shutdown on consecutive hook failures...\n");

	PangeaScriptGameInfo gameInfo = { "TestGame", "Test Game" };
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

	PangeaScriptLevelContext ctx = { 1, "Level 1" };

	// Call with OK status, should be fine
	g_mock_hook_status = PANGEA_SCRIPT_OK;
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_GetStatusScriptsDisabled());
	assert(PangeaScript_GetStatusErrorCount() == 0);

	// Fail the hook calls repeatedly
	g_mock_hook_status = PANGEA_SCRIPT_RUNTIME_ERROR;

	for (int i = 0; i < 4; i++) {
		status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx);
		assert(status == PANGEA_SCRIPT_RUNTIME_ERROR);
		assert(!PangeaScript_GetStatusScriptsDisabled());
		assert(PangeaScript_GetStatusErrorCount() == i + 1);
	}

	// 5th failure should trigger host shutdown
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx);
	assert(status == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(PangeaScript_GetStatusScriptsDisabled());
	assert(PangeaScript_GetStatusErrorCount() == 5);

	// Subsequent calls should return PANGEA_SCRIPT_RUNTIME_ERROR immediately and not call backend
	g_mock_hook_status = PANGEA_SCRIPT_OK; // Even if backend works now, host should remain disabled
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &ctx);
	assert(status == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(PangeaScript_GetStatusScriptsDisabled());

	system("rm -rf Data");
	PangeaScript_Shutdown();
	printf("Consecutive failures tests passed!\n");
}

int main(void)
{
	printf("========================================\n");
	printf(" Running Native PangeaScript Unit Tests \n");
	printf("========================================\n");

	test_capability_gates();
	test_config_parsing_and_sandbox();
	test_level_settings_accessors();
	test_consecutive_failures();

	printf("========================================\n");
	printf(" All Native Unit Tests Passed!          \n");
	printf("========================================\n");
	return 0;
}

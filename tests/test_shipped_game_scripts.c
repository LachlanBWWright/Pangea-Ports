#include "pangea_script.h"
#include "pangea_script_backend.h"
#include "pangea_script_config.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* gGameDirectory;
static const char* gFixtureScriptPath;

/* Exercise packaged transport against the repository's real authored files. */
static char* read_shipped_file(const char* path, long* outSize)
{
	if (gFixtureScriptPath && strcmp(path, "Data/Scripts/config/bob-test.json") == 0)
	{
		char fixture[1024];
		int length = snprintf(fixture, sizeof fixture,
			"{\"version\":1,\"levels\":{\"0\":{\"script\":\"%s\",\"levelSettings\":{\"scriptBobHeight\":12,\"scriptBobSpeed\":1}}}}", gFixtureScriptPath);
		assert(length > 0 && length < (int)sizeof fixture);
		char* copy = malloc((size_t)length + 1);
		assert(copy);
		memcpy(copy, fixture, (size_t)length + 1);
		if (outSize) *outSize = length;
		return copy;
	}
	char filename[2048];
	int written = snprintf(filename, sizeof filename, "%s/games/%s/%s", PANGEA_SOURCE_ROOT, gGameDirectory, path);
	assert(written > 0 && written < (int)sizeof filename);
	FILE* file = fopen(filename, "rb");
	if (!file) return NULL;
	assert(fseek(file, 0, SEEK_END) == 0);
	long size = ftell(file);
	assert(size >= 0 && fseek(file, 0, SEEK_SET) == 0);
	char* text = malloc((size_t)size + 1);
	assert(text && fread(text, 1, (size_t)size, file) == (size_t)size);
	assert(fclose(file) == 0);
	text[size] = '\0';
	if (outSize) *outSize = size;
	return text;
}

static bool get_position(void* object, PangeaScriptVector3* outPosition)
{
	*outPosition = *(PangeaScriptVector3*)object;
	return true;
}

static const char* player_tag(void)
{
	if (strcmp(gGameDirectory, "Bugdom-android") == 0) return "bugdom.player";
	if (strcmp(gGameDirectory, "Bugdom2-Android") == 0) return "bugdom2.player";
	if (strcmp(gGameDirectory, "Nanosaur-android") == 0) return "nanosaur.player";
	assert(!"Add the shipped player's native tag to this integration fixture");
	return NULL;
}

static void check_bob_result(bool enabled)
{
	PangeaScriptVector3 nativePosition = {10, 20, 30};
	const char* tags[] = {player_tag()};
	const PangeaScriptObjectOps ops = {.getPosition = get_position};
	PangeaScriptObjectRegistration registration = {
		.nativeObject = &nativePosition, .ops = &ops, .objectType = tags[0],
		.tags = tags, .tagCount = 1, .capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
	};
	PangeaScriptObjectHandle handle;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.levelNum = 0, .levelTimeSeconds = 1.57079632679f};
	PangeaScriptObjectFrameResult result = {0};
	assert(PangeaScript_CallObjectFrame(handle, &frame, &result) == PANGEA_SCRIPT_OK);
	assert(result.hasPositionOffset == enabled);
	if (enabled)
	{
		assert(fabsf(result.positionOffset.y - 12) < .0001f);
		assert(result.positionOffset.x == 0 && result.positionOffset.z == 0);
		frame.levelTimeSeconds = 0;
		assert(PangeaScript_CallObjectFrame(handle, &frame, &result) == PANGEA_SCRIPT_OK);
		assert(result.hasPositionOffset && result.positionOffset.y == 0);
	}
	assert(nativePosition.x == 10 && nativePosition.y == 20 && nativePosition.z == 30);
	assert(PangeaScript_UnregisterObject(handle));
	const char* unrelatedTags[] = {"unrelated.player"};
	registration.tags = unrelatedTags;
	assert(PangeaScript_RegisterObject(&registration, &handle) == PANGEA_SCRIPT_OK);
	result = (PangeaScriptObjectFrameResult){0};
	assert(PangeaScript_CallObjectFrame(handle, &frame, &result) == PANGEA_SCRIPT_OK);
	assert(!result.hasPositionOffset);
	assert(PangeaScript_UnregisterObject(handle));
}

int main(int argc, char** argv)
{
	assert(argc == 2);
	gGameDirectory = argv[1];
	PangeaScriptGameInfo game = {
		.gameId = gGameDirectory,
		.gameName = gGameDirectory,
		.readTextFile = read_shipped_file,
	};
	long configSize = 0;
	char* configText = read_shipped_file("Data/Scripts/config/levels.json", &configSize);
	assert(configText && configSize > 0);
	PangeaConfig* config = calloc(1, sizeof *config);
	assert(config);
	char error[1024] = {0};
	int modules = 0;
	char shippedScriptPath[260] = {0};
	for (int level = 0; level < 16; level++)
	{
		PangeaScriptStatus status = PangeaScript_ParseConfig(configText, level, config, error, sizeof error);
		if (status != PANGEA_SCRIPT_OK) fprintf(stderr, "%s level %d: %s\n", gGameDirectory, level, error);
		assert(status == PANGEA_SCRIPT_OK);
		assert(PangeaScript_Init(&game) == PANGEA_SCRIPT_OK);
		assert(PangeaScript_SetConfigPath("Data/Scripts/config/levels.json") == PANGEA_SCRIPT_OK);
		status = PangeaScript_LoadLevelConfig(level);
		if (status != PANGEA_SCRIPT_OK) fprintf(stderr, "%s runtime config: %s\n", gGameDirectory, PangeaScript_GetLastError());
		assert(status == PANGEA_SCRIPT_OK);
		if (config->level.hasConfig && config->level.scriptPath[0])
		{
			long scriptSize = 0;
			char* script = read_shipped_file(config->level.scriptPath, &scriptSize);
			if (!script) fprintf(stderr, "%s references missing shipped script %s\n", gGameDirectory, config->level.scriptPath);
			assert(script && scriptSize > 0);
			assert(PangeaScript_HasRunnableModule());
			assert(strcmp(PangeaScript_GetStatusActiveScriptPath(), config->level.scriptPath) == 0);
			PangeaScriptLevelContext context = {.levelNum = level, .levelName = "shipped-config"};
			assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, &context) == PANGEA_SCRIPT_OK);
			assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &context) == PANGEA_SCRIPT_OK);
			check_bob_result(false);
			PangeaScriptFrameContext frame = {.levelNum = level, .frameNum = 1, .deltaSeconds = .016f};
			assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
			assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE, &context) == PANGEA_SCRIPT_OK);
			assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD, &context) == PANGEA_SCRIPT_OK);
			/* Optional hook dispatch reports success too: prove the authored load hook executes. */
			PangeaScriptBackend* backend = PangeaScriptBackend_Create(&game);
			assert(backend);
			assert(PangeaScriptBackend_Load(backend, script, error, sizeof error) == PANGEA_SCRIPT_OK);
			assert(PangeaScriptBackend_CallLevelHook(backend, PANGEA_SCRIPT_HOOK_LEVEL_LOAD, &context, error, sizeof error) == PANGEA_SCRIPT_OK);
			assert(PangeaScriptBackend_DidExecuteHook(backend));
			PangeaScriptBackend_Destroy(backend);
			free(script);
			snprintf(shippedScriptPath, sizeof shippedScriptPath, "%s", config->level.scriptPath);
			modules++;
		}
		PangeaScript_Shutdown();
	}
	if (modules)
	{
		/* Enable an authored module's real optional behavior through level settings. */
		gFixtureScriptPath = shippedScriptPath;
		assert(PangeaScript_Init(&game) == PANGEA_SCRIPT_OK);
		assert(PangeaScript_SetConfigPath("Data/Scripts/config/bob-test.json") == PANGEA_SCRIPT_OK);
		assert(PangeaScript_LoadLevelConfig(0) == PANGEA_SCRIPT_OK);
		PangeaScriptLevelContext context = {.levelNum = 0};
		assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, &context) == PANGEA_SCRIPT_OK);
		check_bob_result(true);
		PangeaScript_Shutdown();
	}
	free(config);
	free(configText);
	printf("%s: 16 shipped configuration levels, %d real module lifecycles passed\n", gGameDirectory, modules);
	return 0;
}

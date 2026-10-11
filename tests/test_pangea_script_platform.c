#include "../shared/script/pangea_script_platform_sdl.h"
#include <assert.h>

static const char* storageDirectory;
static bool usePackagedAsset;

void* SDL_LoadFile(const char* path, size_t* size)
{
	const char* expected = usePackagedAsset ? "Scripts/dist/main.lua" : "/bundle/Data/Scripts/dist/main.lua";
	if (strcmp(path, expected) != 0) return NULL;
	const char source[] = "return { onFrame = function(ctx) end }";
	*size = sizeof source - 1;
	void* data = malloc(sizeof source);
	assert(data);
	memcpy(data, source, sizeof source);
	return data;
}

const char* SDL_GetBasePath(void) { return "/bundle/"; }
void SDL_free(void* allocation) { free(allocation); }
char* SDL_GetPrefPath(const char* organization, const char* application)
{
	assert(strcmp(organization, "PangeaSoftware") == 0);
	assert(strcmp(application, "test-game") == 0);
	size_t size = strlen(storageDirectory) + 2;
	char* directory = malloc(size);
	assert(directory);
	snprintf(directory, size, "%s/", storageDirectory);
	return directory;
}

int main(int argc, char** argv)
{
	assert(argc == 2);
	storageDirectory = argv[1];
	PangeaScriptGameInfo gameInfo = {.gameId = "test-game"};
	PangeaScriptPlatform_Configure(&gameInfo);
	assert(gameInfo.readTextFile && gameInfo.loadPersistent && gameInfo.savePersistent);
	for (int packaged = 0; packaged < 2; packaged++)
	{
		usePackagedAsset = packaged != 0;
		long size = -1;
		char* source = gameInfo.readTextFile("Data/Scripts/dist/main.lua", &size);
		assert(source && size == (long)strlen(source));
		assert(strstr(source, "onFrame"));
		free(source);
		assert(!gameInfo.readTextFile("Data/Scripts/dist/missing.lua", &size));
		assert(size == 0);
	}
	const unsigned char value[] = {0, 1, 2, 255};
	unsigned char output[sizeof value];
	int size = -1;
	assert(gameInfo.savePersistent("progress", value, sizeof value) == PANGEA_SCRIPT_OK);
	assert(gameInfo.loadPersistent("progress", output, sizeof output, &size) == PANGEA_SCRIPT_OK);
	assert(size == sizeof value && memcmp(output, value, sizeof value) == 0);
	assert(gameInfo.loadPersistent("progress", output, sizeof output - 1, &size) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(size == 0);
	assert(gameInfo.savePersistent("progress", value, 2) == PANGEA_SCRIPT_OK);
	assert(gameInfo.loadPersistent("progress", output, sizeof output, &size) == PANGEA_SCRIPT_OK);
	assert(size == 2 && memcmp(output, value, 2) == 0);
	assert(gameInfo.savePersistent("../escape", value, sizeof value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(gameInfo.savePersistent("progress", NULL, 0) == PANGEA_SCRIPT_OK);
	assert(gameInfo.loadPersistent("progress", output, sizeof output, &size) == PANGEA_SCRIPT_FILE_NOT_FOUND);
	assert(gameInfo.savePersistent("progress", NULL, 0) == PANGEA_SCRIPT_OK);
	puts("Packaged Lua assets, executable-relative loading, and app persistence passed");
	return 0;
}

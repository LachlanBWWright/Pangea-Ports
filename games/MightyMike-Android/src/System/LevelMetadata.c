#include "myglobals.h"
#include "externs.h"

#include <stdlib.h>
#include <string.h>

static char* gLevelMetadataJSON = nil;

static void ClearLevelMetadata(void)
{
	if (gLevelMetadataJSON)
	{
		DisposePtr(gLevelMetadataJSON);
		gLevelMetadataJSON = nil;
	}
}

static Boolean ReadMetadataString(const char* json, const char* key, char* value, size_t valueSize)
{
	char needle[96];
	const char* valueStart;
	const char* valueEnd;
	size_t length;

	if (!json || !key || !value || valueSize == 0)
		return false;
	SDL_snprintf(needle, sizeof(needle), "\"%s\":\"", key);
	valueStart = strstr(json, needle);
	if (!valueStart)
		return false;
	valueStart += strlen(needle);
	valueEnd = strchr(valueStart, '\"');
	if (!valueEnd)
		return false;
	length = (size_t)(valueEnd - valueStart);
	if (length >= valueSize)
		return false;
	SDL_memcpy(value, valueStart, length);
	value[length] = '\0';
	return true;
}

void LoadLevelMetadata(const char* mapPath)
{
	FSSpec metadataSpec;
	short metadataFile;
	short previousFile;
	Handle resource;
	Size resourceSize;
	char metadataPath[512];
	char* json;

	ClearLevelMetadata();
	if (!mapPath)
		return;

	SDL_snprintf(metadataPath, sizeof(metadataPath), "%s.Meta.rsrc", mapPath);
	if (FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, metadataPath, &metadataSpec) != noErr)
		return;
	metadataFile = FSpOpenResFile(&metadataSpec, fsRdPerm);
	if (metadataFile == -1)
		return;

	previousFile = CurResFile();
	UseResFile(metadataFile);
	resource = GetResource('Meta', 1000);
	if (!resource)
	{
		CloseResFile(metadataFile);
		UseResFile(previousFile);
		return;
	}

	resourceSize = GetHandleSize(resource);
	if (resourceSize < 1)
	{
		ReleaseResource(resource);
		CloseResFile(metadataFile);
		UseResFile(previousFile);
		return;
	}
	json = (char*)NewPtrClear((size_t)resourceSize + 1);
	if (!json)
	{
		ReleaseResource(resource);
		CloseResFile(metadataFile);
		UseResFile(previousFile);
		return;
	}
	SDL_memcpy(json, *resource, (size_t)resourceSize);
	ReleaseResource(resource);
	CloseResFile(metadataFile);
	UseResFile(previousFile);

	if (!strstr(json, "\"schemaVersion\":1") || !strstr(json, "\"properties\":{") ||
		(!strstr(json, "\"game\":\"mightymike\"") && !strstr(json, "\"game\":\"Mighty Mike\"")))
	{
		DisposePtr(json);
		return;
	}
	gLevelMetadataJSON = json;
}

int LevelMetadataScene(const char* key, int fallback)
{
	char value[32];
	if (!ReadMetadataString(gLevelMetadataJSON, key, value, sizeof(value)) || !SDL_strcasecmp(value, "source-default"))
		return fallback;
	if (!SDL_strcasecmp(value, "jurassic")) return SCENE_JURASSIC;
	if (!SDL_strcasecmp(value, "candy")) return SCENE_CANDY;
	if (!SDL_strcasecmp(value, "fairy")) return SCENE_FAIRY;
	if (!SDL_strcasecmp(value, "clown")) return SCENE_CLOWN;
	if (!SDL_strcasecmp(value, "bargain")) return SCENE_BARGAIN;
	return fallback;
}

Boolean LevelMetadataProfileIs(const char* key, const char* profile, Boolean fallback)
{
	char value[32];
	if (!ReadMetadataString(gLevelMetadataJSON, key, value, sizeof(value)) || !SDL_strcasecmp(value, "source-default"))
		return fallback;
	return !SDL_strcasecmp(value, profile);
}

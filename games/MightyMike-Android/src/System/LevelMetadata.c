#include "myglobals.h"
#include "externs.h"
#include "LevelMetadataJSON.h"

#include <stdlib.h>
#include <string.h>

#if defined(PANGEA_ENABLE_LEVEL_METADATA) && PANGEA_ENABLE_LEVEL_METADATA

static char* gLevelMetadataJSON = nil;

static Boolean IsBalancedJSON(const char* json)
{
	int braceDepth = 0;
	int bracketDepth = 0;
	Boolean inString = false;
	Boolean escaped = false;
	Boolean rootClosed = false;
	const unsigned char* cursor = (const unsigned char*)json;

	if (!cursor || *cursor != '{')
		return false;

	for (; *cursor; cursor++)
	{
		if (rootClosed)
		{
			if (*cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n')
				return false;
			continue;
		}
		if (inString)
		{
			if (escaped)
			{
				escaped = false;
				continue;
			}
			if (*cursor == '\\')
			{
				escaped = true;
				continue;
			}
			if (*cursor == '"')
				inString = false;
			continue;
		}

		if (*cursor == '"')
		{
			inString = true;
			continue;
		}
		if (*cursor == '{') braceDepth++;
		else if (*cursor == '}' && --braceDepth < 0) return false;
		else if (*cursor == '[') bracketDepth++;
		else if (*cursor == ']' && --bracketDepth < 0) return false;
		if (braceDepth == 0)
			rootClosed = true;
	}

	return rootClosed && !inString && !escaped && braceDepth == 0 && bracketDepth == 0;
}

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
	if (!json || !key || !value || valueSize == 0)
		return false;
	return PangeaLevelMetadataJSONGetString(json, strlen(json), key, value, valueSize) != 0;
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

	if (!IsBalancedJSON(json) ||
		(!PangeaLevelMetadataJSONIsValid(json, (size_t)resourceSize, "mightymike") &&
		 !PangeaLevelMetadataJSONIsValid(json, (size_t)resourceSize, "Mighty Mike")))
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

#else

void LoadLevelMetadata(const char* mapPath)
{
	(void)mapPath;
}

int LevelMetadataScene(const char* key, int fallback)
{
	(void)key;
	return fallback;
}

Boolean LevelMetadataProfileIs(const char* key, const char* profile, Boolean fallback)
{
	(void)key;
	(void)profile;
	return fallback;
}

#endif

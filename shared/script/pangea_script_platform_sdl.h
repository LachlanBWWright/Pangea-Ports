#pragma once

#include "pangea_script.h"
#include <SDL3/SDL.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SDL can read APK assets as well as files; returned text belongs to libc. */
static char* PangeaScriptPlatform_ReadTextFile(const char* path, long* outSize)
{
	if (!path || !outSize) return NULL;
	*outSize = 0;
	size_t size = 0;
	void* data = SDL_LoadFile(path, &size);
	const char* relative = strncmp(path, "Data/", 5) == 0 ? path + 5 : path;
	if (!data && relative != path) data = SDL_LoadFile(relative, &size);
	const char* base = SDL_GetBasePath();
	if (!data && base && path[0] != '/')
	{
		char candidate[2048];
		int length = snprintf(candidate, sizeof candidate, "%s%s", base, path);
		if (length >= 0 && (size_t)length < sizeof candidate) data = SDL_LoadFile(candidate, &size);
		if (!data && relative != path)
		{
			length = snprintf(candidate, sizeof candidate, "%s%s", base, relative);
			if (length >= 0 && (size_t)length < sizeof candidate) data = SDL_LoadFile(candidate, &size);
		}
	}
	if (!data) return NULL;
	if (size >= (size_t)LONG_MAX || size == SIZE_MAX) { SDL_free(data); return NULL; }
	char* text = (char*)malloc(size + 1);
	if (text)
	{
		memcpy(text, data, size);
		text[size] = '\0';
		*outSize = (long)size;
	}
	SDL_free(data);
	return text;
}

#ifdef __ANDROID__
static const char* pangeaScriptPlatformGameId;

static bool PangeaScriptPlatform_PersistencePath(const char* key, char* path, size_t capacity)
{
	if (!key || !*key) return false;
	for (const char* cursor = key; *cursor; cursor++)
	{
		unsigned char c = (unsigned char)*cursor;
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
	}
	char* directory = SDL_GetPrefPath("PangeaSoftware", pangeaScriptPlatformGameId);
	if (!directory) return false;
	int length = snprintf(path, capacity, "%s.pangea-persistence-%s.bin", directory, key);
	SDL_free(directory);
	return length > 0 && (size_t)length < capacity;
}

static PangeaScriptStatus PangeaScriptPlatform_LoadPersistent(const char* key,
	unsigned char* data, int capacity, int* outSize)
{
	char path[2048];
	if (!data || capacity <= 0 || !outSize
		|| !PangeaScriptPlatform_PersistencePath(key, path, sizeof path)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	*outSize = 0;
	FILE* file = fopen(path, "rb");
	if (!file) return errno == ENOENT ? PANGEA_SCRIPT_FILE_NOT_FOUND : PANGEA_SCRIPT_RUNTIME_ERROR;
	size_t size = fread(data, 1, (size_t)capacity, file);
	int extra = fgetc(file);
	bool failed = ferror(file) || extra != EOF;
	if (fclose(file) != 0) failed = true;
	if (failed) return PANGEA_SCRIPT_RUNTIME_ERROR;
	*outSize = (int)size;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus PangeaScriptPlatform_SavePersistent(const char* key,
	const unsigned char* data, int size)
{
	char path[2048], temporary[2052];
	if (size < 0 || (size > 0 && !data)
		|| !PangeaScriptPlatform_PersistencePath(key, path, sizeof path)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (size == 0) return remove(path) == 0 || errno == ENOENT ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_RUNTIME_ERROR;
	int length = snprintf(temporary, sizeof temporary, "%s.tmp", path);
	if (length <= 0 || (size_t)length >= sizeof temporary) return PANGEA_SCRIPT_BAD_ARGUMENT;
	FILE* file = fopen(temporary, "wb");
	if (!file) return PANGEA_SCRIPT_RUNTIME_ERROR;
	bool failed = fwrite(data, 1, (size_t)size, file) != (size_t)size;
	if (fclose(file) != 0) failed = true;
	if (failed || rename(temporary, path) != 0)
	{
		remove(temporary);
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	return PANGEA_SCRIPT_OK;
}
#endif

static void PangeaScriptPlatform_Configure(PangeaScriptGameInfo* gameInfo)
{
	if (!gameInfo->readTextFile) gameInfo->readTextFile = PangeaScriptPlatform_ReadTextFile;
#ifdef __ANDROID__
	pangeaScriptPlatformGameId = gameInfo->gameId;
	if (!gameInfo->loadPersistent) gameInfo->loadPersistent = PangeaScriptPlatform_LoadPersistent;
	if (!gameInfo->savePersistent) gameInfo->savePersistent = PangeaScriptPlatform_SavePersistent;
#endif
}

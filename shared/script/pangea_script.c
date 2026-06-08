#include "pangea_script.h"
#include "pangea_script_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PANGEA_SCRIPT_ERROR_CAPACITY 512
#define PANGEA_SCRIPT_PATH_CAPACITY 260
#define PANGEA_SCRIPT_CONFIG_CAPACITY 65536
#define PANGEA_SCRIPT_MAX_REMAPS 64
#define PANGEA_SCRIPT_MAX_NATIVE_ITEMS 128
#define PANGEA_SCRIPT_MAX_OBJECTS 2048
#define PANGEA_SCRIPT_MAX_OBJECT_TAGS 8

typedef struct ItemRemap
{
	int levelNum;
	int fromType;
	int toType;
} ItemRemap;

typedef struct RegisteredObject
{
	bool active;
	uint32_t generation;
	void* nativeObject;
	const PangeaScriptObjectOps* ops;
	const char* tags[PANGEA_SCRIPT_MAX_OBJECT_TAGS];
	int tagCount;
	PangeaScriptCapabilityLevel capabilityLevel;
} RegisteredObject;

static PangeaScriptGameInfo gGameInfo;
static char gStartupScriptPath[PANGEA_SCRIPT_PATH_CAPACITY];
static char gConfigPath[PANGEA_SCRIPT_PATH_CAPACITY] = "Data/Scripts/config/levels.json";
static char gLastError[PANGEA_SCRIPT_ERROR_CAPACITY];
static int gErrorCount;
static PangeaScriptStatus gLastStatus = PANGEA_SCRIPT_NOT_ENABLED;
static bool gInitialized;
static bool gScriptLoaded;
static PangeaScriptBackend* gBackend;
static ItemRemap gItemRemaps[PANGEA_SCRIPT_MAX_REMAPS];
static int gItemRemapCount;
static PangeaScriptNativeItem gNativeItems[PANGEA_SCRIPT_MAX_NATIVE_ITEMS];
static int gNativeItemCount;
static RegisteredObject gRegisteredObjects[PANGEA_SCRIPT_MAX_OBJECTS];
static int gBudgetExceededCount;
static int gHooksCalledCount;
static int gConsecutiveHookFailures;
static bool gScriptsDisabled;

static void reset_objects(void)
{
	memset(gRegisteredObjects, 0, sizeof(gRegisteredObjects));
}

static void clear_registered_object(RegisteredObject* object)
{
	if (!object)
		return;

	object->active = false;
	object->nativeObject = NULL;
	object->ops = NULL;
	object->tagCount = 0;
	memset(object->tags, 0, sizeof(object->tags));
	object->generation++;
	if (object->generation == 0)
		object->generation = 1;
}

static RegisteredObject* resolve_object(PangeaScriptObjectHandle handle)
{
	if (handle.id <= 0 || handle.id > PANGEA_SCRIPT_MAX_OBJECTS)
		return NULL;

	RegisteredObject* object = &gRegisteredObjects[handle.id - 1];
	if (!object->active)
		return NULL;

	if (object->generation != handle.generation)
		return NULL;

	return object;
}

static void configure_registered_object(RegisteredObject* object, const PangeaScriptObjectRegistration* registration)
{
	object->active = true;
	object->nativeObject = registration->nativeObject;
	object->ops = registration->ops;
	object->tagCount = registration->tagCount;
	if (registration->capabilityLevel == PANGEA_SCRIPT_CAPABILITY_DEFAULT)
	{
		object->capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL;
	}
	else
	{
		object->capabilityLevel = registration->capabilityLevel;
	}
	memset(object->tags, 0, sizeof(object->tags));
	for (int i = 0; i < registration->tagCount; i++)
		object->tags[i] = registration->tags[i];
	if (object->generation == 0)
		object->generation = 1;
}

static void set_error(PangeaScriptStatus status, const char* message)
{
	gLastStatus = status;
	if (status != PANGEA_SCRIPT_OK && status != PANGEA_SCRIPT_FILE_NOT_FOUND)
	{
		gErrorCount++;
		gConsecutiveHookFailures++;
		if (status == PANGEA_SCRIPT_BUDGET_EXCEEDED)
		{
			gBudgetExceededCount++;
		}
		if (gConsecutiveHookFailures >= 5)
		{
			gScriptsDisabled = true;
			PangeaScript_Log(PANGEA_LOG_ERROR, "Native", "Disabling scripting host: exceeded maximum consecutive hook failures (5)");
		}
	}
	else
	{
		gConsecutiveHookFailures = 0;
	}

	if (!message)
	{
		gLastError[0] = '\0';
		return;
	}

	snprintf(gLastError, sizeof(gLastError), "%s", message);
}

static void set_backend_error(PangeaScriptStatus status, const char* message)
{
	if (message && message[0])
		set_error(status, message);
	else if (status == PANGEA_SCRIPT_RUNTIME_ERROR)
		set_error(status, "JavaScript engine is not linked; script execution is unavailable");
	else
		set_error(status, "Script execution failed");
}

static bool copy_string(char* dest, size_t destSize, const char* source)
{
	if (!dest || destSize == 0 || !source || !source[0])
		return false;

	snprintf(dest, destSize, "%s", source);
	return true;
}

static char* read_text_file(const char* path, long* outSize)
{
	FILE* file = fopen(path, "rb");
	if (!file)
		return NULL;

	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return NULL;
	}

	long size = ftell(file);
	if (size < 0)
	{
		fclose(file);
		return NULL;
	}

	if (fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return NULL;
	}

	char* bytes = (char*) malloc((size_t)size + 1);
	if (!bytes)
	{
		fclose(file);
		return NULL;
	}

	size_t readCount = fread(bytes, 1, (size_t)size, file);
	fclose(file);
	if (readCount != (size_t)size)
	{
		free(bytes);
		return NULL;
	}

	bytes[size] = '\0';
	if (outSize)
		*outSize = size;
	return bytes;
}

typedef enum {
	JSON_TOKEN_ERROR,
	JSON_TOKEN_EOF,
	JSON_TOKEN_LBRACE,
	JSON_TOKEN_RBRACE,
	JSON_TOKEN_LBRACKET,
	JSON_TOKEN_RBRACKET,
	JSON_TOKEN_COLON,
	JSON_TOKEN_COMMA,
	JSON_TOKEN_STRING,
	JSON_TOKEN_NUMBER,
	JSON_TOKEN_TRUE,
	JSON_TOKEN_FALSE,
	JSON_TOKEN_NULL
} JsonTokenType;

typedef struct {
	const char* start;
	const char* end;
	JsonTokenType type;
	union {
		double number_value;
		char string_value[512];
	};
} JsonToken;

static const char* skip_whitespace(const char* cursor) {
	while (*cursor && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) {
		cursor++;
	}
	return cursor;
}

static const char* next_token(const char* cursor, JsonToken* token) {
	cursor = skip_whitespace(cursor);
	if (!*cursor) {
		token->type = JSON_TOKEN_EOF;
		return cursor;
	}

	char c = *cursor;
	token->start = cursor;
	if (c == '{') { token->type = JSON_TOKEN_LBRACE; return cursor + 1; }
	if (c == '}') { token->type = JSON_TOKEN_RBRACE; return cursor + 1; }
	if (c == '[') { token->type = JSON_TOKEN_LBRACKET; return cursor + 1; }
	if (c == ']') { token->type = JSON_TOKEN_RBRACKET; return cursor + 1; }
	if (c == ':') { token->type = JSON_TOKEN_COLON; return cursor + 1; }
	if (c == ',') { token->type = JSON_TOKEN_COMMA; return cursor + 1; }

	if (c == '"') {
		const char* end = strchr(cursor + 1, '"');
		if (!end) {
			token->type = JSON_TOKEN_ERROR;
			return cursor;
		}
		size_t len = (size_t)(end - (cursor + 1));
		if (len >= sizeof(token->string_value)) {
			token->type = JSON_TOKEN_ERROR;
			return cursor;
		}
		memcpy(token->string_value, cursor + 1, len);
		token->string_value[len] = '\0';
		token->type = JSON_TOKEN_STRING;
		return end + 1;
	}

	if ((c >= '0' && c <= '9') || c == '-') {
		char* end = NULL;
		token->number_value = strtod(cursor, &end);
		if (end == cursor) {
			token->type = JSON_TOKEN_ERROR;
			return cursor;
		}
		token->type = JSON_TOKEN_NUMBER;
		return end;
	}

	if (strncmp(cursor, "true", 4) == 0) { token->type = JSON_TOKEN_TRUE; return cursor + 4; }
	if (strncmp(cursor, "false", 5) == 0) { token->type = JSON_TOKEN_FALSE; return cursor + 5; }
	if (strncmp(cursor, "null", 4) == 0) { token->type = JSON_TOKEN_NULL; return cursor + 4; }

	token->type = JSON_TOKEN_ERROR;
	return cursor;
}

static bool parse_levels_json(const char* json, int targetLevelNum, char* outScriptPath, size_t outScriptPathSize, PangeaScriptStatus* outStatus, char* outErrorMsg, size_t outErrorSize) {
	const char* cursor = json;
	JsonToken token;
	cursor = next_token(cursor, &token);
	if (token.type != JSON_TOKEN_LBRACE) {
		*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
		snprintf(outErrorMsg, outErrorSize, "Config is not a JSON object");
		return false;
	}

	int version = -1;
	bool foundLevel = false;

	while (true) {
		cursor = next_token(cursor, &token);
		if (token.type == JSON_TOKEN_RBRACE) {
			break;
		}
		if (token.type != JSON_TOKEN_STRING) {
			*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
			snprintf(outErrorMsg, outErrorSize, "Expected string key in object");
			return false;
		}
		char key[256];
		snprintf(key, sizeof(key), "%s", token.string_value);

		cursor = next_token(cursor, &token);
		if (token.type != JSON_TOKEN_COLON) {
			*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
			snprintf(outErrorMsg, outErrorSize, "Expected colon after key");
			return false;
		}

		if (strcmp(key, "version") == 0) {
			cursor = next_token(cursor, &token);
			if (token.type != JSON_TOKEN_NUMBER) {
				*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
				snprintf(outErrorMsg, outErrorSize, "version must be a number");
				return false;
			}
			version = (int)token.number_value;
			if (version != 1) {
				*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
				snprintf(outErrorMsg, outErrorSize, "Unknown schema version: %d", version);
				return false;
			}
		} else if (strcmp(key, "levels") == 0) {
			cursor = next_token(cursor, &token);
			if (token.type != JSON_TOKEN_LBRACE) {
				*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
				snprintf(outErrorMsg, outErrorSize, "levels must be an object");
				return false;
			}
			// Parse levels object
			while (true) {
				cursor = next_token(cursor, &token);
				if (token.type == JSON_TOKEN_RBRACE) {
					break;
				}
				if (token.type != JSON_TOKEN_STRING) {
					*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
					snprintf(outErrorMsg, outErrorSize, "Expected string level key");
					return false;
				}
				char levelKey[64];
				snprintf(levelKey, sizeof(levelKey), "%s", token.string_value);
				int levelKeyNum = atoi(levelKey);

				cursor = next_token(cursor, &token);
				if (token.type != JSON_TOKEN_COLON) {
					*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
					snprintf(outErrorMsg, outErrorSize, "Expected colon after level key");
					return false;
				}

				cursor = next_token(cursor, &token);
				if (token.type != JSON_TOKEN_LBRACE) {
					*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
					snprintf(outErrorMsg, outErrorSize, "level config must be an object");
					return false;
				}

				bool isTargetLevel = (levelKeyNum == targetLevelNum || strcmp(levelKey, "current") == 0);
				if (isTargetLevel) {
					foundLevel = true;
				}

				// Parse individual level config
				while (true) {
					cursor = next_token(cursor, &token);
					if (token.type == JSON_TOKEN_RBRACE) {
						break;
					}
					if (token.type != JSON_TOKEN_STRING) {
						*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
						snprintf(outErrorMsg, outErrorSize, "Expected string key in level config");
						return false;
					}
					char subKey[256];
					snprintf(subKey, sizeof(subKey), "%s", token.string_value);

					cursor = next_token(cursor, &token);
					if (token.type != JSON_TOKEN_COLON) {
						*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
						snprintf(outErrorMsg, outErrorSize, "Expected colon in level config");
						return false;
					}

					if (strcmp(subKey, "script") == 0) {
						cursor = next_token(cursor, &token);
						if (token.type != JSON_TOKEN_STRING) {
							*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
							snprintf(outErrorMsg, outErrorSize, "script path must be a string");
							return false;
						}
						if (isTargetLevel) {
							snprintf(outScriptPath, outScriptPathSize, "%s", token.string_value);
						}
					} else if (strcmp(subKey, "itemOverrides") == 0) {
						cursor = next_token(cursor, &token);
						if (token.type != JSON_TOKEN_LBRACKET) {
							*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
							snprintf(outErrorMsg, outErrorSize, "itemOverrides must be an array");
							return false;
						}
						while (true) {
							cursor = next_token(cursor, &token);
							if (token.type == JSON_TOKEN_RBRACKET) {
								break;
							}
							if (token.type != JSON_TOKEN_LBRACE) {
								*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
								snprintf(outErrorMsg, outErrorSize, "override item must be an object");
								return false;
							}
							int fromType = -1;
							int toType = -1;
							while (true) {
								cursor = next_token(cursor, &token);
								if (token.type == JSON_TOKEN_RBRACE) {
									break;
								}
								if (token.type != JSON_TOKEN_STRING) {
									*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
									snprintf(outErrorMsg, outErrorSize, "Expected string key in override item");
									return false;
								}
								char overrideKey[64];
								snprintf(overrideKey, sizeof(overrideKey), "%s", token.string_value);

								cursor = next_token(cursor, &token);
								if (token.type != JSON_TOKEN_COLON) {
									*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
									snprintf(outErrorMsg, outErrorSize, "Expected colon in override item");
									return false;
								}

								cursor = next_token(cursor, &token);
								if (token.type != JSON_TOKEN_NUMBER) {
									*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
									snprintf(outErrorMsg, outErrorSize, "override values must be numbers");
									return false;
								}
								if (strcmp(overrideKey, "from") == 0) {
									fromType = (int)token.number_value;
								} else if (strcmp(overrideKey, "to") == 0) {
									toType = (int)token.number_value;
								}

								cursor = next_token(cursor, &token);
								if (token.type == JSON_TOKEN_COMMA) {
									// continue parsing keys
								} else if (token.type == JSON_TOKEN_RBRACE) {
									break;
								} else {
									*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
									snprintf(outErrorMsg, outErrorSize, "Expected comma or closing brace in override item");
									return false;
								}
							}
							if (isTargetLevel && fromType != -1 && toType != -1 && gItemRemapCount < PANGEA_SCRIPT_MAX_REMAPS) {
								gItemRemaps[gItemRemapCount].levelNum = targetLevelNum;
								gItemRemaps[gItemRemapCount].fromType = fromType;
								gItemRemaps[gItemRemapCount].toType = toType;
								gItemRemapCount++;
							}
							cursor = next_token(cursor, &token);
							if (token.type == JSON_TOKEN_COMMA) {
								// continue array
							} else if (token.type == JSON_TOKEN_RBRACKET) {
								break;
							} else {
								*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
								snprintf(outErrorMsg, outErrorSize, "Expected comma or closing bracket in itemOverrides array");
								return false;
							}
						}
					} else {
						// Skip unknown field value
						int braceCount = 0;
						int bracketCount = 0;
						while (true) {
							cursor = next_token(cursor, &token);
							if (token.type == JSON_TOKEN_LBRACE) braceCount++;
							else if (token.type == JSON_TOKEN_RBRACE) {
								if (braceCount == 0) {
									break;
								}
								braceCount--;
							}
							else if (token.type == JSON_TOKEN_LBRACKET) bracketCount++;
							else if (token.type == JSON_TOKEN_RBRACKET) {
								bracketCount--;
							}
							if (braceCount == 0 && bracketCount == 0 && (token.type == JSON_TOKEN_COMMA || token.type == JSON_TOKEN_RBRACE)) {
								break;
							}
						}
						if (token.type == JSON_TOKEN_RBRACE) {
							break;
						}
						continue;
					}

					cursor = next_token(cursor, &token);
					if (token.type == JSON_TOKEN_COMMA) {
						// continue level config fields
					} else if (token.type == JSON_TOKEN_RBRACE) {
						break;
					} else {
						*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
						snprintf(outErrorMsg, outErrorSize, "Expected comma or closing brace in level config");
						return false;
					}
				}

				cursor = next_token(cursor, &token);
				if (token.type == JSON_TOKEN_COMMA) {
					// continue parsing levels
				} else if (token.type == JSON_TOKEN_RBRACE) {
					break;
				} else {
					*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
					snprintf(outErrorMsg, outErrorSize, "Expected comma or closing brace in levels object");
					return false;
				}
			}
		} else {
			// Skip unknown property value
			int braceCount = 0;
			int bracketCount = 0;
			while (true) {
				cursor = next_token(cursor, &token);
				if (token.type == JSON_TOKEN_LBRACE) braceCount++;
				else if (token.type == JSON_TOKEN_RBRACE) braceCount--;
				else if (token.type == JSON_TOKEN_LBRACKET) bracketCount++;
				else if (token.type == JSON_TOKEN_RBRACKET) bracketCount--;
				if (braceCount <= 0 && bracketCount <= 0 && (token.type == JSON_TOKEN_COMMA || token.type == JSON_TOKEN_RBRACE)) {
					break;
				}
			}
			if (token.type == JSON_TOKEN_RBRACE) {
				break;
			}
			continue;
		}

		cursor = next_token(cursor, &token);
		if (token.type == JSON_TOKEN_COMMA) {
			// continue object properties
		} else if (token.type == JSON_TOKEN_RBRACE) {
			break;
		} else {
			*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
			snprintf(outErrorMsg, outErrorSize, "Expected comma or closing brace in config object");
			return false;
		}
	}

	if (version == -1) {
		*outStatus = PANGEA_SCRIPT_CONFIG_ERROR;
		snprintf(outErrorMsg, outErrorSize, "Missing required field: version");
		return false;
	}

	return foundLevel;
}

PangeaScriptStatus PangeaScript_Init(const PangeaScriptGameInfo* gameInfo)
{
	if (!gameInfo || !gameInfo->gameId || !gameInfo->gameName)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_Init received incomplete game info");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	gGameInfo = *gameInfo;
	if (gBackend)
	{
		PangeaScriptBackend_Destroy(gBackend);
		gBackend = NULL;
	}
	gBackend = PangeaScriptBackend_Create(gameInfo);
	gInitialized = true;
	gScriptLoaded = false;
	gErrorCount = 0;
	gItemRemapCount = 0;
	gBudgetExceededCount = 0;
	gHooksCalledCount = 0;
	gConsecutiveHookFailures = 0;
	gScriptsDisabled = false;
	reset_objects();
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

void PangeaScript_Shutdown(void)
{
	gInitialized = false;
	gScriptLoaded = false;
	gItemRemapCount = 0;
	gBudgetExceededCount = 0;
	gHooksCalledCount = 0;
	gConsecutiveHookFailures = 0;
	gScriptsDisabled = false;
	reset_objects();
	if (gBackend)
	{
		PangeaScriptBackend_Destroy(gBackend);
		gBackend = NULL;
	}
}

bool PangeaScript_IsEnabled(void)
{
	return gInitialized;
}

bool PangeaScript_HasRunnableModule(void)
{
	return gScriptLoaded && gBackend != NULL;
}

PangeaScriptStatus PangeaScript_SetStartupScript(const char* path)
{
	if (!copy_string(gStartupScriptPath, sizeof(gStartupScriptPath), path))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Startup script path is empty");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	return PangeaScript_Reload();
}

PangeaScriptStatus PangeaScript_SetConfigPath(const char* path)
{
	if (!copy_string(gConfigPath, sizeof(gConfigPath), path))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "Config path is empty");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScript_Reload(void)
{
	if (!gInitialized)
	{
		set_error(PANGEA_SCRIPT_NOT_ENABLED, "Scripting host is not initialized");
		return PANGEA_SCRIPT_NOT_ENABLED;
	}

	const char* scriptPath = gStartupScriptPath[0] ? gStartupScriptPath : "Data/Scripts/dist/main.js";
	long scriptSize = 0;
	char* script = read_text_file(scriptPath, &scriptSize);
	if (!script)
	{
		gScriptLoaded = false;
		set_error(PANGEA_SCRIPT_FILE_NOT_FOUND, "Script file not found");
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	}

	gScriptLoaded = scriptSize > 0;
	if (!gScriptLoaded)
	{
		free(script);
		set_error(PANGEA_SCRIPT_PARSE_ERROR, "Script file is empty");
		return PANGEA_SCRIPT_PARSE_ERROR;
	}

	if (gBackend)
	{
		char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
		backendError[0] = '\0';
		PangeaScriptStatus status = PangeaScriptBackend_Load(gBackend, script, backendError, (int)sizeof(backendError));
		free(script);
		if (status != PANGEA_SCRIPT_OK)
		{
			gScriptLoaded = false;
			set_backend_error(status, backendError);
			return status;
		}

		set_error(PANGEA_SCRIPT_OK, "");
		return PANGEA_SCRIPT_OK;
	}

	free(script);
	set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "JavaScript engine is not linked; script execution is unavailable");
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

const char* PangeaScript_GetLastError(void)
{
	return gLastError;
}

int PangeaScript_GetErrorCount(void)
{
	return gErrorCount;
}

PangeaScriptStatus PangeaScript_GetLastStatus(void)
{
	return gLastStatus;
}

PangeaScriptStatus PangeaScript_LoadLevelConfig(int levelNum)
{
	gItemRemapCount = 0;

	long configSize = 0;
	char* config = read_text_file(gConfigPath, &configSize);
	if (!config)
	{
		set_error(PANGEA_SCRIPT_FILE_NOT_FOUND, "Script config file not found");
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	}

	if (configSize > PANGEA_SCRIPT_CONFIG_CAPACITY)
	{
		free(config);
		set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Script config file is too large");
		return PANGEA_SCRIPT_CONFIG_ERROR;
	}

	char scriptPath[PANGEA_SCRIPT_PATH_CAPACITY];
	scriptPath[0] = '\0';
	PangeaScriptStatus parseStatus = PANGEA_SCRIPT_OK;
	char errorMsg[256];
	errorMsg[0] = '\0';

	bool found = parse_levels_json(config, levelNum, scriptPath, sizeof(scriptPath), &parseStatus, errorMsg, sizeof(errorMsg));
	if (parseStatus != PANGEA_SCRIPT_OK)
	{
		free(config);
		set_error(parseStatus, errorMsg);
		return parseStatus;
	}

	if (found && scriptPath[0])
	{
		if (strncmp(scriptPath, "Data/Scripts/", 13) != 0)
		{
			free(config);
			set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Invalid script path: must be within Data/Scripts/");
			return PANGEA_SCRIPT_CONFIG_ERROR;
		}
		if (strstr(scriptPath, "..") != NULL || strchr(scriptPath, '\\') != NULL)
		{
			free(config);
			set_error(PANGEA_SCRIPT_CONFIG_ERROR, "Invalid script path: path traversal detected");
			return PANGEA_SCRIPT_CONFIG_ERROR;
		}

		copy_string(gStartupScriptPath, sizeof(gStartupScriptPath), scriptPath);
		PangeaScriptStatus reloadStatus = PangeaScript_Reload();
		if (reloadStatus != PANGEA_SCRIPT_OK && reloadStatus != PANGEA_SCRIPT_FILE_NOT_FOUND)
		{
			free(config);
			return reloadStatus;
		}
	}

	free(config);
	set_error(PANGEA_SCRIPT_OK, "");
	return PANGEA_SCRIPT_OK;
}

int PangeaScript_RemapTerrainItemType(int levelNum, int itemType)
{
	for (int i = 0; i < gItemRemapCount; i++)
	{
		if (gItemRemaps[i].levelNum == levelNum && gItemRemaps[i].fromType == itemType)
			return gItemRemaps[i].toType;
	}
	return itemType;
}

PangeaScriptStatus PangeaScript_CallLevelHook(PangeaScriptHook hook, const PangeaScriptLevelContext* context)
{
	(void) hook;
	(void) context;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallLevelHook(gBackend, hook, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* context)
{
	(void) context;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallFrameHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallTerrainItemHook(PangeaScriptTerrainItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallTerrainItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallSplineItemHook(PangeaScriptSplineItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallSplineItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallMapItemHook(PangeaScriptMapItemContext* context)
{
	if (!context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallMapItemHook(gBackend, context, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

PangeaScriptStatus PangeaScript_CallObjectFrame(PangeaScriptObjectHandle handle, const PangeaScriptFrameContext* frameContext, PangeaScriptObjectFrameResult* outResult)
{
	if (!frameContext || !outResult)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame received an incomplete frame context or result pointer");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	outResult->hasPositionOffset = false;
	outResult->positionOffset.x = 0.0f;
	outResult->positionOffset.y = 0.0f;
	outResult->positionOffset.z = 0.0f;

	if (!gInitialized)
		return PANGEA_SCRIPT_NOT_ENABLED;
	if (gScriptsDisabled)
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	gHooksCalledCount++;
	if (!gScriptLoaded)
		return PANGEA_SCRIPT_FILE_NOT_FOUND;
	if (!gBackend)
		return PANGEA_SCRIPT_RUNTIME_ERROR;

	RegisteredObject* object = resolve_object(handle);
	if (!object)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame received an unknown or stale object handle");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	if (!object->ops || !object->ops->getPosition)
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame received an object without position access");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	PangeaScriptVector3 position;
	if (!object->ops->getPosition(object->nativeObject, &position))
	{
		set_error(PANGEA_SCRIPT_BAD_ARGUMENT, "PangeaScript_CallObjectFrame could not read the object's current position");
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	}

	const PangeaScriptObjectFrameContext context =
	{
		.levelNum = frameContext->levelNum,
		.frameNum = frameContext->frameNum,
		.deltaSeconds = frameContext->deltaSeconds,
		.levelTimeSeconds = frameContext->levelTimeSeconds,
		.object = handle,
		.position = position,
		.tags = object->tags,
		.tagCount = object->tagCount,
	};

	char backendError[PANGEA_SCRIPT_ERROR_CAPACITY];
	backendError[0] = '\0';
	PangeaScriptStatus status = PangeaScriptBackend_CallObjectFrameHook(gBackend, &context, outResult, backendError, (int)sizeof(backendError));
	if (status != PANGEA_SCRIPT_OK)
		set_backend_error(status, backendError);
	return status;
}

void PangeaScript_ResetObjects(void)
{
	reset_objects();
}

PangeaScriptStatus PangeaScript_RegisterObject(const PangeaScriptObjectRegistration* registration, PangeaScriptObjectHandle* outHandle)
{
	if (!registration || !registration->nativeObject || !registration->ops || !registration->ops->getPosition)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (registration->tagCount < 0 || registration->tagCount > PANGEA_SCRIPT_MAX_OBJECT_TAGS)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	int freeIndex = -1;
	for (int i = 0; i < PANGEA_SCRIPT_MAX_OBJECTS; i++)
	{
		RegisteredObject* object = &gRegisteredObjects[i];
		if (object->active)
		{
			if (object->nativeObject == registration->nativeObject)
			{
				configure_registered_object(object, registration);
				if (outHandle)
				{
					outHandle->id = i + 1;
					outHandle->generation = object->generation;
				}
				return PANGEA_SCRIPT_OK;
			}
			continue;
		}

		if (freeIndex < 0)
			freeIndex = i;
	}

	if (freeIndex < 0)
		return PANGEA_SCRIPT_BUDGET_EXCEEDED;

	RegisteredObject* object = &gRegisteredObjects[freeIndex];
	configure_registered_object(object, registration);
	if (outHandle)
	{
		outHandle->id = freeIndex + 1;
		outHandle->generation = object->generation;
	}
	return PANGEA_SCRIPT_OK;
}

bool PangeaScript_UnregisterObject(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object)
		return false;

	clear_registered_object(object);
	return true;
}

bool PangeaScript_GetObjectPosition(PangeaScriptObjectHandle handle, PangeaScriptVector3* outPosition)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !outPosition || !object->ops || !object->ops->getPosition)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_READ_ONLY)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks read capability level");
		return false;
	}

	return object->ops->getPosition(object->nativeObject, outPosition);
}

bool PangeaScript_SetObjectPosition(PangeaScriptObjectHandle handle, const PangeaScriptVector3* position)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !position || !object->ops || !object->ops->setPosition)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_BASE)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks transform/position capability level");
		return false;
	}

	return object->ops->setPosition(object->nativeObject, position);
}

bool PangeaScript_SetObjectVelocity(PangeaScriptObjectHandle handle, const PangeaScriptVector3* velocity)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !velocity || !object->ops || !object->ops->setVelocity)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks movable/velocity capability level");
		return false;
	}

	return object->ops->setVelocity(object->nativeObject, velocity);
}

bool PangeaScript_DeleteObject(PangeaScriptObjectHandle handle)
{
	RegisteredObject* object = resolve_object(handle);
	if (!object || !object->ops || !object->ops->deleteObject)
		return false;

	if (object->capabilityLevel < PANGEA_SCRIPT_CAPABILITY_FULL)
	{
		set_error(PANGEA_SCRIPT_RUNTIME_ERROR, "Permission denied: object lacks deletion/cleanup-safe capability level");
		return false;
	}

	return object->ops->deleteObject(object->nativeObject);
}

PangeaScriptStatus PangeaScript_RegisterNativeItems(const PangeaScriptNativeItem* items, int count)
{
	if (!items || count < 0 || count > PANGEA_SCRIPT_MAX_NATIVE_ITEMS)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	for (int i = 0; i < count; i++)
		gNativeItems[i] = items[i];

	gNativeItemCount = count;
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScript_SpawnNative(const char* id, float x, float y, float z)
{
	(void) x;
	(void) y;
	(void) z;
	if (!id || !id[0])
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	for (int i = 0; i < gNativeItemCount; i++)
	{
		if (gNativeItems[i].id && strcmp(gNativeItems[i].id, id) == 0)
			return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
}

void PangeaScript_Log(PangeaScriptLogLevel level, const char* source, const char* message)
{
	const char* levelStr = "INFO";
	FILE* out = stdout;
	if (level == PANGEA_LOG_WARN)
	{
		levelStr = "WARNING";
		out = stderr;
	}
	else if (level == PANGEA_LOG_ERROR)
	{
		levelStr = "ERROR";
		out = stderr;
	}

	fprintf(out, "[PangeaScript %s] [%s] %s\n", levelStr, source ? source : "Native", message ? message : "");
	fflush(out);
}

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
EMSCRIPTEN_KEEPALIVE
void PangeaScript_LogJS(int level, const char* message)
{
	PangeaScript_Log((PangeaScriptLogLevel)level, "JS", message);
}
#endif

void PangeaScript_GetStatusInfo(PangeaScriptStatusInfo* outInfo)
{
	if (!outInfo)
		return;

	outInfo->enabled = gInitialized;
	outInfo->configLoaded = gConfigPath[0] != '\0';
	outInfo->bundleLoaded = gScriptLoaded;
	snprintf(outInfo->activeScriptPath, sizeof(outInfo->activeScriptPath), "%s", gStartupScriptPath[0] ? gStartupScriptPath : "Data/Scripts/dist/main.js");
	snprintf(outInfo->lastError, sizeof(outInfo->lastError), "%s", gLastError);
	outInfo->errorCount = gErrorCount;
	outInfo->budgetExceededCount = gBudgetExceededCount;
	outInfo->hooksCalledCount = gHooksCalledCount;
	outInfo->scriptsDisabled = gScriptsDisabled;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_GetStatusEnabled(void) { return gInitialized; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_GetStatusConfigLoaded(void) { return gConfigPath[0] != '\0'; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_GetStatusBundleLoaded(void) { return gScriptLoaded; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* PangeaScript_GetStatusActiveScriptPath(void) { return gStartupScriptPath[0] ? gStartupScriptPath : "Data/Scripts/dist/main.js"; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* PangeaScript_GetStatusLastError(void) { return gLastError; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int PangeaScript_GetStatusErrorCount(void) { return gErrorCount; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int PangeaScript_GetStatusBudgetExceededCount(void) { return gBudgetExceededCount; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int PangeaScript_GetStatusHooksCalledCount(void) { return gHooksCalledCount; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool PangeaScript_GetStatusScriptsDisabled(void) { return gScriptsDisabled; }

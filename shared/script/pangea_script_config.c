#include "pangea_script_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	const char* cursor;
	char* errorMsg;
	int errorCapacity;
} Parser;

static void skip_whitespace(Parser* p)
{
	while (*p->cursor && (*p->cursor == ' ' || *p->cursor == '\t' || *p->cursor == '\r' || *p->cursor == '\n'))
	{
		p->cursor++;
	}
}

static bool match_char(Parser* p, char expected)
{
	skip_whitespace(p);
	if (*p->cursor == expected)
	{
		p->cursor++;
		return true;
	}
	return false;
}

static bool parse_string(Parser* p, char* outStr, int capacity)
{
	skip_whitespace(p);
	if (*p->cursor != '"')
	{
		snprintf(p->errorMsg, p->errorCapacity, "Expected '\"' at start of string");
		return false;
	}
	p->cursor++; // skip '"'
	int len = 0;
	while (*p->cursor && *p->cursor != '"')
	{
		char c = *p->cursor;
		if (c == '\\')
		{
			p->cursor++;
			if (!*p->cursor)
			{
				snprintf(p->errorMsg, p->errorCapacity, "Unterminated escape sequence in string");
				return false;
			}
			char esc = *p->cursor;
			switch (esc)
			{
				case '"':  c = '"'; break;
				case '\\': c = '\\'; break;
				case '/':  c = '/'; break;
				case 'b':  c = '\b'; break;
				case 'f':  c = '\f'; break;
				case 'n':  c = '\n'; break;
				case 'r':  c = '\r'; break;
				case 't':  c = '\t'; break;
				default:
					snprintf(p->errorMsg, p->errorCapacity, "Unsupported escape sequence '\\%c'", esc);
					return false;
			}
		}
		if (len < capacity - 1)
		{
			outStr[len++] = c;
		}
		p->cursor++;
	}
	if (*p->cursor != '"')
	{
		snprintf(p->errorMsg, p->errorCapacity, "Unterminated string");
		return false;
	}
	p->cursor++; // skip '"'
	outStr[len] = '\0';
	return true;
}

static bool parse_number(Parser* p, double* outNum)
{
	skip_whitespace(p);
	char* endptr = NULL;
	double val = strtod(p->cursor, &endptr);
	if (endptr == p->cursor)
	{
		snprintf(p->errorMsg, p->errorCapacity, "Expected number");
		return false;
	}
	p->cursor = endptr;
	*outNum = val;
	return true;
}

static bool parse_bool(Parser* p, bool* outBool)
{
	skip_whitespace(p);
	if (strncmp(p->cursor, "true", 4) == 0)
	{
		*outBool = true;
		p->cursor += 4;
		return true;
	}
	if (strncmp(p->cursor, "false", 5) == 0)
	{
		*outBool = false;
		p->cursor += 5;
		return true;
	}
	snprintf(p->errorMsg, p->errorCapacity, "Expected boolean value");
	return false;
}

static bool skip_value(Parser* p);

static bool skip_value(Parser* p)
{
	skip_whitespace(p);
	char c = *p->cursor;
	if (c == '"')
	{
		char dummy[512];
		return parse_string(p, dummy, sizeof(dummy));
	}
	else if (c == '{')
	{
		p->cursor++;
		while (true)
		{
			skip_whitespace(p);
			if (*p->cursor == '}')
			{
				p->cursor++;
				return true;
			}
			char key[256];
			if (!parse_string(p, key, sizeof(key)))
				return false;
			skip_whitespace(p);
			if (*p->cursor != ':')
			{
				snprintf(p->errorMsg, p->errorCapacity, "Expected ':' in object");
				return false;
			}
			p->cursor++; // skip ':'
			if (!skip_value(p))
				return false;
			skip_whitespace(p);
			if (*p->cursor == ',')
			{
				p->cursor++;
			}
			else if (*p->cursor != '}')
			{
				snprintf(p->errorMsg, p->errorCapacity, "Expected ',' or '}' in object");
				return false;
			}
		}
	}
	else if (c == '[')
	{
		p->cursor++;
		while (true)
		{
			skip_whitespace(p);
			if (*p->cursor == ']')
			{
				p->cursor++;
				return true;
			}
			if (!skip_value(p))
				return false;
			skip_whitespace(p);
			if (*p->cursor == ',')
			{
				p->cursor++;
			}
			else if (*p->cursor != ']')
			{
				snprintf(p->errorMsg, p->errorCapacity, "Expected ',' or ']' in array");
				return false;
			}
		}
	}
	else if (c == 't' || c == 'f')
	{
		bool dummy;
		return parse_bool(p, &dummy);
	}
	else if (c == 'n')
	{
		if (strncmp(p->cursor, "null", 4) == 0)
		{
			p->cursor += 4;
			return true;
		}
		snprintf(p->errorMsg, p->errorCapacity, "Expected null");
		return false;
	}
	else if ((c >= '0' && c <= '9') || c == '-' || c == '.')
	{
		double dummy;
		return parse_number(p, &dummy);
	}
	snprintf(p->errorMsg, p->errorCapacity, "Unexpected character '%c' in JSON", c);
	return false;
}

static bool is_integer_number(double value)
{
	int intValue = (int)value;
	return (double)intValue == value;
}

static bool parse_level_settings_object(Parser* p, PangeaConfigLevel* level)
{
	if (!match_char(p, '{'))
	{
		snprintf(p->errorMsg, p->errorCapacity, "levelSettings must be an object");
		return false;
	}

	while (true)
	{
		skip_whitespace(p);
		if (match_char(p, '}'))
		{
			break;
		}

		char key[64];
		if (!parse_string(p, key, sizeof(key)))
			return false;

		if (!match_char(p, ':'))
		{
			snprintf(p->errorMsg, p->errorCapacity, "Expected ':' after levelSettings key");
			return false;
		}

		skip_whitespace(p);
		if (strcmp(key, "assetDependencies") == 0)
		{
			if (!match_char(p, '['))
			{
				snprintf(p->errorMsg, p->errorCapacity, "assetDependencies must be an array");
				return false;
			}

			while (true)
			{
				skip_whitespace(p);
				if (match_char(p, ']'))
				{
					break;
				}

				if (!match_char(p, '{'))
				{
					snprintf(p->errorMsg, p->errorCapacity, "assetDependencies entries must be objects");
					return false;
				}

				char depKind[32] = "";
				char depId[96] = "";

				while (true)
				{
					skip_whitespace(p);
					if (match_char(p, '}'))
					{
						break;
					}

					char depKey[32];
					if (!parse_string(p, depKey, sizeof(depKey)))
						return false;

					if (!match_char(p, ':'))
					{
						snprintf(p->errorMsg, p->errorCapacity, "Expected ':' in asset dependency");
						return false;
					}

					if (strcmp(depKey, "kind") == 0)
					{
						if (!parse_string(p, depKind, sizeof(depKind)))
						{
							snprintf(p->errorMsg, p->errorCapacity, "asset dependency kind must be a string");
							return false;
						}
					}
					else if (strcmp(depKey, "id") == 0)
					{
						if (!parse_string(p, depId, sizeof(depId)))
						{
							snprintf(p->errorMsg, p->errorCapacity, "asset dependency id must be a string");
							return false;
						}
					}
					else
					{
						char dummy[128];
						if (!parse_string(p, dummy, sizeof(dummy)))
						{
							snprintf(p->errorMsg, p->errorCapacity, "asset dependency values must be strings");
							return false;
						}
					}

					skip_whitespace(p);
					if (match_char(p, ','))
					{
						// continue
					}
					else if (*p->cursor != '}')
					{
						snprintf(p->errorMsg, p->errorCapacity, "Expected comma or closing brace in asset dependency");
						return false;
					}
				}

				if (level->assetDependencyCount < PANGEA_CONFIG_MAX_ASSET_DEPENDENCIES)
				{
					PangeaScriptAssetDependency* dep = &level->assetDependencies[level->assetDependencyCount++];
					snprintf(dep->kind, sizeof(dep->kind), "%s", depKind);
					snprintf(dep->id, sizeof(dep->id), "%s", depId);
				}

				skip_whitespace(p);
				if (match_char(p, ','))
				{
					// continue
				}
				else if (*p->cursor != ']')
				{
					snprintf(p->errorMsg, p->errorCapacity, "Expected comma or closing bracket in assetDependencies");
					return false;
				}
			}
		}
		else
		{
			// Normal level setting
			char c = *p->cursor;
			if (c == '"')
			{
				char strVal[256];
				if (!parse_string(p, strVal, sizeof(strVal)))
					return false;

				if (level->levelSettingCount < PANGEA_CONFIG_MAX_LEVEL_SETTINGS)
				{
					LevelSetting* setting = &level->levelSettings[level->levelSettingCount++];
					snprintf(setting->key, sizeof(setting->key), "%s", key);
					setting->type = LEVEL_SETTING_STRING;
					snprintf(setting->stringValue, sizeof(setting->stringValue), "%s", strVal);
				}
			}
			else if (c == 't' || c == 'f')
			{
				bool boolVal;
				if (!parse_bool(p, &boolVal))
					return false;

				if (level->levelSettingCount < PANGEA_CONFIG_MAX_LEVEL_SETTINGS)
				{
					LevelSetting* setting = &level->levelSettings[level->levelSettingCount++];
					snprintf(setting->key, sizeof(setting->key), "%s", key);
					setting->type = LEVEL_SETTING_BOOL;
					setting->boolValue = boolVal;
				}
			}
			else if ((c >= '0' && c <= '9') || c == '-' || c == '.')
			{
				double numVal;
				if (!parse_number(p, &numVal))
					return false;

				if (level->levelSettingCount < PANGEA_CONFIG_MAX_LEVEL_SETTINGS)
				{
					LevelSetting* setting = &level->levelSettings[level->levelSettingCount++];
					snprintf(setting->key, sizeof(setting->key), "%s", key);
					if (is_integer_number(numVal))
					{
						setting->type = LEVEL_SETTING_INT;
						setting->intValue = (int) numVal;
					}
					else
					{
						setting->type = LEVEL_SETTING_FLOAT;
						setting->floatValue = (float) numVal;
					}
				}
			}
			else
			{
				// Nested objects or arrays are skipped
				if (!skip_value(p))
					return false;
			}
		}

		skip_whitespace(p);
		if (match_char(p, ','))
		{
			// continue
		}
		else if (*p->cursor != '}')
		{
			snprintf(p->errorMsg, p->errorCapacity, "Expected ',' or '}' in levelSettings");
			return false;
		}
	}

	return true;
}

static bool parse_item_overrides_array(Parser* p, PangeaConfigLevel* level, int targetLevelNum)
{
	if (!match_char(p, '['))
	{
		snprintf(p->errorMsg, p->errorCapacity, "itemOverrides must be an array");
		return false;
	}

	while (true)
	{
		skip_whitespace(p);
		if (match_char(p, ']'))
		{
			break;
		}

		if (!match_char(p, '{'))
		{
			snprintf(p->errorMsg, p->errorCapacity, "override item must be an object");
			return false;
		}

		int fromType = -1;
		int toType = -1;

		while (true)
		{
			skip_whitespace(p);
			if (match_char(p, '}'))
			{
				break;
			}

			char key[64];
			if (!parse_string(p, key, sizeof(key)))
				return false;

			if (!match_char(p, ':'))
			{
				snprintf(p->errorMsg, p->errorCapacity, "Expected ':' in override item");
				return false;
			}

			double val;
			if (!parse_number(p, &val))
			{
				snprintf(p->errorMsg, p->errorCapacity, "override values must be numbers");
				return false;
			}

			if (strcmp(key, "from") == 0)
			{
				fromType = (int) val;
			}
			else if (strcmp(key, "to") == 0)
			{
				toType = (int) val;
			}

			skip_whitespace(p);
			if (match_char(p, ','))
			{
				// continue
			}
			else if (*p->cursor != '}')
			{
				snprintf(p->errorMsg, p->errorCapacity, "Expected comma or closing brace in override item");
				return false;
			}
		}

		if (fromType != -1 && toType != -1)
		{
			if (level->itemRemapCount < PANGEA_CONFIG_MAX_REMAPS)
			{
				ItemRemap* remap = &level->itemRemaps[level->itemRemapCount++];
				remap->levelNum = targetLevelNum;
				remap->fromType = fromType;
				remap->toType = toType;
			}
		}

		skip_whitespace(p);
		if (match_char(p, ','))
		{
			// continue
		}
		else if (*p->cursor != ']')
		{
			snprintf(p->errorMsg, p->errorCapacity, "Expected comma or closing bracket in itemOverrides array");
			return false;
		}
	}

	return true;
}

static bool parse_level_object(Parser* p, PangeaConfigLevel* level, int targetLevelNum)
{
	if (!match_char(p, '{'))
	{
		snprintf(p->errorMsg, p->errorCapacity, "level config must be an object");
		return false;
	}

	level->hasConfig = true;

	while (true)
	{
		skip_whitespace(p);
		if (match_char(p, '}'))
		{
			break;
		}

		char key[64];
		if (!parse_string(p, key, sizeof(key)))
			return false;

		if (!match_char(p, ':'))
		{
			snprintf(p->errorMsg, p->errorCapacity, "Expected ':' after level key");
			return false;
		}

		if (strcmp(key, "script") == 0)
		{
			char path[260];
			if (!parse_string(p, path, sizeof(path)))
			{
				snprintf(p->errorMsg, p->errorCapacity, "script path must be a string");
				return false;
			}
			snprintf(level->scriptPath, sizeof(level->scriptPath), "%s", path);
		}
		else if (strcmp(key, "itemOverrides") == 0)
		{
			if (!parse_item_overrides_array(p, level, targetLevelNum))
				return false;
		}
		else if (strcmp(key, "levelSettings") == 0)
		{
			if (!parse_level_settings_object(p, level))
				return false;
		}
		else
		{
			if (!skip_value(p))
				return false;
		}

		skip_whitespace(p);
		if (match_char(p, ','))
		{
			// continue
		}
		else if (*p->cursor != '}')
		{
			snprintf(p->errorMsg, p->errorCapacity, "Expected comma or closing brace in level config");
			return false;
		}
	}

	return true;
}

PangeaScriptStatus PangeaScript_ParseConfig(const char* json, int targetLevelNum, PangeaConfig* outConfig, char* errorMsg, int errorCapacity)
{
	Parser p = { json, errorMsg, errorCapacity };
	outConfig->version = -1;
	memset(&outConfig->level, 0, sizeof(outConfig->level));

	if (!match_char(&p, '{'))
	{
		snprintf(errorMsg, errorCapacity, "Config is not a JSON object");
		return PANGEA_SCRIPT_CONFIG_ERROR;
	}

	while (true)
	{
		skip_whitespace(&p);
		if (match_char(&p, '}'))
		{
			break;
		}

		char key[64];
		if (!parse_string(&p, key, sizeof(key)))
			return PANGEA_SCRIPT_CONFIG_ERROR;

		if (!match_char(&p, ':'))
		{
			snprintf(errorMsg, errorCapacity, "Expected ':' after config key");
			return PANGEA_SCRIPT_CONFIG_ERROR;
		}

		if (strcmp(key, "version") == 0)
		{
			double versionVal;
			if (!parse_number(&p, &versionVal))
			{
				snprintf(errorMsg, errorCapacity, "version must be a number");
				return PANGEA_SCRIPT_CONFIG_ERROR;
			}
			outConfig->version = (int) versionVal;
			if (outConfig->version != 1)
			{
				snprintf(errorMsg, errorCapacity, "Unknown schema version: %d", outConfig->version);
				return PANGEA_SCRIPT_CONFIG_ERROR;
			}
		}
		else if (strcmp(key, "levels") == 0)
		{
			if (!match_char(&p, '{'))
			{
				snprintf(errorMsg, errorCapacity, "levels must be an object");
				return PANGEA_SCRIPT_CONFIG_ERROR;
			}

			while (true)
			{
				skip_whitespace(&p);
				if (match_char(&p, '}'))
				{
					break;
				}

				char levelKey[64];
				if (!parse_string(&p, levelKey, sizeof(levelKey)))
					return PANGEA_SCRIPT_CONFIG_ERROR;

				if (!match_char(&p, ':'))
				{
					snprintf(errorMsg, errorCapacity, "Expected ':' after level key");
					return PANGEA_SCRIPT_CONFIG_ERROR;
				}

				int levelKeyNum = atoi(levelKey);
				bool isTarget = (levelKeyNum == targetLevelNum || strcmp(levelKey, "current") == 0);

				if (isTarget)
				{
					if (!parse_level_object(&p, &outConfig->level, targetLevelNum))
						return PANGEA_SCRIPT_CONFIG_ERROR;
				}
				else
				{
					if (!skip_value(&p))
						return PANGEA_SCRIPT_CONFIG_ERROR;
				}

				skip_whitespace(&p);
				if (match_char(&p, ','))
				{
					// continue
				}
				else if (*p.cursor != '}')
				{
					snprintf(errorMsg, errorCapacity, "Expected comma or closing brace in levels object");
					return PANGEA_SCRIPT_CONFIG_ERROR;
				}
			}
		}
		else
		{
			if (!skip_value(&p))
				return PANGEA_SCRIPT_CONFIG_ERROR;
		}

		skip_whitespace(&p);
		if (match_char(&p, ','))
		{
			// continue
		}
		else if (*p.cursor != '}')
		{
			snprintf(errorMsg, errorCapacity, "Expected comma or closing brace in config object");
			return PANGEA_SCRIPT_CONFIG_ERROR;
		}
	}

	if (outConfig->version == -1)
	{
		snprintf(errorMsg, errorCapacity, "Missing required field: version");
		return PANGEA_SCRIPT_CONFIG_ERROR;
	}

	return PANGEA_SCRIPT_OK;
}

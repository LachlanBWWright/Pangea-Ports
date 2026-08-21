#include "pangea_script_config.h"
#include <limits.h>
#include <math.h>
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
	bool truncated = false;
	if (!outStr || capacity <= 0)
	{
		snprintf(p->errorMsg, p->errorCapacity, "String output buffer is invalid");
		return false;
	}
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
		else
		{
			truncated = true;
		}
		p->cursor++;
	}
	if (*p->cursor != '"')
	{
		snprintf(p->errorMsg, p->errorCapacity, "Unterminated string");
		return false;
	}
	p->cursor++; // skip '"'
	if (truncated)
	{
		snprintf(p->errorMsg, p->errorCapacity, "String exceeds the supported length of %d characters", capacity - 1);
		return false;
	}
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
	if (!isfinite(val))
	{
		snprintf(p->errorMsg, p->errorCapacity, "Number must be finite");
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
	return isfinite(value) && value >= (double)INT_MIN && value <= (double)INT_MAX && floor(value) == value;
}

static bool parse_integer(Parser* p, int* outValue)
{
	double value;
	if (!parse_number(p, &value))
		return false;
	if (!is_integer_number(value))
	{
		snprintf(p->errorMsg, p->errorCapacity, "Expected a 32-bit integer");
		return false;
	}
	*outValue = (int)value;
	return true;
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

			if (strcmp(key, "from") == 0)
			{
				if (!parse_integer(p, &fromType)) return false;
			}
			else if (strcmp(key, "to") == 0)
			{
				if (!parse_integer(p, &toType)) return false;
			}
			else if (!skip_value(p)) return false;

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

static PangeaScriptVisualKind parse_visual_kind(const char* value)
{
	if (strcmp(value, "nativeDisplayGroup") == 0) return PANGEA_SCRIPT_VISUAL_NATIVE_DISPLAY_GROUP;
	if (strcmp(value, "customDisplayGroup") == 0) return PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP;
	if (strcmp(value, "nativeSkeleton") == 0) return PANGEA_SCRIPT_VISUAL_NATIVE_SKELETON;
	if (strcmp(value, "customSkeleton") == 0) return PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON;
	return PANGEA_SCRIPT_VISUAL_NONE;
}

static PangeaScriptCollisionPreset parse_collision_preset(const char* value)
{
	if (strcmp(value, "solidBox") == 0) return PANGEA_SCRIPT_COLLISION_SOLID_BOX;
	if (strcmp(value, "triggerBox") == 0) return PANGEA_SCRIPT_COLLISION_TRIGGER_BOX;
	if (strcmp(value, "pickup") == 0) return PANGEA_SCRIPT_COLLISION_PICKUP;
	if (strcmp(value, "enemy") == 0) return PANGEA_SCRIPT_COLLISION_ENEMY;
	if (strcmp(value, "platform") == 0) return PANGEA_SCRIPT_COLLISION_PLATFORM;
	return PANGEA_SCRIPT_COLLISION_NONE;
}

static bool parse_custom_visual(Parser* p, PangeaScriptCustomObjectDefinition* definition)
{
	if (!match_char(p, '{')) return false;
	while (true)
	{
		if (match_char(p, '}')) return true;
		char key[64];
		if (!parse_string(p, key, sizeof(key)) || !match_char(p, ':')) return false;
		if (strcmp(key, "kind") == 0)
		{
			char value[64];
			if (!parse_string(p, value, sizeof(value))) return false;
			definition->visualKind = parse_visual_kind(value);
		}
		else if (strcmp(key, "group") == 0)
		{
			if (!parse_string(p, definition->nativeGroup, sizeof(definition->nativeGroup))) return false;
		}
		else if (strcmp(key, "modelPath") == 0)
		{
			if (!parse_string(p, definition->modelPath, sizeof(definition->modelPath))) return false;
		}
		else if (strcmp(key, "skeletonPath") == 0)
		{
			if (!parse_string(p, definition->skeletonPath, sizeof(definition->skeletonPath))) return false;
		}
		else if (strcmp(key, "initialAnimation") == 0)
		{
			skip_whitespace(p);
			if (*p->cursor == '"')
			{
				if (!parse_string(p, definition->initialAnimationName, sizeof(definition->initialAnimationName))) return false;
			}
			else
			{
				if (!parse_integer(p, &definition->initialAnimation)) return false;
			}
		}
		else if (strcmp(key, "animations") == 0)
		{
			if (!match_char(p, '{')) return false;
			while (true)
			{
				if (match_char(p, '}')) break;
				char animationName[64];
				int animationIndex;
				if (!parse_string(p, animationName, sizeof(animationName)) ||
					!match_char(p, ':') || !parse_integer(p, &animationIndex)) return false;
				if (definition->animationCount < 16)
				{
					int index = definition->animationCount++;
					snprintf(definition->animationNames[index], sizeof(definition->animationNames[index]), "%s", animationName);
					definition->animationIndices[index] = animationIndex;
				}
				if (match_char(p, ',')) continue;
				if (*p->cursor != '}') return false;
			}
		}
		else if (strcmp(key, "modelObject") == 0 || strcmp(key, "skeletonType") == 0 || strcmp(key, "slot") == 0)
		{
			if (strcmp(key, "modelObject") == 0) {
				if (!parse_integer(p, &definition->modelObject)) return false;
			}
			else if (strcmp(key, "skeletonType") == 0) {
				if (!parse_integer(p, &definition->skeletonType)) return false;
			}
			else if (!parse_integer(p, &definition->slot)) return false;
		}
		else if (strcmp(key, "scale") == 0 || strcmp(key, "animationSpeed") == 0)
		{
			double value;
			if (!parse_number(p, &value)) return false;
			if (strcmp(key, "scale") == 0) definition->scale = (float)value;
			else definition->animationSpeed = (float)value;
		}
		else if (!skip_value(p)) return false;
		if (match_char(p, ',')) continue;
		if (*p->cursor != '}') return false;
	}
}

static bool parse_custom_collision(Parser* p, PangeaScriptCustomObjectDefinition* definition)
{
	bool hasWidth = false;
	bool hasHeight = false;
	bool hasDepth = false;
	if (!match_char(p, '{')) return false;
	while (true)
	{
		if (match_char(p, '}'))
		{
			if ((hasWidth || hasHeight || hasDepth) && (!hasWidth || !hasHeight || !hasDepth))
			{
				snprintf(p->errorMsg, p->errorCapacity, "collision bounds require width, height, and depth");
				return false;
			}
			definition->collisionBoundsSet = hasWidth && hasHeight && hasDepth;
			return true;
		}
		char key[64];
		if (!parse_string(p, key, sizeof(key)) || !match_char(p, ':')) return false;
		if (strcmp(key, "preset") == 0)
		{
			char value[64];
			if (!parse_string(p, value, sizeof(value))) return false;
			definition->collisionPreset = parse_collision_preset(value);
		}
		else if (strcmp(key, "bounds") == 0)
		{
			if (!match_char(p, '{')) return false;
			while (true)
			{
				if (match_char(p, '}')) break;
				char boundsKey[32];
				double value;
				if (!parse_string(p, boundsKey, sizeof(boundsKey)) || !match_char(p, ':') || !parse_number(p, &value)) return false;
				if (strcmp(boundsKey, "width") == 0)
				{
					definition->collisionWidth = (float)value;
					hasWidth = true;
				}
				else if (strcmp(boundsKey, "height") == 0)
				{
					definition->collisionHeight = (float)value;
					hasHeight = true;
				}
				else if (strcmp(boundsKey, "depth") == 0)
				{
					definition->collisionDepth = (float)value;
					hasDepth = true;
				}
				else
				{
					snprintf(p->errorMsg, p->errorCapacity, "Unknown collision bounds field: %s", boundsKey);
					return false;
				}
				if (match_char(p, ',')) continue;
				if (*p->cursor != '}') return false;
			}
		}
		else if (!skip_value(p)) return false;
		if (match_char(p, ',')) continue;
		if (*p->cursor != '}') return false;
	}
}

static bool has_safe_custom_asset_path(const char* path, const char* prefix)
{
	if (!path || !prefix)
		return false;

	size_t prefixLength = strlen(prefix);
	if (strncmp(path, prefix, prefixLength) != 0 || path[prefixLength] == '\0')
		return false;
	if (strstr(path + prefixLength, "..") != NULL || strchr(path + prefixLength, '\\') != NULL)
		return false;

	const char* segmentStart = path + prefixLength;
	size_t segmentLength = 0;
	for (const char* cursor = segmentStart;; cursor++)
	{
		if (*cursor != '/' && *cursor != '\0')
		{
			segmentLength++;
			continue;
		}

		if (segmentLength == 0 ||
			(segmentLength == 1 && segmentStart[0] == '.') ||
			(segmentLength == 2 && segmentStart[0] == '.' && segmentStart[1] == '.'))
			return false;
		if (*cursor == '\0')
			break;
		segmentStart = cursor + 1;
		segmentLength = 0;
	}
	return true;
}

static bool has_custom_object_id(const PangeaConfigLevel* level, const char* id)
{
	for (int i = 0; i < level->customObjectCount; i++)
	{
		if (strcmp(level->customObjects[i].id, id) == 0)
			return true;
	}
	return false;
}

static bool validate_custom_object(const PangeaConfigLevel* level, int index, char* errorMsg, int errorCapacity)
{
	const PangeaScriptCustomObjectDefinition* definition = &level->customObjects[index];
	for (int previous = 0; previous < index; previous++)
	{
		if (strcmp(level->customObjects[previous].id, definition->id) == 0)
		{
			snprintf(errorMsg, errorCapacity, "Duplicate custom object id: %s", definition->id);
			return false;
		}
	}

	if (!isfinite(definition->scale) || definition->scale <= 0.0f || definition->scale > 100.0f)
	{
		snprintf(errorMsg, errorCapacity, "Custom object '%s' scale must be finite and between 0 and 100", definition->id);
		return false;
	}
	if (!isfinite(definition->animationSpeed) || definition->animationSpeed < 0.0f)
	{
		snprintf(errorMsg, errorCapacity, "Custom object '%s' animationSpeed must be finite and non-negative", definition->id);
		return false;
	}
	if (definition->collisionBoundsSet &&
		(!isfinite(definition->collisionWidth) || definition->collisionWidth <= 0.0f || definition->collisionWidth > 1000.0f ||
		 !isfinite(definition->collisionHeight) || definition->collisionHeight <= 0.0f || definition->collisionHeight > 1000.0f ||
		 !isfinite(definition->collisionDepth) || definition->collisionDepth <= 0.0f || definition->collisionDepth > 1000.0f))
	{
		snprintf(errorMsg, errorCapacity, "Custom object '%s' collision bounds must be finite and between 0 and 1000", definition->id);
		return false;
	}

	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP)
	{
		if (!definition->modelPath[0] || definition->modelObject < 0)
		{
			snprintf(errorMsg, errorCapacity, "Custom object '%s' requires a non-negative modelObject and modelPath", definition->id);
			return false;
		}
	}
	if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON &&
		(!definition->modelPath[0] || !definition->skeletonPath[0]))
	{
		snprintf(errorMsg, errorCapacity, "Custom object '%s' requires modelPath and skeletonPath", definition->id);
		return false;
	}

	for (int animation = 0; animation < definition->animationCount; animation++)
	{
		if (!definition->animationNames[animation][0] || definition->animationIndices[animation] < 0)
		{
			snprintf(errorMsg, errorCapacity, "Custom object '%s' has an invalid animation declaration", definition->id);
			return false;
		}
		for (int previous = 0; previous < animation; previous++)
		{
			if (strcmp(definition->animationNames[previous], definition->animationNames[animation]) == 0)
			{
				snprintf(errorMsg, errorCapacity, "Custom object '%s' declares animation '%s' more than once", definition->id, definition->animationNames[animation]);
				return false;
			}
		}
	}

	return true;
}

static bool validate_replacement_references(const PangeaConfigLevel* level, char* errorMsg, int errorCapacity)
{
	for (int i = 0; i < level->terrainReplacementCount; i++)
	{
		const PangeaScriptTerrainReplacement* replacement = &level->terrainReplacements[i];
		if (!isfinite(replacement->x) || !isfinite(replacement->z))
		{
			snprintf(errorMsg, errorCapacity, "Terrain replacement coordinates must be finite");
			return false;
		}
		if (!has_custom_object_id(level, replacement->customObjectId))
		{
			snprintf(errorMsg, errorCapacity, "Terrain replacement references unknown custom object: %s", replacement->customObjectId);
			return false;
		}
		for (int previous = 0; previous < i; previous++)
		{
			const PangeaScriptTerrainReplacement* prior = &level->terrainReplacements[previous];
			if (prior->itemIndex == replacement->itemIndex && prior->nativeType == replacement->nativeType &&
				fabsf(prior->x - replacement->x) < 0.5f && fabsf(prior->z - replacement->z) < 0.5f)
			{
				snprintf(errorMsg, errorCapacity, "Duplicate terrain replacement for item %d and native type %d", replacement->itemIndex, replacement->nativeType);
				return false;
			}
		}
	}
	for (int i = 0; i < level->mapReplacementCount; i++)
	{
		const PangeaScriptMapReplacement* replacement = &level->mapReplacements[i];
		if (!isfinite(replacement->x) || !isfinite(replacement->y))
		{
			snprintf(errorMsg, errorCapacity, "Map replacement coordinates must be finite");
			return false;
		}
		if (!has_custom_object_id(level, replacement->customObjectId))
		{
			snprintf(errorMsg, errorCapacity, "Map replacement references unknown custom object: %s", replacement->customObjectId);
			return false;
		}
		for (int previous = 0; previous < i; previous++)
		{
			const PangeaScriptMapReplacement* prior = &level->mapReplacements[previous];
			if (prior->itemIndex == replacement->itemIndex && prior->nativeType == replacement->nativeType &&
				fabsf(prior->x - replacement->x) < 0.5f && fabsf(prior->y - replacement->y) < 0.5f)
			{
				snprintf(errorMsg, errorCapacity, "Duplicate map replacement for item %d and native type %d", replacement->itemIndex, replacement->nativeType);
				return false;
			}
		}
	}
	for (int i = 0; i < level->splineReplacementCount; i++)
	{
		const PangeaScriptSplineReplacement* replacement = &level->splineReplacements[i];
		if (!isfinite(replacement->placement) || replacement->placement < 0.0f || replacement->placement > 1.0f)
		{
			snprintf(errorMsg, errorCapacity, "Spline replacement placement must be finite and between 0 and 1");
			return false;
		}
		if (!has_custom_object_id(level, replacement->customObjectId))
		{
			snprintf(errorMsg, errorCapacity, "Spline replacement references unknown custom object: %s", replacement->customObjectId);
			return false;
		}
		for (int previous = 0; previous < i; previous++)
		{
			const PangeaScriptSplineReplacement* prior = &level->splineReplacements[previous];
			if (prior->splineNum == replacement->splineNum && prior->itemIndex == replacement->itemIndex &&
				prior->nativeType == replacement->nativeType && fabsf(prior->placement - replacement->placement) < 0.0001f)
			{
				snprintf(errorMsg, errorCapacity, "Duplicate spline replacement for spline %d, item %d, and native type %d", replacement->splineNum, replacement->itemIndex, replacement->nativeType);
				return false;
			}
		}
	}
	return true;
}

static bool validate_level_config(const PangeaConfigLevel* level, char* errorMsg, int errorCapacity)
{
	for (int i = 0; i < level->customObjectCount; i++)
	{
		if (!validate_custom_object(level, i, errorMsg, errorCapacity))
			return false;
	}
	return validate_replacement_references(level, errorMsg, errorCapacity);
}

static bool parse_custom_objects_array(Parser* p, PangeaConfigLevel* level)
{
	if (!match_char(p, '[')) return false;
	while (true)
	{
		if (match_char(p, ']')) return true;
		if (level->customObjectCount >= PANGEA_CONFIG_MAX_CUSTOM_OBJECTS) return false;
		if (!match_char(p, '{')) return false;
		PangeaScriptCustomObjectDefinition* definition = &level->customObjects[level->customObjectCount];
		memset(definition, 0, sizeof(*definition));
		definition->scale = 1.0f;
		definition->animationSpeed = 1.0f;
		definition->slot = 300;
		while (true)
		{
			if (match_char(p, '}')) break;
			char key[64];
			if (!parse_string(p, key, sizeof(key)) || !match_char(p, ':')) return false;
			if (strcmp(key, "id") == 0)
			{
				if (!parse_string(p, definition->id, sizeof(definition->id))) return false;
			}
			else if (strcmp(key, "visual") == 0)
			{
				if (!parse_custom_visual(p, definition)) return false;
			}
			else if (strcmp(key, "collision") == 0)
			{
				if (!parse_custom_collision(p, definition)) return false;
			}
			else if (!skip_value(p)) return false;
			if (match_char(p, ',')) continue;
			if (*p->cursor != '}') return false;
		}
		if (!definition->id[0]) return false;
		if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_DISPLAY_GROUP &&
			!has_safe_custom_asset_path(definition->modelPath, "Data/Scripts/assets/models/")) return false;
		if (definition->visualKind == PANGEA_SCRIPT_VISUAL_CUSTOM_SKELETON &&
			(!has_safe_custom_asset_path(definition->modelPath, "Data/Scripts/assets/skeletons/") ||
			 !has_safe_custom_asset_path(definition->skeletonPath, "Data/Scripts/assets/skeletons/"))) return false;
		level->customObjectCount++;
		if (match_char(p, ',')) continue;
		if (*p->cursor != ']') return false;
	}
}

static bool parse_terrain_replacements_array(Parser* p, PangeaConfigLevel* level)
{
	if (!match_char(p, '[')) return false;
	while (true)
	{
		if (match_char(p, ']')) return true;
		if (level->terrainReplacementCount >= PANGEA_CONFIG_MAX_TERRAIN_REPLACEMENTS || !match_char(p, '{')) return false;
		PangeaScriptTerrainReplacement* replacement = &level->terrainReplacements[level->terrainReplacementCount];
		replacement->itemIndex = -1;
		replacement->nativeType = -1;
		while (true)
		{
			if (match_char(p, '}')) break;
			char key[64];
			if (!parse_string(p, key, sizeof(key)) || !match_char(p, ':')) return false;
			if (strcmp(key, "customObjectId") == 0)
			{
				if (!parse_string(p, replacement->customObjectId, sizeof(replacement->customObjectId))) return false;
			}
			else if (strcmp(key, "strict") == 0)
			{
				if (!parse_bool(p, &replacement->strict)) return false;
			}
			else if (strcmp(key, "itemIndex") == 0 || strcmp(key, "nativeType") == 0 ||
				strcmp(key, "x") == 0 || strcmp(key, "z") == 0)
			{
				if (strcmp(key, "itemIndex") == 0) {
					if (!parse_integer(p, &replacement->itemIndex)) return false;
				}
				else if (strcmp(key, "nativeType") == 0) {
					if (!parse_integer(p, &replacement->nativeType)) return false;
				}
				else
				{
					double value;
					if (!parse_number(p, &value)) return false;
					if (strcmp(key, "x") == 0) replacement->x = (float)value;
					else replacement->z = (float)value;
				}
			}
			else if (!skip_value(p)) return false;
			if (match_char(p, ',')) continue;
			if (*p->cursor != '}') return false;
		}
		if (replacement->itemIndex < 0 || replacement->nativeType < 0 || !replacement->customObjectId[0]) return false;
		level->terrainReplacementCount++;
		if (match_char(p, ',')) continue;
		if (*p->cursor != ']') return false;
	}
}

static bool parse_map_replacements_array(Parser* p, PangeaConfigLevel* level)
{
	if (!match_char(p, '[')) return false;
	while (true)
	{
		if (match_char(p, ']')) return true;
		if (level->mapReplacementCount >= PANGEA_CONFIG_MAX_MAP_REPLACEMENTS || !match_char(p, '{')) return false;
		PangeaScriptMapReplacement* replacement = &level->mapReplacements[level->mapReplacementCount];
		replacement->itemIndex = -1;
		replacement->nativeType = -1;
		while (true)
		{
			if (match_char(p, '}')) break;
			char key[64];
			if (!parse_string(p, key, sizeof(key)) || !match_char(p, ':')) return false;
			if (strcmp(key, "customObjectId") == 0)
			{
				if (!parse_string(p, replacement->customObjectId, sizeof(replacement->customObjectId))) return false;
			}
			else if (strcmp(key, "strict") == 0)
			{
				if (!parse_bool(p, &replacement->strict)) return false;
			}
			else if (strcmp(key, "itemIndex") == 0 || strcmp(key, "nativeType") == 0 ||
				strcmp(key, "x") == 0 || strcmp(key, "y") == 0)
			{
				if (strcmp(key, "itemIndex") == 0)
				{
					if (!parse_integer(p, &replacement->itemIndex)) return false;
				}
				else if (strcmp(key, "nativeType") == 0)
				{
					if (!parse_integer(p, &replacement->nativeType)) return false;
				}
				else
				{
					double value;
					if (!parse_number(p, &value)) return false;
					if (strcmp(key, "x") == 0) replacement->x = (float)value;
					else replacement->y = (float)value;
				}
			}
			else if (!skip_value(p)) return false;
			if (match_char(p, ',')) continue;
			if (*p->cursor != '}') return false;
		}
		if (replacement->itemIndex < 0 || replacement->nativeType < 0 || !replacement->customObjectId[0]) return false;
		level->mapReplacementCount++;
		if (match_char(p, ',')) continue;
		if (*p->cursor != ']') return false;
	}
}

static bool parse_spline_replacements_array(Parser* p, PangeaConfigLevel* level)
{
	if (!match_char(p, '[')) return false;
	while (true)
	{
		if (match_char(p, ']')) return true;
		if (level->splineReplacementCount >= PANGEA_CONFIG_MAX_SPLINE_REPLACEMENTS || !match_char(p, '{')) return false;
		PangeaScriptSplineReplacement* replacement = &level->splineReplacements[level->splineReplacementCount];
		replacement->splineNum = -1;
		replacement->itemIndex = -1;
		replacement->nativeType = -1;
		while (true)
		{
			if (match_char(p, '}')) break;
			char key[64];
			if (!parse_string(p, key, sizeof(key)) || !match_char(p, ':')) return false;
			if (strcmp(key, "customObjectId") == 0)
			{
				if (!parse_string(p, replacement->customObjectId, sizeof(replacement->customObjectId))) return false;
			}
			else if (strcmp(key, "strict") == 0)
			{
				if (!parse_bool(p, &replacement->strict)) return false;
			}
			else if (strcmp(key, "splineNum") == 0 || strcmp(key, "itemIndex") == 0 ||
				strcmp(key, "nativeType") == 0 || strcmp(key, "placement") == 0)
			{
				if (strcmp(key, "splineNum") == 0) {
					if (!parse_integer(p, &replacement->splineNum)) return false;
				}
				else if (strcmp(key, "itemIndex") == 0) {
					if (!parse_integer(p, &replacement->itemIndex)) return false;
				}
				else if (strcmp(key, "nativeType") == 0) {
					if (!parse_integer(p, &replacement->nativeType)) return false;
				}
				else
				{
					double value;
					if (!parse_number(p, &value)) return false;
					replacement->placement = (float)value;
				}
			}
			else if (!skip_value(p)) return false;
			if (match_char(p, ',')) continue;
			if (*p->cursor != '}') return false;
		}
		if (replacement->splineNum < 0 || replacement->itemIndex < 0 || replacement->nativeType < 0 || !replacement->customObjectId[0]) return false;
		level->splineReplacementCount++;
		if (match_char(p, ',')) continue;
		if (*p->cursor != ']') return false;
	}
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
		else if (strcmp(key, "customObjects") == 0)
		{
			if (!parse_custom_objects_array(p, level))
				return false;
		}
		else if (strcmp(key, "terrainReplacements") == 0)
		{
			if (!parse_terrain_replacements_array(p, level))
				return false;
		}
		else if (strcmp(key, "mapReplacements") == 0)
		{
			if (!parse_map_replacements_array(p, level))
				return false;
		}
		else if (strcmp(key, "splineReplacements") == 0)
		{
			if (!parse_spline_replacements_array(p, level))
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
			if (!parse_integer(&p, &outConfig->version))
			{
				snprintf(errorMsg, errorCapacity, "version must be a number");
				return PANGEA_SCRIPT_CONFIG_ERROR;
			}
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

	if (!validate_level_config(&outConfig->level, errorMsg, errorCapacity))
		return PANGEA_SCRIPT_CONFIG_ERROR;

	return PANGEA_SCRIPT_OK;
}

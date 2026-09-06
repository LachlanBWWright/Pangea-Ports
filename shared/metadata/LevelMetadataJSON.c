#include "LevelMetadataJSON.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct
{
	const char *cursor;
	const char *end;
	int depth;
} MetadataParser;

static void SkipWhitespace(MetadataParser *parser)
{
	while (parser->cursor < parser->end &&
		(*parser->cursor == ' ' || *parser->cursor == '\t' ||
		 *parser->cursor == '\r' || *parser->cursor == '\n'))
		parser->cursor++;
}

static int IsHexDigit(char character)
{
	return (character >= '0' && character <= '9') ||
		(character >= 'a' && character <= 'f') ||
		(character >= 'A' && character <= 'F');
}

static int ParseString(MetadataParser *parser, char *value, size_t valueSize)
{
	size_t length = 0;

	if (parser->cursor >= parser->end || *parser->cursor != '"') return 0;
	parser->cursor++;
	while (parser->cursor < parser->end)
	{
		unsigned char character = (unsigned char)*parser->cursor++;
		if (character == '"')
		{
			if (value)
			{
				if (length >= valueSize) return 0;
				value[length] = '\0';
			}
			return 1;
		}
		if (character < 0x20) return 0;
		if (character == '\\')
		{
			if (parser->cursor >= parser->end) return 0;
			character = (unsigned char)*parser->cursor++;
			switch (character)
			{
				case '"': case '\\': case '/': case 'b': case 'f':
				case 'n': case 'r': case 't':
					break;
				case 'u':
					if (parser->end - parser->cursor < 4 ||
						!IsHexDigit(parser->cursor[0]) || !IsHexDigit(parser->cursor[1]) ||
						!IsHexDigit(parser->cursor[2]) || !IsHexDigit(parser->cursor[3]))
						return 0;
					parser->cursor += 4;
					break;
				default: return 0;
			}
		}
		if (value)
		{
			if (length + 1 >= valueSize) return 0;
			value[length++] = (char)character;
		}
	}
	return 0;
}

static int ParseNumber(MetadataParser *parser, int *isOne)
{
	const char *start = parser->cursor;
	char *end;
	double value;

	if (parser->cursor < parser->end && *parser->cursor == '-') parser->cursor++;
	if (parser->cursor >= parser->end) return 0;
	if (*parser->cursor == '0') parser->cursor++;
	else
	{
		if (*parser->cursor < '1' || *parser->cursor > '9') return 0;
		while (parser->cursor < parser->end && *parser->cursor >= '0' && *parser->cursor <= '9') parser->cursor++;
	}
	if (parser->cursor < parser->end && *parser->cursor == '.')
	{
		parser->cursor++;
		if (parser->cursor >= parser->end || *parser->cursor < '0' || *parser->cursor > '9') return 0;
		while (parser->cursor < parser->end && *parser->cursor >= '0' && *parser->cursor <= '9') parser->cursor++;
	}
	if (parser->cursor < parser->end && (*parser->cursor == 'e' || *parser->cursor == 'E'))
	{
		parser->cursor++;
		if (parser->cursor < parser->end && (*parser->cursor == '+' || *parser->cursor == '-')) parser->cursor++;
		if (parser->cursor >= parser->end || *parser->cursor < '0' || *parser->cursor > '9') return 0;
		while (parser->cursor < parser->end && *parser->cursor >= '0' && *parser->cursor <= '9') parser->cursor++;
	}

	if (isOne)
	{
		char number[64];
		size_t length = (size_t)(parser->cursor - start);
		if (length >= sizeof(number)) return 0;
		memcpy(number, start, length);
		number[length] = '\0';
		value = strtod(number, &end);
		*isOne = end != number && value == 1.0;
	}
	return 1;
}

static int ParseValue(MetadataParser *parser, int depth);

static int ParseArray(MetadataParser *parser, int depth)
{
	if (*parser->cursor++ != '[') return 0;
	SkipWhitespace(parser);
	if (parser->cursor < parser->end && *parser->cursor == ']')
	{
		parser->cursor++;
		return 1;
	}
	while (parser->cursor < parser->end)
	{
		if (!ParseValue(parser, depth + 1)) return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end) return 0;
		if (*parser->cursor == ']')
		{
			parser->cursor++;
			return 1;
		}
		if (*parser->cursor++ != ',') return 0;
		SkipWhitespace(parser);
	}
	return 0;
}

static int ParseObject(MetadataParser *parser, int depth)
{
	if (*parser->cursor++ != '{') return 0;
	SkipWhitespace(parser);
	if (parser->cursor < parser->end && *parser->cursor == '}')
	{
		parser->cursor++;
		return 1;
	}
	while (parser->cursor < parser->end)
	{
		if (!ParseString(parser, NULL, 0)) return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end || *parser->cursor++ != ':') return 0;
		SkipWhitespace(parser);
		if (!ParseValue(parser, depth + 1)) return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end) return 0;
		if (*parser->cursor == '}')
		{
			parser->cursor++;
			return 1;
		}
		if (*parser->cursor++ != ',') return 0;
		SkipWhitespace(parser);
	}
	return 0;
}

static int ParseValue(MetadataParser *parser, int depth)
{
	if (depth > 32 || parser->cursor >= parser->end) return 0;
	if (*parser->cursor == '"') return ParseString(parser, NULL, 0);
	if (*parser->cursor == '{') return ParseObject(parser, depth);
	if (*parser->cursor == '[') return ParseArray(parser, depth);
	if (*parser->cursor == '-' || (*parser->cursor >= '0' && *parser->cursor <= '9')) return ParseNumber(parser, NULL);
	if (parser->end - parser->cursor >= 4 && memcmp(parser->cursor, "true", 4) == 0) { parser->cursor += 4; return 1; }
	if (parser->end - parser->cursor >= 5 && memcmp(parser->cursor, "false", 5) == 0) { parser->cursor += 5; return 1; }
	if (parser->end - parser->cursor >= 4 && memcmp(parser->cursor, "null", 4) == 0) { parser->cursor += 4; return 1; }
	return 0;
}

static int ParseProperties(MetadataParser *parser)
{
	if (parser->cursor >= parser->end || *parser->cursor++ != '{') return 0;
	SkipWhitespace(parser);
	if (parser->cursor < parser->end && *parser->cursor == '}') { parser->cursor++; return 1; }
	while (parser->cursor < parser->end)
	{
		if (!ParseString(parser, NULL, 0)) return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end || *parser->cursor++ != ':') return 0;
		SkipWhitespace(parser);
		if (!ParseString(parser, NULL, 0)) return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end) return 0;
		if (*parser->cursor == '}') { parser->cursor++; return 1; }
		if (*parser->cursor++ != ',') return 0;
		SkipWhitespace(parser);
	}
	return 0;
}

static int ParsePropertiesForKey(
	MetadataParser *parser,
	const char *targetKey,
	char *value,
	size_t valueSize)
{
	char propertyKey[128];
	int found = 0;

	if (parser->cursor >= parser->end || *parser->cursor++ != '{') return 0;
	SkipWhitespace(parser);
	if (parser->cursor < parser->end && *parser->cursor == '}')
	{
		parser->cursor++;
		return 0;
	}
	while (parser->cursor < parser->end)
	{
		if (!ParseString(parser, propertyKey, sizeof(propertyKey))) return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end || *parser->cursor++ != ':') return 0;
		SkipWhitespace(parser);
		if (strcmp(propertyKey, targetKey) == 0)
		{
			if (found || !ParseString(parser, value, valueSize)) return 0;
			found = 1;
		}
		else if (!ParseString(parser, NULL, 0)) return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end) return 0;
		if (*parser->cursor == '}')
		{
			parser->cursor++;
			return found;
		}
		if (*parser->cursor++ != ',') return 0;
		SkipWhitespace(parser);
	}
	return 0;
}

static int ParseEnvelope(MetadataParser *parser, const char *expectedGame)
{
	char key[64];
	char game[64];
	char identity[256];
	int schemaIsOne = 0;
	int hasSchema = 0;
	int hasGame = 0;
	int hasIdentity = 0;
	int hasProperties = 0;
	int objectClosed = 0;

	SkipWhitespace(parser);
	if (parser->cursor >= parser->end || *parser->cursor++ != '{') return 0;
	SkipWhitespace(parser);
	if (parser->cursor < parser->end && *parser->cursor == '}') return 0;
	while (parser->cursor < parser->end)
	{
		if (!ParseString(parser, key, sizeof(key))) return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end || *parser->cursor++ != ':') return 0;
		SkipWhitespace(parser);
		if (strcmp(key, "schemaVersion") == 0)
		{
			if (hasSchema || !ParseNumber(parser, &schemaIsOne)) return 0;
			hasSchema = 1;
		}
		else if (strcmp(key, "game") == 0)
		{
			if (hasGame || !ParseString(parser, game, sizeof(game))) return 0;
			hasGame = 1;
		}
		else if (strcmp(key, "identity") == 0)
		{
			if (hasIdentity || !ParseString(parser, identity, sizeof(identity)) || identity[0] == '\0') return 0;
			hasIdentity = 1;
		}
		else if (strcmp(key, "properties") == 0)
		{
			if (hasProperties || !ParseProperties(parser)) return 0;
			hasProperties = 1;
		}
		else return 0;
		SkipWhitespace(parser);
		if (parser->cursor >= parser->end) return 0;
		if (*parser->cursor == '}') { parser->cursor++; objectClosed = 1; break; }
		if (*parser->cursor++ != ',') return 0;
		SkipWhitespace(parser);
	}
	SkipWhitespace(parser);
	return objectClosed && parser->cursor == parser->end && hasSchema && schemaIsOne && hasGame &&
		hasIdentity && hasProperties && strcmp(game, expectedGame) == 0;
}

int PangeaLevelMetadataJSONIsValid(const char *json, size_t size, const char *expectedGame)
{
	MetadataParser parser;
	if (!json || !expectedGame || size == 0) return 0;
	parser.cursor = json;
	parser.end = json + size;
	parser.depth = 0;
	return ParseEnvelope(&parser, expectedGame);
}

int PangeaLevelMetadataJSONGetString(
	const char *json,
	size_t size,
	const char *key,
	char *value,
	size_t valueSize)
{
	MetadataParser parser;
	char envelopeKey[64];
	int found = 0;

	if (!json || !key || !value || size == 0 || valueSize == 0) return 0;
	parser.cursor = json;
	parser.end = json + size;
	parser.depth = 0;
	SkipWhitespace(&parser);
	if (parser.cursor >= parser.end || *parser.cursor++ != '{') return 0;
	SkipWhitespace(&parser);
	while (parser.cursor < parser.end)
	{
		if (!ParseString(&parser, envelopeKey, sizeof(envelopeKey))) return 0;
		SkipWhitespace(&parser);
		if (parser.cursor >= parser.end || *parser.cursor++ != ':') return 0;
		SkipWhitespace(&parser);
		if (strcmp(envelopeKey, "properties") == 0)
		{
			if (found || !ParsePropertiesForKey(&parser, key, value, valueSize)) return 0;
			found = 1;
		}
		else if (!ParseValue(&parser, 1)) return 0;
		SkipWhitespace(&parser);
		if (parser.cursor >= parser.end) return 0;
		if (*parser.cursor == '}')
		{
			parser.cursor++;
			break;
		}
		if (*parser.cursor++ != ',') return 0;
		SkipWhitespace(&parser);
	}
	SkipWhitespace(&parser);
	return found && parser.cursor == parser.end;
}

int PangeaLevelMetadataJSONGetFloat(
	const char *json,
	size_t size,
	const char *key,
	float *value)
{
	char text[64];
	char *valueEnd;
	float parsed;

	if (!value || !PangeaLevelMetadataJSONGetString(json, size, key, text, sizeof(text))) return 0;
	parsed = strtof(text, &valueEnd);
	if (valueEnd == text || *valueEnd != '\0' || !isfinite(parsed)) return 0;
	*value = parsed;
	return 1;
}

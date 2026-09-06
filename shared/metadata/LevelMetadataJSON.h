#ifndef PANGEA_LEVEL_METADATA_JSON_H
#define PANGEA_LEVEL_METADATA_JSON_H

#include <stddef.h>

/*
 * Validates the complete editor metadata envelope. Property names are
 * intentionally game-owned; this boundary validates only their string shape.
 */
int PangeaLevelMetadataJSONIsValid(
	const char *json,
	size_t size,
	const char *expectedGame);

/* Reads one string-valued property from the validated envelope. */
int PangeaLevelMetadataJSONGetString(
	const char *json,
	size_t size,
	const char *key,
	char *value,
	size_t valueSize);

/* Reads one numeric property encoded as a string from the metadata envelope. */
int PangeaLevelMetadataJSONGetFloat(
	const char *json,
	size_t size,
	const char *key,
	float *value);

#endif

#include "pangea_script_backend.h"

#include <stddef.h>

struct PangeaScriptBackend
{
	int unused;
};

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo)
{
	(void) gameInfo;
	return NULL;
}

void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend)
{
	(void) backend;
}

PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity)
{
	(void) backend;
	(void) source;
	if (error && errorCapacity > 0)
		error[0] = '\0';
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) hook;
	(void) context;
	if (error && errorCapacity > 0)
		error[0] = '\0';
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	if (error && errorCapacity > 0)
		error[0] = '\0';
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	if (error && errorCapacity > 0)
		error[0] = '\0';
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	if (error && errorCapacity > 0)
		error[0] = '\0';
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	if (error && errorCapacity > 0)
		error[0] = '\0';
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity)
{
	(void) backend;
	(void) context;
	(void) result;
	if (error && errorCapacity > 0)
		error[0] = '\0';
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

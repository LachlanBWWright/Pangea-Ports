#include "pangea_script_backend.h"

#include <duktape.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PangeaScriptBackend
{
	duk_context* ctx;
	PangeaScriptGameInfo gameInfo;
};

static void copy_error(char* dest, int capacity, const char* message)
{
	if (!dest || capacity <= 0)
		return;
	snprintf(dest, (size_t) capacity, "%s", message ? message : "Unknown script error");
}

static duk_ret_t log_info(duk_context* ctx)
{
	printf("[PangeaScript] %s\n", duk_safe_to_lstring(ctx, 0, NULL));
	return 0;
}

static duk_ret_t log_warn(duk_context* ctx)
{
	printf("[PangeaScript warning] %s\n", duk_safe_to_lstring(ctx, 0, NULL));
	return 0;
}

static duk_ret_t log_error(duk_context* ctx)
{
	printf("[PangeaScript error] %s\n", duk_safe_to_lstring(ctx, 0, NULL));
	return 0;
}

static duk_ret_t level_current(duk_context* ctx)
{
	(void) ctx;
	return 0;
}

static duk_ret_t spawn_native(duk_context* ctx)
{
	(void) ctx;
	printf("[PangeaScript warning] pangea.spawn.native is not bound for this game yet\n");
	return 0;
}

static duk_ret_t spawn_scripted(duk_context* ctx)
{
	(void) ctx;
	printf("[PangeaScript warning] pangea.spawn.scripted is not bound for this game yet\n");
	return 0;
}

static void put_function(duk_context* ctx, const char* name, duk_ret_t (*func)(duk_context*))
{
	duk_push_c_function(ctx, func, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, name);
}

static void install_module_exports(duk_context* ctx)
{
	duk_push_global_object(ctx);
	duk_dup(ctx, -1);
	duk_put_global_string(ctx, "globalThis");
	duk_pop(ctx);

	duk_push_object(ctx);
	duk_dup(ctx, -1);
	duk_put_global_string(ctx, "exports");

	duk_push_object(ctx);
	duk_dup(ctx, -2);
	duk_put_prop_string(ctx, -2, "exports");
	duk_put_global_string(ctx, "module");

	duk_pop(ctx);
}

static void install_pangea_api(PangeaScriptBackend* backend)
{
	duk_context* ctx = backend->ctx;

	duk_push_object(ctx);

	duk_push_object(ctx);
	duk_push_int(ctx, 1);
	duk_put_prop_string(ctx, -2, "version");
	duk_put_prop_string(ctx, -2, "api");

	duk_push_object(ctx);
	duk_push_string(ctx, backend->gameInfo.gameId);
	duk_put_prop_string(ctx, -2, "id");
	duk_push_string(ctx, backend->gameInfo.gameName);
	duk_put_prop_string(ctx, -2, "name");
	duk_put_prop_string(ctx, -2, "game");

	duk_push_object(ctx);
	put_function(ctx, "current", level_current);
	duk_put_prop_string(ctx, -2, "level");

	duk_push_object(ctx);
	put_function(ctx, "info", log_info);
	put_function(ctx, "warn", log_warn);
	put_function(ctx, "error", log_error);
	duk_put_prop_string(ctx, -2, "log");

	duk_push_object(ctx);
	put_function(ctx, "native", spawn_native);
	put_function(ctx, "scripted", spawn_scripted);
	duk_put_prop_string(ctx, -2, "spawn");

	duk_push_object(ctx);
	put_function(ctx, "log", log_info);
	put_function(ctx, "warn", log_warn);
	put_function(ctx, "error", log_error);
	duk_dup(ctx, -2);
	duk_put_global_string(ctx, "console");
	duk_put_prop_string(ctx, -2, "console");

	duk_put_global_string(ctx, "pangea");
}

static const char* hook_name(PangeaScriptHook hook)
{
	switch (hook)
	{
		case PANGEA_SCRIPT_HOOK_GAME_START: return "onGameStart";
		case PANGEA_SCRIPT_HOOK_LEVEL_LOAD: return "onLevelLoad";
		case PANGEA_SCRIPT_HOOK_LEVEL_START: return "onLevelStart";
		case PANGEA_SCRIPT_HOOK_FRAME: return "onFrame";
		case PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE: return "onLevelComplete";
		case PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD: return "onLevelUnload";
		case PANGEA_SCRIPT_HOOK_GAME_SHUTDOWN: return "onGameShutdown";
		case PANGEA_SCRIPT_HOOK_TERRAIN_ITEM: return "onTerrainItem";
		case PANGEA_SCRIPT_HOOK_SPLINE_ITEM: return "onSplineItem";
		case PANGEA_SCRIPT_HOOK_MAP_ITEM: return "onMapItem";
	}
	return "";
}

static void push_game_context(duk_context* ctx, const PangeaScriptGameInfo* gameInfo)
{
	duk_push_object(ctx);
	duk_push_string(ctx, gameInfo->gameId);
	duk_put_prop_string(ctx, -2, "gameId");
	duk_push_string(ctx, gameInfo->gameName);
	duk_put_prop_string(ctx, -2, "gameName");
}

static void push_level_context(PangeaScriptBackend* backend, const PangeaScriptLevelContext* context)
{
	push_game_context(backend->ctx, &backend->gameInfo);
	duk_push_int(backend->ctx, context->levelNum);
	duk_put_prop_string(backend->ctx, -2, "levelNum");
	if (context->levelName)
	{
		duk_push_string(backend->ctx, context->levelName);
		duk_put_prop_string(backend->ctx, -2, "levelName");
	}
}

static void push_frame_context(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context)
{
	PangeaScriptLevelContext levelContext =
	{
		.levelNum = context->levelNum,
		.levelName = NULL,
	};
	push_level_context(backend, &levelContext);
	duk_push_int(backend->ctx, (duk_int_t) context->frameNum);
	duk_put_prop_string(backend->ctx, -2, "frameNum");
	duk_push_number(backend->ctx, context->deltaSeconds);
	duk_put_prop_string(backend->ctx, -2, "deltaSeconds");
	duk_push_number(backend->ctx, context->levelTimeSeconds);
	duk_put_prop_string(backend->ctx, -2, "levelTimeSeconds");
}

static void push_params_array(duk_context* ctx, const unsigned char* params, int paramCount)
{
	duk_push_array(ctx);
	for (int i = 0; i < paramCount; i++)
	{
		duk_push_int(ctx, params[i]);
		duk_put_prop_index(ctx, -2, (duk_uint_t) i);
	}
}

static void push_terrain_context(PangeaScriptBackend* backend, const PangeaScriptTerrainItemContext* context)
{
	PangeaScriptLevelContext levelContext =
	{
		.levelNum = context->levelNum,
		.levelName = NULL,
	};
	push_level_context(backend, &levelContext);

	duk_push_int(backend->ctx, context->itemType);
	duk_put_prop_string(backend->ctx, -2, "itemType");
	duk_push_int(backend->ctx, context->remappedItemType);
	duk_put_prop_string(backend->ctx, -2, "remappedItemType");
	duk_push_int(backend->ctx, context->playerNum);
	duk_put_prop_string(backend->ctx, -2, "playerNum");
	duk_push_boolean(backend->ctx, context->networked);
	duk_put_prop_string(backend->ctx, -2, "networked");

	duk_push_object(backend->ctx);
	duk_push_number(backend->ctx, context->x);
	duk_put_prop_string(backend->ctx, -2, "x");
	duk_push_number(backend->ctx, 0.0);
	duk_put_prop_string(backend->ctx, -2, "y");
	duk_push_number(backend->ctx, context->z);
	duk_put_prop_string(backend->ctx, -2, "z");
	duk_put_prop_string(backend->ctx, -2, "position");

	duk_push_int(backend->ctx, (duk_int_t) context->flags);
	duk_put_prop_string(backend->ctx, -2, "flags");
	push_params_array(backend->ctx, context->params, context->paramCount);
	duk_put_prop_string(backend->ctx, -2, "params");
}

static void push_spline_context(PangeaScriptBackend* backend, const PangeaScriptSplineItemContext* context)
{
	PangeaScriptLevelContext levelContext =
	{
		.levelNum = context->levelNum,
		.levelName = NULL,
	};
	push_level_context(backend, &levelContext);

	duk_push_int(backend->ctx, context->itemType);
	duk_put_prop_string(backend->ctx, -2, "itemType");
	duk_push_int(backend->ctx, context->splineNum);
	duk_put_prop_string(backend->ctx, -2, "splineNum");
	duk_push_number(backend->ctx, context->placement);
	duk_put_prop_string(backend->ctx, -2, "placement");
	push_params_array(backend->ctx, context->params, context->paramCount);
	duk_put_prop_string(backend->ctx, -2, "params");
}

static void push_map_item_context(PangeaScriptBackend* backend, const PangeaScriptMapItemContext* context)
{
	PangeaScriptLevelContext levelContext =
	{
		.levelNum = context->levelNum,
		.levelName = NULL,
	};
	push_level_context(backend, &levelContext);

	duk_push_int(backend->ctx, context->sceneNum);
	duk_put_prop_string(backend->ctx, -2, "sceneNum");
	duk_push_int(backend->ctx, context->areaNum);
	duk_put_prop_string(backend->ctx, -2, "areaNum");
	duk_push_int(backend->ctx, context->itemType);
	duk_put_prop_string(backend->ctx, -2, "itemType");

	duk_push_object(backend->ctx);
	duk_push_number(backend->ctx, context->x);
	duk_put_prop_string(backend->ctx, -2, "x");
	duk_push_number(backend->ctx, context->y);
	duk_put_prop_string(backend->ctx, -2, "y");
	duk_put_prop_string(backend->ctx, -2, "position");

	push_params_array(backend->ctx, context->params, context->paramCount);
	duk_put_prop_string(backend->ctx, -2, "params");
}

static PangeaScriptStatus call_function_on_top(PangeaScriptBackend* backend, char* error, int errorCapacity)
{
	duk_context* ctx = backend->ctx;
	if (duk_pcall(ctx, 1) != 0)
	{
		copy_error(error, errorCapacity, duk_safe_to_lstring(ctx, -1, NULL));
		return PANGEA_SCRIPT_RUNTIME_ERROR;
	}

	return PANGEA_SCRIPT_OK;
}

static bool push_global_hook(PangeaScriptBackend* backend, const char* name)
{
	duk_get_global_string(backend->ctx, name);
	if (duk_is_function(backend->ctx, -1))
		return true;

	duk_pop(backend->ctx);

	duk_get_global_string(backend->ctx, "exports");
	if (duk_get_prop_string(backend->ctx, -1, name) && duk_is_function(backend->ctx, -1))
	{
		duk_remove(backend->ctx, -2);
		return true;
	}

	duk_pop_2(backend->ctx);
	return false;
}

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo)
{
	if (!gameInfo)
		return NULL;

	PangeaScriptBackend* backend = (PangeaScriptBackend*) calloc(1, sizeof(PangeaScriptBackend));
	if (!backend)
		return NULL;

	backend->ctx = duk_create_heap(NULL, NULL, NULL, NULL, NULL);
	if (!backend->ctx)
	{
		free(backend);
		return NULL;
	}

	backend->gameInfo = *gameInfo;
	install_module_exports(backend->ctx);
	install_pangea_api(backend);
	return backend;
}

void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend)
{
	if (!backend)
		return;
	if (backend->ctx)
		duk_destroy_heap(backend->ctx);
	free(backend);
}

PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity)
{
	if (!backend || !backend->ctx || !source)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (duk_peval_lstring(backend->ctx, source, strlen(source)) != 0)
	{
		copy_error(error, errorCapacity, duk_safe_to_lstring(backend->ctx, -1, NULL));
		duk_pop(backend->ctx);
		return PANGEA_SCRIPT_PARSE_ERROR;
	}

	duk_pop(backend->ctx);
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (!push_global_hook(backend, hook_name(hook)))
		return PANGEA_SCRIPT_OK;

	push_level_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	duk_pop(backend->ctx);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (!push_global_hook(backend, "onFrame"))
		return PANGEA_SCRIPT_OK;

	push_frame_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	duk_pop(backend->ctx);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (!push_global_hook(backend, "onTerrainItem"))
		return PANGEA_SCRIPT_OK;

	push_terrain_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status != PANGEA_SCRIPT_OK)
	{
		duk_pop(backend->ctx);
		return status;
	}

	duk_context* ctx = backend->ctx;
	if (duk_get_top(ctx) <= 0)
		return PANGEA_SCRIPT_OK;

	if (duk_get_type(ctx, -1) == DUK_TYPE_OBJECT)
	{
		if (duk_get_prop_string(ctx, -1, "handled"))
			context->handled = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
		if (duk_get_prop_string(ctx, -1, "markInUse"))
			context->markInUse = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
	}
	duk_pop(ctx);
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (!push_global_hook(backend, "onSplineItem"))
		return PANGEA_SCRIPT_OK;

	push_spline_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status != PANGEA_SCRIPT_OK)
	{
	duk_pop(backend->ctx);
	return status;
}

PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity)
{
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	if (!push_global_hook(backend, "onMapItem"))
		return PANGEA_SCRIPT_OK;

	push_map_item_context(backend, context);
	PangeaScriptStatus status = call_function_on_top(backend, error, errorCapacity);
	if (status == PANGEA_SCRIPT_OK && duk_is_object(backend->ctx, -1))
	{
		duk_get_prop_string(backend->ctx, -1, "handled");
		context->handled = duk_to_boolean(backend->ctx, -1);
		duk_pop(backend->ctx);
		duk_get_prop_string(backend->ctx, -1, "markInUse");
		context->markInUse = duk_to_boolean(backend->ctx, -1);
		duk_pop(backend->ctx);
	}
	duk_pop(backend->ctx);
	return status;
}

	duk_context* ctx = backend->ctx;
	if (duk_get_top(ctx) <= 0)
		return PANGEA_SCRIPT_OK;

	if (duk_get_type(ctx, -1) == DUK_TYPE_OBJECT)
	{
		if (duk_get_prop_string(ctx, -1, "handled"))
			context->handled = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
		if (duk_get_prop_string(ctx, -1, "markInUse"))
			context->markInUse = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
	}
	duk_pop(ctx);
	return PANGEA_SCRIPT_OK;
}

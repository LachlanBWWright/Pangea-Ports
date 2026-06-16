#include "pangea_script_backend.h"

#include <emscripten/emscripten.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define EMSCRIPTEN_SCRIPT_JSON_CAPACITY 2048

struct PangeaScriptBackend
{
	PangeaScriptGameInfo gameInfo;
};

static void copy_error(char* dest, int capacity, const char* message)
{
	if (!dest || capacity <= 0)
		return;
	snprintf(dest, (size_t) capacity, "%s", message ? message : "Unknown script error");
}

static void write_json_string(char* dest, int capacity, const char* value)
{
	int used = 0;
	if (!dest || capacity <= 0)
		return;

	dest[0] = '"';
	used = 1;

	for (const char* cursor = value ? value : ""; *cursor && used < capacity - 2; cursor++)
	{
		if (*cursor == '"' || *cursor == '\\')
		{
			if (used >= capacity - 3)
				break;
			dest[used++] = '\\';
		}
		dest[used++] = *cursor;
	}

	dest[used++] = '"';
	dest[used] = '\0';
}

static void write_tags_json(const char* const* tags, int tagCount, char* dest, int capacity)
{
	int used = 0;
	if (!dest || capacity <= 0)
		return;

	dest[used++] = '[';
	for (int i = 0; i < tagCount && used < capacity - 2; i++)
	{
		char tagJson[256];
		write_json_string(tagJson, (int)sizeof(tagJson), tags[i]);
		int written = snprintf(dest + used, (size_t)(capacity - used), "%s%s", i == 0 ? "" : ",", tagJson);
		if (written < 0)
			break;
		used += written;
		if (used >= capacity)
		{
			used = capacity - 1;
			break;
		}
	}

	if (used < capacity - 1)
		dest[used++] = ']';
	dest[used] = '\0';
}

static void write_params_json(const unsigned char* params, int paramCount, char* dest, int capacity)
{
	int used = 0;
	if (!dest || capacity <= 0)
		return;

	dest[used++] = '[';
	for (int i = 0; i < paramCount && used < capacity - 2; i++)
	{
		int written = snprintf(dest + used, (size_t)(capacity - used), "%s%d", i == 0 ? "" : ",", params[i]);
		if (written < 0)
			break;
		used += written;
		if (used >= capacity)
		{
			used = capacity - 1;
			break;
		}
	}

	if (used < capacity - 1)
		dest[used++] = ']';
	dest[used] = '\0';
}

EM_JS(void, pangea_script_install_game_info_js, (const char* gameIdJson, const char* gameNameJson), {
	const gameId = JSON.parse(UTF8ToString(gameIdJson));
	const gameName = JSON.parse(UTF8ToString(gameNameJson));
	globalThis.pangea = globalThis.pangea || {};
	globalThis.pangea.api = {
		version: 1,
		capabilities: () => ({
			objectPosition: true,
			objectMutation: true,
			spawnNative: true,
			spawnScripted: true,
			levelSettings: true
		})
	};
	globalThis.pangea.game = { id: gameId, name: gameName };
	globalThis.pangea.log = globalThis.pangea.log || {
		info: (message) => {
			console.info(`[PangeaScript] ${String(message)}`);
			if (typeof _PangeaScript_LogJS === "function") {
				const str = String(message);
				const len = lengthBytesUTF8(str) + 1;
				const ptr = _malloc(len);
				stringToUTF8(str, ptr, len);
				_PangeaScript_LogJS(0, ptr);
				_free(ptr);
			}
		},
		warn: (message) => {
			console.warn(`[PangeaScript warning] ${String(message)}`);
			if (typeof _PangeaScript_LogJS === "function") {
				const str = String(message);
				const len = lengthBytesUTF8(str) + 1;
				const ptr = _malloc(len);
				stringToUTF8(str, ptr, len);
				_PangeaScript_LogJS(1, ptr);
				_free(ptr);
			}
		},
		error: (message) => {
			console.error(`[PangeaScript error] ${String(message)}`);
			if (typeof _PangeaScript_LogJS === "function") {
				const str = String(message);
				const len = lengthBytesUTF8(str) + 1;
				const ptr = _malloc(len);
				stringToUTF8(str, ptr, len);
				_PangeaScript_LogJS(2, ptr);
				_free(ptr);
			}
		},
	};
	globalThis.pangea.spawn = {
		native: (id, pos, options) => {
			if (typeof id !== "string" || !pos || typeof pos.x !== "number" || typeof pos.y !== "number" || typeof pos.z !== "number") return undefined;
			
			let subtype = -1;
			let amount = -1;
			if (options && typeof options === "object") {
				if (typeof options.subtype === "number") subtype = options.subtype;
				if (typeof options.amount === "number") amount = options.amount;
			}

			const idLen = lengthBytesUTF8(id) + 1;
			const idPtr = _malloc(idLen);
			stringToUTF8(id, idPtr, idLen);
			
			const outHandleIdPtr = _malloc(4);
			const outHandleGenPtr = _malloc(4);
			
			const status = _PangeaScript_SpawnNativeJS(idPtr, pos.x, pos.y, pos.z, subtype, amount, outHandleIdPtr, outHandleGenPtr);
			_free(idPtr);
			
			if (status === 0) { // PANGEA_SCRIPT_OK
				const handleId = HEAP32[outHandleIdPtr >> 2];
				const handleGen = HEAP32[outHandleGenPtr >> 2];
				_free(outHandleIdPtr);
				_free(outHandleGenPtr);
				if (handleId > 0) {
					return { id: handleId, generation: handleGen };
				}
			} else {
				_free(outHandleIdPtr);
				_free(outHandleGenPtr);
			}
			return undefined;
		}
	};
	globalThis.pangea.object = {
		position: (handle) => {
			if (!handle || typeof handle.id !== "number" || typeof handle.generation !== "number") return undefined;
			const outXPtr = _malloc(4);
			const outYPtr = _malloc(4);
			const outZPtr = _malloc(4);
			const ok = _PangeaScript_GetObjectPositionJS(handle.id, handle.generation, outXPtr, outYPtr, outZPtr);
			if (!ok) {
				_free(outXPtr); _free(outYPtr); _free(outZPtr);
				return undefined;
			}
			const x = HEAPF32[outXPtr >> 2];
			const y = HEAPF32[outYPtr >> 2];
			const z = HEAPF32[outZPtr >> 2];
			_free(outXPtr); _free(outYPtr); _free(outZPtr);
			return { x, y, z };
		},
		setPosition: (handle, pos) => {
			if (!handle || typeof handle.id !== "number" || typeof handle.generation !== "number") return false;
			if (!pos || typeof pos.x !== "number" || typeof pos.y !== "number" || typeof pos.z !== "number") return false;
			return !!_PangeaScript_SetObjectPositionJS(handle.id, handle.generation, pos.x, pos.y, pos.z);
		},
		setVelocity: (handle, vel) => {
			if (!handle || typeof handle.id !== "number" || typeof handle.generation !== "number") return false;
			if (!vel || typeof vel.x !== "number" || typeof vel.y !== "number" || typeof vel.z !== "number") return false;
			return !!_PangeaScript_SetObjectVelocityJS(handle.id, handle.generation, vel.x, vel.y, vel.z);
		},
		delete: (handle) => {
			if (!handle || typeof handle.id !== "number" || typeof handle.generation !== "number") return false;
			return !!_PangeaScript_DeleteObjectJS(handle.id, handle.generation);
		}
	};
	globalThis.pangea.experimental = {
		level: { current: () => null },
		player: { get: () => null },
		spawn: {
			scripted: (id, pos) => {
				if (typeof id !== "string" || !pos || typeof pos.x !== "number" || typeof pos.y !== "number" || typeof pos.z !== "number") return undefined;
				const idLen = lengthBytesUTF8(id) + 1;
				const idPtr = _malloc(idLen);
				stringToUTF8(id, idPtr, idLen);
				const outHandleIdPtr = _malloc(4);
				const outHandleGenPtr = _malloc(4);
				const status = _PangeaScript_RegisterScriptedObjectJS(idPtr, pos.x, pos.y, pos.z, outHandleIdPtr, outHandleGenPtr);
				_free(idPtr);
				if (status === 0) {
					const handleId = HEAP32[outHandleIdPtr >> 2];
					const handleGen = HEAP32[outHandleGenPtr >> 2];
					_free(outHandleIdPtr);
					_free(outHandleGenPtr);
					if (handleId > 0) {
						return { id: handleId, generation: handleGen };
					}
				} else {
					_free(outHandleIdPtr);
					_free(outHandleGenPtr);
				}
				return undefined;
			}
		}
	};
	globalThis.exports = {};
	globalThis.module = { exports: globalThis.exports };
});

static void install_game_info(const PangeaScriptGameInfo* gameInfo)
{
	char gameId[256];
	char gameName[256];
	write_json_string(gameId, (int)sizeof(gameId), gameInfo->gameId);
	write_json_string(gameName, (int)sizeof(gameName), gameInfo->gameName);
	pangea_script_install_game_info_js(gameId, gameName);
}

static PangeaScriptStatus copy_js_error(char* error, int errorCapacity)
{
	const char* message = (const char*) EM_ASM_PTR({
		const message = globalThis.__pangeaScriptLastError || "Script execution failed";
		const length = lengthBytesUTF8(message) + 1;
		const ptr = _malloc(length);
		stringToUTF8(message, ptr, length);
		return ptr;
	});
	copy_error(error, errorCapacity, message);
	free((void*) message);
	return PANGEA_SCRIPT_RUNTIME_ERROR;
}

EM_JS(int, pangea_script_load_js, (const char* source), {
	try {
		globalThis.exports = {};
		globalThis.module = { exports: globalThis.exports };
		(0, eval)(UTF8ToString(source));
		return 0;
	} catch (error) {
		globalThis.__pangeaScriptLastError = error && error.stack ? error.stack : String(error);
		return 1;
	}
});

EM_JS(int, pangea_script_call_js, (const char* hookName, const char* contextJson, double* outX, double* outY, double* outZ, int* outHasOffset, int* outHandled, int* outMarkInUse), {
	const name = UTF8ToString(hookName);
	try {
		if (outHasOffset) {
			HEAP32[outHasOffset >> 2] = 0;
		}
		if (outHandled) {
			HEAP32[outHandled >> 2] = 0;
		}
		if (outMarkInUse) {
			HEAP32[outMarkInUse >> 2] = 0;
		}
		const hook = globalThis[name] || (globalThis.exports && globalThis.exports[name]);
		if (typeof hook !== "function") {
			return 0;
		}
		const result = hook(JSON.parse(UTF8ToString(contextJson)));
		if (result && typeof result === "object") {
			if (outHandled && "handled" in result) {
				HEAP32[outHandled >> 2] = result.handled ? 1 : 0;
			}
			if (outMarkInUse && "markInUse" in result) {
				HEAP32[outMarkInUse >> 2] = result.markInUse ? 1 : 0;
			}
		}
		const positionOffset =
			result && typeof result === "object" && result.positionOffset && typeof result.positionOffset === "object"
				? result.positionOffset
				: null;
		if (positionOffset && outX && outY && outZ && outHasOffset) {
			HEAPF64[outX >> 3] = Number(positionOffset.x) || 0;
			HEAPF64[outY >> 3] = Number(positionOffset.y) || 0;
			HEAPF64[outZ >> 3] = Number(positionOffset.z) || 0;
			HEAP32[outHasOffset >> 2] = 1;
		}
		return 0;
	} catch (error) {
		const detail = error && error.stack ? error.stack : String(error);
		globalThis.__pangeaScriptLastError = `${name}: ${detail}`;
		return 1;
	}
});

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo)
{
	if (!gameInfo)
		return NULL;

	PangeaScriptBackend* backend = (PangeaScriptBackend*) calloc(1, sizeof(PangeaScriptBackend));
	if (!backend)
		return NULL;

	backend->gameInfo = *gameInfo;
	install_game_info(gameInfo);
	return backend;
}

void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend)
{
	free(backend);
}

PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity)
{
	if (!backend || !source)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	install_game_info(&backend->gameInfo);
	if (pangea_script_load_js(source) != 0)
		return copy_js_error(error, errorCapacity);

	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	const char* hookName = NULL;
	char contextJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	switch (hook)
	{
		case PANGEA_SCRIPT_HOOK_LEVEL_LOAD: hookName = "onLevelLoad"; break;
		case PANGEA_SCRIPT_HOOK_LEVEL_START: hookName = "onLevelStart"; break;
		case PANGEA_SCRIPT_HOOK_LEVEL_COMPLETE: hookName = "onLevelComplete"; break;
		case PANGEA_SCRIPT_HOOK_LEVEL_UNLOAD: hookName = "onLevelUnload"; break;
		default: return PANGEA_SCRIPT_OK;
	}

	snprintf(contextJson, sizeof(contextJson), "{\"levelNum\":%d,\"levelName\":null}", context->levelNum);
	if (pangea_script_call_js(hookName, contextJson, NULL, NULL, NULL, NULL, NULL, NULL) != 0)
		return copy_js_error(error, errorCapacity);

	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity)
{
	char contextJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	snprintf(
		contextJson,
		sizeof(contextJson),
		"{\"levelNum\":%d,\"frameNum\":%u,\"deltaSeconds\":%.9g,\"levelTimeSeconds\":%.9g}",
		context->levelNum,
		context->frameNum,
		context->deltaSeconds,
		context->levelTimeSeconds);
	if (pangea_script_call_js("onFrame", contextJson, NULL, NULL, NULL, NULL, NULL, NULL) != 0)
		return copy_js_error(error, errorCapacity);

	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity)
{
	char paramsJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	char contextJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	int handled = 0;
	int markInUse = 0;

	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	write_params_json(context->params, context->paramCount, paramsJson, (int)sizeof(paramsJson));
	snprintf(
		contextJson,
		sizeof(contextJson),
		"{\"levelNum\":%d,\"itemType\":%d,\"remappedItemType\":%d,\"playerNum\":%d,\"networked\":%s,\"position\":{\"x\":%.9g,\"y\":0,\"z\":%.9g},\"flags\":%u,\"params\":%s}",
		context->levelNum,
		context->itemType,
		context->remappedItemType,
		context->playerNum,
		context->networked ? "true" : "false",
		context->x,
		context->z,
		context->flags,
		paramsJson);

	if (pangea_script_call_js("onTerrainItem", contextJson, NULL, NULL, NULL, NULL, &handled, &markInUse) != 0)
		return copy_js_error(error, errorCapacity);

	context->handled = handled != 0;
	context->markInUse = markInUse != 0;
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity)
{
	char paramsJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	char contextJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	int handled = 0;
	int markInUse = 0;

	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	write_params_json(context->params, context->paramCount, paramsJson, (int)sizeof(paramsJson));
	snprintf(
		contextJson,
		sizeof(contextJson),
		"{\"levelNum\":%d,\"itemType\":%d,\"splineNum\":%d,\"placement\":%.9g,\"params\":%s}",
		context->levelNum,
		context->itemType,
		context->splineNum,
		context->placement,
		paramsJson);

	if (pangea_script_call_js("onSplineItem", contextJson, NULL, NULL, NULL, NULL, &handled, &markInUse) != 0)
		return copy_js_error(error, errorCapacity);

	context->handled = handled != 0;
	context->markInUse = markInUse != 0;
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity)
{
	char paramsJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	char contextJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	int handled = 0;
	int markInUse = 0;

	if (!backend || !context)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	write_params_json(context->params, context->paramCount, paramsJson, (int)sizeof(paramsJson));
	snprintf(
		contextJson,
		sizeof(contextJson),
		"{\"levelNum\":%d,\"sceneNum\":%d,\"areaNum\":%d,\"itemType\":%d,\"position\":{\"x\":%.9g,\"y\":%.9g},\"params\":%s}",
		context->levelNum,
		context->sceneNum,
		context->areaNum,
		context->itemType,
		context->x,
		context->y,
		paramsJson);

	if (pangea_script_call_js("onMapItem", contextJson, NULL, NULL, NULL, NULL, &handled, &markInUse) != 0)
		return copy_js_error(error, errorCapacity);

	context->handled = handled != 0;
	context->markInUse = markInUse != 0;
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity)
{
	char tagsJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	char contextJson[EMSCRIPTEN_SCRIPT_JSON_CAPACITY];
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
	int hasOffset = 0;

	if (!backend || !context || !result)
		return PANGEA_SCRIPT_BAD_ARGUMENT;

	result->hasPositionOffset = false;
	result->positionOffset.x = 0.0f;
	result->positionOffset.y = 0.0f;
	result->positionOffset.z = 0.0f;

	write_tags_json(context->tags, context->tagCount, tagsJson, (int)sizeof(tagsJson));
	snprintf(
		contextJson,
		sizeof(contextJson),
		"{\"levelNum\":%d,\"frameNum\":%u,\"deltaSeconds\":%.9g,\"levelTimeSeconds\":%.9g,\"object\":{\"id\":%d,\"generation\":%u},\"position\":{\"x\":%.9g,\"y\":%.9g,\"z\":%.9g},\"tags\":%s}",
		context->levelNum,
		context->frameNum,
		context->deltaSeconds,
		context->levelTimeSeconds,
		context->object.id,
		context->object.generation,
		context->position.x,
		context->position.y,
		context->position.z,
		tagsJson);

	if (pangea_script_call_js("onObjectFrame", contextJson, &x, &y, &z, &hasOffset, NULL, NULL) != 0)
		return copy_js_error(error, errorCapacity);

	if (hasOffset)
	{
		result->hasPositionOffset = true;
		result->positionOffset.x = (float) x;
		result->positionOffset.y = (float) y;
		result->positionOffset.z = (float) z;
	}

	return PANGEA_SCRIPT_OK;
}

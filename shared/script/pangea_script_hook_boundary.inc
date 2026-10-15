#undef PangeaScriptBackend_CallLevelHook
#undef PangeaScriptBackend_CallFrameHook
#undef PangeaScriptBackend_CallTerrainItemHook
#undef PangeaScriptBackend_CallSplineItemHook
#undef PangeaScriptBackend_CallMapItemHook
#undef PangeaScriptBackend_CallObjectFrameHook
#undef PangeaScriptBackend_CallTriggerHook
#undef PangeaScriptBackend_CallPickupHook
#undef PangeaScriptBackend_CallWeaponHitHook
#undef PangeaScriptBackend_CallDamageHook
#undef PangeaScriptBackend_CallDamageAppliedHook
#undef PangeaScriptBackend_CallPlayerEvent

typedef struct HookBoundary
{
	PangeaScriptBackend* backend;
	void (*invoke)(struct HookBoundary* boundary);
	void* arguments;
	char* error;
	int errorCapacity;
	PangeaScriptStatus status;
} HookBoundary;

static int invoke_hook_boundary(lua_State* lua)
{
	HookBoundary* boundary = lua_touserdata(lua, 1);
	boundary->invoke(boundary);
	return 0;
}

static PangeaScriptStatus run_hook_boundary(HookBoundary* boundary, int budget)
{
	PangeaScriptBackend* backend = boundary->backend;
	if (!backend || !backend->lua) return PANGEA_SCRIPT_BAD_ARGUMENT;
	lua_State* lua = backend->lua;
	int stackTop = lua_gettop(lua);
	if (!lua_checkstack(lua, 12))
	{
		copy_error(boundary->error, boundary->errorCapacity, "Lua hook stack allocation failed");
		return PANGEA_SCRIPT_BUDGET_EXCEEDED;
	}
	lua_Hook previousHook = lua_gethook(lua);
	int previousMask = lua_gethookmask(lua), previousCount = lua_gethookcount(lua);
	bool previousHasObject = backend->hasCurrentObject;
	PangeaScriptObjectHandle previousObject = backend->currentObject;
	if (!backend->callbackDepth) backend->remainingInstructions = budget;
	backend->callbackDepth++;
	lua_sethook(lua, instruction_budget_hook, LUA_MASKCOUNT, 100);
	/* Context allocation and result metamethods need the same protection as callbacks. */
	lua_pushcfunction(lua, invoke_hook_boundary);
	lua_pushlightuserdata(lua, boundary);
	int status = lua_pcall(lua, 1, 0, 0);
	lua_sethook(lua, previousHook, previousMask, previousCount);
	backend->callbackDepth--;
	backend->hasCurrentObject = previousHasObject;
	backend->currentObject = previousObject;
	if (status != LUA_OK)
	{
		const char* message = lua_type(lua, -1) == LUA_TSTRING ? lua_tostring(lua, -1) : "Lua hook failed";
		copy_error(boundary->error, boundary->errorCapacity, message);
		boundary->status = status == LUA_ERRMEM || strstr(message, "instruction budget")
			? PANGEA_SCRIPT_BUDGET_EXCEEDED : PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	lua_settop(lua, stackTop);
	return boundary->status;
}

#define SIMPLE_HOOK(Name, Impl, Context, Qualifier, Budget) \
static void invoke_##Impl(HookBoundary* boundary) \
{ \
	Qualifier Context* context = boundary->arguments; \
	boundary->status = Impl(boundary->backend, context, boundary->error, boundary->errorCapacity); \
} \
PangeaScriptStatus PangeaScriptBackend_##Name(PangeaScriptBackend* backend, Qualifier Context* context, char* error, int errorCapacity) \
{ \
	if (!context) return PANGEA_SCRIPT_BAD_ARGUMENT; \
	HookBoundary boundary = {backend, invoke_##Impl, (void*) context, error, errorCapacity, PANGEA_SCRIPT_OK}; \
	return run_hook_boundary(&boundary, Budget); \
}

SIMPLE_HOOK(CallFrameHook, call_frame_impl, PangeaScriptFrameContext, const, PANGEA_LUA_FRAME_BUDGET)
SIMPLE_HOOK(CallTerrainItemHook, call_terrain_impl, PangeaScriptTerrainItemContext, , PANGEA_LUA_EVENT_BUDGET)
SIMPLE_HOOK(CallSplineItemHook, call_spline_impl, PangeaScriptSplineItemContext, , PANGEA_LUA_EVENT_BUDGET)
SIMPLE_HOOK(CallMapItemHook, call_map_impl, PangeaScriptMapItemContext, , PANGEA_LUA_EVENT_BUDGET)
SIMPLE_HOOK(CallDamageAppliedHook, call_damage_applied_impl, PangeaScriptDamageContext, const, PANGEA_LUA_EVENT_BUDGET)
#undef SIMPLE_HOOK

#define RESULT_HOOK(Name, Impl, Context, Result, Budget) \
typedef struct Impl##_arguments { const Context* context; Result* result; } Impl##_arguments; \
static void invoke_##Impl(HookBoundary* boundary) \
{ \
	Impl##_arguments* arguments = boundary->arguments; \
	boundary->status = Impl(boundary->backend, arguments->context, arguments->result, boundary->error, boundary->errorCapacity); \
} \
PangeaScriptStatus PangeaScriptBackend_##Name(PangeaScriptBackend* backend, const Context* context, Result* result, char* error, int errorCapacity) \
{ \
	if (!context || !result) return PANGEA_SCRIPT_BAD_ARGUMENT; \
	Impl##_arguments arguments = {context, result}; \
	HookBoundary boundary = {backend, invoke_##Impl, &arguments, error, errorCapacity, PANGEA_SCRIPT_OK}; \
	return run_hook_boundary(&boundary, Budget); \
}

RESULT_HOOK(CallObjectFrameHook, call_object_impl, PangeaScriptObjectFrameContext, PangeaScriptObjectFrameResult, PANGEA_LUA_FRAME_BUDGET)
RESULT_HOOK(CallTriggerHook, call_trigger_impl, PangeaScriptTriggerContext, PangeaScriptTriggerResult, PANGEA_LUA_EVENT_BUDGET)
RESULT_HOOK(CallPickupHook, call_pickup_impl, PangeaScriptPickupContext, PangeaScriptPickupResult, PANGEA_LUA_EVENT_BUDGET)
RESULT_HOOK(CallWeaponHitHook, call_weapon_impl, PangeaScriptWeaponHitContext, PangeaScriptWeaponHitResult, PANGEA_LUA_EVENT_BUDGET)
RESULT_HOOK(CallDamageHook, call_damage_impl, PangeaScriptDamageContext, PangeaScriptDamageResult, PANGEA_LUA_EVENT_BUDGET)
#undef RESULT_HOOK

typedef struct LevelHookArguments
{
	PangeaScriptHook hook;
	const PangeaScriptLevelContext* context;
} LevelHookArguments;

static void invoke_level_boundary(HookBoundary* boundary)
{
	LevelHookArguments* arguments = boundary->arguments;
	boundary->status = call_level_impl(boundary->backend, arguments->hook, arguments->context, boundary->error, boundary->errorCapacity);
}

PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity)
{
	if (!context) return PANGEA_SCRIPT_BAD_ARGUMENT;
	LevelHookArguments arguments = {hook, context};
	HookBoundary boundary = {backend, invoke_level_boundary, &arguments, error, errorCapacity, PANGEA_SCRIPT_OK};
	return run_hook_boundary(&boundary, PANGEA_LUA_EVENT_BUDGET);
}

typedef struct PlayerHookArguments
{
	const PangeaScriptPlayerEventContext* context;
	const char* event;
} PlayerHookArguments;

static void invoke_player_boundary(HookBoundary* boundary)
{
	PlayerHookArguments* arguments = boundary->arguments;
	boundary->status = call_player_impl(boundary->backend, arguments->context, arguments->event, boundary->error, boundary->errorCapacity);
}

PangeaScriptStatus PangeaScriptBackend_CallPlayerEvent(PangeaScriptBackend* backend, const PangeaScriptPlayerEventContext* context, const char* event, char* error, int errorCapacity)
{
	if (!context || !is_supported_player_event(event)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	PlayerHookArguments arguments = {context, event};
	HookBoundary boundary = {backend, invoke_player_boundary, &arguments, error, errorCapacity, PANGEA_SCRIPT_OK};
	return run_hook_boundary(&boundary, PANGEA_LUA_EVENT_BUDGET);
}

static bool operation_integer(lua_State* lua, int index, int* output)
{
	if (!lua_isinteger(lua, index)) return false;
	lua_Integer value = lua_tointeger(lua, index);
	if (value < 0 || value > INT_MAX) return false;
	*output = (int)value;
	return true;
}

static bool operation_string(lua_State* lua, int index, const char** output)
{
	if (lua_type(lua, index) != LUA_TSTRING) return false;
	size_t length = 0;
	const char* value = lua_tolstring(lua, index, &length);
	if (!length || length > 64 || memchr(value, '\0', length)) return false;
	*output = value;
	return true;
}

static bool operation_plain_table(lua_State* lua, int index)
{
	if (!lua_istable(lua, index)) return false;
	if (lua_getmetatable(lua, index)) { lua_pop(lua, 1); return false; }
	return true;
}

static bool operation_handle(lua_State* lua, int index, PangeaScriptObjectHandle* output)
{
	return operation_plain_table(lua, index) && read_handle(lua, index, output);
}

static bool operation_vector(lua_State* lua, int index, PangeaScriptVector3* output)
{
	return operation_plain_table(lua, index) && read_vector(lua, index, output);
}

static bool operation_options(lua_State* lua, int index, PangeaScriptOperationRequest* request)
{
	if (lua_isnoneornil(lua, index)) return true;
	if (!operation_plain_table(lua, index)) return false;
	index = lua_absindex(lua, index);
	lua_pushnil(lua);
	while (lua_next(lua, index))
	{
		bool valid = false;
		const char* key = NULL;
		(void)operation_string(lua, -2, &key);
		if (key && !strcmp(key, "position")) { request->hasPosition = true; valid = operation_vector(lua, -1, &request->position); }
		else if (key && !strcmp(key, "velocity")) { request->hasVelocity = true; valid = operation_vector(lua, -1, &request->velocity); }
		else if (key && !strcmp(key, "aim")) { request->hasAim = true; valid = operation_vector(lua, -1, &request->aim); }
		else if (key && !strcmp(key, "weapon")) { request->hasWeapon = true; valid = operation_integer(lua, -1, &request->weapon); }
		else if (key && !strcmp(key, "joint")) { request->hasJoint = true; valid = operation_integer(lua, -1, &request->joint); }
		else if (key && !strcmp(key, "mode")) valid = operation_string(lua, -1, &request->mode);
		else if (key && !strcmp(key, "target"))
		{
			valid = !request->hasTarget && operation_handle(lua, -1, &request->target);
			request->hasTarget = true;
		}
		lua_pop(lua, 1);
		if (!valid) { lua_pop(lua, 1); return false; }
	}
	return true;
}

static int lua_player_operation(lua_State* lua)
{
	PangeaScriptOperationRequest request = {.operation = (PangeaScriptPlayerOperation)lua_tointeger(lua, lua_upvalueindex(1))};
	if (!operation_integer(lua, 1, &request.playerNum)) return push_object_argument_error(lua, "Player index must be a non-negative integer");
	int optionsIndex = 0;
	switch (request.operation)
	{
		case PANGEA_SCRIPT_OP_CARRY: case PANGEA_SCRIPT_OP_HANDOVER: case PANGEA_SCRIPT_OP_MOUNT:
		case PANGEA_SCRIPT_OP_CAPTURE: case PANGEA_SCRIPT_OP_GRAB: case PANGEA_SCRIPT_OP_INTERACT:
			request.hasTarget = true;
			if (!operation_handle(lua, 2, &request.target)) return push_object_argument_error(lua, "A target object handle is required");
			if (request.operation == PANGEA_SCRIPT_OP_MOUNT || request.operation == PANGEA_SCRIPT_OP_GRAB || request.operation == PANGEA_SCRIPT_OP_INTERACT) optionsIndex = 3;
			break;
		case PANGEA_SCRIPT_OP_TRANSFORM:
			if (!operation_string(lua, 2, &request.form)) return push_object_argument_error(lua, "A native form name is required");
			optionsIndex = 3;
			break;
		case PANGEA_SCRIPT_OP_COMMAND_BUDDY:
			if (!operation_string(lua, 2, &request.mode)) return push_object_argument_error(lua, "A buddy command name is required");
			if (!lua_isnoneornil(lua, 3)) { request.hasTarget = true; if (!operation_handle(lua, 3, &request.target)) return push_object_argument_error(lua, "Invalid buddy target"); }
			break;
		case PANGEA_SCRIPT_OP_DELIVER:
			if (!lua_isnoneornil(lua, 2)) { request.hasTarget = true; if (!operation_handle(lua, 2, &request.target)) return push_object_argument_error(lua, "Invalid delivery target"); }
			break;
		case PANGEA_SCRIPT_OP_DROP: case PANGEA_SCRIPT_OP_FIRE: case PANGEA_SCRIPT_OP_RESPAWN: optionsIndex = 2; break;
		default: break;
	}
	if (optionsIndex && !operation_options(lua, optionsIndex, &request)) return push_object_argument_error(lua, "Invalid native action options");
	PangeaScriptOperationResult output = {0};
	PangeaScriptStatus status = PangeaScript_RunPlayerOperation(&request, &output);
	push_object_command_result(lua, status, PangeaScript_GetLastError(), false, (PangeaScriptObjectHandle){0});
	if (status == PANGEA_SCRIPT_OK && output.object.id) { push_handle(lua, output.object); lua_setfield(lua, -2, "object"); }
	return 1;
}

static int lua_player_actions(lua_State* lua)
{
	lua_createtable(lua, PANGEA_SCRIPT_OP_COUNT, 0);
	int count = 0;
	for (int i = 0; i < PANGEA_SCRIPT_OP_COUNT; i++)
		if (PangeaScript_OperationSupported((PangeaScriptPlayerOperation)i))
		{ lua_pushstring(lua, PangeaScript_GetOperationName((PangeaScriptPlayerOperation)i)); lua_rawseti(lua, -2, ++count); }
	return 1;
}

static int lua_player_relations(lua_State* lua)
{
	int playerNum;
	PangeaScriptPlayerRelations relations = {0};
	if (!operation_integer(lua, 1, &playerNum) || !PangeaScript_GetPlayerRelations(playerNum, &relations)) { lua_pushnil(lua); return 1; }
	lua_createtable(lua, 0, 6);
	const char* names[] = {"carried", "mounted", "buddy", "grabbed"};
	PangeaScriptObjectHandle handles[] = {relations.carried, relations.mounted, relations.buddy, relations.grabbed};
	for (int i = 0; i < 4; i++) if (handles[i].id) { push_handle(lua, handles[i]); lua_setfield(lua, -2, names[i]); }
	lua_pushstring(lua, relations.form ? relations.form : ""); lua_setfield(lua, -2, "form");
	lua_pushinteger(lua, relations.capturedCount); lua_setfield(lua, -2, "capturedCount");
	return 1;
}

static void install_player_operations(lua_State* lua)
{
	lua_getfield(lua, -1, "player");
	set_function(lua, "actions", lua_player_actions);
	set_function(lua, "relations", lua_player_relations);
	for (int i = 0; i < PANGEA_SCRIPT_OP_COUNT; i++)
	{
		lua_pushinteger(lua, i);
		lua_pushcclosure(lua, lua_player_operation, 1);
		lua_setfield(lua, -2, PangeaScript_GetOperationName((PangeaScriptPlayerOperation)i));
	}
	lua_setfield(lua, -2, "player");
}

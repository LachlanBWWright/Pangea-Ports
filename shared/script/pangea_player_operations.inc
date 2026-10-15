static const char* const operationNames[PANGEA_SCRIPT_OP_COUNT] = {
	"carry", "drop", "handover", "mount", "dismount", "fire", "reload", "transform",
	"respawn", "capture", "deliver", "resetCarried", "releaseBuddy", "grab", "interact", "commandBuddy"
};

static bool playerOperationRunning;

const char* PangeaScript_GetOperationName(PangeaScriptPlayerOperation operation)
{
	return operation >= 0 && operation < PANGEA_SCRIPT_OP_COUNT ? operationNames[operation] : NULL;
}

bool PangeaScript_OperationSupported(PangeaScriptPlayerOperation operation)
{
	const PangeaScriptGameplayOps* ops = gGameInfo.gameplay;
	return PangeaScript_GetOperationName(operation) && ops && ops->runOperation &&
		(ops->operationMask & (1u << operation)) != 0;
}

static bool operation_player_valid(int playerNum)
{
	return playerNum >= 0 && gGameInfo.getPlayerCount && playerNum < gGameInfo.getPlayerCount();
}

static bool operation_request_valid(const PangeaScriptOperationRequest* request)
{
	if (!request || !PangeaScript_GetOperationName(request->operation) || !operation_player_valid(request->playerNum)) return false;
	if (request->hasTarget && !PangeaScript_ObjectExists(request->target)) return false;
	if (request->hasPosition && !is_finite_vector(&request->position)) return false;
	if (request->hasVelocity && !is_finite_vector(&request->velocity)) return false;
	if (request->hasAim && !is_finite_vector(&request->aim)) return false;
	if ((request->hasWeapon && request->weapon < 0) || (request->hasJoint && request->joint < 0)) return false;
	if ((request->form && (!request->form[0] || strlen(request->form) > 64)) ||
		(request->mode && (!request->mode[0] || strlen(request->mode) > 64))) return false;
	switch (request->operation)
	{
		case PANGEA_SCRIPT_OP_CARRY: case PANGEA_SCRIPT_OP_HANDOVER: case PANGEA_SCRIPT_OP_MOUNT:
		case PANGEA_SCRIPT_OP_CAPTURE: case PANGEA_SCRIPT_OP_GRAB: case PANGEA_SCRIPT_OP_INTERACT:
			return request->hasTarget;
		case PANGEA_SCRIPT_OP_TRANSFORM: return request->form != NULL;
		case PANGEA_SCRIPT_OP_COMMAND_BUDDY: return request->mode != NULL;
		default: return true;
	}
}

PangeaScriptStatus PangeaScript_RunPlayerOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output)
{
	if (output) *output = (PangeaScriptOperationResult){0};
	PangeaScriptStatus status = reject_networked_execution();
	if (status == PANGEA_SCRIPT_OK && !gInitialized) status = PANGEA_SCRIPT_NOT_ENABLED;
	if (status == PANGEA_SCRIPT_OK && playerOperationRunning) status = PANGEA_SCRIPT_RUNTIME_ERROR;
	if (status == PANGEA_SCRIPT_OK && (!output || !operation_request_valid(request))) status = PANGEA_SCRIPT_BAD_ARGUMENT;
	if (status == PANGEA_SCRIPT_OK && !PangeaScript_OperationSupported(request->operation)) status = PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (status == PANGEA_SCRIPT_OK)
	{
		playerOperationRunning = true;
		status = gGameInfo.gameplay->runOperation(request, output);
		playerOperationRunning = false;
	}
	if (status < PANGEA_SCRIPT_OK || status > PANGEA_SCRIPT_CONFIG_ERROR ||
		(status == PANGEA_SCRIPT_OK && output->object.id && !PangeaScript_ObjectExists(output->object))) status = PANGEA_SCRIPT_RUNTIME_ERROR;
	if (status != PANGEA_SCRIPT_OK && output) *output = (PangeaScriptOperationResult){0};
	char command[96];
	const char* name = request ? PangeaScript_GetOperationName(request->operation) : NULL;
	snprintf(command, sizeof(command), "pangea.player.%s", name ? name : "operation");
	record_command(command, request && request->hasTarget ? request->target : (PangeaScriptObjectHandle){0}, status);
	set_error(status, status == PANGEA_SCRIPT_OK ? "" : "Player action is unavailable, has invalid arguments, or cannot run in the current native state");
	return status;
}

bool PangeaScript_GetPlayerRelations(int playerNum, PangeaScriptPlayerRelations* output)
{
	if (!output) return false;
	*output = (PangeaScriptPlayerRelations){0};
	const PangeaScriptGameplayOps* ops = gGameInfo.gameplay;
	PangeaScriptPlayerRelations relations = {0};
	if (!gInitialized || !operation_player_valid(playerNum) || !ops || !ops->getRelations || !ops->getRelations(playerNum, &relations)) return false;
	PangeaScriptObjectHandle handles[] = {relations.carried, relations.mounted, relations.buddy, relations.grabbed};
	for (unsigned int i = 0; i < sizeof(handles) / sizeof(handles[0]); i++)
		if (handles[i].id && !PangeaScript_ObjectExists(handles[i])) return false;
	if (relations.capturedCount < 0 || (relations.form && (!relations.form[0] || strlen(relations.form) > 64))) return false;
	*output = relations;
	return true;
}

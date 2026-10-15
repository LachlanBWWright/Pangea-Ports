static PangeaScriptObjectHandle ScriptRelationHandle(ObjNode* node)
{
	if (!node || node->ScriptObjectID <= 0) return (PangeaScriptObjectHandle){0};
	PangeaScriptObjectHandle handle = {node->ScriptObjectID, node->ScriptObjectGeneration};
	return PangeaScript_ObjectExists(handle) ? handle : (PangeaScriptObjectHandle){0};
}

static bool GetScriptPlayerRelations(int playerNum, PangeaScriptPlayerRelations* output)
{
	if (!output || playerNum < 0 || playerNum >= gNumTotalPlayers || !gPlayerInfo[playerNum].objNode)
		return false;
	*output = (PangeaScriptPlayerRelations){
		.carried = ScriptRelationHandle((ObjNode*)gPlayerInfo[playerNum].objNode->CapturedFlag),
		.form = "vehicle",
		.capturedCount = gCapturedFlagCount[gPlayerInfo[playerNum].team & 1],
	};
	return true;
}

static PangeaScriptStatus RespawnScriptCar(const PangeaScriptOperationRequest* request, ObjNode* car)
{
	int playerNum = request->playerNum;
	PangeaScriptVector3 position = request->hasPosition ? request->position :
		(PangeaScriptVector3){car->InitCoord.x, car->InitCoord.y, car->InitCoord.z};
	PangeaScriptVector3 velocity = request->hasVelocity ? request->velocity : (PangeaScriptVector3){0};
	if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
		!isfinite(velocity.x) || !isfinite(velocity.y) || !isfinite(velocity.z) ||
		!isfinite(hypotf(hypotf(velocity.x, velocity.z), velocity.y))) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!CroMagScript_SetObjectPosition(car, &position)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (car->CapturedFlag) PlayerResetCarriedFlag(car);
	CroMagScript_SetObjectVelocity(car, &velocity);
	car->DeltaRot = (OGLVector3D){0};
	car->Rot.x = car->Rot.z = 0;
	car->StatusBits &= ~(STATUS_BIT_ONGROUND | STATUS_BIT_ONTERRAIN);
	gPlayerInfo[playerNum].health = 1;
	if (gPlayerInfo[playerNum].isEliminated && gNumPlayersEliminated > 0) gNumPlayersEliminated--;
	gPlayerInfo[playerNum].isEliminated = false;
	ClearWinLose(playerNum);
	gPlayerInfo[playerNum].isPlaning = false;
	gPlayerInfo[playerNum].frozenTimer = 0;
	gPlayerInfo[playerNum].submarineImmobilized = 0;
	gPlayerInfo[playerNum].impactResetTimer = 1.5f;
	gPlayerInfo[playerNum].currentThrust = 0;
	gPlayerInfo[playerNum].steering = 0;
	gPlayerInfo[playerNum].braking = false;
	PangeaScriptStateValue thaw = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 0};
	SetScriptState(playerNum, "frozenTime", &thaw);
	UpdateObjectTransforms(car);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus RunScriptPlayerOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output)
{
	if (!request || !output || request->playerNum < 0 || request->playerNum >= gNumTotalPlayers)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasAim || request->hasJoint || request->form) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	bool targetOperation = request->operation == PANGEA_SCRIPT_OP_CAPTURE || request->operation == PANGEA_SCRIPT_OP_CARRY ||
		request->operation == PANGEA_SCRIPT_OP_HANDOVER || request->operation == PANGEA_SCRIPT_OP_DELIVER;
	bool movementOperation = request->operation == PANGEA_SCRIPT_OP_DROP || request->operation == PANGEA_SCRIPT_OP_RESPAWN;
	if ((request->hasTarget && !targetOperation) || ((request->hasPosition || request->hasVelocity) && !movementOperation) ||
		((request->hasWeapon || request->mode) && request->operation != PANGEA_SCRIPT_OP_FIRE))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	ObjNode* car = gPlayerInfo[request->playerNum].objNode;
	if (!car || !gPlayerInfo[request->playerNum].onThisMachine || gIsNetworkClient)
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	*output = (PangeaScriptOperationResult){0};
	ObjNode* target = NULL;
	if (request->hasTarget)
	{
		void* native = NULL;
		if (!PangeaScript_GetObjectNativeObject(request->target, &native)) return PANGEA_SCRIPT_BAD_ARGUMENT;
		target = native;
	}
	bool success = false;
	switch (request->operation)
	{
		case PANGEA_SCRIPT_OP_CARRY:
		case PANGEA_SCRIPT_OP_CAPTURE:
			success = PlayerCaptureFlag(car, target);
			if (success) output->object = request->target;
			break;
		case PANGEA_SCRIPT_OP_DROP:
			output->object = ScriptRelationHandle((ObjNode*)car->CapturedFlag);
			if (!car->CapturedFlag) break;
			target = (ObjNode*)car->CapturedFlag;
			if (request->hasPosition && (!isfinite(request->position.x) || !isfinite(request->position.y) ||
				!isfinite(request->position.z))) return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (request->hasVelocity && (!isfinite(request->velocity.x) || !isfinite(request->velocity.y) ||
				!isfinite(request->velocity.z) || !isfinite(hypotf(hypotf(request->velocity.x, request->velocity.z), request->velocity.y))))
				return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (request->hasPosition)
			{
				if (!CroMagScript_SetObjectPosition(target, &request->position)) return PANGEA_SCRIPT_BAD_ARGUMENT;
				car->CapturedFlag = nil;
				target->Mode = 0;
				target->Delta.y = 1200;
			}
			else PlayerDropFlag(car);
			if (request->hasVelocity) CroMagScript_SetObjectVelocity(target, &request->velocity);
			success = true;
			break;
		case PANGEA_SCRIPT_OP_DELIVER:
			output->object = ScriptRelationHandle((ObjNode*)car->CapturedFlag);
			success = PlayerDeliverFlag(car, target);
			break;
		case PANGEA_SCRIPT_OP_RESET_CARRIED:
			output->object = ScriptRelationHandle((ObjNode*)car->CapturedFlag);
			success = PlayerResetCarriedFlag(car);
			break;
		case PANGEA_SCRIPT_OP_HANDOVER:
		{
			if (!target || !car->CapturedFlag || target == car || target->CapturedFlag) break;
			int recipient = -1;
			for (int i = 0; i < gNumTotalPlayers; i++)
				if (gPlayerInfo[i].objNode == target && gPlayerInfo[i].onThisMachine && !gPlayerInfo[i].isEliminated) recipient = i;
			if (recipient < 0) break;
			ObjNode* flag = (ObjNode*)car->CapturedFlag;
			output->object = ScriptRelationHandle(flag);
			car->CapturedFlag = nil;
			target->CapturedFlag = (Ptr)flag;
			flag->PlayerNum = recipient;
			success = true;
			break;
		}
		case PANGEA_SCRIPT_OP_FIRE:
			if (request->hasAim || request->hasPosition || request->hasVelocity || request->hasTarget)
				return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			if (request->mode && strcmp(request->mode, "forward") != 0 && strcmp(request->mode, "backward") != 0)
				return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (request->hasWeapon && request->weapon != gPlayerInfo[request->playerNum].powType)
				return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
			success = PlayerFirePowerup(request->playerNum, !request->mode || strcmp(request->mode, "backward") != 0);
			break;
		case PANGEA_SCRIPT_OP_RESPAWN:
			output->object = ScriptRelationHandle(car);
			return RespawnScriptCar(request, car);
		default: return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	return success ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
}

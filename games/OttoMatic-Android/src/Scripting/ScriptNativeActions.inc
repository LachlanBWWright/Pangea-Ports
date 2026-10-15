extern Boolean gPlayerEnterSaucer;

static PangeaScriptObjectHandle gOttoDelegatedPlayer;
static PangeaScriptPlayerOperation gOttoDelegatedOperation;
static float gOttoDelegatedSeconds;

bool OttoScript_IsNativeDelegated(ObjNode* player)
{
	return player && gOttoDelegatedPlayer.id && player->ScriptObjectID == gOttoDelegatedPlayer.id &&
		player->ScriptObjectGeneration == gOttoDelegatedPlayer.generation;
}

static void PollOttoNativeDelegation(float delta)
{
	ObjNode* player = gPlayerInfo.objNode;
	ObjNode* car = OttoScript_GetNativeCar();
	if (player && car && PangeaScript_ObjectIsScriptControlled((PangeaScriptObjectHandle){
		player->ScriptObjectID, player->ScriptObjectGeneration}))
	{
		player->Coord = car->Coord;
		player->Coord.y += 90.0f;
		player->Rot.y = car->Rot.y;
		gPlayerInfo.coord = player->Coord;
		UpdateObjectTransforms(player);
	}
	if (!OttoScript_IsNativeDelegated(player) || gPlayerIsDead)
	{
		gOttoDelegatedPlayer = (PangeaScriptObjectHandle){0};
		return;
	}
	gOttoDelegatedSeconds -= delta;
	gPlayerInfo.analogControlX = gPlayerInfo.analogControlZ = 0.0f;
	bool done = false;
	if (gOttoDelegatedOperation == PANGEA_SCRIPT_OP_CARRY)
		done = !gTargetPickup || (player->IsHoldingPickup && gTargetPickup->POWType == POW_TYPE_MAGNET);
	else if (gOttoDelegatedOperation == PANGEA_SCRIPT_OP_DELIVER)
		done = gNumHumansInSaucer == 0 && gNumHumansInTransit == 0;
	else if (gOttoDelegatedOperation == PANGEA_SCRIPT_OP_MOUNT)
		done = gPlayerEnterSaucer && !player->MoveCall;
	else if (player->Skeleton)
	{
		int anim = player->Skeleton->AnimNum;
		done = anim != PLAYER_ANIM_PUNCH && anim != PLAYER_ANIM_THROW && anim != PLAYER_ANIM_DRINK &&
			anim != PLAYER_ANIM_CHARGING && anim != PLAYER_ANIM_CHANGEWEAPON;
	}
	if (!done && gOttoDelegatedSeconds <= 0.0f)
	{
		if (gOttoDelegatedOperation == PANGEA_SCRIPT_OP_CARRY) (void)OttoScript_DropNativePickup();
		else if (gOttoDelegatedOperation == PANGEA_SCRIPT_OP_MOUNT) (void)OttoScript_ExitNativeSaucer();
		else if (gOttoDelegatedOperation == PANGEA_SCRIPT_OP_DELIVER) OttoScript_CancelNativeDelivery();
		else OttoScript_CancelNativeWeapon();
		done = true;
	}
	if (done) gOttoDelegatedPlayer = (PangeaScriptObjectHandle){0};
}

static PangeaScriptObjectHandle OttoNativeHandle(ObjNode* node)
{
	if (!node || node->CType == INVALID_NODE_FLAG) return (PangeaScriptObjectHandle){0};
	PangeaScriptObjectHandle handle = {node->ScriptObjectID, node->ScriptObjectGeneration};
	if (!PangeaScript_ObjectExists(handle))
	{
		OttoScript_RegisterObjectNode(node, "ottomatic.relationship", PANGEA_SCRIPT_CAPABILITY_FULL, NULL, 0);
		handle = (PangeaScriptObjectHandle){node->ScriptObjectID, node->ScriptObjectGeneration};
	}
	return handle;
}

static bool GetOttoNativeRelations(int playerNum, PangeaScriptPlayerRelations* output)
{
	if (playerNum != 0 || !output || !gPlayerInfo.objNode) return false;
	ObjNode* mounted = OttoScript_GetNativeCar();
	if (!mounted && (gPlayerInfo.objNode == gPlayerSaucer || gPlayerEnterSaucer)) mounted = gPlayerSaucer;
	*output = (PangeaScriptPlayerRelations){
		.carried = OttoNativeHandle(gTargetPickup),
		.mounted = OttoNativeHandle(mounted),
		.form = gPlayerInfo.objNode == gPlayerSaucer ? "saucer" : "robot",
		.capturedCount = gNumHumansInSaucer + gNumHumansInTransit,
	};
	return true;
}

static PangeaScriptStatus RunOttoNativeOperation(const PangeaScriptOperationRequest* request,
	PangeaScriptOperationResult* output)
{
	if (!request || !output || request->playerNum != 0 || !gPlayerInfo.objNode)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasPosition || request->hasVelocity || request->hasAim || request->hasJoint ||
		(request->mode && request->mode[0]) || (request->form && request->form[0]) ||
		(request->hasWeapon && request->operation != PANGEA_SCRIPT_OP_FIRE))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	bool needsTarget = request->operation == PANGEA_SCRIPT_OP_CARRY || request->operation == PANGEA_SCRIPT_OP_MOUNT ||
		request->operation == PANGEA_SCRIPT_OP_CAPTURE;
	if (request->hasTarget != needsTarget) return PANGEA_SCRIPT_BAD_ARGUMENT;
	*output = (PangeaScriptOperationResult){0};
	ObjNode* target = NULL;
	if (request->hasTarget)
	{
		void* native = NULL;
		if (!PangeaScript_GetObjectNativeObject(request->target, &native) || !native)
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		target = native;
		if (target->CType == INVALID_NODE_FLAG) return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	bool owned = PangeaScript_ObjectIsScriptControlled((PangeaScriptObjectHandle){
		gPlayerInfo.objNode->ScriptObjectID, gPlayerInfo.objNode->ScriptObjectGeneration});
	if (OttoScript_IsNativeDelegated(gPlayerInfo.objNode) && request->operation != PANGEA_SCRIPT_OP_DROP &&
		request->operation != PANGEA_SCRIPT_OP_RESET_CARRIED && request->operation != PANGEA_SCRIPT_OP_DISMOUNT)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	bool success = false;
	switch (request->operation)
	{
		case PANGEA_SCRIPT_OP_CARRY:
			success = OttoScript_CarryNativePickup(target);
			break;
		case PANGEA_SCRIPT_OP_DROP:
		case PANGEA_SCRIPT_OP_RESET_CARRIED:
			success = gPlayerInfo.objNode == gPlayerSaucer ? OttoScript_DropNativeHumans() : OttoScript_DropNativePickup();
			break;
		case PANGEA_SCRIPT_OP_MOUNT:
			success = OttoScript_MountNativeCar(target) || OttoScript_EnterNativeSaucer(target);
			break;
		case PANGEA_SCRIPT_OP_DISMOUNT:
			success = OttoScript_DismountNativeCar() || OttoScript_ExitNativeSaucer();
			break;
		case PANGEA_SCRIPT_OP_CAPTURE:
			success = OttoScript_CaptureNativeHuman(target);
			break;
		case PANGEA_SCRIPT_OP_DELIVER:
			success = OttoScript_DeliverNativeHumans();
			break;
		case PANGEA_SCRIPT_OP_FIRE:
		{
			int weapon = request->hasWeapon ? request->weapon : gPlayerInfo.currentWeaponType;
			if (weapon < 0 || weapon >= NUM_WEAPON_TYPES) break;
			if (request->hasWeapon && !SelectPlayerWeapon((short)weapon)) break;
			success = OttoScript_FireNativeWeapon();
			break;
		}
		case PANGEA_SCRIPT_OP_RESPAWN:
			if (!gPlayerIsDead || gPlayerInfo.objNode == gPlayerSaucer) break;
			ResetPlayerAtBestCheckpoint();
			success = !gPlayerIsDead;
			break;
		default: return PANGEA_SCRIPT_BAD_ARGUMENT;
	}
	if (success && owned && (request->operation == PANGEA_SCRIPT_OP_CARRY ||
		request->operation == PANGEA_SCRIPT_OP_DELIVER || request->operation == PANGEA_SCRIPT_OP_FIRE ||
		(request->operation == PANGEA_SCRIPT_OP_MOUNT && gPlayerEnterSaucer)))
	{
		gOttoDelegatedPlayer = (PangeaScriptObjectHandle){gPlayerInfo.objNode->ScriptObjectID,
			gPlayerInfo.objNode->ScriptObjectGeneration};
		gOttoDelegatedOperation = request->operation;
		gOttoDelegatedSeconds = request->operation == PANGEA_SCRIPT_OP_DELIVER ? 120.0f : 15.0f;
	}
	if (success && request->hasTarget && PangeaScript_ObjectExists(request->target)) output->object = request->target;
	if (success && (request->operation == PANGEA_SCRIPT_OP_DROP || request->operation == PANGEA_SCRIPT_OP_RESET_CARRIED ||
		request->operation == PANGEA_SCRIPT_OP_DISMOUNT)) gOttoDelegatedPlayer = (PangeaScriptObjectHandle){0};
	return success ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_BAD_ARGUMENT;
}

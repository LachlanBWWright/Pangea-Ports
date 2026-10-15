static PangeaScriptObjectHandle NativeHandle(ObjNode* object)
{
	return object ? (PangeaScriptObjectHandle){object->ScriptObjectID, object->ScriptObjectGeneration}
		: (PangeaScriptObjectHandle){0};
}

static bool GetScriptRelations(int playerNum, PangeaScriptPlayerRelations* output)
{
	if (playerNum != 0 || !gPlayerObj || !output) return false;
	*output = (PangeaScriptPlayerRelations){.carried = NativeHandle(gPlayerObj->CarriedObj), .form = "dinosaur"};
	return true;
}

static bool ScriptOperationOptionsValid(const PangeaScriptOperationRequest* request)
{
	if (request->hasAim || request->hasJoint || request->hasPosition || request->form || request->mode) return false;
	if (request->hasWeapon && request->operation != PANGEA_SCRIPT_OP_FIRE) return false;
	if (request->hasTarget && (request->operation == PANGEA_SCRIPT_OP_FIRE || request->operation == PANGEA_SCRIPT_OP_RESPAWN)) return false;
	if (!request->hasVelocity) return true;
	return request->operation == PANGEA_SCRIPT_OP_DROP && isfinite(request->velocity.x)
		&& isfinite(request->velocity.y) && isfinite(request->velocity.z);
}

static PangeaScriptStatus RunScriptOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output)
{
	if (!request || !output || request->playerNum != 0 || !gPlayerObj || !gPlayerObj->Skeleton)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!ScriptOperationOptionsValid(request)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	ObjNode* player = gPlayerObj;
	void* native = NULL;
	if (request->hasTarget && !PangeaScript_GetObjectNativeObject(request->target, &native)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	ObjNode* target = native;
	*output = (PangeaScriptOperationResult){0};
	if (gPlayerGotKilledFlag && request->operation != PANGEA_SCRIPT_OP_RESPAWN) return PANGEA_SCRIPT_BAD_ARGUMENT;
	switch (request->operation)
	{
		case PANGEA_SCRIPT_OP_CARRY:
			if (!target || !CarryEgg(player, target)) return PANGEA_SCRIPT_BAD_ARGUMENT;
			output->object = NativeHandle(target);
			return PANGEA_SCRIPT_OK;
		case PANGEA_SCRIPT_OP_DROP:
		case PANGEA_SCRIPT_OP_RESET_CARRIED:
		{
			ObjNode* egg = player->CarriedObj;
			if (!egg || (target && target != egg)) return PANGEA_SCRIPT_BAD_ARGUMENT;
			DropItem(player);
			if (request->hasVelocity) egg->Delta = (TQ3Vector3D){request->velocity.x, request->velocity.y, request->velocity.z};
			if (request->operation == PANGEA_SCRIPT_OP_RESET_CARRIED)
			{
				egg->Delta = (TQ3Vector3D){0};
				egg->Coord = (TQ3Point3D){egg->SpecialF[0], egg->SpecialF[1], egg->SpecialF[2]};
			}
			UpdateObjectTransforms(egg);
			CalcObjectBoxFromNode(egg);
			output->object = NativeHandle(egg);
			return PANGEA_SCRIPT_OK;
		}
		case PANGEA_SCRIPT_OP_DELIVER:
		{
			ObjNode* egg = player->CarriedObj;
			if (!target || !egg || !DeliverEgg(egg, target)) return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (PangeaScript_ObjectIsScriptControlled(NativeHandle(egg))) DeleteObject(egg);
			return PANGEA_SCRIPT_OK;
		}
		case PANGEA_SCRIPT_OP_FIRE:
			if (target) return PANGEA_SCRIPT_BAD_ARGUMENT;
			if (request->hasWeapon)
			{
				if (request->weapon < 0 || request->weapon >= NUM_ATTACK_MODES || gWeaponInventory[request->weapon] <= 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
				gCurrentAttackMode = request->weapon;
				gInfobarUpdateBits |= UPDATE_WEAPONICON;
			}
			return FirePlayerWeapon(player) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_BAD_ARGUMENT;
		case PANGEA_SCRIPT_OP_RESPAWN:
			if (target || !gPlayerGotKilledFlag || gNumLives <= 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
			ResetPlayer();
			output->object = NativeHandle(gPlayerObj);
			return PANGEA_SCRIPT_OK;
		case PANGEA_SCRIPT_OP_INTERACT:
			if (!target || target->Genre != EVENT_GENRE || target->Kind != PORTAL_TYPE_EXIT
				|| !(target->CType & CTYPE_PORTAL) || request->mode
				|| fabsf(player->Coord.x - target->Coord.x) > 60 || fabsf(player->Coord.z - target->Coord.z) > 60)
				return PANGEA_SCRIPT_BAD_ARGUMENT;
			DropItem(player);
			MorphToSkeletonAnim(player->Skeleton, PLAYER_ANIM_EXIT, 3);
			player->Flag[0] = false;
			player->ExitTimer = 0;
			BeginExitTransition(player);
			return PANGEA_SCRIPT_OK;
		default: return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
}

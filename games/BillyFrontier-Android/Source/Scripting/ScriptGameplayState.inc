static const char* BillyScript_AreaMode(int areaNum);

static const PangeaScriptStateField kBillyStateFields[] =
{
	{"area.time", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 86400, false},
	{"area.completed", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, false},
	{"player.dead", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, true},
	{"weapon.reloadPending", PANGEA_SCRIPT_STATE_BOOLEAN, true, 0, 1, false},
	{"duel.stage", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequenceMode", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequenceIndex", PANGEA_SCRIPT_STATE_NUMBER, false, 0, MAX_DUEL_KEY_SEQUENCE_LENGTH, false},
	{"duel.sequenceLength", PANGEA_SCRIPT_STATE_NUMBER, false, 0, MAX_DUEL_KEY_SEQUENCE_LENGTH, false},
	{"duel.nextKey", PANGEA_SCRIPT_STATE_NUMBER, false, -1, 255, false},
	{"duel.sequence.0", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequence.1", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequence.2", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequence.3", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequence.4", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequence.5", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequence.6", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.sequence.7", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"duel.reflex", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 100000, false},
	{"duel.delay", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, false},
	{"duel.timeLimit", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, false},
	{"duel.playerWins", PANGEA_SCRIPT_STATE_BOOLEAN, true, 0, 1, false},
	{"shootout.mode", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, false},
	{"shootout.stopPoint", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 19, false},
	{"shootout.enemyCount", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 100000, false},
	{"shootout.canAdvance", PANGEA_SCRIPT_STATE_BOOLEAN, true, 0, 1, false},
	{"targetPractice.timer", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, false},
	{"stampede.worldZ", PANGEA_SCRIPT_STATE_NUMBER, false, -1000000, 1000000, true},
	{"stampede.animation", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 255, true},
};

static bool GetDuelSequenceEntry(const char* key, double* output)
{
	for (int i = 0; i < gDuelKeySequenceLength && i < MAX_DUEL_KEY_SEQUENCE_LENGTH; i++)
	{
		char entryKey[32];
		snprintf(entryKey, sizeof(entryKey), "duel.sequence.%d", i);
		if (strcmp(key, entryKey) != 0) continue;
		*output = gDuelKeySequence[i];
		return true;
	}
	return false;
}

static bool GetBillyState(int playerNum, const char* key, PangeaScriptStateValue* output)
{
	if (playerNum != 0 || !key || !output) return false;
	*output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER};
	if (strcmp(key, "area.time") == 0) output->number = gScriptAreaTime;
	else if (strcmp(key, "area.completed") == 0) *output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_BOOLEAN, .boolean = gLevelCompleted};
	else if (strcmp(key, "player.dead") == 0) *output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_BOOLEAN, .boolean = gPlayerIsDead};
	else if (strcmp(key, "weapon.reloadPending") == 0 && GetBillyAreaMode() == BILLY_AREA_MODE_SHOOTOUT) *output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_BOOLEAN, .boolean = gNeedToReloadNextAmmoClip};
	else if (strcmp(BillyScript_AreaMode(gCurrentArea), "duel") == 0)
	{
		if (strcmp(key, "duel.stage") == 0) output->number = BillyScript_GetDuelStage();
		else if (strcmp(key, "duel.sequenceMode") == 0) output->number = gDuelKeySequenceMode;
		else if (strcmp(key, "duel.sequenceIndex") == 0) output->number = gDuelKeyBufferIndex;
		else if (strcmp(key, "duel.sequenceLength") == 0) output->number = gDuelKeySequenceLength;
		else if (strcmp(key, "duel.nextKey") == 0) output->number = gDuelKeyBufferIndex >= 0 && gDuelKeyBufferIndex < gDuelKeySequenceLength && gDuelKeyBufferIndex < MAX_DUEL_KEY_SEQUENCE_LENGTH ? gDuelKeySequence[gDuelKeyBufferIndex] : -1;
		else if (strcmp(key, "duel.reflex") == 0) output->number = gDuelReflex;
		else if (strcmp(key, "duel.delay") == 0) output->number = BillyScript_GetDuelDelay();
		else if (strcmp(key, "duel.timeLimit") == 0) output->number = BillyScript_GetDuelTimeLimit();
		else if (strcmp(key, "duel.playerWins") == 0) *output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_BOOLEAN, .boolean = BillyScript_GetDuelOutcome()};
		else return GetDuelSequenceEntry(key, &output->number);
	}
	else if (strcmp(BillyScript_AreaMode(gCurrentArea), "shootout") == 0)
	{
		if (strcmp(key, "shootout.mode") == 0) output->number = gShootoutMode;
		else if (strcmp(key, "shootout.stopPoint") == 0) output->number = gStopPointNum;
		else if (strcmp(key, "shootout.enemyCount") == 0 && gStopPointNum >= 0 && gStopPointNum < 20) output->number = gNumEnemiesThisStopPoint[gStopPointNum];
		else if (strcmp(key, "shootout.canAdvance") == 0) *output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_BOOLEAN, .boolean = gShootoutCanProceedToNextStopPoint};
		else return false;
	}
	else if (strcmp(key, "targetPractice.timer") == 0 && strcmp(BillyScript_AreaMode(gCurrentArea), "targetPractice") == 0) output->number = gTargetPracticeTimer;
	else if (strcmp(BillyScript_AreaMode(gCurrentArea), "stampede") == 0 && gPlayerInfo.objNode)
	{
		if (strcmp(key, "stampede.worldZ") == 0) output->number = gPlayerInfo.objNode->Coord.z;
		else if (strcmp(key, "stampede.animation") == 0 && gPlayerInfo.objNode->Skeleton) output->number = gPlayerInfo.objNode->Skeleton->AnimNum;
		else return false;
	}
	else return false;
	return true;
}

static PangeaScriptStatus SetBillyState(int playerNum, const char* key, const PangeaScriptStateValue* value)
{
	PangeaScriptStateValue current;
	if (!value || !GetBillyState(playerNum, key, &current) || value->type != current.type) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (value->type == PANGEA_SCRIPT_STATE_NUMBER && (!isfinite(value->number) || value->number < 0 || value->number > 3600)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (strcmp(key, "duel.delay") == 0) BillyScript_SetDuelDelay(value->number);
	else if (strcmp(key, "weapon.reloadPending") == 0) gNeedToReloadNextAmmoClip = value->boolean;
	else if (strcmp(key, "duel.timeLimit") == 0) BillyScript_SetDuelTimeLimit(value->number);
	else if (strcmp(key, "duel.playerWins") == 0) BillyScript_SetDuelOutcome(value->boolean);
	else if (strcmp(key, "shootout.canAdvance") == 0) gShootoutCanProceedToNextStopPoint = value->boolean;
	else if (strcmp(key, "targetPractice.timer") == 0) gTargetPracticeTimer = value->number;
	else return PANGEA_SCRIPT_BAD_ARGUMENT;
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus CompleteBillyArea(int playerNum, bool success)
{
	if (playerNum != 0 || !gScriptAreaActive || !gGameViewInfoPtr || gLevelCompleted) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gPlayerIsDead = !success;
	if (success) MarkLevelWon(gCurrentArea / 2);
	if (strcmp(BillyScript_AreaMode(gCurrentArea), "duel") == 0) BillyScript_SetDuelOutcome(success);
	StartLevelCompletion(0.05f);
	return PANGEA_SCRIPT_OK;
}

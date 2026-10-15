static const PangeaScriptStateField kScriptStateFields[] = {
	{"fuel", PANGEA_SCRIPT_STATE_NUMBER, true, 0, MAX_FUEL_CAPACITY, true},
	{"timeRemaining", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 86400, false},
	{"activeWeapon", PANGEA_SCRIPT_STATE_NUMBER, true, 0, NUM_ATTACK_MODES - 1, true},
	{"spawnX", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 2147483647, true},
	{"spawnZ", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 2147483647, true},
	{"spawnAim", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 7, true},
	{"eggCount1", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, true},
	{"eggCount2", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, true},
	{"eggCount3", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, true},
	{"eggCount4", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, true},
	{"eggCount5", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, true},
	{"eggRequired1", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, false},
	{"eggRequired2", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, false},
	{"eggRequired3", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, false},
	{"eggRequired4", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, false},
	{"eggRequired5", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32766, false},
};

static int FindScriptStateField(const char* key)
{
	if (!key) return -1;
	for (int i = 0; i < (int)(sizeof(kScriptStateFields) / sizeof(kScriptStateFields[0])); i++)
		if (!strcmp(key, kScriptStateFields[i].key)) return i;
	return -1;
}

static bool GetScriptState(int playerNum, const char* key, PangeaScriptStateValue* output)
{
	int field = FindScriptStateField(key);
	if (playerNum != 0 || !output || field < 0 || !gPlayerObj) return false;
	double number = 0;
	switch (field)
	{
		case 0: number = gFuel; break;
		case 1: number = gTimeRemaining; break;
		case 2: number = gCurrentAttackMode; break;
		case 3: number = gMyStartX; break;
		case 4: number = gMyStartZ; break;
		case 5: number = gMyStartAim; break;
		default: number = field < 11 ? gRecoveredEggs[field - 6] : gRequiredEggs[field - 11]; break;
	}
	*output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER, .number = number};
	return true;
}

static PangeaScriptStatus SetScriptState(int playerNum, const char* key, const PangeaScriptStateValue* value)
{
	int field = FindScriptStateField(key);
	if (field < 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (playerNum != 0 || !gPlayerObj || !value || value->type != PANGEA_SCRIPT_STATE_NUMBER || !isfinite(value->number)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	double number = value->number;
	const PangeaScriptStateField* descriptor = &kScriptStateFields[field];
	if (number < descriptor->min || number > descriptor->max || (field >= 2 && floor(number) != number)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if ((field == 3 && number >= gTerrainUnitWidth) || (field == 4 && number >= gTerrainUnitDepth)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	switch (field)
	{
		case 0: gFuel = (float)number; gInfobarUpdateBits |= UPDATE_FUEL; break;
		case 1: gTimeRemaining = (float)number; gInfobarUpdateBits |= UPDATE_IMPACTTIME; break;
		case 2: gCurrentAttackMode = (Byte)number; gInfobarUpdateBits |= UPDATE_WEAPONICON; break;
		case 3: gMyStartX = (long)number; break;
		case 4: gMyStartZ = (long)number; break;
		case 5: gMyStartAim = (Byte)number; break;
		default:
			if (field < 11) gRecoveredEggs[field - 6] = (short)number;
			else gRequiredEggs[field - 11] = (short)number;
			gInfobarUpdateBits |= UPDATE_EGGS;
			break;
	}
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus CompleteScriptMission(int playerNum, bool success)
{
	if (playerNum != 0 || !gPlayerObj) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gWonGameFlag = success;
	gGameOverFlag = true;
	return PANGEA_SCRIPT_OK;
}

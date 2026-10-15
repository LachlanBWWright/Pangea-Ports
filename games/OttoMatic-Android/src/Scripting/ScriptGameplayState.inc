static const PangeaScriptStateField kOttoStateFields[] =
{
	{"fuel", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 1, true},
	{"jumpJet", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 1, true},
	{"activeWeapon", PANGEA_SCRIPT_STATE_NUMBER, true, 0, NUM_WEAPON_TYPES - 1, true},
	{"checkpoint", PANGEA_SCRIPT_STATE_NUMBER, true, -1, 255, false},
	{"humansInLevel", PANGEA_SCRIPT_STATE_NUMBER, false, 0, INT_MAX, false},
	{"humansInTransit", PANGEA_SCRIPT_STATE_NUMBER, false, 0, INT_MAX, false},
	{"humansInSaucer", PANGEA_SCRIPT_STATE_NUMBER, false, 0, INT_MAX, false},
	{"humansRescued", PANGEA_SCRIPT_STATE_NUMBER, false, 0, INT_MAX, false},
	{"farmersRescued", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 16383, false},
	{"beeWomenRescued", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 16383, false},
	{"scientistsRescued", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 16383, false},
	{"skirtLadiesRescued", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 16383, false},
};

static int OttoStateIndex(const char* key)
{
	if (!key) return -1;
	for (size_t i = 0; i < sizeof(kOttoStateFields) / sizeof(kOttoStateFields[0]); i++)
		if (strcmp(key, kOttoStateFields[i].key) == 0) return (int)i;
	return -1;
}

static bool GetOttoGameplayState(int playerNum, const char* key, PangeaScriptStateValue* output)
{
	if (playerNum != 0 || !output || !gPlayerInfo.objNode) return false;
	int field = OttoStateIndex(key);
	if (field < 0) return false;
	*output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER};
	switch (field)
	{
		case 0: output->number = gPlayerInfo.fuel; break;
		case 1: output->number = gPlayerInfo.jumpJet; break;
		case 2: output->number = gPlayerInfo.currentWeaponType; break;
		case 3: output->number = gBestCheckpointNum; break;
		case 4: output->number = gNumHumansInLevel; break;
		case 5: output->number = gNumHumansInTransit; break;
		case 6: output->number = gNumHumansInSaucer; break;
		case 7: output->number = gNumHumansRescuedTotal; break;
		default: output->number = gNumHumansRescuedOfType[field - 8]; break;
	}
	return true;
}

static bool SetOttoCheckpoint(int index)
{
	if (index < 0 || !gMasterItemList || !*gMasterItemList) return false;
	for (int i = 0; i < gNumTerrainItems; i++)
	{
		const TerrainItemEntryType* item = &(*gMasterItemList)[i];
		if (item->type != MAP_ITEM_CHECKPOINT || item->parm[0] != index) continue;
		gBestCheckpointNum = (short)index;
		gBestCheckpointCoord = (OGLPoint2D){item->x, item->y};
		gBestCheckpointAim = item->parm[1] * (PI2 / 8.0f);
		return true;
	}
	return false;
}

static PangeaScriptStatus SetOttoGameplayState(int playerNum, const char* key, const PangeaScriptStateValue* value)
{
	int field = OttoStateIndex(key);
	if (playerNum != 0 || !gPlayerInfo.objNode || !value || field < 0 ||
		value->type != PANGEA_SCRIPT_STATE_NUMBER || !isfinite(value->number))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	const PangeaScriptStateField* descriptor = &kOttoStateFields[field];
	if (!descriptor->writable) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (value->number < descriptor->min || value->number > descriptor->max ||
		(field >= 2 && floor(value->number) != value->number)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	switch (field)
	{
		case 0: gPlayerInfo.fuel = (float)value->number; break;
		case 1: gPlayerInfo.jumpJet = (float)value->number; break;
		case 2:
			return SelectPlayerWeapon((short)value->number) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_BAD_ARGUMENT;
		case 3:
			return SetOttoCheckpoint((int)value->number) ? PANGEA_SCRIPT_OK : PANGEA_SCRIPT_BAD_ARGUMENT;
		default:
			gNumHumansRescuedTotal += (int)value->number - gNumHumansRescuedOfType[field - 8];
			gNumHumansRescuedOfType[field - 8] = (int)value->number;
			break;
	}
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus CompleteOttoGameplay(int playerNum, bool success)
{
	if (playerNum != 0 || !gPlayerInfo.objNode || gGameOver || gLevelCompleted)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (success)
	{
		gLevelCompleted = true;
		gLevelCompletedCoolDownTimer = 0;
	}
	else gGameOver = true;
	return PANGEA_SCRIPT_OK;
}

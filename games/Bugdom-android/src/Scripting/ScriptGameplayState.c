#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"
#include <math.h>
#include <string.h>

const PangeaScriptStateField kBugdomStateFields[] = {
	{"ballTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 1, true},
	{"shieldTime", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 3600, true},
	{"money", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, true},
	{"greenClovers", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, true},
	{"blueClovers", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, true},
	{"goldClovers", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, true},
	{"rescuedLadybugs", PANGEA_SCRIPT_STATE_NUMBER, true, 0, 32767, false},
	{"requiredLadybugs", PANGEA_SCRIPT_STATE_NUMBER, false, 0, 32767, false},
	{"bestCheckpoint", PANGEA_SCRIPT_STATE_NUMBER, false, -1, 32767, false},
	{"checkpointX", PANGEA_SCRIPT_STATE_NUMBER, false, -1e9, 1e9, false},
	{"checkpointY", PANGEA_SCRIPT_STATE_NUMBER, false, -1e9, 1e9, false},
	{"checkpointZ", PANGEA_SCRIPT_STATE_NUMBER, false, -1e9, 1e9, false},
	{"checkpointRotation", PANGEA_SCRIPT_STATE_NUMBER, false, -1e9, 1e9, false},
	{"completed", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, false},
	{"gameOver", PANGEA_SCRIPT_STATE_BOOLEAN, false, 0, 1, false},
};

const int kBugdomStateFieldCount = sizeof(kBugdomStateFields) / sizeof(kBugdomStateFields[0]);

bool BugdomScript_CompleteNativeObjective(void)
{
	if (!BugdomScript_NativeRulesEnabled())
		return false;
	gAreaCompleted = true;
	return true;
}

bool BugdomScript_GetState(int player, const char* key, PangeaScriptStateValue* output)
{
	if (player != 0 || !key || !output)
		return false;
	*output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER};
	if (!strcmp(key, "ballTime")) output->number = gBallTimer;
	else if (!strcmp(key, "shieldTime")) output->number = gShieldTimer;
	else if (!strcmp(key, "money")) output->number = gMoney;
	else if (!strcmp(key, "greenClovers")) output->number = gNumGreenClovers;
	else if (!strcmp(key, "blueClovers")) output->number = gNumBlueClovers;
	else if (!strcmp(key, "goldClovers")) output->number = gNumGoldClovers;
	else if (!strcmp(key, "rescuedLadybugs")) output->number = gNumLadyBugsThisArea;
	else if (!strcmp(key, "requiredLadybugs")) output->number = gNumLadyBugsOnThisLevel;
	else if (!strcmp(key, "bestCheckpoint")) output->number = gBestCheckPoint;
	else if (!strcmp(key, "checkpointX")) output->number = gMostRecentCheckPointCoord.x;
	else if (!strcmp(key, "checkpointY")) output->number = gMostRecentCheckPointCoord.y;
	else if (!strcmp(key, "checkpointZ")) output->number = gMostRecentCheckPointCoord.z;
	else if (!strcmp(key, "checkpointRotation")) output->number = gCheckPointRot;
	else if (!strcmp(key, "completed") || !strcmp(key, "gameOver"))
	{
		output->type = PANGEA_SCRIPT_STATE_BOOLEAN;
		output->boolean = !strcmp(key, "completed") ? gAreaCompleted : gGameOverFlag;
	}
	else return false;
	return true;
}

PangeaScriptStatus BugdomScript_SetState(int player, const char* key, const PangeaScriptStateValue* value)
{
	if (player != 0 || !key || !value || value->type != PANGEA_SCRIPT_STATE_NUMBER || !isfinite(value->number))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	const PangeaScriptStateField* field = NULL;
	for (int i = 0; i < kBugdomStateFieldCount; i++)
		if (!strcmp(key, kBugdomStateFields[i].key)) field = &kBugdomStateFields[i];
	if (!field || !field->writable || value->number < field->min || value->number > field->max)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	double number = value->number;
	if (!strcmp(key, "ballTime"))
	{
		gBallTimer = number;
		gInfobarUpdateBits |= UPDATE_TIMER;
		return PANGEA_SCRIPT_OK;
	}
	if (!strcmp(key, "shieldTime"))
	{
		gShieldTimer = number;
		return PANGEA_SCRIPT_OK;
	}
	if (floor(number) != number)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (!strcmp(key, "money")) BugdomScript_SetMoney(number);
	else if (!strcmp(key, "greenClovers")) gNumGreenClovers = number;
	else if (!strcmp(key, "blueClovers"))
	{
		gNumBlueClovers = number;
		gInfobarUpdateBits |= UPDATE_BLUECLOVER;
	}
	else if (!strcmp(key, "goldClovers"))
	{
		gNumGoldClovers = number;
		gInfobarUpdateBits |= UPDATE_GOLDCLOVER;
	}
	else if (!strcmp(key, "rescuedLadybugs"))
	{
		if (number > gNumLadyBugsOnThisLevel)
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		gNumLadyBugsThisArea = number;
		gInfobarUpdateBits |= UPDATE_LADYBUGS;
	}
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus BugdomScript_Complete(int player, bool success)
{
	if (player != 0 || !gPlayerObj || gAreaCompleted || gGameOverFlag)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (success) gAreaCompleted = true;
	else gGameOverFlag = true;
	return PANGEA_SCRIPT_OK;
}

#endif

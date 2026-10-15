#include "game.h"
#include <assert.h>

Boolean gGameOverFlag;
float gFramesPerSecondFrac = 1.0f;
static bool gRulesControlled;
bool PangeaScript_RulesAreScriptControlled(void) { return gRulesControlled; }
short PlayEffect_Parms(short effect, unsigned char volume, unsigned long rate)
{
	(void)effect; (void)volume; (void)rate;
	return -1;
}

int main(void)
{
	InitMyInventory();
	for (int i = 0; i < NUM_EGG_SPECIES; i++)
	{
		assert(gRecoveredEggs[i] == 0 && gRequiredEggs[i] == 1);
		gRecoveredEggs[i] = 32766;
		gRequiredEggs[i] = 3;
	}
	InitMyInventory();
	for (int i = 0; i < NUM_EGG_SPECIES; i++)
		assert(gRecoveredEggs[i] == 0 && gRequiredEggs[i] == 1);
	assert(gTimeRemaining == LEVEL_DURATION);
	gTimeRemaining = 5.0f;
	DecAsteroidTimer();
	assert(gTimeRemaining == 4.0f && !gGameOverFlag);
	gRulesControlled = true;
	DecAsteroidTimer();
	assert(gTimeRemaining == 4.0f && !gGameOverFlag);
	gTimeRemaining = 0.0f;
	DecAsteroidTimer();
	assert(!gGameOverFlag);
	gRulesControlled = false;
	DecAsteroidTimer();
	assert(gGameOverFlag);
	return 0;
}

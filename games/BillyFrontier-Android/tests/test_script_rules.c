#include "game.h"
#include "ScriptRules.h"
#include <assert.h>

static bool rulesOwned;
static int completionCount;
static int victoryCount;
uint32_t gScore;
int gStopPointNum;
int gCurrentArea;
Byte gNumEnemiesThisStopPoint[20];
Boolean gShootoutCanProceedToNextStopPoint;

bool PangeaScript_RulesAreScriptControlled(void) { return rulesOwned; }
void StartLevelCompletion(float delay) { assert(delay > 0); completionCount++; }
void MarkLevelWon(int level) { assert(level == gCurrentArea / 2); victoryCount++; }

int main(void)
{
	gCurrentArea = AREA_SWAMP_SHOOTOUT;
	gStopPointNum = 7;
	gNumEnemiesThisStopPoint[7] = 1;
	assert(BillyScript_NativeRulesAllowed());
	DecEnemiesAtStopPoint();
	assert(completionCount == 1 && victoryCount == 1 && gScore > 0);
	uint32_t score = gScore;
	rulesOwned = true;
	gNumEnemiesThisStopPoint[7] = 1;
	assert(!BillyScript_NativeRulesAllowed());
	DecEnemiesAtStopPoint();
	assert(gNumEnemiesThisStopPoint[7] == 0);
	assert(completionCount == 1 && victoryCount == 1 && gScore == score);
	gStopPointNum = 0;
	gNumEnemiesThisStopPoint[0] = 1;
	DecEnemiesAtStopPoint();
	assert(!gShootoutCanProceedToNextStopPoint);
	rulesOwned = false;
	gNumEnemiesThisStopPoint[0] = 1;
	DecEnemiesAtStopPoint();
	assert(gShootoutCanProceedToNextStopPoint);
	DecEnemiesAtStopPoint();
	assert(gNumEnemiesThisStopPoint[0] == 0);
	return 0;
}

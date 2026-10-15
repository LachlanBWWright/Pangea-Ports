#include "myglobals.h"
#include "bonus.h"
#include "ScriptRulePolicy.h"
#include <assert.h>

extern short gNumBunnies;
extern short gShieldTimer;
extern ObjNode* gShieldNodePtr;
ObjNode* gMyNodePtr;
ObjNode* gThisNodePtr;
static bool controlled;
static int liftoffs;
static int deletions;
static int warnings;

#ifdef PANGEA_ENABLE_SCRIPTING
bool PangeaScript_RulesAreScriptControlled(void) { return controlled; }
#endif
void DisposeFrog(void) {}
void DisposeSpaceShip(void) {}
void StartMyLiftoff(void) { liftoffs++; }
void ShowNumBunnies(void) {}
void DeleteObject(ObjNode* object) { assert(object == gShieldNodePtr); deletions++; }
void SwitchAnim(ObjNode* object, short animation) { assert(object == gShieldNodePtr && animation == 1); warnings++; }

int main(void)
{
	ObjNode player = {0};
	ObjNode shield = {0};
	player.X.Int = 10;
	player.Y.Int = 20;
	gMyNodePtr = &player;
	gThisNodePtr = gShieldNodePtr = &shield;
	gNumBunnies = 1;
	DecBunnyCount();
	assert(gNumBunnies == 0 && liftoffs == 1);
	gShieldTimer = 1;
	MoveShield();
	assert(gShieldTimer == 0 && deletions == 1);
	assert(shield.X.Int == 10 && shield.Y.Int == 20);
	controlled = true;
	gNumBunnies = 1;
	gShieldTimer = 1;
	player.X.Int = 30;
	DecBunnyCount();
	MoveShield();
#ifdef PANGEA_ENABLE_SCRIPTING
	assert(gNumBunnies == 1 && liftoffs == 1);
	assert(gShieldTimer == 1 && deletions == 1);
#else
	assert(gNumBunnies == 0 && liftoffs == 2);
	assert(gShieldTimer == 0 && deletions == 2);
#endif
	assert(shield.X.Int == 30);
	controlled = false;
	gShieldTimer = GAME_FPS * 2 + 1;
	MoveShield();
	assert(warnings == 1);
	return 0;
}

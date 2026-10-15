#include "ScriptMechanics.h"
#include "ScriptBindings.h"
#include "myglobals.h"
#include "externs.h"
#include "myguy.h"
#include "myguy2.h"
#include "object.h"
#include "shape.h"
#include "weapon.h"
#include <assert.h>
#include <math.h>

ObjNode* gMyNodePtr;
ObjNode* gMostRecentShape;
ObjNode* gThisNodePtr;
Ptr gSHAPE_HEADER_Ptrs[MAX_SHAPE_GROUPS][MAX_SHAPES_IN_FILE];
Boolean gFinishedArea, gTeleportingFlag;
Byte gSceneNum, gMyMode, gCurrentWeaponType, gCurrentWeaponIndex, gNumWeaponsIHave;
short gMyHealth, gMyMaxHealth, gMyDirection, gLastNonDeathX, gLastNonDeathY;
short gEnemyFreezeTimer, gSoundNum_Frog, gSoundNum_Ship, gSoundNum_ExitShip;
long gMyX, gMyY, gMyDX, gMyDY, gMySumDX, gMySumDY;
long gMyNormalMaxSpeed, gMyMaxSpeedX, gMyMaxSpeedY, gMyAcceleration, gMyWindDX, gMyWindDY;
long gDX, gDY;
short gPlayfieldWidth = 1000, gPlayfieldHeight = 1000;
long gActiveAreaShapeGroup = GROUP_AREA_SPECIFIC, gActiveAreaShapeGroup2 = GROUP_AREA_SPECIFIC2;
WeaponType gMyWeapons[MAX_WEAPONS];
static ObjNode player, frog, ship, projectile, poof;
static bool controlled;
static bool allocationWorks = true;
static int shots, revives, boxes;
static int registrations, deletions, stops;
int LevelMetadataScene(const char* key, int fallback) { (void)key; return fallback; }

bool PangeaScript_ObjectIsScriptControlled(PangeaScriptObjectHandle handle) { (void)handle; return controlled; }
bool PangeaScript_SetObjectScriptControlled(PangeaScriptObjectHandle handle, bool value)
{ assert(handle.id == (int)gMyNodePtr->ScriptObjectID); controlled = value; return true; }
bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** output)
{ if (handle.id != 3 || handle.generation != 1) return false; *output = &ship; return true; }
ObjNode* MakeNewShape(long group, long type, long animation, short x, short y, short z, void (*move)(void), Boolean relative)
{
	(void)animation; (void)move; (void)relative;
	ObjNode* object = group == GroupNum_MeFrog ? (type == ObjType_MeFrog ? &frog : &poof) : &ship;
	object->X.Int = x; object->Y.Int = y; object->Z = z;
	return allocationWorks ? object : NULL;
}
void MikeScript_UnregisterPlayerObject(ObjNode* object) { (void)object; controlled = false; }
void MikeScript_RegisterPlayerObject(ObjNode* object) { assert(object == gMyNodePtr); registrations++; }
void MikeScript_RegisterObject(ObjNode* object, const char* id, const char* category) { (void)object; (void)id; (void)category; }
void DeleteObject(ObjNode* object) { assert(object == &frog || object == &ship); deletions++; }
short PlaySound(short sound) { (void)sound; return 0; }
void StopAChannel(short channel) { assert(channel == 0); stops++; }
void GetObjectInfo(void) {}
void DoMyScreenScroll(void) {}
void HandleMyCenter(void) {}
void DoMyMove(void) {}
Boolean CalcMyAim(void) { return false; }
Boolean DoMyCollisionDetect(void) { return false; }
void UpdateMe(void) {}
void MoveMyController(void) {}
Boolean GetNeedState(int need) { (void)need; return false; }
void SwitchAnim(ObjNode* object, short animation) { (void)object; (void)animation; }
void SetMyStandAnim(void) {}
void CalcObjectBox2(ObjNode* object) { assert(object == gMyNodePtr); boxes++; }
Boolean MikeWeapon_FireFromScript(void) { shots++; gMostRecentShape = &projectile; return true; }
void ReviveMe(void) { revives++; gMyNodePtr->X.Int = gLastNonDeathX; gMyNodePtr->Y.Int = gLastNonDeathY; gMyHealth = gMyMaxHealth; }

int main(void)
{
	player.ScriptObjectID = 1; player.ScriptObjectGeneration = 1;
	frog.ScriptObjectID = 2; frog.ScriptObjectGeneration = 1;
	ship.ScriptObjectID = 3; ship.ScriptObjectGeneration = 1;
	projectile.ScriptObjectID = 4; projectile.ScriptObjectGeneration = 1;
	gMyNodePtr = &player;
	gMyHealth = gMyMaxHealth = 5;
	gNumWeaponsIHave = 1;
	gMyWeapons[0] = (WeaponType){WEAPON_TYPE_SUCTIONCUP, 3};
	gSHAPE_HEADER_Ptrs[GROUP_WEAPONS][ObjType_SuctionCup] = "loaded";
	PangeaScriptOperationResult result;
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_FIRE, .hasAim = true, .aim = {1, 0, 0}};
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(shots == 1 && gMyDirection == 2 && result.object.id == 4);
	request.hasPosition = true;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && shots == 1);
	request.hasPosition = false;
	request.aim.x = NAN;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && shots == 1);
	request.aim = (PangeaScriptVector3){0};
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && shots == 1);
	request.hasAim = false; request.hasWeapon = true; request.weapon = WEAPON_TYPE_CAKE;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM && shots == 1);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_TRANSFORM, .form = "frog"};
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	gSceneNum = SCENE_FAIRY;
	gSHAPE_HEADER_Ptrs[gActiveAreaShapeGroup2][ObjType_MeFrog] = "loaded";
	gSHAPE_HEADER_Ptrs[gActiveAreaShapeGroup2][ObjType_FrogPoof] = "loaded";
	controlled = true;
	allocationWorks = false;
	player.DrawFlag = player.MoveFlag = true;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(gMyNodePtr == &player && player.DrawFlag && player.MoveFlag && controlled && registrations == 0);
	allocationWorks = true;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK && controlled && gFrogFlag && result.object.id == 2);
	frog.X.Int = 40; frog.Y.Int = 50;
	PangeaScriptPlayerRelations relations;
	assert(MikeScript_GetRelations(0, &relations) && relations.form[0] == 'f');
	request.form = "ship";
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM && gFrogFlag);
	request.operation = PANGEA_SCRIPT_OP_DISMOUNT;
	request.form = NULL;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK && controlled && gMyNodePtr == &player);
	assert(player.X.Int == 40 && player.Y.Int == 50 && deletions == 1);
	gSceneNum = SCENE_BARGAIN;
	gSHAPE_HEADER_Ptrs[gActiveAreaShapeGroup][ObjType_SpaceShip] = "loaded";
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_MOUNT, .hasTarget = true, .target = {3, 1}};
	ship.Type = ObjType_SpaceShip; ship.SpriteGroupNum = gActiveAreaShapeGroup;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK && controlled && gSpaceShipFlag);
	assert(MikeScript_GetRelations(0, &relations) && relations.mounted.id == 3);
	request.target.id = 4;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && gSpaceShipFlag);
	request.target.id = 3;
	request.form = "frog";
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && gSpaceShipFlag);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_RESPAWN, .hasPosition = true, .position = {12.5f, 25, 0}};
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_OK && revives == 1);
	assert(gMyNodePtr == &player && controlled && !gSpaceShipFlag && gMyX == 12 && gMyY == 25);
	assert(deletions == 2 && stops == 1 && registrations == 4);
	request.position.x = 1001;
	assert(MikeScript_RunOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT && revives == 1);
	assert(boxes == 4);
	return 0;
}

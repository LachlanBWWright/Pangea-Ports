#ifdef PANGEA_ENABLE_SCRIPTING
#include "ScriptMechanics.h"
#include "ScriptBindings.h"
#include "ScriptGeometry.h"
#include "myglobals.h"
#include "externs.h"
#include "myguy.h"
#include "myguy2.h"
#include "object.h"
#include "shape.h"
#include "weapon.h"
#include <math.h>
#include <string.h>

static PangeaScriptObjectHandle PlayerHandle(void)
{
	return (PangeaScriptObjectHandle){gMyNodePtr->ScriptObjectID, gMyNodePtr->ScriptObjectGeneration};
}

static bool ShapeAvailable(int group, int type)
{
#if PANGEA_SAFE_ITEM_LOADING
	if (group == GROUP_AREA_SPECIFIC) group = gActiveAreaShapeGroup;
	else if (group == GROUP_AREA_SPECIFIC2) group = gActiveAreaShapeGroup2;
#endif
	return group >= 0 && group < MAX_SHAPE_GROUPS && type >= 0 && type < MAX_SHAPES_IN_FILE
		&& gSHAPE_HEADER_Ptrs[group][type];
}

static ObjNode* ResolveShip(const PangeaScriptOperationRequest* request)
{
	void* native = NULL;
	if (!request->hasTarget || !PangeaScript_GetObjectNativeObject(request->target, &native)) return NULL;
	ObjNode* ship = native;
	int group = GroupNum_SpaceShip;
#if PANGEA_SAFE_ITEM_LOADING
	group = gActiveAreaShapeGroup;
#endif
	return ship && ship != gMyNodePtr && ship->CType != INVALID_NODE_FLAG
		&& ship->SpriteGroupNum == group && ship->Type == ObjType_SpaceShip ? ship : NULL;
}

static PangeaScriptStatus Transform(const PangeaScriptOperationRequest* request)
{
	const char* form = request->form;
	if (request->operation == PANGEA_SCRIPT_OP_MOUNT) form = "ship";
	if (request->operation == PANGEA_SCRIPT_OP_DISMOUNT) form = "mike";
	if (!form) return PANGEA_SCRIPT_BAD_ARGUMENT;
	bool normal = !strcmp(form, "mike");
	bool frog = !strcmp(form, "frog");
	bool ship = !strcmp(form, "ship");
	if (!normal && !frog && !ship) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (frog && gFrogFlag) return PANGEA_SCRIPT_OK;
	if (ship && gSpaceShipFlag)
	{
		PangeaScriptObjectHandle current = PlayerHandle();
		if (request->hasTarget && (request->target.id != current.id || request->target.generation != current.generation))
			return PANGEA_SCRIPT_BAD_ARGUMENT;
		return PANGEA_SCRIPT_OK;
	}
	if (!normal && (gFrogFlag || gSpaceShipFlag)) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (gMyHealth < 0 || gFinishedArea) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	ObjNode* shipObject = NULL;
	if (frog && (LevelMetadataScene("area.character", gSceneNum) != SCENE_FAIRY || !ShapeAvailable(GroupNum_MeFrog, ObjType_MeFrog)
		|| !ShapeAvailable(GroupNum_FrogPoof, ObjType_FrogPoof)))
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (ship)
	{
		if (LevelMetadataScene("area.character", gSceneNum) != SCENE_BARGAIN || !ShapeAvailable(GroupNum_SpaceShip, ObjType_SpaceShip))
			return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		shipObject = ResolveShip(request);
		if (request->hasTarget && !shipObject) return PANGEA_SCRIPT_BAD_ARGUMENT;
		if (!shipObject)
		{
			shipObject = MakeNewShape(GroupNum_SpaceShip, ObjType_SpaceShip, 0,
				gMyNodePtr->X.Int, gMyNodePtr->Y.Int, gMyNodePtr->Z, NULL, PLAYFIELD_RELATIVE);
			if (shipObject)
			{
				shipObject->CBits = CBITS_TOUCHABLE;
				shipObject->TopOff = -40; shipObject->BottomOff = 0;
				shipObject->LeftOff = -20; shipObject->RightOff = 20;
			}
		}
		if (!shipObject) return PANGEA_SCRIPT_RUNTIME_ERROR;
	}
	bool controlled = PangeaScript_ObjectIsScriptControlled(PlayerHandle());
	if (frog && !TryTurnMeIntoFrog()) return PANGEA_SCRIPT_RUNTIME_ERROR;
	if (ship) { TurnMeIntoShip(shipObject); gMyMode = MY_MODE_SPACESHIP; }
	if (frog) gMyMode = MY_MODE_FROG;
	if (normal) { DisposeFrog(); DisposeSpaceShip(); gMyMode = MY_MODE_BASICSTAND; SetMyStandAnim(); }
	PangeaScript_SetObjectScriptControlled(PlayerHandle(), controlled);
	gMyX = gMyNodePtr->X.Int;
	gMyY = gMyNodePtr->Y.Int;
	CalcObjectBox2(gMyNodePtr);
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus Fire(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output)
{
	static const int types[NUM_WEAPON_TYPES] = {ObjType_SuctionCup, ObjType_Cake, ObjType_Oozie,
		ObjType_RBand, ObjType_Toothpaste, ObjType_Tracer, ObjType_PixieDust, ObjType_Rock,
		ObjType_HeatSeek, ObjType_ElephantGun, ObjType_Pie, ObjType_DoubleShot, ObjType_TripleShot,
		ObjType_Flamethrower, ObjType_RocketGun};
	int weapon = request->hasWeapon ? request->weapon : gCurrentWeaponType;
	if (weapon < 0 || weapon >= NUM_WEAPON_TYPES) return PANGEA_SCRIPT_BAD_ARGUMENT;
	int index = 0;
	while (index < gNumWeaponsIHave && gMyWeapons[index].type != weapon) index++;
	if (index == gNumWeaponsIHave || !gMyWeapons[index].life || !ShapeAvailable(GROUP_WEAPONS, types[weapon])
		|| gFrogFlag || gSpaceShipFlag || gMyHealth < 0 || gFinishedArea) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (!request->hasAim && (gMyDirection < AIM_UP || gMyDirection > AIM_UP_LEFT)) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasAim)
	{
		if (!isfinite(request->aim.x) || !isfinite(request->aim.y) || request->aim.z != 0
			|| (request->aim.x == 0 && request->aim.y == 0)) return PANGEA_SCRIPT_BAD_ARGUMENT;
		float angle = atan2f(request->aim.x, -request->aim.y);
		gMyDirection = ((int)roundf(angle / 0.7853981634f) + 8) % 8;
	}
	gCurrentWeaponIndex = index;
	gCurrentWeaponType = weapon;
	gMyX = gMyNodePtr->X.Int;
	gMyY = gMyNodePtr->Y.Int;
	ObjNode* previous = gMostRecentShape;
	if (!MikeWeapon_FireFromScript()) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (gMostRecentShape && gMostRecentShape != previous && gMostRecentShape->ScriptObjectID)
		output->object = (PangeaScriptObjectHandle){gMostRecentShape->ScriptObjectID, gMostRecentShape->ScriptObjectGeneration};
	return PANGEA_SCRIPT_OK;
}

static PangeaScriptStatus Respawn(const PangeaScriptOperationRequest* request)
{
	if (gFinishedArea) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	int32_t x = 0, y = 0;
	if (request->hasPosition && (!MikeScript_ToFixed(request->position.x, &x)
		|| !MikeScript_ToFixed(request->position.y, &y) || request->position.z != 0
		|| request->position.x < 0 || request->position.y < 0
		|| request->position.x >= gPlayfieldWidth || request->position.y >= gPlayfieldHeight))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	bool controlled = PangeaScript_ObjectIsScriptControlled(PlayerHandle());
	DisposeFrog();
	DisposeSpaceShip();
	PangeaScript_SetObjectScriptControlled(PlayerHandle(), controlled);
	if (request->hasPosition) { gLastNonDeathX = (short)(x >> 16); gLastNonDeathY = (short)(y >> 16); }
	gMyNodePtr->DX = gMyNodePtr->DY = 0;
	gMyMode = MY_MODE_BASICSTAND;
	ReviveMe();
	gMyX = gMyNodePtr->X.Int;
	gMyY = gMyNodePtr->Y.Int;
	gMyDX = gMyDY = gMySumDX = gMySumDY = 0;
	CalcObjectBox2(gMyNodePtr);
	return PANGEA_SCRIPT_OK;
}

PangeaScriptStatus MikeScript_RunOperation(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output)
{
	if (!request || !output || request->playerNum != 0 || !gMyNodePtr || gMyNodePtr->CType == INVALID_NODE_FLAG)
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	*output = (PangeaScriptOperationResult){0};
	if (request->hasJoint || request->mode || request->hasVelocity) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->form && request->operation != PANGEA_SCRIPT_OP_TRANSFORM) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasAim && request->operation != PANGEA_SCRIPT_OP_FIRE) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasWeapon && request->operation != PANGEA_SCRIPT_OP_FIRE) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasPosition && request->operation != PANGEA_SCRIPT_OP_RESPAWN) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->hasTarget && request->operation != PANGEA_SCRIPT_OP_MOUNT
		&& !(request->operation == PANGEA_SCRIPT_OP_TRANSFORM && request->form && !strcmp(request->form, "ship")))
		return PANGEA_SCRIPT_BAD_ARGUMENT;
	PangeaScriptStatus status;
	switch (request->operation)
	{
		case PANGEA_SCRIPT_OP_TRANSFORM:
		case PANGEA_SCRIPT_OP_MOUNT:
		case PANGEA_SCRIPT_OP_DISMOUNT: status = Transform(request); break;
		case PANGEA_SCRIPT_OP_FIRE: return Fire(request, output);
		case PANGEA_SCRIPT_OP_RESPAWN: status = Respawn(request); break;
		default: return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	}
	if (status == PANGEA_SCRIPT_OK) output->object = PlayerHandle();
	return status;
}

bool MikeScript_GetRelations(int playerNum, PangeaScriptPlayerRelations* output)
{
	if (playerNum != 0 || !gMyNodePtr || !output) return false;
	*output = (PangeaScriptPlayerRelations){.form = gSpaceShipFlag ? "ship" : gFrogFlag ? "frog" : "mike"};
	if (gSpaceShipFlag) output->mounted = PlayerHandle();
	return true;
}
#endif

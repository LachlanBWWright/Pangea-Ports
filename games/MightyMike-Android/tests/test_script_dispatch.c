#include "myglobals.h"
#include "object.h"
#include "pangea_script.h"
#include "ScriptGeometry.h"

#include <assert.h>

extern ObjNode* FirstNodePtr;
extern ObjNode* FreeNodeStack[MAX_OBJECTS];
extern long NodeStackFront;
extern long NumObjects;
static int nativeCalls;
static int scriptCalls;
static int animationCalls;
static bool controlled;
static bool deleteInNative;
static bool deleteInScript;
static bool replaceInNative;
static bool replaceInScript;

static void ReplaceActor(void)
{
	ObjNode* old = FirstNodePtr;
	FirstNodePtr = NULL;
	NumObjects = NodeStackFront = 0;
	FreeNodeStack[0] = old;
	ObjNode* replacement = MakeNewObject(SPRITE_GENRE, 0, 0, 0, NULL);
	assert(replacement == old);
	replacement->AnimFlag = true;
	replacement->ScriptObjectID = 2;
	replacement->ScriptObjectGeneration = 1;
}

static void MoveNative(void)
{
	nativeCalls++;
	if (deleteInNative) FirstNodePtr->CType = INVALID_NODE_FLAG;
	if (replaceInNative) ReplaceActor();
}

bool PangeaScript_ObjectIsScriptControlled(PangeaScriptObjectHandle handle)
{
	return handle.id != 0 && controlled;
}

void MikeScript_RunObjectFrame(ObjNode* object)
{
	assert(object == FirstNodePtr);
	scriptCalls++;
	if (deleteInScript) object->CType = INVALID_NODE_FLAG;
	if (replaceInScript) ReplaceActor();
}

void AnimateASprite(ObjNode* object)
{
	assert(object == FirstNodePtr);
	animationCalls++;
}

int main(void)
{
	int32_t fixed;
	assert(MikeScript_ToFixed(1.5f, &fixed) && fixed == 98304);
	assert(MikeScript_ToFixed(-32768.0f, &fixed) && fixed == INT32_MIN);
	assert(!MikeScript_ToFixed(32768.0f, &fixed));
	assert(!MikeScript_ToFixed(INFINITY, &fixed));
	assert(!MikeScript_ToFixed(NAN, &fixed));
	assert(!MikeScript_ToFixed(1, NULL));
	assert(MikeScript_CoordinateValid(32767) && MikeScript_CoordinateValid(-32768));
	assert(!MikeScript_CoordinateValid(32768) && !MikeScript_CoordinateValid(NAN));
	int16_t animations[] = {3, 0};
	assert(MikeScript_AnimationValid(&animations[1], 0));
	assert(MikeScript_AnimationValid(&animations[1], 2));
	assert(!MikeScript_AnimationValid(&animations[1], 3));
	assert(!MikeScript_AnimationValid(&animations[1], -1));
	assert(!MikeScript_AnimationValid(NULL, 0));
	ObjNode collisionObject = {.CType = CTYPE_MISC, .LeftOff = -12, .RightOff = 18, .TopOff = -4, .BottomOff = 24};
	PangeaScriptBounds bounds;
	collisionObject.X.Int = 100;
	collisionObject.Y.Int = 200;
	CalcObjectBox2(&collisionObject);
	assert(MikeScript_GetObjectBounds(&collisionObject, &bounds));
	assert(bounds.min.x == 88 && bounds.max.x == 118 && bounds.min.y == 196 && bounds.max.y == 224);
	assert(bounds.min.z == 0 && bounds.max.z == 0);
	collisionObject.X.Int = 300;
	collisionObject.Y.Int = 400;
	CalcObjectBox2(&collisionObject);
	assert(MikeScript_GetObjectBounds(&collisionObject, &bounds));
	assert(bounds.min.x == 288 && bounds.max.x == 318 && bounds.min.y == 396 && bounds.max.y == 424);
	collisionObject.CType = INVALID_NODE_FLAG;
	assert(!MikeScript_GetObjectBounds(&collisionObject, &bounds));
	collisionObject.CType = CTYPE_MISC;
	collisionObject.RightSide = collisionObject.LeftSide;
	assert(!MikeScript_GetObjectBounds(&collisionObject, &bounds));
	assert(!MikeScript_GetObjectBounds(NULL, &bounds));
	assert(!MikeScript_GetObjectBounds(&collisionObject, NULL));
	ObjNode actor = {.MoveFlag = true, .AnimFlag = true, .ScriptObjectID = 1, .ScriptObjectGeneration = 1};
	FirstNodePtr = &actor;
	MoveObjects();
	assert(nativeCalls == 0 && scriptCalls == 1 && animationCalls == 1);
	actor.MoveCall = MoveNative;
	MoveObjects();
	assert(nativeCalls == 1 && scriptCalls == 2 && animationCalls == 2);
	controlled = true;
	MoveObjects();
	assert(nativeCalls == 1 && scriptCalls == 3 && animationCalls == 3);
	actor.MoveFlag = false;
	MoveObjects();
	assert(nativeCalls == 1 && scriptCalls == 3);
	actor.MoveFlag = true;
	controlled = false;
	deleteInNative = true;
	MoveObjects();
	assert(nativeCalls == 2 && scriptCalls == 3 && animationCalls == 4);
	actor.CType = CTYPE_MISC;
	deleteInNative = false;
	deleteInScript = true;
	MoveObjects();
	assert(nativeCalls == 3 && scriptCalls == 4 && animationCalls == 4);
	actor.CType = CTYPE_MISC;
	actor.MoveCall = MoveNative;
	deleteInScript = false;
	replaceInNative = true;
	MoveObjects();
	assert(nativeCalls == 4 && scriptCalls == 4 && animationCalls == 4);
	assert(actor.AllocationGeneration == 1 && actor.ScriptObjectID == 2);
	MoveObjects();
	assert(nativeCalls == 4 && scriptCalls == 5 && animationCalls == 5);
	replaceInScript = true;
	MoveObjects();
	assert(nativeCalls == 4 && scriptCalls == 6 && animationCalls == 5);
	assert(actor.AllocationGeneration == 2 && actor.ScriptObjectID == 2);
	return 0;
}

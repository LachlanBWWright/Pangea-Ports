#include "game.h"
#include "ScriptBindings.h"
#include <assert.h>
#include <math.h>
#include <string.h>

float gFramesPerSecondFrac = .1f;
OGLPoint3D gCoord;
OGLVector3D gDelta;
OGLMatrix4x4 gWorldToFrustumMatrix;
Boolean gOpenPlayerWormhole;
uint32_t gAutoFadeStatusBits;
static ObjNode objects[24];
static int objectCount;
ObjNode* eggDestination;

ObjNode* MakeNewDisplayGroupObject(NewObjectDefinitionType* definition)
{
	assert(objectCount < 24);
	ObjNode* object = &objects[objectCount++];
	*object = (ObjNode){.Coord = definition->coord, .InitCoord = definition->coord,
		.MoveCall = definition->moveCall, .Type = definition->type, .Group = definition->group};
	return object;
}
ObjNode* CreateScriptTestEgg(TerrainItemEntryType* item)
{
	assert(AddEgg(item, 10, 20));
	return gEggObjs[gNumEggs - 1];
}
float RandomFloat(void) { return .5f; }
void GetObjectInfo(ObjNode* object) { gCoord = object->Coord; gDelta = object->Delta; }
void UpdateObject(ObjNode* object) { object->Coord = gCoord; object->Delta = gDelta; }
void CalcObjectBoxFromNode(ObjNode* object) { (void)object; }
void CreateCollisionBoxFromBoundingBox(ObjNode* object, float x, float y) { (void)object; (void)x; (void)y; }
ObjNode* AttachShadowToObject(ObjNode* object, int type, float x, float z, Boolean blockers)
{ (void)object; (void)type; (void)x; (void)z; (void)blockers; return NULL; }
Boolean TrackTerrainItem(ObjNode* object) { (void)object; return false; }
void ApplyFrictionToDeltasXZ(float friction, OGLVector3D* delta) { (void)friction; (void)delta; }
Byte HandleCollisions(ObjNode* object, uint32_t mask, float bounce) { (void)object; (void)mask; (void)bounce; return 0; }
float CalcDistanceToClosestPlayer(OGLPoint3D* point, short* player) { (void)point; *player = 0; return 10000; }
Boolean IsObjectTotallyCulled(ObjNode* object) { (void)object; return false; }
void OGLMatrix4x4_Multiply(const OGLMatrix4x4* a, const OGLMatrix4x4* b, OGLMatrix4x4* output)
{ (void)b; *output = *a; }
Boolean OGL_IsBBoxVisible(const OGLBoundingBox* box, OGLMatrix4x4* matrix) { (void)box; (void)matrix; return true; }
void FindCoordOfJoint(ObjNode* object, long joint, OGLPoint3D* output) { (void)joint; *output = object->Coord; }
float OGLPoint3D_Distance(const OGLPoint3D* a, const OGLPoint3D* b)
{ return sqrtf((a->x-b->x)*(a->x-b->x)+(a->y-b->y)*(a->y-b->y)+(a->z-b->z)*(a->z-b->z)); }
void OGLVector3D_Normalize(const OGLVector3D* input, OGLVector3D* output) { *output = *input; }
short PlayEffect_Parms3D(short effect, OGLPoint3D* point, uint32_t rate, float volume)
{ (void)effect; (void)point; (void)rate; (void)volume; return -1; }
short PlayEffect3D(short effect, OGLPoint3D* point) { (void)effect; (void)point; return -1; }
void PlayRumbleEffect(short effect, int player) { (void)effect; (void)player; }
void HighlightInfobarEgg(int kind) { (void)kind; }
void UpdateCarriedObject(ObjNode* player, ObjNode* object) { object->Coord = player->Coord; }
ObjNode* FindClosestEggWormholeInRange(short kind, OGLPoint3D* point) { (void)kind; (void)point; return eggDestination; }
Boolean Nanosaur2Script_OnPickupCollected(ObjNode* pickup, ObjNode* player, int kind, float amount, const char* id)
{ (void)pickup; (void)player; (void)kind; (void)amount; (void)id; return true; }
void DeleteObject(ObjNode* object)
{
	ClearEggObjectReferences(object);
	object->CType = INVALID_NODE_FLAG;
}

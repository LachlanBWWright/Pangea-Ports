#include "game.h"
#include <assert.h>

short gRecoveredEggs[NUM_EGG_SPECIES], gRequiredEggs[NUM_EGG_SPECIES];
uint32_t gInfobarUpdateBits;
ObjNode* gPlayerObj;
TQ3Point3D gMyCoord;
float gFramesPerSecondFrac;
static long score;
bool PangeaScript_RulesAreScriptControlled(void) { return true; }
void AddToScore(long points) { score += points; }
short PlayEffect(short effect) { (void)effect; return -1; }
short PlayEffect_Parms(short effect, unsigned char volume, unsigned long frequency)
{ (void)effect; (void)volume; (void)frequency; return -1; }
void UpdateObjectTransforms(ObjNode* object) { (void)object; }
void DeleteObject(ObjNode* object) { (void)object; }
void MakeObjectTransparent(ObjNode* object, float alpha) { (void)object; (void)alpha; }
ObjNode* MakeTimePortal(Byte kind, float x, float z) { (void)kind; (void)x; (void)z; return NULL; }
void MorphToSkeletonAnim(SkeletonObjDataType* skeleton, long animation, float speed)
{ (void)skeleton; (void)animation; (void)speed; }

int main(void)
{
	SkeletonObjDataType skeleton = {0};
	ObjNode holder = {.Skeleton = &skeleton};
	ObjNode egg = {.Genre = DISPLAY_GROUP_GENRE, .Group = LEVEL0_MGroupNum_Egg,
		.Type = LEVEL0_MObjType_Egg1, .CType = CTYPE_MISC | CTYPE_PICKUP, .CBits = CBITS_TOUCHABLE};
	assert(CarryEgg(&holder, &egg));
	assert(holder.CarriedObj == &egg && (holder.StatusBits & STATUS_BIT_ISCARRYING));
	assert(egg.SpecialRef[0] == &holder && egg.CType == 0 && egg.CBits == 0);
	assert(!CarryEgg(&holder, &egg));
	ReleasePickupRelationships(&egg);
	assert(!holder.CarriedObj && !(holder.StatusBits & STATUS_BIT_ISCARRYING));
	assert(!egg.SpecialRef[0] && egg.CType == (CTYPE_MISC | CTYPE_PICKUP) && egg.CBits == CBITS_TOUCHABLE);
	assert(CarryEgg(&holder, &egg));
	ReleasePickupRelationships(&holder);
	assert(!holder.CarriedObj && !egg.SpecialRef[0]);
	egg.Flag[0] = true;
	assert(!CarryEgg(&holder, &egg));
	egg.Flag[0] = false;
	egg.Type = LEVEL0_MObjType_Egg5 + 1;
	assert(!CarryEgg(&holder, &egg));
	holder.StatusBits |= STATUS_BIT_ISCARRYING;
	DropItem(&holder);
	assert(!(holder.StatusBits & STATUS_BIT_ISCARRYING));
	egg.Type = LEVEL0_MObjType_Egg1;
	assert(CarryEgg(&holder, &egg));
	ObjNode portal = {.Genre = EVENT_GENRE, .Kind = PORTAL_TYPE_EGG, .CType = CTYPE_PORTAL, .Coord = {20, 30, 40}};
	portal.Kind = PORTAL_TYPE_EXIT;
	assert(!DeliverEgg(&egg, &portal));
	assert(holder.CarriedObj == &egg && gRecoveredEggs[0] == 0);
	portal.Kind = PORTAL_TYPE_EGG;
	assert(DeliverEgg(&egg, &portal));
	assert(!holder.CarriedObj && !egg.SpecialRef[0] && egg.Flag[0]);
	assert(egg.CType == 0 && egg.TerrainItemPtr == NULL && egg.MoveCall != NULL);
	assert(egg.Coord.x == 20 && egg.Coord.y == 30 && egg.Coord.z == 40);
	assert(gRecoveredEggs[0] == 1 && score == EGG_POINTS);
	assert(!DeliverEgg(&egg, &portal));
	assert(gRecoveredEggs[0] == 1 && score == EGG_POINTS);
	return 0;
}

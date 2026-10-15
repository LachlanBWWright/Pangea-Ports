#include "game.h"
#include <assert.h>
#include <stdlib.h>

Boolean gGamePaused;
static int meshes;

void DoFatalAlert(const char* format, ...) { (void)format; abort(); }
void SafeDisposePtr(void* pointer) { free(pointer); }
void KeepOldCollisionBoxes(ObjNode* object) { (void)object; }
void UpdateSkeletonAnimation(ObjNode* object) { (void)object; }
void UpdateSkinnedGeometry(ObjNode* object) { (void)object; meshes++; }

static void DeleteImmediately(ObjNode* object)
{
	gFirstNodePtr = NULL;
	free(object);
}

static void ReplaceImmediately(ObjNode* object)
{
	object->Cookie++;
	object->MoveCall = NULL;
}

int main(void)
{
	SkeletonObjDataType skeleton = {0};
	ObjNode* object = calloc(1, sizeof(*object));
	assert(object);
	*object = (ObjNode){.Cookie = 1, .CType = CTYPE_MISC, .Skeleton = &skeleton, .MoveCall = DeleteImmediately};
	gFirstNodePtr = object;
	MoveObjects();
	assert(gFirstNodePtr == NULL && meshes == 0);
	object = calloc(1, sizeof(*object));
	assert(object);
	*object = (ObjNode){.Cookie = 2, .CType = CTYPE_MISC, .Skeleton = &skeleton, .MoveCall = ReplaceImmediately};
	gFirstNodePtr = object;
	MoveObjects();
	assert(meshes == 0 && object->Cookie == 3);
	MoveObjects();
	assert(meshes == 1);
	gFirstNodePtr = NULL;
	free(object);
	return 0;
}

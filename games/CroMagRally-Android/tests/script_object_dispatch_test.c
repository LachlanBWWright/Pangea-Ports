#include "../Source/System/Objects.c"
#include <assert.h>
#include <stdlib.h>

Boolean gGamePaused;
static ObjNode* registered;
static int frameCalls;
static int freed;
static bool deleteDuringAnimation;

void DoFatalAlert(const char* format, ...) { (void)format; abort(); }
void SafeDisposePtr(void* pointer) { if (registered == pointer) registered = NULL; freed++; free(pointer); }
void DoSoundMaintenance(void) {}
void KeepOldCollisionBoxes(ObjNode* node) { (void)node; }
void StopObjectStreamEffect(ObjNode* node) { (void)node; }
void FreeSkeletonBaseData(SkeletonObjDataType* skeleton) { (void)skeleton; }
void MO_DisposeObjectReference(MetaObjectPtr object) { (void)object; }
Boolean RemoveFromSplineObjectList(ObjNode* node) { (void)node; return false; }
void CroMagScript_OnObjectDeleted(ObjNode* node) { (void)node; }
void CroMagScript_UnregisterObject(ObjNode* node)
{
	assert(node == registered);
	registered = NULL;
	node->ScriptObjectID = 0;
}
bool PangeaScript_ObjectIsScriptControlled(PangeaScriptObjectHandle handle) { (void)handle; return false; }
bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** output)
{
	if (!registered || handle.id != 1 || handle.generation != 1) return false;
	*output = registered;
	return true;
}
void CroMagScript_RunObjectFrame(ObjNode* node) { assert(node == registered); frameCalls++; }
void UpdateSkeletonAnimation(ObjNode* node)
{
	if (deleteDuringAnimation) DeleteObject(node);
}

static void DeleteWhileMoving(ObjNode* node) { DeleteObject(node); }

static void SaturateQueue(void)
{
	for (int i = 0; i < OBJ_DEL_Q_SIZE - 1; i++)
	{
		ObjNode* node = calloc(1, sizeof(*node));
		assert(node);
		gObjectDeleteQueue[gNumObjsInDeleteQueue++] = node;
	}
}

static void RunDeletingFrame(bool animation, bool unregistered)
{
	SaturateQueue();
	registered = calloc(1, sizeof(*registered));
	assert(registered);
	registered->ScriptObjectID = registered->ScriptObjectGeneration = 1;
	if (unregistered) registered->ScriptObjectID = 0;
	registered->MoveCall = DeleteWhileMoving;
	SkeletonObjDataType skeleton = {0};
	if (animation) registered->Skeleton = &skeleton;
	gFirstNodePtr = registered;
	deleteDuringAnimation = animation;
	MoveObjects();
	assert(!registered && !gFirstNodePtr && gNumObjsInDeleteQueue == 0);
	assert(frameCalls == 0);
}

int main(void)
{
	RunDeletingFrame(false, false);
	RunDeletingFrame(true, false);
	RunDeletingFrame(false, true);
	RunDeletingFrame(true, true);
	assert(freed == OBJ_DEL_Q_SIZE * 4);
	return 0;
}

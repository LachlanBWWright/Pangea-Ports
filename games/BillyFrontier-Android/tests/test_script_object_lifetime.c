#include "ScriptBindings.h"
#include <assert.h>
#include <stdlib.h>

ObjNode* gFirstNodePtr;
static void* registeredObject;
static PangeaScriptObjectHandle registeredHandle;

bool PangeaScript_GetObjectNativeObject(PangeaScriptObjectHandle handle, void** output)
{
	if (!registeredObject || handle.id != registeredHandle.id || handle.generation != registeredHandle.generation) return false;
	*output = registeredObject;
	return true;
}

int main(void)
{
	ObjNode live = {.CType = CTYPE_PLAYER};
	gFirstNodePtr = &live;
	assert(BillyScript_ObjectStillAlive(&live, (PangeaScriptObjectHandle){0}));
	ObjNode* removed = calloc(1, sizeof(*removed));
	assert(removed);
	registeredHandle = (PangeaScriptObjectHandle){1, 4};
	registeredObject = removed;
	removed->NextNode = &live;
	gFirstNodePtr = removed;
	assert(BillyScript_ObjectStillAlive(removed, registeredHandle));
	registeredObject = NULL;
	gFirstNodePtr = &live;
	free(removed);
	assert(!BillyScript_ObjectStillAlive(removed, registeredHandle));
	assert(!BillyScript_ObjectStillAlive(removed, (PangeaScriptObjectHandle){0}));
	registeredObject = &live;
	assert(!BillyScript_ObjectStillAlive(removed, registeredHandle));
	assert(!BillyScript_ObjectStillAlive(&live, (PangeaScriptObjectHandle){1, 3}));
	live.CType = INVALID_NODE_FLAG;
	assert(!BillyScript_ObjectStillAlive(&live, registeredHandle));
	assert(!BillyScript_ObjectStillAlive(&live, (PangeaScriptObjectHandle){0}));
	return 0;
}

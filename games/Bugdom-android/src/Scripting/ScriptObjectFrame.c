#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"
#include "ScriptBindings.h"

bool BugdomScript_ObjectStillAlive(const ObjNode* object, PangeaScriptObjectHandle handle)
{
	if (handle.id > 0)
	{
		void* native = NULL;
		if (!PangeaScript_GetObjectNativeObject(handle, &native) || native != object)
			return false;
	}
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
		if (node == object)
			return node->CType != INVALID_NODE_FLAG;
	return false;
}

void BugdomScript_RunObjectFrame(ObjNode* object)
{
	PangeaScriptObjectHandle handle = {object->ScriptObjectID, object->ScriptObjectGeneration};
	if (!object->ScriptDeleteRequested)
		BugdomScript_ApplyObjectScripting(object);
	if (!PangeaScript_ObjectExists(handle))
		return;
	if (object->CType == INVALID_NODE_FLAG)
		return;
	if (object->ScriptDeleteRequested)
	{
		DeleteObject(object);
		return;
	}
	if ((!object->ScriptActiveStateInitialized || object->ScriptActive)
		&& object->Skeleton && object->Skeleton->AnimHasStopped && !object->ScriptAnimationCompletionSent)
	{
		(void) PangeaScript_CallObjectEvent(handle, BugdomScript_GetFrameContext(), "animationComplete");
		if (PangeaScript_ObjectExists(handle))
			object->ScriptAnimationCompletionSent = true;
	}
}

#endif

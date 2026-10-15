#ifdef PANGEA_ENABLE_SCRIPTING
#include "ScriptBindings.h"

bool BillyScript_ObjectStillAlive(const ObjNode* object, PangeaScriptObjectHandle handle)
{
	if (!object) return false;
	if (handle.id > 0)
	{
		void* native = NULL;
		if (!PangeaScript_GetObjectNativeObject(handle, &native) || native != object) return false;
	}
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
		if (node == object) return node->CType != INVALID_NODE_FLAG;
	return false;
}
#endif

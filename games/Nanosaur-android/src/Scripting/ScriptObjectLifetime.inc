static bool ObjectRemainsLive(ObjNode* object, PangeaScriptObjectHandle handle)
{
	if (handle.id)
	{
		void* native = NULL;
		return PangeaScript_GetObjectNativeObject(handle, &native) && native == object;
	}
	for (ObjNode* candidate = gFirstNodePtr; candidate; candidate = candidate->NextNode)
		if (candidate == object) return true;
	return false;
}

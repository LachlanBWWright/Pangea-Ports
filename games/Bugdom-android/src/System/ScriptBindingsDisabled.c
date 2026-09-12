#ifndef PANGEA_ENABLE_SCRIPTING

void BugdomScript_RegisterObject(void* obj, const char* nativeId, const char* category)
{
	(void) obj;
	(void) nativeId;
	(void) category;
}

#endif

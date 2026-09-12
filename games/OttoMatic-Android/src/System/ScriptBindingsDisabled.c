#ifndef PANGEA_ENABLE_SCRIPTING

void OttoScript_RegisterObjectNode(void* node, const char* objectType, int capabilityLevel, const char* const* tags, int tagCount)
{
	(void) node;
	(void) objectType;
	(void) capabilityLevel;
	(void) tags;
	(void) tagCount;
}

#endif

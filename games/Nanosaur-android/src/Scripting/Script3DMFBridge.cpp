#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"

#include <setjmp.h>

extern TQ3MetaFile* gObjectGroupFile[MAX_3DMF_GROUPS];
extern GLuint* gObjectGroupTextures[MAX_3DMF_GROUPS];

jmp_buf gPangeaScriptFatalJump;
bool gPangeaScriptFatalBoundaryActive;

extern "C" bool NanosaurScript_LoadCustom3DMF(FSSpec* spec, Byte group)
{
	if (!spec || group >= MAX_3DMF_GROUPS)
		return false;
	try
	{
		LoadGrouped3DMF(spec, group);
		return gNumObjectsInGroupList[group] > 0;
	}
	catch (...)
	{
		if (gNumObjectsInGroupList[group] > 0 || gObjectGroupFile[group] || gObjectGroupTextures[group])
			Free3DMFGroup(group);
		return false;
	}
}

extern "C" bool NanosaurScript_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec)
{
	int jumpResult;
	if (!skeletonSpec || !modelSpec || type >= MAX_SKELETON_TYPES)
		return false;
	jumpResult = setjmp(gPangeaScriptFatalJump);
	if (jumpResult != 0)
	{
		gPangeaScriptFatalBoundaryActive = false;
		FreeSkeletonFile(type);
		return false;
	}
	gPangeaScriptFatalBoundaryActive = true;
	Boolean loaded = false;
	try
	{
		loaded = LoadCustomSkeleton(type, skeletonSpec, modelSpec);
	}
	catch (...)
	{
		gPangeaScriptFatalBoundaryActive = false;
		FreeSkeletonFile(type);
		return false;
	}
	gPangeaScriptFatalBoundaryActive = false;
	return loaded;
}

#endif

#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"

#include <setjmp.h>

jmp_buf gPangeaScriptFatalJump;
bool gPangeaScriptFatalBoundaryActive;

bool PangeaScript_LoadCustomBG3D(FSSpec* spec, int group)
{
	int jumpResult = setjmp(gPangeaScriptFatalJump);
	if (!spec || group < 0 || group >= MAX_BG3D_GROUPS)
		return false;
	if (jumpResult != 0)
	{
		gPangeaScriptFatalBoundaryActive = false;
		AbortBG3DImport(group);
		return false;
	}
	gPangeaScriptFatalBoundaryActive = true;
	ImportBG3D(spec, group);
	gPangeaScriptFatalBoundaryActive = false;
	if (gNumObjectsInBG3DGroupList[group] <= 0)
	{
		AbortBG3DImport(group);
		return false;
	}
	return true;
}

bool PangeaScript_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec)
{
	int jumpResult;
	int group = MODEL_GROUP_SKELETONBASE + type;
	if (!skeletonSpec || !modelSpec || type >= MAX_SKELETON_TYPES)
		return false;
	jumpResult = setjmp(gPangeaScriptFatalJump);
	if (jumpResult != 0)
	{
		gPangeaScriptFatalBoundaryActive = false;
		AbortBG3DImport(group);
		FreeSkeletonFile(type);
		return false;
	}
	gPangeaScriptFatalBoundaryActive = true;
	Boolean loaded = LoadCustomSkeleton(type, skeletonSpec, modelSpec);
	gPangeaScriptFatalBoundaryActive = false;
	return loaded;
}

#endif

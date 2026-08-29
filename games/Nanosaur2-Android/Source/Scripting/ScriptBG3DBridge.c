#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"

#include <setjmp.h>

jmp_buf gNanosaur2ScriptFatalJump;
bool gNanosaur2ScriptFatalBoundaryActive;

bool Nanosaur2Script_LoadCustomBG3D(FSSpec* spec, int group)
{
	int jumpResult;

	if (!spec || group < 0 || group >= MAX_BG3D_GROUPS)
		return false;

	jumpResult = setjmp(gNanosaur2ScriptFatalJump);
	if (jumpResult != 0)
	{
		gNanosaur2ScriptFatalBoundaryActive = false;
		AbortBG3DImport(group);
		return false;
	}

	gNanosaur2ScriptFatalBoundaryActive = true;
	ImportBG3D(spec, group, -1);
	gNanosaur2ScriptFatalBoundaryActive = false;

	if (gNumObjectsInBG3DGroupList[group] <= 0)
	{
		AbortBG3DImport(group);
		return false;
	}

	return true;
}

bool Nanosaur2Script_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec)
{
	int jumpResult;
	int group = MODEL_GROUP_SKELETONBASE + type;

	if (!skeletonSpec || !modelSpec || type >= MAX_SKELETON_TYPES)
		return false;

	jumpResult = setjmp(gNanosaur2ScriptFatalJump);
	if (jumpResult != 0)
	{
		gNanosaur2ScriptFatalBoundaryActive = false;
		AbortBG3DImport(group);
		FreeSkeletonFile(type);
		return false;
	}

	gNanosaur2ScriptFatalBoundaryActive = true;
	Boolean loaded = LoadCustomSkeleton(type, skeletonSpec, modelSpec);
	gNanosaur2ScriptFatalBoundaryActive = false;
	return loaded;
}

#endif

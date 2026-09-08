#ifdef PANGEA_ENABLE_SCRIPTING

#include "game.h"

#include <setjmp.h>
#include <cstring>

extern TQ3MetaFile* gObjectGroupFile[MAX_3DMF_GROUPS];
extern GLuint* gObjectGroupTextures[MAX_3DMF_GROUPS];

jmp_buf gPangeaScriptFatalJump;
bool gPangeaScriptFatalBoundaryActive;

static bool Has3DMFHeader(const FSSpec* spec)
{
	short refNum;
	long fileSize;
	long bytesToRead = 4;
	char magic[4];
	if (!spec || FSpOpenDF(spec, fsRdPerm, &refNum) != noErr)
		return false;
	const bool hasValidSize = GetEOF(refNum, &fileSize) == noErr && fileSize >= bytesToRead;
	const bool hasValidMagic = hasValidSize && FSRead(refNum, &bytesToRead, magic) == noErr
		&& bytesToRead == 4 && std::memcmp(magic, "3DMF", 4) == 0;
	FSClose(refNum);
	return hasValidMagic;
}

extern "C" bool BugdomScript_LoadCustom3DMF(FSSpec* spec, Byte group)
{
	if (!spec || group >= MAX_3DMF_GROUPS)
		return false;
	if (!Has3DMFHeader(spec))
		return false;
	if (!BugdomScript_MakeRendererCurrent())
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

extern "C" bool BugdomScript_LoadCustomSkeleton(Byte type, FSSpec* skeletonSpec, FSSpec* modelSpec)
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

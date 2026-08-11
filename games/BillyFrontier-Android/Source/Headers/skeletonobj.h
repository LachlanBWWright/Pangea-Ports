//
// SkeletonObj.h
//

#pragma once

// Externals
#include "game.h"

enum
{
	SKELETON_TYPE_BILLY = 0,
	SKELETON_TYPE_BANDITO,
	SKELETON_TYPE_RYGAR,
	SKELETON_TYPE_SHORTY,
	SKELETON_TYPE_KANGACOW,
	SKELETON_TYPE_KANGAREX,
	SKELETON_TYPE_WALKER,
	SKELETON_TYPE_TREMORALIEN,
	SKELETON_TYPE_TREMORGHOST,
	SKELETON_TYPE_FROGMAN,
	SKELETON_TYPE_SCRIPT_CUSTOM_BASE,
	SKELETON_TYPE_SCRIPT_CUSTOM_COUNT = 4,
	MAX_SKELETON_TYPES = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + SKELETON_TYPE_SCRIPT_CUSTOM_COUNT
};




//===============================

extern	ObjNode	*MakeNewSkeletonObject(NewObjectDefinitionType *newObjDef);
extern	void AllocSkeletonDefinitionMemory(SkeletonDefType *skeleton);
extern	void InitSkeletonManager(void);
void LoadASkeleton(Byte num);
Boolean IsSkeletonTypeLoaded(short skeletonType);
Boolean LoadCustomSkeleton(Byte num, FSSpec* skeletonSpec, FSSpec* modelSpec);
extern	void FreeSkeletonFile(Byte skeletonType);
extern	void FreeAllSkeletonFiles(short skipMe);
extern	void FreeSkeletonBaseData(SkeletonObjDataType *data);
void DrawSkeleton(ObjNode *theNode);

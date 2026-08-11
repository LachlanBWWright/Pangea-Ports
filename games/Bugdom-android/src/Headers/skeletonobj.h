//
// SkeletonObj.h
//
#pragma once

#define 	SKELETON_TYPE_SCRIPT_CUSTOM_BASE	24
#define 	SKELETON_TYPE_SCRIPT_CUSTOM_COUNT	4
#define 	MAX_SKELETON_TYPES	(SKELETON_TYPE_SCRIPT_CUSTOM_BASE + SKELETON_TYPE_SCRIPT_CUSTOM_COUNT)

enum
{
	SKELETON_TYPE_BOXERFLY = 0,
	SKELETON_TYPE_ME,
	SKELETON_TYPE_SLUG,
	SKELETON_TYPE_ANT,
	SKELETON_TYPE_FIREANT,
	SKELETON_TYPE_WATERBUG,
	SKELETON_TYPE_DRAGONFLY,
	SKELETON_TYPE_PONDFISH,
	SKELETON_TYPE_MOSQUITO,
	SKELETON_TYPE_FOOT,
	SKELETON_TYPE_SPIDER,
	SKELETON_TYPE_CATERPILLER,
	SKELETON_TYPE_FIREFLY,
	SKELETON_TYPE_BAT,
	SKELETON_TYPE_LADYBUG,
	SKELETON_TYPE_ROOTSWING,
	SKELETON_TYPE_KINGANT,
	SKELETON_TYPE_LARVA,
	SKELETON_TYPE_FLYINGBEE,
	SKELETON_TYPE_WORKERBEE,
	SKELETON_TYPE_QUEENBEE,
	SKELETON_TYPE_ROACH,
	SKELETON_TYPE_BUDDY,
	SKELETON_TYPE_SKIPPY
	
				// NOTE: Check MAX_SKELETON_TYPES above
};




//===============================

extern	ObjNode	*MakeNewSkeletonObject(NewObjectDefinitionType *newObjDef);
extern	void AllocSkeletonDefinitionMemory(SkeletonDefType *skeleton);
extern	void InitSkeletonManager(void);
extern	void LoadASkeleton(Byte num);
extern	Boolean IsSkeletonTypeLoaded(Byte skeletonType);
extern	Boolean LoadCustomSkeleton(Byte num, FSSpec* skeletonSpec, FSSpec* modelSpec);
extern	void FreeSkeletonFile(Byte skeletonType);
extern	void FreeAllSkeletonFiles(short skipMe);
extern	void FreeSkeletonBaseData(SkeletonObjDataType *data);

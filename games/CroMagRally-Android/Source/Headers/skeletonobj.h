//
// SkeletonObj.h
//

#ifndef __SKELOBJ
#define __SKELOBJ

enum
{
	SKELETON_TYPE_PLAYER_MALE = 0,
	SKELETON_TYPE_PLAYER_FEMALE,

	SKELETON_TYPE_BIRDBOMB,
	SKELETON_TYPE_YETI,
	SKELETON_TYPE_BEETLE,
	SKELETON_TYPE_CAMEL,
	SKELETON_TYPE_CATAPULT,
	SKELETON_TYPE_BRONTONECK,
	SKELETON_TYPE_SHARK,
	SKELETON_TYPE_FLAG,
	SKELETON_TYPE_PTERADACTYL,
	SKELETON_TYPE_MALESTANDING,
	SKELETON_TYPE_FEMALESTANDING,
	SKELETON_TYPE_DRAGON,
	SKELETON_TYPE_MUMMY,
	SKELETON_TYPE_TROLL,
	SKELETON_TYPE_DRUID,
	SKELETON_TYPE_POLARBEAR,
	SKELETON_TYPE_FLOWER,
	SKELETON_TYPE_VIKING,
	SKELETON_TYPE_SCRIPT_CUSTOM_BASE,
	SKELETON_TYPE_SCRIPT_CUSTOM_COUNT = 4,
	MAX_SKELETON_TYPES = SKELETON_TYPE_SCRIPT_CUSTOM_BASE + SKELETON_TYPE_SCRIPT_CUSTOM_COUNT
};




//===============================

extern	ObjNode	*MakeNewSkeletonObject(NewObjectDefinitionType *newObjDef);
extern	void DisposeSkeletonObjectMemory(SkeletonDefType *skeleton);
extern	void AllocSkeletonDefinitionMemory(SkeletonDefType *skeleton);
extern	void InitSkeletonManager(void);
void ShutdownSkeletonManager(void);
void LoadASkeleton(Byte num);
Boolean IsSkeletonTypeLoaded(short skeletonType);
Boolean LoadCustomSkeleton(Byte num, FSSpec* skeletonSpec, FSSpec* modelSpec);
extern	void FreeSkeletonFile(Byte skeletonType);
extern	void FreeAllSkeletonFiles(short skipMe);
extern	void FreeSkeletonBaseData(SkeletonObjDataType *data);


#endif

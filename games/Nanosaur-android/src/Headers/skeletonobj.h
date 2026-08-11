//
// SkeletonObj.h
//

#define 	SKELETON_TYPE_SCRIPT_CUSTOM_BASE	6
#define 	SKELETON_TYPE_SCRIPT_CUSTOM_COUNT	4
#define 	MAX_SKELETON_TYPES	(SKELETON_TYPE_SCRIPT_CUSTOM_BASE + SKELETON_TYPE_SCRIPT_CUSTOM_COUNT)

enum
{
	SKELETON_TYPE_PTERA,
	SKELETON_TYPE_REX,
	SKELETON_TYPE_STEGO,
	SKELETON_TYPE_DEINON,
	SKELETON_TYPE_TRICER,
	SKELETON_TYPE_SPITTER
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

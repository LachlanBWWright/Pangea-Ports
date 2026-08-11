//
// 3dmf.h
//

#include "qd3d_support.h"


#define	MODEL_GROUP_SCRIPT_CUSTOM_BASE	25
#define	MODEL_GROUP_SCRIPT_CUSTOM_COUNT	4
#define	MAX_3DMF_GROUPS			(MODEL_GROUP_SCRIPT_CUSTOM_BASE + MODEL_GROUP_SCRIPT_CUSTOM_COUNT)
#define	MAX_OBJECTS_IN_GROUP	100


//====================================

extern	void Init3DMFManager(void);
extern	void LoadGrouped3DMF(FSSpec *spec, Byte groupNum);
extern	void Free3DMFGroup(Byte groupNum);
extern	void DeleteAll3DMFGroups(void);


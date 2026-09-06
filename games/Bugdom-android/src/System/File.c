/****************************/
/*      FILE ROUTINES       */
/* By Brian Greenstone      */
/* (c)1999 Pangea Software  */
/* (c)2023 Iliyas Jorio     */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include "game.h"
#include "LevelMetadataJSON.h"
#ifdef PANGEA_ENABLE_SCRIPTING
#include "pangea_script.h"
#endif


/****************************/
/*    PROTOTYPES            */
/****************************/

static void ReadDataFromSkeletonFile(SkeletonDefType *skeleton, const FSSpec* fsSpec3DMF);
static void ReadDataFromPlayfieldFile(void);
static void ReadLevelMetadata(void);
static void ClearLevelMetadata(void);
static Boolean MetadataJSONIsValid(const char *json, Size size);
static Boolean MetadataParseValue(const char **cursor, const char *end, int depth);
static Boolean MetadataParseString(const char **cursor, const char *end);
static void MetadataSkipWhitespace(const char **cursor, const char *end);
static Boolean MetadataProfileValueIsValid(const char *key, const char *value);
static char *gLevelMetadataJSON = NULL;


/****************************/
/*    CONSTANTS             */
/****************************/

#define	BASE_PATH_TILE		900					// tile # of 1st path tile

#define	PICT_HEADER_SIZE	512

#define	SKELETON_FILE_VERS_NUM	0x0110			// v1.1

#define	SAVE_GAME_VERSION	0x00000120			// Bugdom v1.2

#define PREFS_HEADER_LENGTH 16
#define PREFS_FOLDER_NAME "Bugdom"
#define PREFS_FILE_NAME "Prefs"

// Bump this every time prefs struct changes
// Note: this will reset user prefs!
const char PREFS_HEADER_STRING[PREFS_HEADER_LENGTH+1] = "NewBugdomPrefs06";


		/* PLAYFIELD HEADER */
		// (READ IN FROM FILE -- MUST BE BYTESWAPPED!)
		
typedef struct
{
	NumVersion	version;							// version of file
	int32_t		numItems;							// # items in map
	int32_t		mapWidth;							// width of map
	int32_t		mapHeight;							// height of map
	int32_t		numTilePages;						// # tile pages
	int32_t		numTilesInList;						// # extracted tiles in list
	float		tileSize;							// 3D unit size of a tile
	float		minY,maxY;							// min/max height values
	int32_t		numSplines;							// # splines
	int32_t		numFences;							// # fences
}PlayfieldHeaderType;

		/* FENCE STRUCTURE IN FILE */
		//
		// note: we copy this data into our own fence list
		//		since the game uses a slightly different
		//		data structure.
		//
		// (READ IN FROM FILE -- MUST BE BYTESWAPPED!)
		
typedef struct
{
	uint16_t		type;				// type of fence
	int16_t			numNubs;			// # nubs in fence
	int32_t 		_junkptr1;
	Rect			bBox;				// bounding box of fence area	
} File_FenceDefType;


/**********************/
/*     VARIABLES      */
/**********************/

float	g3DTileSize, g3DMinY, g3DMaxY;

int		gCurrentSaveSlot = -1;

/* Level terrain override: colon-path relative to gDataSpec, e.g. ":Terrain:Custom.ter"
 * Consumed (cleared) on first use by LoadLevelArt. */
char	gLevelTerrainOverride[512] = {'\0'};

/******************* LOAD SKELETON *******************/
//
// Loads a skeleton file & creates storage for it.
// 
// NOTE: Skeleton types 0..NUM_CHARACTERS-1 are reserved for player character skeletons.
//		Skeleton types NUM_CHARACTERS and over are for other skeleton entities.
//
// OUTPUT:	Ptr to skeleton data
//

SkeletonDefType *LoadSkeletonFile(short skeletonType)
{
short		fRefNum;
FSSpec		fsSpecSkeleton;
FSSpec		fsSpec3DMF;
SkeletonDefType	*skeleton;
const char* modelName = NULL;
char		pathBuf[128];

				/* SET CORRECT FILENAME */

	switch(skeletonType)
	{
		case	SKELETON_TYPE_BOXERFLY:		modelName = "BoxerFly";			break;
		case	SKELETON_TYPE_ME:			modelName = "DoodleBug";		break;
		case	SKELETON_TYPE_SLUG:			modelName = "Slug";				break;
		case	SKELETON_TYPE_ANT:			modelName = "Ant";				break;
		case	SKELETON_TYPE_FIREANT:		modelName = "WingedFireAnt";	break;
		case	SKELETON_TYPE_WATERBUG:		modelName = "WaterBug";			break;
		case	SKELETON_TYPE_DRAGONFLY:	modelName = "DragonFly";		break;
		case	SKELETON_TYPE_PONDFISH:		modelName = "PondFish";			break;
		case	SKELETON_TYPE_MOSQUITO:		modelName = "Mosquito";			break;
		case	SKELETON_TYPE_FOOT:			modelName = "Foot";				break;
		case	SKELETON_TYPE_SPIDER:		modelName = "Spider";			break;
		case	SKELETON_TYPE_CATERPILLER:	modelName = "Caterpillar";		break;
		case	SKELETON_TYPE_FIREFLY:		modelName = "FireFly";			break;
		case	SKELETON_TYPE_BAT:			modelName = "Bat";				break;
		case	SKELETON_TYPE_LADYBUG:		modelName = "LadyBug";			break;
		case	SKELETON_TYPE_ROOTSWING:	modelName = "RootSwing";		break;
		case	SKELETON_TYPE_LARVA:		modelName = "Larva";			break;
		case	SKELETON_TYPE_FLYINGBEE:	modelName = "FlyingBee";		break;
		case	SKELETON_TYPE_WORKERBEE:	modelName = "WorkerBee";		break;
		case	SKELETON_TYPE_QUEENBEE:		modelName = "QueenBee";			break;
		case	SKELETON_TYPE_ROACH:		modelName = "Roach";			break;
		case	SKELETON_TYPE_BUDDY:		modelName = "Buddy";			break;
		case	SKELETON_TYPE_SKIPPY:		modelName = "Skippy";			break;
		case	SKELETON_TYPE_KINGANT:		modelName = "AntKing";			break;
		default:
				DoFatalAlert("LoadSkeleton: Unknown skeletonType!");
	}



	SDL_snprintf(pathBuf, sizeof(pathBuf), ":Skeletons:%s.skeleton", modelName);
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, pathBuf, &fsSpecSkeleton);

	SDL_snprintf(pathBuf, sizeof(pathBuf), ":Skeletons:%s.3dmf", modelName);
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, pathBuf, &fsSpec3DMF);



			/* OPEN THE FILE'S REZ FORK */

	fRefNum = FSpOpenResFile(&fsSpecSkeleton, fsRdPerm);
	GAME_ASSERT(fRefNum != -1);

	UseResFile(fRefNum);
	GAME_ASSERT(noErr == ResError());

			
			/* ALLOC MEMORY FOR SKELETON INFO STRUCTURE */
			
	skeleton = (SkeletonDefType *)AllocPtr(sizeof(SkeletonDefType));
	GAME_ASSERT(skeleton);


			/* READ SKELETON RESOURCES */
			
	ReadDataFromSkeletonFile(skeleton, &fsSpec3DMF);
	PrimeBoneData(skeleton);
	
			/* CLOSE REZ FILE */
			
	CloseResFile(fRefNum);


	return(skeleton);
}

SkeletonDefType* LoadSkeletonFileFromSpecs(short skeletonType, FSSpec* skeletonSpec, FSSpec* modelSpec)
{
	short refNum;
	SkeletonDefType* skeleton;
	if (!skeletonSpec || !modelSpec || skeletonType < SKELETON_TYPE_SCRIPT_CUSTOM_BASE || skeletonType >= MAX_SKELETON_TYPES) return nil;
	refNum = FSpOpenResFile(skeletonSpec, fsRdPerm);
	if (refNum < 0) return nil;
	UseResFile(refNum);
	if (ResError()) { CloseResFile(refNum); return nil; }
	skeleton = (SkeletonDefType*)AllocPtr(sizeof(SkeletonDefType));
	if (!skeleton) { CloseResFile(refNum); return nil; }
	ReadDataFromSkeletonFile(skeleton, modelSpec);
	PrimeBoneData(skeleton);
	CloseResFile(refNum);
	return skeleton;
}


/************* READ DATA FROM SKELETON FILE *******************/
//
// Current rez file is set to the file. 
//

static void ReadDataFromSkeletonFile(
		SkeletonDefType *skeleton,
		const FSSpec *fsSpec3DMF)
{
Handle				hand;
short				i,k,j;
long				numJoints,numAnims,numKeyframes;
AnimEventType		*animEventPtr;
JointKeyframeType	*keyFramePtr;
SkeletonFile_Header_Type	*headerPtr;
short				version;
TQ3Point3D				*pointPtr;
SkeletonFile_AnimHeader_Type	*animHeaderPtr;


			/************************/
			/* READ HEADER RESOURCE */
			/************************/

	hand = GetResource('Hedr',1000);
	GAME_ASSERT(hand);
	headerPtr = (SkeletonFile_Header_Type *)*hand;
	UNPACK_STRUCTS(SkeletonFile_Header_Type, 1, headerPtr);
	version = headerPtr->version;
	GAME_ASSERT_MESSAGE(version == SKELETON_FILE_VERS_NUM, "Skeleton file has wrong version #");
	
	numAnims = skeleton->NumAnims = headerPtr->numAnims;			// get # anims in skeleton
	numJoints = skeleton->NumBones = headerPtr->numJoints;			// get # joints in skeleton
	ReleaseResource(hand);

	GAME_ASSERT(numJoints <= MAX_JOINTS);							// check for overload


				/*************************************/
				/* ALLOCATE MEMORY FOR SKELETON DATA */
				/*************************************/

	AllocSkeletonDefinitionMemory(skeleton);



		/********************************/
		/* 	LOAD THE REFERENCE GEOMETRY */
		/********************************/

#if 1
	// Source port change: original game used to resolve path to 3DMF via alias resource within skeleton rez fork.
	// Instead, we're forcing the 3DMF's filename (sans extension) to match the skeleton's.
	LoadBonesReferenceModel(fsSpec3DMF, skeleton);
#else
	AliasHandle				alias;
	FSSpec					target;
	alias = (AliasHandle)GetResource(rAliasType,1000);				// alias to geometry 3DMF file
	if (alias != nil)
	{
		iErr = ResolveAlias(fsSpec, alias, &target, &wasChanged);	// try to resolve alias
		GAME_ASSERT_MESSAGE(noErr == iErr, "Cannot find Skeleton's 3DMF file!");
		LoadBonesReferenceModel(&target,skeleton);
		ReleaseResource((Handle)alias);
	}
#endif



		/***********************************/
		/*  READ BONE DEFINITION RESOURCES */
		/***********************************/

	for (i=0; i < numJoints; i++)
	{
		File_BoneDefinitionType	*bonePtr;
		u_short					*indexPtr;

			/* READ BONE DATA */
			
		hand = GetResource('Bone',1000+i);
		GAME_ASSERT(hand);
		HLock(hand);
		bonePtr = (File_BoneDefinitionType *)*hand;
		UNPACK_STRUCTS(File_BoneDefinitionType, 1, bonePtr);

			/* COPY BONE DATA INTO ARRAY */
		
		skeleton->Bones[i].parentBone = bonePtr->parentBone;								// index to previous bone
		skeleton->Bones[i].coord = bonePtr->coord;											// absolute coord (not relative to parent!)
		skeleton->Bones[i].numPointsAttachedToBone = bonePtr->numPointsAttachedToBone;		// # vertices/points that this bone has
		skeleton->Bones[i].numNormalsAttachedToBone = bonePtr->numNormalsAttachedToBone;	// # vertex normals this bone has		
		ReleaseResource(hand);

			/* ALLOC THE POINT & NORMALS SUB-ARRAYS */
				
		skeleton->Bones[i].pointList = (u_short *)AllocPtr(sizeof(u_short) * (int)skeleton->Bones[i].numPointsAttachedToBone);
		GAME_ASSERT(skeleton->Bones[i].pointList);

		skeleton->Bones[i].normalList = (u_short *)AllocPtr(sizeof(u_short) * (int)skeleton->Bones[i].numNormalsAttachedToBone);
		GAME_ASSERT(skeleton->Bones[i].normalList);

			/* READ POINT INDEX ARRAY */
			
		hand = GetResource('BonP',1000+i);
		GAME_ASSERT(hand);
		HLock(hand);
		indexPtr = (u_short *)(*hand);
		UNPACK_BE_SCALARS_HANDLE(u_short, skeleton->Bones[i].numPointsAttachedToBone, hand);
			
			/* COPY POINT INDEX ARRAY INTO BONE STRUCT */

		for (j=0; j < skeleton->Bones[i].numPointsAttachedToBone; j++)
			skeleton->Bones[i].pointList[j] = indexPtr[j];
		ReleaseResource(hand);


			/* READ NORMAL INDEX ARRAY */
			
		hand = GetResource('BonN',1000+i);
		GAME_ASSERT(hand);
		HLock(hand);
		indexPtr = (u_short *)(*hand);
		UNPACK_BE_SCALARS_HANDLE(u_short, skeleton->Bones[i].numNormalsAttachedToBone, hand);
			
			/* COPY NORMAL INDEX ARRAY INTO BONE STRUCT */

		for (j=0; j < skeleton->Bones[i].numNormalsAttachedToBone; j++)
			skeleton->Bones[i].normalList[j] = indexPtr[j];
		ReleaseResource(hand);
						
	}
	
	
		/*******************************/
		/* READ POINT RELATIVE OFFSETS */
		/*******************************/
		//
		// The "relative point offsets" are the only things
		// which do not get rebuilt in the ModelDecompose function.
		// We need to restore these manually.
	
	hand = GetResource('RelP', 1000);
	GAME_ASSERT(hand);
	HLock(hand);
	pointPtr = (TQ3Point3D *)*hand;
	
	if ((long)(GetHandleSize(hand) / sizeof(TQ3Point3D)) != skeleton->numDecomposedPoints)
		DoFatalAlert("# of points in Reference Model has changed!");
	else
	{
		UNPACK_STRUCTS(TQ3Point3D, skeleton->numDecomposedPoints, pointPtr);
		for (i = 0; i < skeleton->numDecomposedPoints; i++)
			skeleton->decomposedPointList[i].boneRelPoint = pointPtr[i];
	}

	ReleaseResource(hand);
	
	
			/*********************/
			/* READ ANIM INFO   */
			/*********************/
			
	for (i=0; i < numAnims; i++)
	{
				/* READ ANIM HEADER */

		hand = GetResource('AnHd',1000+i);
		GAME_ASSERT(hand);
		HLock(hand);
		animHeaderPtr = (SkeletonFile_AnimHeader_Type *)*hand;
		UNPACK_STRUCTS(SkeletonFile_AnimHeader_Type, 1, animHeaderPtr);

		skeleton->NumAnimEvents[i] = animHeaderPtr->numAnimEvents;			// copy # anim events in anim	
		ReleaseResource(hand);

			/* READ ANIM-EVENT DATA */
			
		hand = GetResource('Evnt',1000+i);
		GAME_ASSERT(hand);
		animEventPtr = (AnimEventType *)*hand;
		UNPACK_STRUCTS(AnimEventType, skeleton->NumAnimEvents[i], animEventPtr);
		for (j=0;  j < skeleton->NumAnimEvents[i]; j++)
			skeleton->AnimEventsList[i][j] = *animEventPtr++;
		ReleaseResource(hand);		


			/* READ # KEYFRAMES PER JOINT IN EACH ANIM */
					
		hand = GetResource('NumK',1000+i);									// read array of #'s for this anim
		GAME_ASSERT(hand);
		// (no need to byteswap, it's an array of chars)
		for (j=0; j < numJoints; j++)
			skeleton->JointKeyframes[j].numKeyFrames[i] = (*hand)[j];
		ReleaseResource(hand);
	}


	for (j=0; j < numJoints; j++)
	{
				/* ALLOC 2D ARRAY FOR KEYFRAMES */
				
		Alloc_2d_array(JointKeyframeType,skeleton->JointKeyframes[j].keyFrames,	numAnims,MAX_KEYFRAMES);
		
		GAME_ASSERT((skeleton->JointKeyframes[j].keyFrames) && (skeleton->JointKeyframes[j].keyFrames[0]));

					/* READ THIS JOINT'S KF'S FOR EACH ANIM */
					
		for (i=0; i < numAnims; i++)								
		{
			numKeyframes = skeleton->JointKeyframes[j].numKeyFrames[i];					// get actual # of keyframes for this joint
			GAME_ASSERT(numKeyframes <= MAX_KEYFRAMES);

					/* READ A JOINT KEYFRAME */
					
			hand = GetResource('KeyF',1000+(i*100)+j);
			GAME_ASSERT(hand);
			keyFramePtr = (JointKeyframeType *)*hand;
			UNPACK_STRUCTS(JointKeyframeType, numKeyframes, keyFramePtr);
			for (k = 0; k < numKeyframes; k++)												// copy this joint's keyframes for this anim
				skeleton->JointKeyframes[j].keyFrames[i][k] = *keyFramePtr++;
			ReleaseResource(hand);		
		}
	}
	
}

#pragma mark -

/**************** OPEN GAME FILE **********************/

short OpenGameFile(const char* filename)
{
OSErr		iErr;
FSSpec		spec;
short		refNum;

				/* FIRST SEE IF WE CAN GET IT OFF OF DEFAULT VOLUME */

	iErr = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, filename, &spec);
	if (iErr == noErr)
	{
		iErr = FSpOpenDF(&spec, fsRdPerm, &refNum);
		if (iErr == noErr)
			return refNum;
	}

	DoFatalAlert("Error %d when trying to open %s", iErr, filename);

	return -1;
}


/******************** INIT PREFS FOLDER **********************/

void InitPrefsFolder(bool createIt)
{
	OSErr iErr;
	long createdDirID;

			/* CHECK PREFERENCES FOLDER */

	iErr = FindFolder(kOnSystemDisk, kPreferencesFolderType, kDontCreateFolder,		// locate the folder
		&gPrefsFolderVRefNum, &gPrefsFolderDirID);
	if (iErr != noErr)
		DoAlert("Warning: Cannot locate the Preferences folder.");

	if (createIt)
	{
		iErr = DirCreate(gPrefsFolderVRefNum, gPrefsFolderDirID, PREFS_FOLDER_NAME, &createdDirID);		// make folder in there
	}
}

/******************** MAKE FSSPEC IN PREFS FOLDER **********************/

OSErr MakePrefsFSSpec(const char* filename, bool createFolder, FSSpec* spec)
{
	InitPrefsFolder(createFolder);
	
	char path[256];
	SDL_snprintf(path, sizeof(path), ":%s:%s", PREFS_FOLDER_NAME, filename);

	return FSMakeFSSpec(gPrefsFolderVRefNum, gPrefsFolderDirID, path, spec);
}

/******************** LOAD PREFS **********************/
//
// Load in standard preferences
//

OSErr LoadPrefs(PrefsType *prefBlock)
{
OSErr		iErr;
short		refNum;
FSSpec		file;
long		count;
char		header[PREFS_HEADER_LENGTH + 1];
PrefsType	prefBuffer;

				/*************/
				/* READ FILE */
				/*************/
					
	MakePrefsFSSpec(PREFS_FILE_NAME, false, &file);
	iErr = FSpOpenDF(&file, fsRdPerm, &refNum);	
	if (iErr)
		return(iErr);

				/* CHECK FILE LENGTH */

	long eof = 0;
	GetEOF(refNum, &eof);

	if (eof != sizeof(PrefsType) + PREFS_HEADER_LENGTH)
		goto fileIsCorrupt;

				/* READ HEADER */

	count = PREFS_HEADER_LENGTH;
	iErr = FSRead(refNum, &count, header);
	header[PREFS_HEADER_LENGTH] = '\0';
	if (iErr ||
		count != PREFS_HEADER_LENGTH ||
		0 != strncmp(header, PREFS_HEADER_STRING, PREFS_HEADER_LENGTH))
	{
		goto fileIsCorrupt;
	}

				/* READ PREFS STRUCT */

	count = sizeof(PrefsType);
	iErr = FSRead(refNum, &count, (Ptr)&prefBuffer);		// read data from file
	if (iErr || count != sizeof(PrefsType))
		goto fileIsCorrupt;

	FSClose(refNum);			


				/* SAFETY CHECKS */

	if (prefBlock->mouseSensitivityLevel >= NUM_MOUSE_SENSITIVITY_LEVELS)
	{
		DoAlert("Illegal mouse sensitivity level in prefs!");
		prefBlock->mouseSensitivityLevel = DEFAULT_MOUSE_SENSITIVITY_LEVEL;
	}

				/* PREFS ARE OK */

	*prefBlock = prefBuffer;
	return noErr;

fileIsCorrupt:
	FSClose(refNum);
	return badFileFormat;
}


/******************** SAVE PREFS **********************/

void SavePrefs(PrefsType *prefs)
{
FSSpec				file;
OSErr				iErr;
short				refNum;
long				count;

				/* CREATE BLANK FILE */
				
	MakePrefsFSSpec(PREFS_FILE_NAME, true, &file);
	FSpDelete(&file);															// delete any existing file
	iErr = FSpCreate(&file, 'BalZ', 'Pref', smSystemScript);					// create blank file
	if (iErr)
		return;

				/* OPEN FILE */
					
	iErr = FSpOpenDF(&file, fsRdWrPerm, &refNum);
	if (iErr)
	{
		FSpDelete(&file);
		return;
	}


				/* WRITE HEADER */

	count = PREFS_HEADER_LENGTH;
	iErr = FSWrite(refNum, &count, (Ptr) PREFS_HEADER_STRING);
	GAME_ASSERT(iErr == noErr);
	GAME_ASSERT(count == PREFS_HEADER_LENGTH);


				/* WRITE DATA */

	count = sizeof(PrefsType);
	iErr = FSWrite(refNum, &count, (Ptr) prefs);
	GAME_ASSERT(iErr == noErr);
	GAME_ASSERT(count == sizeof(PrefsType));

	FSClose(refNum);
}

#pragma mark -

/******************* GET FSSPEC FOR SAVE GAME SLOT *********************/

static FSSpec MakeSaveGameFSSpec(int slot)
{
	char			path[128];
	FSSpec			spec;

	GAME_ASSERT(slot >= 0);
	GAME_ASSERT(slot < NUM_SAVE_FILES);

	SDL_snprintf(path, sizeof(path), "Save%c", slot + 'A');
	MakePrefsFSSpec(path, true, &spec);

	return spec;
}


/***************************** SAVE GAME ********************************/

void SaveGame(int slot)
{
#ifdef PANGEA_ENABLE_SCRIPTING
	(void) PangeaScript_CallNativeSaveHook(gRealLevel, slot, false);
#endif
SaveGameType 	saveData;
short			fRefNum;
FSSpec			spec;
OSErr			err;
SDL_Time		timestampNanoseconds = 0;

			/* GET TIMESTAMP */

	SDL_GetCurrentTime(&timestampNanoseconds);

			/*************************/
			/* CREATE SAVE GAME DATA */
			/*************************/	

	SDL_memset(&saveData, 0, sizeof(saveData));
	saveData.version		= SAVE_GAME_VERSION;				// save file version #
	saveData.score 			= gScore;
	saveData.realLevel		= gRealLevel+1;						// save @ beginning of next level
	saveData.numLives 		= gNumLives;
	saveData.health			= gMyHealth;
	saveData.ballTimer		= gBallTimer;
	saveData.numGoldClovers	= gNumGoldClovers;
	saveData.timestamp		= timestampNanoseconds / 1e9;

			/* CREATE & OPEN THE DATA FORK */

	spec = MakeSaveGameFSSpec(slot);

	err = FSpCreate(&spec,'BalZ','BSav',0);
	if (err != noErr)
	{
		DoAlert("Error creating save file");
		return;
	}

	err = FSpOpenDF(&spec, fsWrPerm, &fRefNum);
	if (err != noErr)
	{
		DoAlert("Error opening save file for writing");
		return;
	}

			/* WRITE TO FILE */

	long count = sizeof(SaveGameType);
	err = FSWrite(fRefNum, &count, (Ptr) &saveData);

			/* CLOSE FILE */

	FSClose(fRefNum);

			/* CHECK THAT WRITING WENT OK */

	if (err != noErr || count != sizeof(SaveGameType))
	{
		DoAlert("Error writing save file");
		return;
	}
}


/***************************** LOAD SAVED GAME ********************************/

OSErr GetSaveGameData(int slot, SaveGameType* saveData)
{
short			fRefNum;
OSErr			err;

	FSSpec spec = MakeSaveGameFSSpec(slot);

			/* OPEN THE DATA FORK */

	err = FSpOpenDF(&spec, fsRdPerm, &fRefNum);
	if (err != noErr)
		return err;

			/* READ STRUCT FROM FILE */

	long count = sizeof(SaveGameType);
	err = FSRead(fRefNum, &count, (Ptr) saveData);
	FSClose(fRefNum);

	if (err != noErr)
		return err;

			/* CHECK INTEGRITY */

	if (count != sizeof(SaveGameType))
		return badFileFormat;

	if (saveData->version != SAVE_GAME_VERSION)
		return badFileFormat;

			/* IT'S ALL GOOD */

	return noErr;
}


OSErr LoadSavedGame(int slot)
{
OSErr err;
SaveGameType saveData;

	gCurrentSaveSlot = slot;

	err = GetSaveGameData(slot, &saveData);

	if (err != noErr)
	{
		InitInventoryForGame();
		gRealLevel = 0;
		return noErr;
	}

			/**********************/
			/* USE SAVE GAME DATA */
			/**********************/	

	gScore 			= saveData.score;
	gRealLevel 		= saveData.realLevel;
	gMyHealth		= saveData.health;
	gNumLives		= saveData.numLives;
	gBallTimer		= saveData.ballTimer;
	gNumGoldClovers = saveData.numGoldClovers;

	gRestoringSavedGame = true;

#ifdef PANGEA_ENABLE_SCRIPTING
	(void) PangeaScript_CallNativeSaveHook(gRealLevel, slot, true);
#endif

	return(noErr);
}

OSErr DeleteSavedGame(int slot)
{

	FSSpec spec = MakeSaveGameFSSpec(slot);
	return FSpDelete(&spec);
}



#pragma mark -

/************************** LEVEL METADATA ***************************/

Boolean GetLevelMetadataString(const char *key, char *value, size_t valueSize)
{
	if (!gLevelMetadataJSON || !key || !value || valueSize == 0) return false;
	return PangeaLevelMetadataJSONGetString(
		gLevelMetadataJSON, strlen(gLevelMetadataJSON), key, value, valueSize) != 0;
}

Boolean GetLevelMetadataBool(const char *key, Boolean fallback)
{
	char value[8];
	if (!GetLevelMetadataString(key, value, sizeof(value))) return fallback;
	if (!SDL_strcasecmp(value, "true")) return true;
	if (!SDL_strcasecmp(value, "false")) return false;
	return fallback;
}

Boolean LevelMetadataProfileIs(const char *key, const char *profile, Boolean fallback)
{
	char value[64];
	if (!GetLevelMetadataString(key, value, sizeof(value)) || !SDL_strcasecmp(value, "source-default")) return fallback;
	if (!MetadataProfileValueIsValid(key, value)) return fallback;
	return !SDL_strcasecmp(value, profile);
}

static Boolean MetadataProfileValueIsValid(const char *key, const char *value)
{
	if (!SDL_strcasecmp(key, "level.flyingBeeSetup") || !SDL_strcasecmp(key, "level.workerBeeSetup") || !SDL_strcasecmp(key, "level.splineItems"))
		return !SDL_strcasecmp(value, "hive");
	if (!SDL_strcasecmp(key, "level.beeFlightRegeneration"))
		return !SDL_strcasecmp(value, "flight");
	if (!SDL_strcasecmp(key, "level.queenBeeRegeneration"))
		return !SDL_strcasecmp(value, "queen-bee");
	if (!SDL_strcasecmp(key, "level.antKing"))
		return !SDL_strcasecmp(value, "ant-king");
	if (!SDL_strcasecmp(key, "level.beachNutRegeneration"))
		return !SDL_strcasecmp(value, "beach");
	if (!SDL_strcasecmp(key, "level.dragonflyRide"))
		return !SDL_strcasecmp(value, "beach") || !SDL_strcasecmp(value, "flight");
	if (!SDL_strcasecmp(key, "presentation.levelIntro"))
		return !SDL_strcasecmp(value, "flight");
	if (!SDL_strcasecmp(key, "presentation.infobar"))
		return !SDL_strcasecmp(value, "flight") || !SDL_strcasecmp(value, "queen-bee") ||
			!SDL_strcasecmp(value, "ant-king");
	return false;
}

int LevelMetadataCaseFor(const char *key, int fallback)
{
	char value[64];
	if (!GetLevelMetadataString(key, value, sizeof(value)) || !SDL_strcasecmp(value, "source-default")) return fallback;
	if (!SDL_strcasecmp(key, "level.terrainFamily"))
	{
		if (!SDL_strcasecmp(value, "lawn")) return LEVEL_TYPE_LAWN;
		if (!SDL_strcasecmp(value, "pond")) return LEVEL_TYPE_POND;
		if (!SDL_strcasecmp(value, "forest")) return LEVEL_TYPE_FOREST;
		if (!SDL_strcasecmp(value, "hive")) return LEVEL_TYPE_HIVE;
		if (!SDL_strcasecmp(value, "night")) return LEVEL_TYPE_NIGHT;
		if (!SDL_strcasecmp(value, "anthill")) return LEVEL_TYPE_ANTHILL;
	}
	if (!SDL_strcasecmp(key, "level.area"))
	{
		if (!SDL_strcasecmp(value, "training") || !SDL_strcasecmp(value, "beach") || !SDL_strcasecmp(value, "hive-area") || !SDL_strcasecmp(value, "ant-hill")) return 0;
		if (!SDL_strcasecmp(value, "lawn-area") || !SDL_strcasecmp(value, "flight") || !SDL_strcasecmp(value, "queen-bee") || !SDL_strcasecmp(value, "ant-king")) return 1;
	}
	if (!SDL_strcasecmp(key, "presentation.levelIntro"))
	{
		if (!SDL_strcasecmp(value, "flight")) return LEVEL_NUM_FLIGHT;
	}
	if (!SDL_strcasecmp(key, "presentation.infobar"))
	{
		if (!SDL_strcasecmp(value, "flight")) return LEVEL_NUM_FLIGHT;
		if (!SDL_strcasecmp(value, "queen-bee")) return LEVEL_NUM_QUEENBEE;
		if (!SDL_strcasecmp(value, "ant-king")) return LEVEL_NUM_ANTKING;
	}
	return fallback;
}

void PrepareLevelMetadata(void)
{
	FSSpec spec;
	short fRefNum;
	ClearLevelMetadata();

	if (gLevelTerrainOverride[0] != '\0')
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, gLevelTerrainOverride, &spec);
	else switch (gResourceLevelType)
	{
		case LEVEL_TYPE_LAWN:
			FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, gResourceAreaNum == 0 ? ":Terrain:Training.ter" : ":Terrain:Lawn.ter", &spec);
			break;
		case LEVEL_TYPE_POND:
			FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:Pond.ter", &spec);
			break;
		case LEVEL_TYPE_FOREST:
			FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, gResourceAreaNum == 0 ? ":Terrain:Beach.ter" : ":Terrain:Flight.ter", &spec);
			break;
		case LEVEL_TYPE_HIVE:
			FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, gResourceAreaNum == 0 ? ":Terrain:BeeHive.ter" : ":Terrain:QueenBee.ter", &spec);
			break;
		case LEVEL_TYPE_NIGHT:
			FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:Night.ter", &spec);
			break;
		case LEVEL_TYPE_ANTHILL:
			FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, gResourceAreaNum == 0 ? ":Terrain:AntHill.ter" : ":Terrain:AntKing.ter", &spec);
			break;
		default:
			return;
	}
	fRefNum = FSpOpenResFile(&spec, fsRdPerm);
	if (fRefNum == -1) return;
	UseResFile(fRefNum);
	ReadLevelMetadata();
	CloseResFile(fRefNum);
}

static void ReadLevelMetadata(void)
{
	Handle hand;
	Size size;
	char *json;

#if !defined(PANGEA_ENABLE_LEVEL_METADATA) || !PANGEA_ENABLE_LEVEL_METADATA
	return;
#endif

	ClearLevelMetadata();
	hand = GetResource('Meta', 1000);
	if (!hand) return;
	size = GetHandleSize(hand);
	json = (char *)AllocPtr(size + 1);
	if (!json)
	{
		ReleaseResource(hand);
		return;
	}
	SDL_memcpy(json, *hand, (size_t)size);
	json[size] = '\0';
	if (!MetadataJSONIsValid(json, size) || !PangeaLevelMetadataJSONIsValid(json, (size_t)size, "bugdom1") || !strstr(json, "\"schemaVersion\":1") || !strstr(json, "\"game\":\"bugdom1\"") ||
		!strstr(json, "\"identity\":\"") || !strstr(json, "\"properties\":{"))
		DisposePtr((Ptr)json);
	else
		gLevelMetadataJSON = json;
	ReleaseResource(hand);
}

static void ClearLevelMetadata(void)
{
	if (gLevelMetadataJSON)
	{
		DisposePtr((Ptr)gLevelMetadataJSON);
		gLevelMetadataJSON = NULL;
	}
}

static Boolean MetadataJSONIsValid(const char *json, Size size)
{
	const char *cursor = json;
	const char *end = json + size;
	if (!MetadataParseValue(&cursor, end, 0)) return false;
	MetadataSkipWhitespace(&cursor, end);
	return cursor == end;
}

static void MetadataSkipWhitespace(const char **cursor, const char *end)
{
	while (*cursor < end && (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n'))
		(*cursor)++;
}

static Boolean MetadataParseString(const char **cursor, const char *end)
{
	if (*cursor >= end || **cursor != '\"') return false;
	(*cursor)++;
	while (*cursor < end)
	{
		unsigned char c = (unsigned char)**cursor;
		if (c == '\"')
		{
			(*cursor)++;
			return true;
		}
		if (c < 0x20) return false;
		if (c == '\\')
		{
			(*cursor)++;
			if (*cursor >= end) return false;
			c = (unsigned char)**cursor;
			if (c == 'u')
			{
				int i;
				for (i = 0; i < 4; i++)
				{
					(*cursor)++;
					if (*cursor >= end || !( (**cursor >= '0' && **cursor <= '9') || (**cursor >= 'a' && **cursor <= 'f') || (**cursor >= 'A' && **cursor <= 'F') )) return false;
				}
			}
			else if (c != '\"' && c != '\\' && c != '/' && c != 'b' && c != 'f' && c != 'n' && c != 'r' && c != 't')
				return false;
		}
		(*cursor)++;
	}
	return false;
}

static Boolean MetadataParseValue(const char **cursor, const char *end, int depth)
{
	MetadataSkipWhitespace(cursor, end);
	if (*cursor >= end || depth > 32) return false;
	if (**cursor == '\"') return MetadataParseString(cursor, end);
	if (**cursor == '{')
	{
		(*cursor)++;
		MetadataSkipWhitespace(cursor, end);
		if (*cursor < end && **cursor == '}') { (*cursor)++; return true; }
		while (*cursor < end)
		{
			if (!MetadataParseString(cursor, end)) return false;
			MetadataSkipWhitespace(cursor, end);
			if (*cursor >= end || **cursor != ':') return false;
			(*cursor)++;
			if (!MetadataParseValue(cursor, end, depth + 1)) return false;
			MetadataSkipWhitespace(cursor, end);
			if (*cursor >= end) return false;
			if (**cursor == '}') { (*cursor)++; return true; }
			if (**cursor != ',') return false;
			(*cursor)++;
			MetadataSkipWhitespace(cursor, end);
		}
		return false;
	}
	if (**cursor == '[')
	{
		(*cursor)++;
		MetadataSkipWhitespace(cursor, end);
		if (*cursor < end && **cursor == ']') { (*cursor)++; return true; }
		while (*cursor < end)
		{
			if (!MetadataParseValue(cursor, end, depth + 1)) return false;
			MetadataSkipWhitespace(cursor, end);
			if (*cursor >= end) return false;
			if (**cursor == ']') { (*cursor)++; return true; }
			if (**cursor != ',') return false;
			(*cursor)++;
			MetadataSkipWhitespace(cursor, end);
		}
		return false;
	}
	if ((end - *cursor >= 4 && !SDL_memcmp(*cursor, "true", 4)) ||
		(end - *cursor >= 5 && !SDL_memcmp(*cursor, "false", 5)) ||
		(end - *cursor >= 4 && !SDL_memcmp(*cursor, "null", 4)))
	{
		if (**cursor == 'f') *cursor += 5; else *cursor += 4;
		return true;
	}
	if (**cursor == '-' || (**cursor >= '0' && **cursor <= '9'))
	{
		if (**cursor == '-') (*cursor)++;
		if (*cursor >= end) return false;
		if (**cursor == '0') (*cursor)++;
		else
		{
			if (**cursor < '1' || **cursor > '9') return false;
			while (*cursor < end && **cursor >= '0' && **cursor <= '9') (*cursor)++;
		}
		if (*cursor < end && **cursor == '.')
		{
			(*cursor)++;
			if (*cursor >= end || **cursor < '0' || **cursor > '9') return false;
			while (*cursor < end && **cursor >= '0' && **cursor <= '9') (*cursor)++;
		}
		if (*cursor < end && (**cursor == 'e' || **cursor == 'E'))
		{
			(*cursor)++;
			if (*cursor < end && (**cursor == '+' || **cursor == '-')) (*cursor)++;
			if (*cursor >= end || **cursor < '0' || **cursor > '9') return false;
			while (*cursor < end && **cursor >= '0' && **cursor <= '9') (*cursor)++;
		}
		return true;
	}
	return false;
}

/******************* LOAD PLAYFIELD *******************/

void LoadPlayfield(FSSpec *specPtr)
{
short	fRefNum;

			
				/* OPEN THE REZ-FORK */
			
	fRefNum = FSpOpenResFile(specPtr,fsRdPerm);
	GAME_ASSERT(fRefNum != -1);
	UseResFile(fRefNum);
	ReadLevelMetadata();
	
	
			/* READ PLAYFIELD RESOURCES */

	ReadDataFromPlayfieldFile();
	gLevelType = (u_short)LevelMetadataCaseFor("level.terrainFamily", gResourceLevelType);
	gAreaNum = (u_short)LevelMetadataCaseFor("level.area", gResourceAreaNum);

	
			/* CLOSE REZ FILE */
			
	CloseResFile(fRefNum);


				/***********************/
				/* DO ADDITIONAL SETUP */
				/***********************/
	
	gTerrainTileWidth = (gTerrainTileWidth/SUPERTILE_SIZE)*SUPERTILE_SIZE;		// round size down to nearest supertile multiple
	gTerrainTileDepth = (gTerrainTileDepth/SUPERTILE_SIZE)*SUPERTILE_SIZE;	
	
	gTerrainUnitWidth = gTerrainTileWidth*TERRAIN_POLYGON_SIZE;					// calc world unit dimensions of terrain
	gTerrainUnitDepth = gTerrainTileDepth*TERRAIN_POLYGON_SIZE;
	gNumSuperTilesDeep = gTerrainTileDepth/SUPERTILE_SIZE;						// calc size in supertiles
	gNumSuperTilesWide = gTerrainTileWidth/SUPERTILE_SIZE;	

#if _DEBUG
	SDL_Log("Terrain dimensions: %ld x %ld\n", gNumSuperTilesWide, gNumSuperTilesDeep);
#endif

			/* PRECALC THE TILE SPLIT MODE MATRIX */
			
	CalculateSplitModeMatrix();

		
	BuildTerrainItemList();	

	
				/* INITIALIZE CURRENT SCROLL SETTINGS */

	InitCurrentScrollSettings();
}


/********************** READ DATA FROM PLAYFIELD FILE ************************/

static void ReadDataFromPlayfieldFile(void)
{
Handle					hand;
PlayfieldHeaderType		**header;
long					i,row,col,numLayers;
float					yScale;
short					**xlateTableHand,*xlateTbl;

	if (gDoCeiling)									// see if need to read in ceiling data
		numLayers = 2;
	else
		numLayers = 1;

			/************************/
			/* READ HEADER RESOURCE */
			/************************/

	hand = GetResource('Hedr',1000);
	GAME_ASSERT(hand);

	UNPACK_STRUCTS_HANDLE(PlayfieldHeaderType, 1, hand);
	header = (PlayfieldHeaderType **)hand;
	gNumTerrainItems		= (**header).numItems;
	gTerrainTileWidth		= (**header).mapWidth;
	gTerrainTileDepth		= (**header).mapHeight;	
	gNumTerrainTextureTiles	= (**header).numTilesInList;	
	g3DTileSize				= (**header).tileSize;
	g3DMinY					= (**header).minY;
	g3DMaxY					= (**header).maxY;
	gNumSplines				= (**header).numSplines;
	gNumFences				= (**header).numFences;
	ReleaseResource(hand);

	GAME_ASSERT(gNumTerrainTextureTiles <= MAX_TERRAIN_TILES);

			/**************************/
			/* TILE RELATED RESOURCES */
			/**************************/


			/* READ TILE IMAGE DATA */

	hand = GetResource('Timg',1000);
	GAME_ASSERT(hand);
	{
		DetachResource(hand);							// lets keep this data around
		UNPACK_BE_SCALARS_AUTOSIZEHANDLE(u_short, hand);
		gTileDataHandle = (u_short **)hand;
	}


			/* READ TILE->IMAGE XLATE TABLE */

	hand = GetResource('Xlat',1000);
	{
		GAME_ASSERT(hand);
		DetachResource(hand);
		HLockHi(hand);									// hold on to this rez until we're done reading maps below
		UNPACK_BE_SCALARS_AUTOSIZEHANDLE(short, hand);
		xlateTableHand = (short **)hand;
		xlateTbl = *xlateTableHand;
	}


			/*******************************/
			/* MAP LAYER RELATED RESOURCES */
			/*******************************/

			/* READ FLOOR MAP */
				
	Alloc_2d_array(u_short, gFloorMap, gTerrainTileDepth, gTerrainTileWidth);		// alloc 2D array for floor map
	
	
	hand = GetResource('Layr',1000);												// load map from rez
	GAME_ASSERT(hand);
																					// copy rez into 2D array
	{
		UNPACK_BE_SCALARS_HANDLE(u_short, gTerrainTileDepth * gTerrainTileWidth, hand);
		u_short	*src = (u_short*) *hand;
		for (row = 0; row < gTerrainTileDepth; row++)
			for (col = 0; col < gTerrainTileWidth; col++)
			{
				u_short	tile, imageNum;
				
				tile = *src++;														// get original tile with all bits
				imageNum = xlateTbl[tile & TILENUM_MASK];							// get image # from xlate table
				gFloorMap[row][col] = (tile&(~TILENUM_MASK)) | imageNum;			// insert image # into bitfield
			}
		ReleaseResource(hand);
	}		


	if (gDoCeiling)
	{
				/* READ CEILING MAP */
					
		Alloc_2d_array(u_short, gCeilingMap, gTerrainTileDepth, gTerrainTileWidth);		// alloc 2D array for map
		
		hand = GetResource('Layr',1001);												// load map from rez
		GAME_ASSERT(hand);
																						// copy rez into 2D array
		{
			UNPACK_BE_SCALARS_HANDLE(u_short, gTerrainTileDepth * gTerrainTileWidth, hand);
			u_short	*src = (u_short*) *hand;
			for (row = 0; row < gTerrainTileDepth; row++)
				for (col = 0; col < gTerrainTileWidth; col++)
				{
					u_short	tile, imageNum;

					tile = *src++;														// get original tile with all bits
					imageNum = xlateTbl[tile & TILENUM_MASK];							// get image # from xlate table
					gCeilingMap[row][col] = (tile&(~TILENUM_MASK)) | imageNum;			// insert image # into bitfield
				}
			ReleaseResource(hand);
		}
	}
	
	ReleaseResource((Handle)xlateTableHand);								// we dont need the xlate table anymore


			/* READ HEIGHT DATA MATRIX */
	
	yScale = TERRAIN_POLYGON_SIZE / g3DTileSize;						// need to scale original geometry units to game units
	
	Alloc_2d_array(TerrainYCoordType, gMapYCoords, gTerrainTileDepth+1, gTerrainTileWidth+1);	// alloc 2D array for map
	
	for (i = 0; i < numLayers; i++)
	{		
		hand = GetResource('YCrd',1000+i);
		GAME_ASSERT(hand);
		{
			UNPACK_BE_SCALARS_HANDLE(float, (gTerrainTileDepth + 1) * (gTerrainTileWidth + 1), hand);
			float* src = (float*) *hand;
			for (row = 0; row <= gTerrainTileDepth; row++)
				for (col = 0; col <= gTerrainTileWidth; col++)
					gMapYCoords[row][col].layerY[i] = *src++ * yScale;
			ReleaseResource(hand);
		}
	}			


			/* READ VERTEX COLOR MATRIX */
			
	
	for (i = 0; i < numLayers; i++)
	{		
		Alloc_2d_array(u_short, gVertexColors[i], gTerrainTileDepth+1, gTerrainTileWidth+1);	// alloc 2D array for map
		GAME_ASSERT(gVertexColors[i]);

		hand = GetResource('Vcol',1000+i);
		GAME_ASSERT(hand);
		{
			UNPACK_BE_SCALARS_HANDLE(u_short, (gTerrainTileDepth + 1) * (gTerrainTileWidth + 1), hand);
			u_short* src = (u_short*) *hand;
			for (row = 0; row <= gTerrainTileDepth; row++)
				for (col = 0; col <= gTerrainTileWidth; col++)
					gVertexColors[i][row][col] = *src++;
			ReleaseResource(hand);
		}
	}			

			/* READ SPLIT MODE MATRIX */
	
	Alloc_2d_array(TerrainInfoMatrixType, gMapInfoMatrix, gTerrainTileDepth, gTerrainTileWidth);	// alloc 2D array for map
	
	for (i = 0; i < numLayers; i++)												// floor & ceiling
	{		
		hand = GetResource('Splt',1000+i);
		GAME_ASSERT(hand);
		{
			Byte* src = (Byte*)*hand;  // no need to byteswap - it's just bytes
			for (row = 0; row < gTerrainTileDepth; row++)						
				for (col = 0; col < gTerrainTileWidth; col++)
					gMapInfoMatrix[row][col].splitMode[i] = *src++;
			ReleaseResource(hand);
		}
	}

	
				/**************************/
				/* ITEM RELATED RESOURCES */
				/**************************/
	
				/* READ ITEM LIST */
				
	hand = GetResource('Itms',1000);
	GAME_ASSERT(hand);
	{
		DetachResource(hand);							// lets keep this data around		
		HLockHi(hand);									// LOCK this one because we have the lookup table into this
		gMasterItemList = (TerrainItemEntryType **)hand;
		UNPACK_STRUCTS_HANDLE(TerrainItemEntryType, gNumTerrainItems, gMasterItemList);
	}
		
	
			/****************************/
			/* SPLINE RELATED RESOURCES */
			/****************************/
	
			/* READ SPLINE LIST */
			
	hand = GetResource('Spln',1000);
	if (hand)
	{	
		DetachResource(hand);
		HLockHi(hand);

		// SOURCE PORT NOTE: we have to convert this structure manually,
		// because the original contains 4-byte pointers
		UNPACK_STRUCTS_HANDLE(File_SplineDefType, gNumSplines, hand);

		gSplineList = (SplineDefType **) NewHandleClear(gNumSplines * sizeof(SplineDefType));

		for (i = 0; i < gNumSplines; i++)
		{
			const File_SplineDefType*	srcSpline = &(*((File_SplineDefType **) hand))[i];
			SplineDefType*				dstSpline = &(*gSplineList)[i];

			dstSpline->numItems		= srcSpline->numItems;
			dstSpline->numNubs		= srcSpline->numNubs;
			dstSpline->numPoints	= srcSpline->numPoints;
			dstSpline->bBox			= srcSpline->bBox;
		}

		DisposeHandle(hand);
	}
	else
	{
		gNumSplines = 0;
		gSplineList = nil;
	}


				/* READ SPLINE NUBS, POINTS, AND ITEMS */
	for (i = 0; i < gNumSplines; i++)
	{
		SplineDefType* spline = &(*gSplineList)[i];

				/* HANDLE EMPTY SPLINES */
				//
				// Level 2's spline #16 has a single nub, 0 points, and 0 items.
				// Skip the byteswapping, but do alloc an empty handle, which the game expects.
				//

		if (spline->numNubs < 2)		// Need two nubs to make a line
		{
#if _DEBUG
			SDL_Log("WARNING: Spline #%ld is empty\n", i);
#endif

			GAME_ASSERT(spline->numPoints == 0);
			GAME_ASSERT(spline->numItems == 0);

			spline->nubList = (SplinePointType**) AllocHandle(0);
			spline->pointList = (SplinePointType**) AllocHandle(0);
			spline->itemList = (SplineItemType**) AllocHandle(0);

			continue;
		}
		else
		{
				/* SPLINE HAS SOME NUBS SO IT CAN'T BE EMPTY */

			GAME_ASSERT(spline->numPoints >= 2);
			GAME_ASSERT(spline->numItems >= 1);
		}

				/* READ SPLINE NUB LIST */

		hand = GetResource('SpNb', 1000 + i);
		GAME_ASSERT(hand);
		DetachResource(hand);
		HLockHi(hand);
		UNPACK_STRUCTS_HANDLE(SplinePointType, spline->numNubs, hand);
		spline->nubList = (SplinePointType**)hand;

				/* READ SPLINE POINT LIST */

		hand = GetResource('SpPt', 1000 + i);
		GAME_ASSERT(hand);
		DetachResource(hand);
		HLockHi(hand);
		UNPACK_STRUCTS_HANDLE(SplinePointType, spline->numPoints, hand);
		
		// Convert points to world coordinates immediately
		SplinePointType* pts = (SplinePointType*)*hand;
		for (int p = 0; p < spline->numPoints; p++)
		{
			pts[p].x *= MAP2UNIT_VALUE;
			pts[p].z *= MAP2UNIT_VALUE;
		}

		spline->pointList = (SplinePointType**)hand;
		
				/* READ SPLINE ITEM LIST */

		hand = GetResource('SpIt', 1000 + i);
		GAME_ASSERT(hand);
		DetachResource(hand);
		HLockHi(hand);
		UNPACK_STRUCTS_HANDLE(SplineItemType, spline->numItems, hand);
		spline->itemList = (SplineItemType**)hand;

				/* PATCH SPLINE TO MAKE IT LOOP SEAMLESSLY */

		PatchSplineLoop(spline);
	}
	
	
			/****************************/
			/* FENCE RELATED RESOURCES */
			/****************************/
	
			/* READ FENCE LIST */
			
	hand = GetResource('Fenc',1000);
	if (hand)
	{	
		File_FenceDefType *inData;

		gFenceList = (FenceDefType *)AllocPtr(sizeof(FenceDefType) * gNumFences);	// alloc new ptr for fence data
		GAME_ASSERT(gFenceList);

		UNPACK_STRUCTS_HANDLE(File_FenceDefType, gNumFences, hand);
		inData = (File_FenceDefType *)*hand;							// get ptr to input fence list
		
		for (i = 0; i < gNumFences; i++)								// copy data from rez to new list
		{
			gFenceList[i].type 		= inData[i].type;
			gFenceList[i].numNubs 	= inData[i].numNubs;
			gFenceList[i].nubList 	= nil;
			gFenceList[i].bBox.top		= inData[i].bBox.top * MAP2UNIT_VALUE;
			gFenceList[i].bBox.bottom	= inData[i].bBox.bottom * MAP2UNIT_VALUE;
			gFenceList[i].bBox.left		= inData[i].bBox.left * MAP2UNIT_VALUE;
			gFenceList[i].bBox.right	= inData[i].bBox.right * MAP2UNIT_VALUE;
		}		
		ReleaseResource(hand);
	}
	else
	{
		gNumFences = 0;
		gFenceList = nil;
	}

	
			/* READ FENCE NUB LIST */
			
	for (i = 0; i < gNumFences; i++)
	{
		hand = GetResource('FnNb',1000+i);
		GAME_ASSERT(hand);
		{
			DetachResource(hand);
			HLockHi(hand);
			UNPACK_STRUCTS_HANDLE(FencePointType, gFenceList[i].numNubs, hand);
			
			// Convert nubs to game coordinates immediately after loading
			FencePointType *nubs = (FencePointType *)*hand;
			for (int n = 0; n < gFenceList[i].numNubs; n++)
			{
				nubs[n].x *= MAP2UNIT_VALUE;
				nubs[n].z *= MAP2UNIT_VALUE;
			}

			gFenceList[i].nubList = (FencePointType **)hand;
		}
	}
}

#pragma mark -

/************************** LOAD LEVEL ART ***************************/

void LoadLevelArt(void)
{
FSSpec	spec;
FSSpec	terrainSpec;
Boolean	hasTerrainOverride = (gLevelTerrainOverride[0] != '\0');

			/* LOAD GLOBAL STUFF */

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Global_Models1.3dmf", &spec);
	LoadGrouped3DMF(&spec,MODEL_GROUP_GLOBAL1);	
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Global_Models2.3dmf", &spec);
	LoadGrouped3DMF(&spec,MODEL_GROUP_GLOBAL2);	

	LoadSoundBank(SOUNDBANK_MAIN);

#if PANGEA_SAFE_ITEM_LOADING
	static const char* levelModelFiles[] =
	{
		":Models:Lawn_Models1.3dmf",
		":Models:Pond_Models.3dmf",
		":Models:Forest_Models.3dmf",
		":Models:BeeHive_Models.3dmf",
		":Models:Night_Models.3dmf",
		":Models:AntHill_Models.3dmf",
	};
	for (int levelIndex = 0; levelIndex < 6; levelIndex++)
	{
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, levelModelFiles[levelIndex], &spec);
		LoadGrouped3DMF(&spec, MODEL_GROUP_LEVEL_BANK_BASE + levelIndex * 2);
	}
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Lawn_Models2.3dmf", &spec);
	LoadGrouped3DMF(&spec, MODEL_GROUP_LEVEL_BANK_BASE + 1);

	LoadSoundBank(SOUNDBANK_LAWN);
	LoadSoundBank(SOUNDBANK_POND);
	LoadSoundBank(SOUNDBANK_HIVE);
	LoadSoundBank(SOUNDBANK_NIGHT);
	LoadSoundBank(SOUNDBANK_FOREST);
	LoadSoundBank(SOUNDBANK_ANTHILL);
#endif

	LoadASkeleton(SKELETON_TYPE_ME);			
	LoadASkeleton(SKELETON_TYPE_LADYBUG);			
	LoadASkeleton(SKELETON_TYPE_BUDDY);			
	
			/*****************************/
			/* LOAD LEVEL SPECIFIC STUFF */
			/*****************************/

		/* Apply terrain override from level editor if provided */
	if (hasTerrainOverride)
	{
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, gLevelTerrainOverride, &terrainSpec);
		gLevelTerrainOverride[0] = '\0';	// consume override: terrainSpec is already set from it
	}
		
	switch(gResourceLevelType)
	{
				/***********************/
				/* LEVEL 1: THE GARDEN */
				/***********************/
				
		case	LEVEL_TYPE_LAWN:
				if (!hasTerrainOverride)
				{
					if (gResourceAreaNum == 0)
						FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:Training.ter", &terrainSpec);
					else
						FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:Lawn.ter", &terrainSpec);
				}
				
				LoadPlayfield(&terrainSpec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Lawn_Models1.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Lawn_Models2.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC2);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_BOXERFLY);			
				LoadASkeleton(SKELETON_TYPE_SLUG);			
				LoadASkeleton(SKELETON_TYPE_ANT);			

				/* LOAD SOUNDS */

				LoadSoundBank(SOUNDBANK_LAWN);
				break;


				/*****************/
				/* LEVEL 2: POND */
				/*****************/
				
		case	LEVEL_TYPE_POND:
				if (!hasTerrainOverride)
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:Pond.ter", &terrainSpec);
				LoadPlayfield(&terrainSpec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Pond_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_MOSQUITO);			
				LoadASkeleton(SKELETON_TYPE_WATERBUG);			
				LoadASkeleton(SKELETON_TYPE_PONDFISH);			
				LoadASkeleton(SKELETON_TYPE_SKIPPY);			
				LoadASkeleton(SKELETON_TYPE_SLUG);			


				/* LOAD SOUNDS */

				LoadSoundBank(SOUNDBANK_POND);
				break;


				/*******************/
				/* LEVEL 3: FOREST */
				/*******************/
				
		case	LEVEL_TYPE_FOREST:
				if (!hasTerrainOverride)
				{
					if (gResourceAreaNum == 0)
						FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:Beach.ter", &terrainSpec);
					else
						FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:Flight.ter", &terrainSpec);
				}
				LoadPlayfield(&terrainSpec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Forest_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_DRAGONFLY);			
				LoadASkeleton(SKELETON_TYPE_FOOT);	
				LoadASkeleton(SKELETON_TYPE_SPIDER);	
				LoadASkeleton(SKELETON_TYPE_CATERPILLER);	
				LoadASkeleton(SKELETON_TYPE_BAT);	
				LoadASkeleton(SKELETON_TYPE_FLYINGBEE);			
				LoadASkeleton(SKELETON_TYPE_ANT);			
				
				/* LOAD SOUNDS */

				LoadSoundBank(SOUNDBANK_FOREST);

				break;


				/*************************/
				/* LEVEL 4:  BEE HIVE    */
				/*************************/
				
		case	LEVEL_TYPE_HIVE:
			
				if (!hasTerrainOverride)
				{
					if (gResourceAreaNum == 0)
						FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:BeeHive.ter", &terrainSpec);
					else
						FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:QueenBee.ter", &terrainSpec);
				}
				LoadPlayfield(&terrainSpec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:BeeHive_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_LARVA);			
				LoadASkeleton(SKELETON_TYPE_FLYINGBEE);			
				LoadASkeleton(SKELETON_TYPE_WORKERBEE);			
				LoadASkeleton(SKELETON_TYPE_QUEENBEE);			

				
				/* LOAD SOUNDS */

				LoadSoundBank(SOUNDBANK_HIVE);

				break;


				/*******************/
				/* LEVEL 5:  NIGHT */
				/*******************/
				
		case	LEVEL_TYPE_NIGHT:
				if (!hasTerrainOverride)
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:Night.ter", &terrainSpec);
				LoadPlayfield(&terrainSpec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Night_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_FIREANT);			
				LoadASkeleton(SKELETON_TYPE_FIREFLY);			
				LoadASkeleton(SKELETON_TYPE_CATERPILLER);	
				LoadASkeleton(SKELETON_TYPE_SLUG);	
				LoadASkeleton(SKELETON_TYPE_ROACH);	
				LoadASkeleton(SKELETON_TYPE_ANT);	

				
				/* LOAD SOUNDS */

				LoadSoundBank(SOUNDBANK_NIGHT);
				break;

	
				/*************************/
				/* LEVEL 6:  ANT HILL    */
				/*************************/
				
		case	LEVEL_TYPE_ANTHILL:
				if (!hasTerrainOverride)
				{
					if (gResourceAreaNum == 0)
						FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:AntHill.ter", &terrainSpec);
					else
						FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Terrain:AntKing.ter", &terrainSpec);
				}
				LoadPlayfield(&terrainSpec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:AntHill_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				if (LevelMetadataProfileIs("level.antKing", "ant-king", gRealLevel == LEVEL_NUM_ANTKING))
					LoadASkeleton(SKELETON_TYPE_KINGANT);			
					
				LoadASkeleton(SKELETON_TYPE_SLUG);			
				LoadASkeleton(SKELETON_TYPE_ANT);			
				LoadASkeleton(SKELETON_TYPE_FIREANT);			
				LoadASkeleton(SKELETON_TYPE_ROOTSWING);			
				LoadASkeleton(SKELETON_TYPE_ROACH);	

				/* LOAD SOUNDS */

				LoadSoundBank(SOUNDBANK_ANTHILL);
				break;

		default:
				DoFatalAlert("LoadLevelArt: unsupported level #");
	}
	
	

			/* CAST SHADOWS */
			
	DoItemShadowCasting();
}

#pragma once

		/* MY BUILD OPTIONS */

// Default to little-endian
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
	#define __LITTLE_ENDIAN__ 1
#endif

#if _MSC_VER
	#define _Static_assert static_assert
#endif

#ifdef __cplusplus
extern "C"
{
#endif

// If enabled, "VIP" enemies are always allowed to spawn and they don't count towards the global enemy budget.
// VIP enemy kinds are: GiantLizard and Flytrap.
#define VIP_ENEMIES 1

		/* HEADERS */

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#include "Pomme.h"

#include "version.h"
#include "pool.h"
#include "globals.h"
#include "structs.h"
#include "metaobjects.h"
#include "ogl_support.h"
#include "main.h"
#include "mobjtypes.h"
#include "misc.h"
#include "sound2.h"
#include "sobjtypes.h"
#include "sprites.h"
#include "sparkle.h"
#include "bg3d.h"
#include "camera.h"
#include "collision.h"
#include 	"input.h"
#include "file.h"
#include "window.h"
#include "player.h"
#include "terrain.h"
#include "humans.h"
#include "skeletonobj.h"
#include "skeletonanim.h"
#include "skeletonjoints.h"
#include	"infobar.h"
#include "triggers.h"
#include "effects.h"
#include "shards.h"
#include "bones.h"
#include "vaportrails.h"
#include "splineitems.h"
#include "mytraps.h"
#include "enemy.h"
#include "items.h"
#include "sky.h"
#include "water.h"
#include "fences.h"
#include "miscscreens.h"
#include "objects.h"
#include "lzss.h"
#include "3dmath.h"
#include "ogl_functions.h"
#include "localization.h"
#include "textmesh.h"
#include "tga.h"
#include "menu.h"

// WebGL compatibility layer (must be included after other headers)
#include "gl_compat.h"

// Emscripten browser yield macro.
// Call this inside long-running while-loops so the browser event loop
// can process rendering, input, and other events.
// On non-Emscripten builds this is a no-op.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#ifdef __cplusplus
extern "C" {
#endif
void emscripten_fast_yield(void);
#ifdef __cplusplus
}
#endif
#define GAME_YIELD_BROWSER() emscripten_fast_yield()
#else
#define GAME_YIELD_BROWSER() ((void)0)
#endif

		/* EXTERNS */

extern	BG3DFileContainer		*gBG3DContainerList[];
extern	Boolean					gAllowAudioKeys;
extern	Boolean					gAutoRotateCamera;
extern	Boolean					gBrainBossDead;
extern	Boolean					gBumperCarGateBlown[];
extern	Boolean					gDisableAnimSounds;
extern	Boolean					gDisableHiccupTimer;
extern	Boolean					gDoDeathExit;
extern	Boolean					gDoJumpJetAtApex;
extern	Boolean					gDrawLensFlare;
extern	Boolean					gExplodePlayerAfterElectrocute;
extern	Boolean					gForceCameraAlignment;
extern	Boolean					gFreezeCameraFromXZ;
extern	Boolean					gFreezeCameraFromY;
extern	Boolean					gG4;
extern	Boolean					gGameOver;
extern	Boolean					gGamePaused;
extern	Boolean					gHelpMessageDisabled[NUM_HELP_MESSAGES];
extern	Boolean					gIceCracked;
extern	Boolean					gIsInGame;
extern	Boolean					gLevelCompleted;
extern	Boolean					gMouseMotionNow;
extern	Boolean					gMyState_Lighting;
extern	Boolean					gPlayerFellIntoBottomlessPit;
extern	Boolean					gPlayerHasLanded;
extern	Boolean					gPlayerIsDead;
extern	Boolean					gPlayingFromSavedGame;
extern	Boolean					gSkipFluff;
extern	Boolean					gUserPrefersGamepad;
extern	Byte					**gMapSplitMode;
extern	Byte					gDebugMode;
extern	Byte					gHumansInSaucerList[];
extern	ChannelInfoType			gChannelInfo[];
extern	CollisionBoxType 		*gSaucerIceBounds;
extern	CollisionRec			gCollisionList[];
extern	FSSpec					gDataSpec;
extern	FenceDefType			*gFenceList;
extern	HighScoreType			gHighScores[];
extern	MOMaterialObject		*gMostRecentMaterial;
extern	MOMaterialObject		*gSuperTileTextureObjects[MAX_SUPERTILE_TEXTURES];

		/* LEVEL EDITOR / WASM INTERFACE */

extern	int						gDirectLevelNum;		// -1 = use default game flow; >=0 = jump directly to this level
extern	char					gTerrainOverridePath[512];	// empty string = use built-in; otherwise path overrides terrain file for current level

FSSpec* GetTerrainOverrideSpec(void);
extern	MOVertexArrayData		**gLocalTriMeshesOfSkelType;
extern	MetaObjectPtr			gBG3DGroupList[MAX_BG3D_GROUPS][MAX_OBJECTS_IN_GROUP];
extern	NewObjectDefinitionType	gNewObjectDefinition;
extern	NewParticleGroupDefType	gNewParticleGroupDef;
extern	OGLBoundingBox			gObjectGroupBBoxList[MAX_BG3D_GROUPS][MAX_OBJECTS_IN_GROUP];
extern	OGLBoundingBox			gWaterBBox[];
extern	OGLColorRGB				gGlobalColorFilter;
extern	OGLMatrix4x4			*gCurrentObjMatrix;
extern	OGLMatrix4x4			gViewToFrustumMatrix;
extern	OGLMatrix4x4			gWorldToFrustumMatrix;
extern	OGLMatrix4x4			gWorldToViewMatrix;
extern	OGLMatrix4x4			gWorldToWindowMatrix;
extern	OGLPoint2D				gBestCheckpointCoord;
extern	OGLPoint2D				gRocketShipHotZone[4];
extern	OGLPoint3D				gCoord;
extern	OGLSetupOutputType		*gGameViewInfoPtr;
extern	OGLVector2D				gCameraControlDelta;
extern	OGLVector3D				gDelta;
extern	OGLVector3D				gRecentTerrainNormal;
extern	OGLVector3D				gWorldSunDirection;
extern	ObjNode					*gAlienSaucer;
extern	ObjNode					*gCurrentNode;
extern	ObjNode					*gCurrentZip;
extern	ObjNode					*gExitRocket;
extern	ObjNode					*gFirstNodePtr;
extern	ObjNode					*gMagnetMonsterList[MAX_MAGNET_MONSTERS];
extern	ObjNode					*gPlayerRocketSled;
extern	ObjNode					*gPlayerSaucer;
extern	ObjNode					*gSaucerTarget;
extern	ObjNode					*gSoapBubble;
extern	ObjNode					*gTargetPickup;
extern	ObjNode					*gTractorBeamObj;
extern	Pool 					*gParticleGroupPool;
extern	Pool					*gShardPool;
extern	Pool					*gSparklePool;
extern	PrefsType				gGamePrefs;
extern	SDL_Gamepad				*gSDLGamepad;
extern	SDL_GLContext			gAGLContext;
extern	SDL_Window				*gSDLWindow;
extern	SparkleType				gSparkles[MAX_SPARKLES];
extern	SplineDefType			**gSplineList;
extern	SpriteType				*gSpriteGroupList[MAX_SPRITE_GROUPS];
extern	SuperTileGridType		**gSuperTileTextureGrid;
extern	SuperTileItemIndexType	**gSuperTileItemIndexGrid;
extern	SuperTileMemoryType		gSuperTileMemoryList[];
extern	SuperTileStatus			**gSuperTileStatusGrid;
extern	TerrainItemEntryType	**gMasterItemList;
extern	TileAttribType			**gTileAttribList;
extern	WaterDefType			**gWaterListHandle;
extern	WaterDefType			*gWaterList;
extern	char					gTextInput[64];
extern	const KeyBinding		kDefaultKeyBindings[NUM_CONTROL_NEEDS];
extern	const MenuStyle			kDefaultMenuStyle;
extern	const OGLPoint3D		gPlayerMuzzleTipOff;
extern	const int				kLevelSoundBanks[NUM_LEVELS];
extern	float					gSkyAltitudeY;
extern	float					**gMapYCoords;
extern	float					**gMapYCoordsOriginal;
extern	float					g2DLogicalHeight;
extern	float					g2DLogicalWidth;
extern	float					gAutoFadeEndDist;
extern	float					gAutoFadeRange_Frac;
extern	float					gAutoFadeStartDist;
extern	float					gAutoRotateCameraSpeed;
extern	float					gBeamCharge;
extern	float					gBestCheckpointAim;
extern	float					gCameraDistFromMe;
extern	float					gCameraLookAtYOff;
extern	float					gCameraUserRotY;
extern	float					gCurrentAspectRatio;
extern	float					gCurrentMaxSpeed;
extern	float					gDeathTimer;
extern	float					gDischargeTimer;
extern	float					gFramesPerSecond;
extern	float					gFramesPerSecondFrac;
extern	float					gLoopUpdateTimeMs;
extern	float					gLoopTerrainTimeMs;
extern	float					gLoopRenderTimeMs;
extern	int						gDrawCallsThisFrame;
extern	int						gVerticesThisFrame;
extern	int						gBufferUploadsThisFrame;
extern	int						gBufferUploadBytesThisFrame;
extern	int						gCacheLookupsThisFrame;
extern	int						gCacheHitsThisFrame;
extern	int						gCacheMissesThisFrame;
extern	int						gCacheEvictionsThisFrame;
extern	int						gCacheInvalidationsThisFrame;
extern	int						gIndexScansThisFrame;
extern	int						gIndicesScannedThisFrame;
extern	float					gGammaFadeFrac;
extern	float					gGlobalTransparency;
extern	float					gGravity;
extern	Boolean					gHasLevelGravityMetadata;
extern	float					gLevelGravityMetadata;
extern	float					gHumanScaleRatio;
extern	float					gJumpJetWarningCooldown;
extern	float					gLevelCompletedCoolDownTimer;
extern	float					gMinHeightOffGround;
extern	float					gPlayerBottomOff;
extern	float					gPlayerToCameraAngle;
extern	float					gRocketScaleAdjust;
extern	float					gSpinningPlatformRot;
extern	float					gTargetMaxSpeed;
extern	float					gTileSlipperyFactor;
extern	Boolean					gHasLevelSlipperinessMetadata;
extern	float					gLevelSlipperinessMetadata;
extern	Boolean				GetLevelMetadataString(const char *key, char *value, size_t valueSize);
extern	Boolean				GetLevelMetadataFloat(const char *key, float *value);
extern	Boolean				GetLevelMetadataBool(const char *key, Boolean fallback);
extern	Boolean				LevelMetadataUsesCustomValues(const char *key);
extern	Boolean				LevelMetadataProfileIs(const char *key, const char *profile, Boolean fallback);
extern	int					LevelMetadataCaseFor(const char *key, int fallback);
extern	float					gTimeSinceLastShoot;
extern	float					gTimeSinceLastThrust;
extern	int						gGameWindowHeight;
extern	int						gGameWindowWidth;
extern	int						gLevelNum;
extern	int						gMaxEnemies;
extern	int						gNumEnemies;
extern	int						gNumEnemyOfKind[NUM_ENEMY_KINDS];
extern	int						gNumHumansInLevel;
extern	int						gNumHumansInTransit;
extern	int						gNumHumansRescuedTotal;
extern	int						gNumIceCracks;
extern	int						gNumObjectNodes;
extern	int						gNumObjectsInBG3DGroupList[MAX_BG3D_GROUPS];
extern	int						gActiveItemModelGroup;
int GetOttoItemModelGroup(int itemType);
int GetOttoCurrentModelGroup(void);
int GetOttoLevelModelGroup(int level);
int GetOttoLevelSpriteGroup(int level);
extern	int						gNumSpritesInGroupList[MAX_SPRITE_GROUPS];
extern	int						gPolysThisFrame;
extern	int						gVRAMUsedThisFrame;
extern	int						gNumFences;
extern	int						gNumSplines;
extern	int						gNumSuperTilesDeep;
extern	int						gNumSuperTilesWide;
extern	int						gNumUniqueSuperTiles;
extern	int						gNumWaterPatches;
extern	long					gPrefsFolderDirID;
extern	int						gTerrainTileDepth;
extern	int						gTerrainTileWidth;
extern	int						gTerrainUnitDepth;
extern	int						gTerrainUnitWidth;
extern	int						*gTerrainItemFileIDs;
extern	short					gBeamMode;
extern	short					gBeamModeSelected;
extern	short					gBestCheckpointNum;
extern	short					gDisplayedHelpMessage;
extern	int						gNumCollisions;
extern	int						gNumFencesDrawn;
extern	int						gNumHumansInSaucer;
extern	int						gNumHumansRescuedOfType[NUM_HUMAN_TYPES];
extern	int						gNumSuperTilesDrawn;
extern	int						gNumTerrainDeformations;
extern	int						gNumTerrainItems;
extern	int						gNumWaterDrawn;
extern	short					gPrefsFolderVRefNum;
extern	uint32_t				gAutoFadeStatusBits;
extern	uint32_t				gGameFrameNum;
extern	uint32_t				gGlobalMaterialFlags;
extern	uint32_t				gLoadedScore;
extern	uint32_t				gScore;
extern	uint16_t				**gTileGrid;
extern	uint16_t				gTileAttribFlags;

#ifdef __cplusplus
};
#endif

/****************************/
/*    NANOSAUR 2 - MAIN 	*/
/* By Brian Greenstone      */
/* (c)2003 Pangea Software  */
/* (c)2022 Iliyas Jorio     */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "game.h"

#if __EMSCRIPTEN__
extern int PangeaNet_IsEnabled(void);
extern int PangeaNet_IsHost(void);
extern int PangeaNet_GetPlayerCount(void);
extern int PangeaNet_GetRemoteLifecycleReason(void);
extern void PangeaNet_UpdateMatchLifecycle(void);
extern void PangeaNet_PublishLocalMatchLifecycle(void);
extern void PangeaNet_ClientSendInput(void);
extern void PangeaNet_HostReceiveInputs(void);
extern void PangeaNet_HostSendSnapshot(void);
extern void PangeaNet_ClientApplySnapshot(void);
#endif
#include "profiling.h"
#include "uieffects.h"
#ifdef PANGEA_ENABLE_SCRIPTING
#include "ScriptBindings.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/****************************/
/*    PROTOTYPES            */
/****************************/

static void CleanupLevel(void);
static void PlayGame_Adventure(void);
static void PlayGame_Versus(void);
static void InitLevel(void);
static void PlayLevel(void);
static void DrawLevelCallback(void);
static void MoveTimeDemoOnSpline(ObjNode *theNode);
static void ShowTimeDemoResults(int numFrames, float numSeconds, float averageFPS);
static Boolean NS2ShouldProcessDeathTimerForPlayer(int playerIndex);

#ifdef __EMSCRIPTEN__
static bool PlayLevelTick(void);
#endif


/****************************/
/*    CONSTANTS             */
/****************************/


/****************************/
/*    VARIABLES             */
/****************************/

short	gPrefsFolderVRefNum;
long	gPrefsFolderDirID;

Byte				gDebugMode = 0;				// 0 == none, 1 = fps, 2 = all

uint32_t				gAutoFadeStatusBits;

OGLSetupOutputType		*gGameViewInfoPtr = nil;

PrefsType			gGamePrefs;

Boolean				gTimeDemo = false;
Boolean				gIsInGame = false;
uint32_t			gTimeDemoStartTime, gTimeDemoEndTime;



OGLVector3D			gWorldSunDirection;		// also serves as lense flare vector
OGLColorRGBA		gFillColor1 = { .8, .8, .7, 1};

uint32_t				gGameFrameNum = 0;
float				gGameLevelTimer = 0;

Boolean				gPlayingFromSavedGame 	= false;
Boolean				gGameOver 				= false;
Boolean				gLevelCompleted 		= false;
float				gLevelCompletedCoolDownTimer = 0;
Boolean				gSkipLevelIntro			= false;

short				gLevelNum;
short				gVSMode = VS_MODE_NONE;						// nano vs. nano mode

float				gRaceReadySetGoTimer;

			/* CHECKPOINTS */

short				gBestCheckpointNum[MAX_PLAYERS];
OGLPoint3D			gBestCheckpointCoord[MAX_PLAYERS];
float				gBestCheckpointAim[MAX_PLAYERS];


			/* LEVEL SONGS */

const short gLevelSongs[NUM_LEVELS] =
{
	SONG_LEVEL1,				// ADVENTURE LEVEL 1
	SONG_LEVEL2,				// ADVENTURE LEVEL 2
	SONG_LEVEL3,				// ADVENTURE LEVEL 3

	SONG_LEVEL3,				// RACE 1
	SONG_LEVEL2,				// RACE 2
	SONG_LEVEL1,				// BATTLE 1
	SONG_LEVEL2,				// BATTLE 2
	SONG_LEVEL3,				// CAPTURE THE FLAG 1
	SONG_LEVEL1,				// CAPTURE THE FLAG 2
};

short GetVSModeForLevel(short levelNum)
{
	switch (levelNum)
	{
		case LEVEL_NUM_RACE1:
		case LEVEL_NUM_RACE2:
			return VS_MODE_RACE;

		case LEVEL_NUM_BATTLE1:
		case LEVEL_NUM_BATTLE2:
			return VS_MODE_BATTLE;

		case LEVEL_NUM_FLAG1:
		case LEVEL_NUM_FLAG2:
			return VS_MODE_CAPTURETHEFLAG;

		default:
			return VS_MODE_NONE;
	}
}

#if __EMSCRIPTEN__
static const char* Nanosaur2VSModeName(short vsMode)
{
	switch (vsMode)
	{
		case VS_MODE_NONE:
			return "adventure";

		case VS_MODE_RACE:
			return "race";

		case VS_MODE_BATTLE:
			return "battle";

		case VS_MODE_CAPTURETHEFLAG:
			return "capture-the-flag";

		default:
			return "unknown";
	}
}
#endif

static Boolean NS2ShouldProcessDeathTimerForPlayer(int playerIndex)
{
#if __EMSCRIPTEN__
	if (PangeaNet_IsEnabled() && !PangeaNet_IsHost())
	{
		return playerIndex == PangeaNet_GetLocalPlayerIndex();
	}
#endif

	(void) playerIndex;

	return true;
}




//======================================================================================
//======================================================================================
//======================================================================================


/****************** TOOLBOX INIT  *****************/

void ToolBoxInit(void)
{
	MyFlushEvents();

		/* FIRST VERIFY SYSTEM BEFORE GOING TOO FAR */

	VerifySystem();
	InitInput();
	InitProfiling();

			/********************/
			/* INIT PREFERENCES */
			/********************/

	InitPrefsFolder(false);
	InitDefaultPrefs();
	LoadPrefs();

#ifdef __EMSCRIPTEN__
	// Fullscreen on the web requires user interaction; run windowed by default.
	gGamePrefs.fullscreen = false;
#endif
	SetFullscreenMode(true);



			/* BOOT OGL */

	OGL_Boot();
}


/************************* INIT DEFAULT PREFS **************************/

void InitDefaultPrefs(void)
{
	SDL_memset(&gGamePrefs, 0, sizeof(gGamePrefs));

		/* DETERMINE WHAT LANGUAGE IS ON THIS MACHINE */

	gGamePrefs.fullscreen				= true;
	gGamePrefs.vsync					= true;

	gGamePrefs.language				= GetBestLanguageIDFromSystemLocale();
	gGamePrefs.cutsceneSubtitles	= !IsNativeEnglishSystem();		// enable subtitles if user's native language isn't English

	gGamePrefs.lowRenderQuality		= false;
	gGamePrefs.splitScreenMode		= SPLITSCREEN_MODE_VERT;
	gGamePrefs.stereoGlassesMode	= STEREO_GLASSES_MODE_OFF;
	gGamePrefs.anaglyphCalibrationRed = DEFAULT_ANAGLYPH_R;
	gGamePrefs.anaglyphCalibrationGreen = DEFAULT_ANAGLYPH_G;
	gGamePrefs.anaglyphCalibrationBlue = DEFAULT_ANAGLYPH_B;
	gGamePrefs.doAnaglyphChannelBalancing = true;

	gGamePrefs.showTargetingCrosshairs	= true;
	gGamePrefs.kiddieMode				= false;

	gGamePrefs.force4x3HUD				= false;
	gGamePrefs.hudScale					= 100;

	gGamePrefs.mouseSensitivityLevel	= DEFAULT_MOUSE_SENSITIVITY_LEVEL;

	gGamePrefs.musicVolumePercent	= 70;
	gGamePrefs.sfxVolumePercent		= 70;

	gGamePrefs.rumbleIntensity		= 100;

	_Static_assert(sizeof(gGamePrefs.bindings) == sizeof(kDefaultInputBindings), "input binding size mismatch: prefs vs defaults");
	SDL_memcpy(&gGamePrefs.bindings, &kDefaultInputBindings, sizeof(kDefaultInputBindings));
}


#pragma mark -


/*********************** PLAY GAME ADVENTURE **************************/
//
// Play the multi-level adventure mode
//

static void PlayGame_Adventure(void)
{
			/* GAME INITIALIZATION */

#ifdef PANGEA_ENABLE_SCRIPTING
	Nanosaur2Script_Init();
#endif

	InitPlayerInfo_Game();					// init player info for entire game


			/*********************************/
			/* PLAY THRU LEVELS SEQUENTIALLY */
			/*********************************/


	for (;gLevelNum <= LEVEL_NUM_ADVENTURE3; gLevelNum++)
	{
				/* DO LEVEL INTRO */

		PlaySong(gLevelSongs[gLevelNum], true);

		if (!gSkipLevelIntro)
		{
			if ((gLevelNum == 0) || gPlayingFromSavedGame)
				DoLevelIntroScreen(INTRO_MODE_NOSAVE);
			else
				DoLevelIntroScreen(INTRO_MODE_SAVEGAME);
		}
		gSkipLevelIntro = false;			// reset skip flag

		MyFlushEvents();


	        /* LOAD ALL OF THE ART & STUFF */

#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_LoadLevelConfig(gLevelNum);
#endif

		InitLevel();

#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_OnLevelLoad(gLevelNum);
#endif


			/***********/
	        /* PLAY IT */
	        /***********/

		PlayLevel();

#ifdef PANGEA_ENABLE_SCRIPTING
		if (gLevelCompleted)
			Nanosaur2Script_OnLevelComplete(gLevelNum);
		Nanosaur2Script_OnLevelUnload(gLevelNum);
#endif

		gPlayingFromSavedGame = false;		// once we've completed a level after restoring, we're not really playing from a saved game anymore


			/* CLEANUP LEVEL */

		MyFlushEvents();
//		GammaFadeOut();
		CleanupLevel();

			/***************/
			/* SEE IF LOST */
			/***************/

		if (gGameOver)									// bail out if game has ended
		{
			break;
		}


		/* DO END-LEVEL BONUS SCREEN */

		if (gLevelNum == LEVEL_NUM_ADVENTURE3)				// if just won game then do win screen first!
			DoWinScreen();

	}


	gPlayingFromSavedGame = false;

#ifdef PANGEA_ENABLE_SCRIPTING
	Nanosaur2Script_Shutdown();
#endif
}


/*********************** PLAY GAME: VERSUS **************************/
//
// Play one of the 2-player versus modes.
//

static void PlayGame_Versus(void)
{

			/* GAME INITIALIZATION */

	InitPlayerInfo_Game();														// init player info for entire game

			/* DO LEVEL INTRO */

	PlaySong(gLevelSongs[gLevelNum], true);

	MyFlushEvents();


        /* LOAD ALL OF THE ART & STUFF */

	InitLevel();


		/***********/
        /* PLAY IT */
        /***********/

	PlayLevel();


		/* CLEANUP LEVEL */

	MyFlushEvents();
//	GammaFadeOut();
	CleanupLevel();
}


#pragma mark -

/***************** INIT LEVEL ************************/
//
// Sets up the OpenGL draw context and loads all the data files
// for the level we're about to play.
//

static void InitLevel(void)
{
short				i;
OGLSetupInputType	viewDef;
float				metadataValue;


	if (gTimeDemo)					// if time demo always reset random seed
		SetMyRandomSeed(0);



		/*********************/
		/* INIT COMMON STUFF */
		/*********************/

	gGameFrameNum 		= 0;
	gGameLevelTimer 	= 0;
	gGameOver 			= false;
	gLevelCompleted 	= false;


	for (i = 0; i < gNumPlayers; i++)
	{
		gBestCheckpointNum[i]	= -1;
		gPlayerInfo[i].objNode = nil;
	}


			/*************/
			/* MAKE VIEW */
			/*************/

	SetTerrainScale(DEFAULT_TERRAIN_SCALE);								// set scale to some default for now


			/* SETUP VIEW DEF */

	OGL_NewViewDef(&viewDef);

	viewDef.camera.hither 			= 20;
	viewDef.camera.fov 				= GetSplitscreenPaneFOV();
	viewDef.view.clearBackBuffer	= false;	//true;
	viewDef.camera.yon 				= (gSuperTileActiveRange * SUPERTILE_SIZE * gTerrainPolygonSize) * .95f;

	switch(LevelMetadataCaseFor("level.rendering", GetDefaultBiomeForLevel(gLevelNum)))
	{
		case	BIOME_DESERT:
				viewDef.view.clearColor.r 		= .968;
				viewDef.view.clearColor.g 		= .537;
				viewDef.view.clearColor.b		= .278;
				viewDef.styles.useFog			= true;
				viewDef.styles.fogStart			= viewDef.camera.yon * .4f;
				viewDef.styles.fogEnd			= viewDef.camera.yon * .95f;
				viewDef.lights.ambientColor.r 		= .45;
				viewDef.lights.ambientColor.g 		= .45;
				viewDef.lights.ambientColor.b 		= .45;
				gWorldSunDirection.x = .4;
				gWorldSunDirection.y = -.3;
				gWorldSunDirection.z = .2;
				gFillColor1.r = .6;
				gFillColor1.g = .6;
				gFillColor1.b = .6;
				gDrawLensFlare = true;
				break;

		case	BIOME_SWAMP:
				viewDef.view.clearColor.r 		= .568;
				viewDef.view.clearColor.g 		= .243;
				viewDef.view.clearColor.b		= .125;
				viewDef.styles.useFog			= true;
				viewDef.styles.fogStart			= viewDef.camera.yon * .4f;
				viewDef.styles.fogEnd			= viewDef.camera.yon * .95f;
				viewDef.lights.ambientColor.r 		= .45;
				viewDef.lights.ambientColor.g 		= .45;
				viewDef.lights.ambientColor.b 		= .45;
				gWorldSunDirection.x = .4;
				gWorldSunDirection.y = -.3;
				gWorldSunDirection.z = .2;
				gFillColor1.r = .6;
				gFillColor1.g = .6;
				gFillColor1.b = .6;
				gDrawLensFlare = true;
				break;

		default:
				viewDef.view.clearColor.r 		= .43;
				viewDef.view.clearColor.g 		= .33;
				viewDef.view.clearColor.b		= .7;
				viewDef.styles.useFog			= true;
				viewDef.styles.fogStart			= viewDef.camera.yon * .35f;
				viewDef.styles.fogEnd			= viewDef.camera.yon * .95f;
				viewDef.lights.ambientColor.r 		= .4;
				viewDef.lights.ambientColor.g 		= .4;
				viewDef.lights.ambientColor.b 		= .4;
				gWorldSunDirection.x = .4;
				gWorldSunDirection.y = -.5;
				gWorldSunDirection.z = .5;
				gFillColor1.r = .7;
				gFillColor1.g = .7;
				gFillColor1.b = .7;
				gDrawLensFlare = true;
				break;

	}

	if (LevelMetadataUsesCustomValues("level.rendering"))
	{
		if (GetLevelMetadataFloat("level.renderingBackgroundR", &metadataValue)) viewDef.view.clearColor.r = metadataValue;
		if (GetLevelMetadataFloat("level.renderingBackgroundG", &metadataValue)) viewDef.view.clearColor.g = metadataValue;
		if (GetLevelMetadataFloat("level.renderingBackgroundB", &metadataValue)) viewDef.view.clearColor.b = metadataValue;
		if (GetLevelMetadataFloat("level.renderingFogStart", &metadataValue)) viewDef.styles.fogStart = viewDef.camera.yon * metadataValue;
		if (GetLevelMetadataFloat("level.renderingFogEnd", &metadataValue)) viewDef.styles.fogEnd = viewDef.camera.yon * metadataValue;
		if (GetLevelMetadataFloat("level.renderingAmbientR", &metadataValue)) viewDef.lights.ambientColor.r = metadataValue;
		if (GetLevelMetadataFloat("level.renderingAmbientG", &metadataValue)) viewDef.lights.ambientColor.g = metadataValue;
		if (GetLevelMetadataFloat("level.renderingAmbientB", &metadataValue)) viewDef.lights.ambientColor.b = metadataValue;
		if (GetLevelMetadataFloat("level.renderingSunX", &metadataValue)) gWorldSunDirection.x = metadataValue;
		if (GetLevelMetadataFloat("level.renderingSunY", &metadataValue)) gWorldSunDirection.y = metadataValue;
		if (GetLevelMetadataFloat("level.renderingSunZ", &metadataValue)) gWorldSunDirection.z = metadataValue;
		if (GetLevelMetadataFloat("level.renderingFillR", &metadataValue)) gFillColor1.r = metadataValue;
		if (GetLevelMetadataFloat("level.renderingFillG", &metadataValue)) gFillColor1.g = metadataValue;
		if (GetLevelMetadataFloat("level.renderingFillB", &metadataValue)) gFillColor1.b = metadataValue;
		gDrawLensFlare = GetLevelMetadataBool("level.renderingLensFlare", gDrawLensFlare);
	}


			/* SET LIGHTS */

	viewDef.lights.numFillLights 		= 1;
	OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);
	viewDef.lights.fillDirection[0] 	= gWorldSunDirection;
	viewDef.lights.fillColor[0] 		= gFillColor1;


			/*********************/
			/* SET ANAGLYPH INFO */
			/*********************/

	if (IsStereo())
	{
		gAnaglyphFocallength	= 200.0f;
		gAnaglyphEyeSeparation 	= 35.0f;

		if (IsStereoAnaglyphMono())
		{
			viewDef.lights.ambientColor.r 		+= .1f;					// make a little brighter
			viewDef.lights.ambientColor.g 		+= .1f;
			viewDef.lights.ambientColor.b 		+= .1f;
		}
	}


			/*********************/
			/* MAKE DRAW CONTEXT */
			/*********************/

	OGL_SetupGameView(&viewDef);


			/**********************/
			/* SET AUTO-FADE INFO */
			/**********************/

	switch(gLevelNum)
	{
//		case	0:
//				gAutoFadeStartDist = 0;
//				break;

		default:
				gAutoFadeStartDist	= gGameViewInfoPtr->yon * .80;
				gAutoFadeEndDist	= gGameViewInfoPtr->yon * .9f;
	}

	gAutoFadeRange_Frac	= 1.0f / (gAutoFadeEndDist - gAutoFadeStartDist);

	if (gAutoFadeStartDist != 0.0f)
		gAutoFadeStatusBits = STATUS_BIT_AUTOFADE;
	else
		gAutoFadeStatusBits = 0;



			/**********************/
			/* LOAD ART & TERRAIN */
			/**********************/
			//
			// NOTE: only call this *after* draw context is created!
			//

	LoadLevelArt();
	InitInfobar();

			/* INIT OTHER MANAGERS */

	InitSplineManager();
	InitContrails();
	InitEnemyManager();
	InitEffects();
	InitSparkles();
	InitItemsManager();


			/****************/
			/* INIT SPECIAL */
			/****************/

			/* INIT LEVEL & MODE SPECIFICS */

	switch(gLevelNum)
	{
//		case	LEVEL_NUM_SIDEWALK:
//				InitSnakeStuff();
//				CountSquishBerries();
//				break;


	}

	switch(gVSMode)
	{
		case	VS_MODE_RACE:
				gRaceReadySetGoTimer = 3.0;
				break;
	}



		/* INIT THE PLAYER & RELATED STUFF */

	PrimeTerrainWater();						// NOTE:  must do this before items get added since some items may be on the water
	InitPlayerAtStartOfLevel();					// NOTE:  this will also cause the initial items in the start area to be created

	PrimeSplines();
	PrimeFences();

			/* INIT CAMERAS */

	for (i = 0; i < gNumPlayers; i++)
		InitCamera_Terrain(i);
 }



/************************* PLAY LEVEL *******************************/

#ifdef __EMSCRIPTEN__

static void PangeaNet_DebugLogEarlyFramePhase(const char* phase)
{
	if (!PangeaNet_IsEnabled())
	{
		return;
	}
	if (gGameFrameNum >= 12)
	{
		return;
	}
	SDL_Log(
		"Nanosaur2 net frame %u phase %s host=%d players=%d",
		(unsigned)gGameFrameNum,
		phase,
		PangeaNet_IsHost(),
		gNumPlayers);
}

// Per-frame tick body used by the ASYNCIFY while-loop in PlayLevel.
// Returns true if the level should continue, false when it is over.
static bool PlayLevelTick(void)
{
	float fps;
	PangeaNet_DebugLogEarlyFramePhase("tick-begin");

			/* INPUT */

	StartProfilePhase(PROFILE_PHASE_INPUT);
	DoSDLMaintenance();
	PangeaNet_DebugLogEarlyFramePhase("after-maintenance");

	if (gGamePaused)
	{
		MoveObjects();
				CalcFramesPerSecond();
		DoPlayerTerrainUpdate();
		OGL_DrawScene(DrawLevelCallback);
		return true;
	}

#if __EMSCRIPTEN__
	if (PangeaNet_IsEnabled())
	{
		if (PangeaNet_IsHost())
			PangeaNet_HostReceiveInputs();
		else
		{
			PangeaNet_ClientApplySnapshot();
		}
	}
	PangeaNet_DebugLogEarlyFramePhase("after-net-recv");
#endif

	for (int i = 0; i < gNumPlayers; i++)
	{
#if __EMSCRIPTEN__
		if (PangeaNet_IsEnabled() && !PangeaNet_ShouldSimulateGameplayForPlayer(i))
		{
			continue;
		}
#endif
		UpdatePlayerSteering(i);
	}
	PangeaNet_DebugLogEarlyFramePhase("after-steering");

#if __EMSCRIPTEN__
	if (PangeaNet_IsEnabled() && !PangeaNet_IsHost())
	{
		PangeaNet_ClientSendInput();
	}
	PangeaNet_DebugLogEarlyFramePhase("after-client-send-input");
#endif

			/* MOVE OBJECTS & UPDATE TERRAIN & DRAW */

		StartProfilePhase(PROFILE_PHASE_GAME_LOGIC);
#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_OnFrame(gLevelNum, gGameFrameNum, gFramesPerSecondFrac, gGameLevelTimer);
#endif
		MoveEverything();
	PangeaNet_DebugLogEarlyFramePhase("after-move-everything");

#if __EMSCRIPTEN__
	if (PangeaNet_IsEnabled() && PangeaNet_IsHost())
		PangeaNet_HostSendSnapshot();
	PangeaNet_DebugLogEarlyFramePhase("after-host-send-snapshot");
#endif

	DoPlayerTerrainUpdate();
	PangeaNet_DebugLogEarlyFramePhase("after-terrain-update");

	OGL_DrawScene(DrawLevelCallback);
	PangeaNet_DebugLogEarlyFramePhase("after-draw");

		/*************************/
		/* UPDATE FPS AND TIMERS */
		/*************************/

	CalcFramesPerSecond();
	fps = gFramesPerSecondFrac;

	gGameFrameNum++;
	gGameLevelTimer += fps;
	gDisableHiccupTimer = false;

			/***************************/
			/* SEE IF RESET PLAYER NOW */
			/***************************/

	for (int i = 0; i < gNumPlayers; i++)
	{
		if (!NS2ShouldProcessDeathTimerForPlayer(i))
		{
			continue;
		}

		if (gPlayerIsDead[i])
		{
			float	oldTimer = gDeathTimer[i];
			gDeathTimer[i] -= fps;
			if (gDeathTimer[i] <= 0.0f)
			{
				const float fadeOutSpeed = 4.0f;
				if (oldTimer > 0.0f)
				{
					if (gNumPlayers > 1 || gPlayerInfo[i].numFreeLives > 0)
					{
						MakeFadeEvent(kFadeFlags_Out | (kFadeFlags_P1<<i), fadeOutSpeed);
					}
				}
				else if (gDeathTimer[i] < -(1.0f / fadeOutSpeed))
				{
					ResetPlayerAtBestCheckpoint(i);
				}
			}
		}
	}

		/*****************/
		/* SEE IF PAUSED */
		/*****************/

	if (IsNeedDown(kNeed_UIPause, ANY_PLAYER))
	{
		DoPaused();
	}

			/* LEVEL CHEAT */

	if ((IsKeyActive(SDL_SCANCODE_LGUI) || IsKeyActive(SDL_SCANCODE_RGUI))
		&& IsKeyDown(SDL_SCANCODE_F10))
	{
		gLevelCompleted = true;
	}

#if __EMSCRIPTEN__
	if (PangeaNet_IsEnabled())
	{
		PangeaNet_UpdateMatchLifecycle();

		if (PangeaNet_IsHost())
		{
			PangeaNet_PublishLocalMatchLifecycle();
		}
		else
		{
			const int remoteReason = PangeaNet_GetRemoteLifecycleReason();
			if (remoteReason == 1)
			{
				gGameOver = true;
			}
			else if (remoteReason == 2)
			{
				if (!gLevelCompleted)
				{
					gLevelCompleted = true;
					gLevelCompletedCoolDownTimer = 0.05f;
				}
			}
			else
			{
				// In network mode the host decides match-end lifecycle.
				gGameOver = false;
			}
		}
	}
#endif

			/*****************************/
			/* SEE IF LEVEL IS COMPLETED */
			/*****************************/

	if (gGameOver)
	{
		return false;
	}

	if (gLevelCompleted)
	{
		gLevelCompletedCoolDownTimer -= fps;
		if (gLevelCompletedCoolDownTimer <= 0.0f)
			return false;
	}

	ResetProfilingForFrame();
	return true;
}

static void PlayLevel(void)
{
		/* PREP STUFF */

	gIsInGame = true;
	DoSDLMaintenance();
	CalcFramesPerSecond();
	CalcFramesPerSecond();

	MakeFadeEvent(kFadeFlags_In, 1.0);

#ifdef PANGEA_ENABLE_SCRIPTING
	Nanosaur2Script_OnLevelStart(gLevelNum);
#endif

	GrabMouse(true);

	// With ASYNCIFY, OGL_DrawScene -> SDL_GL_SwapWindow -> emscripten_sleep(0)
	// yields to the browser event loop between frames, so a plain while loop works.
	while (PlayLevelTick())
	{
		/* continue until game over or level complete */
	}

	GrabMouse(false);

	// Skip the blocking fade-out on Emscripten; just snap to black
	gGammaFadeFrac = 0;
}
#else // !__EMSCRIPTEN__

static void PlayLevel(void)
{
float	fps;


		/* PREP STUFF */

	DoSDLMaintenance();
	CalcFramesPerSecond();
	CalcFramesPerSecond();

		MakeFadeEvent(kFadeFlags_In, 1.0);

#ifdef PANGEA_ENABLE_SCRIPTING
	Nanosaur2Script_OnLevelStart(gLevelNum);
#endif

		if (gTimeDemo)
	{
		gTimeDemoStartTime = TickCount();
	}


	GrabMouse(true);


	/******************/
	/* MAIN GAME LOOP */
	/******************/

	while(true)
	{
				/* INPUT */

		StartProfilePhase(PROFILE_PHASE_INPUT);
		DoSDLMaintenance();


		if (gGamePaused)
		{
			MoveObjects();
			CalcFramesPerSecond();
			DoPlayerTerrainUpdate();
			OGL_DrawScene(DrawLevelCallback);
			EndProfilePhase(PROFILE_PHASE_INPUT);
			continue;
		}


#if __EMSCRIPTEN__
		if (PangeaNet_IsEnabled())
		{
			if (PangeaNet_IsHost())
				PangeaNet_HostReceiveInputs();
			else
			{
				PangeaNet_ClientApplySnapshot();
			}
		}
#endif

		for (int i = 0; i < gNumPlayers; i++)
		{
#if __EMSCRIPTEN__
			if (PangeaNet_IsEnabled() && !PangeaNet_ShouldSimulateGameplayForPlayer(i))
			{
				continue;
			}
#endif
			UpdatePlayerSteering(i);
		}
		EndProfilePhase(PROFILE_PHASE_INPUT);

#if __EMSCRIPTEN__
		if (PangeaNet_IsEnabled() && !PangeaNet_IsHost())
		{
			PangeaNet_ClientSendInput();
		}
#endif

			/* MOVE OBJECTS & UPDATE TERRAIN & DRAW */

		StartProfilePhase(PROFILE_PHASE_GAME_LOGIC);
#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_OnFrame(gLevelNum, gGameFrameNum, gFramesPerSecondFrac, gGameLevelTimer);
#endif
		MoveEverything();

#if __EMSCRIPTEN__
		if (PangeaNet_IsEnabled() && PangeaNet_IsHost())
			PangeaNet_HostSendSnapshot();
#endif

		DoPlayerTerrainUpdate();

		OGL_DrawScene(DrawLevelCallback);


		/*************************/
		/* UPDATE FPS AND TIMERS */
		/*************************/

		CalcFramesPerSecond();
		fps = gFramesPerSecondFrac;

		gGameFrameNum++;
		gGameLevelTimer += fps;
		gDisableHiccupTimer = false;									// reenable this after the 1st frame


				/***************************/
				/* SEE IF RESET PLAYER NOW */
				/***************************/

		for (int i = 0; i < gNumPlayers; i++)							// check all players
		{
			if (!NS2ShouldProcessDeathTimerForPlayer(i))
			{
				continue;
			}

			if (gPlayerIsDead[i])										// is this player dead?
			{
				float	oldTimer = gDeathTimer[i];
				gDeathTimer[i] -= fps;
				if (gDeathTimer[i] <= 0.0f)								// is it time to reincarnate player?
				{
					const float fadeOutSpeed = 4.0f;

					if (oldTimer > 0.0f)								// if just now crossed zero then start fade
					{
						if (gNumPlayers > 1
							|| gPlayerInfo[i].numFreeLives > 0)		// ...only if hasn't lost adventure mode yet (gameover will freeze-frame fadeout)
						{
							MakeFadeEvent(kFadeFlags_Out | (kFadeFlags_P1<<i), fadeOutSpeed);
						}
					}
					else if (gDeathTimer[i] < -(1.0f / fadeOutSpeed))	// once fully faded out reset player @ checkpoint
					{
						ResetPlayerAtBestCheckpoint(i);
					}
				}
			}
		}


		/*****************/
		/* SEE IF PAUSED */
		/*****************/

		if (IsNeedDown(kNeed_UIPause, ANY_PLAYER))						// do regular pause mode
		{
			DoPaused();
		}

#if __APPLE__
		if (IsCmdQDown())
		{
			DoReallyQuit();
		}
#endif

#if 0
		if (GetNewKeyState(KEY_F15))									// do screen-saver-safe paused mode
		{
			glFinish();

			do
			{
				EventRecord	e;
				WaitNextEvent(everyEvent,&e, 0, 0);
				UpdateInput();
			}while(!GetNewKeyState(KEY_F15));

			CalcFramesPerSecond();
		}
#endif

				/* LEVEL CHEAT */

		if ((IsKeyActive(SDL_SCANCODE_LGUI) || IsKeyActive(SDL_SCANCODE_RGUI))
			&& IsKeyDown(SDL_SCANCODE_F10))									// see if skip level
		{
			gLevelCompleted = true;
//			gSkipLevelIntro = true;
		}

#if __EMSCRIPTEN__
		if (PangeaNet_IsEnabled())
		{
			PangeaNet_UpdateMatchLifecycle();

			if (PangeaNet_IsHost())
			{
				PangeaNet_PublishLocalMatchLifecycle();
			}
			else
			{
				const int remoteReason = PangeaNet_GetRemoteLifecycleReason();
				if (remoteReason == 1)
				{
					gGameOver = true;
				}
				else if (remoteReason == 2)
				{
					if (!gLevelCompleted)
					{
						gLevelCompleted = true;
						gLevelCompletedCoolDownTimer = 0.05f;
					}
				}
				else
				{
					// In network mode the host decides match-end lifecycle.
					gGameOver = false;
				}
			}
		}
#endif



				/*****************************/
				/* SEE IF LEVEL IS COMPLETED */
				/*****************************/

		if (gGameOver)													// if we need immediate abort, then bail now
			break;

		if (gLevelCompleted)
		{
			gLevelCompletedCoolDownTimer -= fps;						// game is done, but wait for cool-down timer before bailing
			if (gLevelCompletedCoolDownTimer <= 0.0f)
				break;
		}
		ResetProfilingForFrame();
	}

	GrabMouse(false);

	if (gGammaFadeFrac > 0)												// only fade out if we haven't called MakeFadeEvent(kFadeFlags_Out) already
	{
		gGameViewInfoPtr->fadeSound = true;
		OGL_FadeOutScene(DrawLevelCallback, DoPlayerTerrainUpdate);
	}

	if (gTimeDemo)
	{
		uint32_t	ticks;
		float	seconds;

		gTimeDemoEndTime = TickCount();
		ticks = gTimeDemoEndTime - gTimeDemoStartTime;
		seconds = (float)ticks / 60.0f;

//		ShowSystemErr_NonFatal(seconds);
//		ShowSystemErr_NonFatal(gGameFrameNum / seconds);

		ShowTimeDemoResults(gGameFrameNum, seconds, (float)gGameFrameNum / seconds);


		ExitToShell();
	}

}

#endif // __EMSCRIPTEN__

#ifndef __EMSCRIPTEN__

/************************* SHOW TIME DEMO RESULTS *******************************/

static void ShowTimeDemoResults(int numFrames, float numSeconds, float averageFPS)
{
	DoAlert("%s:\n"
			"Frames: %d\n"
			"Time: %f\n"
			"Average FPS: %f\n"
			"Peak #Objs: %d",
			__func__, numFrames, numSeconds, averageFPS, gNumObjectNodesPeak);
}

#endif


/****************** DRAW LEVEL CALLBACK *********************/

static void DrawLevelCallback(void)
{

	if (IsStereo())
	{
		Byte	p = gCurrentSplitScreenPane;			// get the player # who's draw context is being drawn
#if __EMSCRIPTEN__
		if (PangeaNet_IsOnlineMatch() && gDrawingOverlayPane)
		{
			const int localPlayerIndex = PangeaNet_GetLocalPlayerIndex();
			if (localPlayerIndex >= 0 && localPlayerIndex < MAX_PLAYERS)
			{
				p = (Byte)localPlayerIndex;
			}
		}
#endif

			/* MAKE SURE ANAGLYPH SETTINGS ARE GOOD FOR THIS CAMERA MODE */

		switch(gCameraMode[p])
		{
			case	CAMERA_MODE_NORMAL:
					if (IsStereoShutter())
					{
						gAnaglyphFocallength	= 280.0f;
						gAnaglyphEyeSeparation 	= 45.0f;
					}
					else
					{
						gAnaglyphFocallength	= 260.0f;
						gAnaglyphEyeSeparation 	= 35.0f;
					}
					break;

			case	CAMERA_MODE_FIRSTPERSON:
					gAnaglyphFocallength	= 300.0f;
					gAnaglyphEyeSeparation 	= 80.0f;
					break;

			case	CAMERA_MODE_ANAGLYPHCLOSE:
					gAnaglyphFocallength	= 700.0f;
					gAnaglyphEyeSeparation 	= 50.0f;
					break;
		}
	}

	DrawObjects();
}


/**************** CLEANUP LEVEL **********************/

static void CleanupLevel(void)
{
	gIsInGame = false;
	FreeAllCustomSplines();
	StopAllEffectChannels();
 	EmptySplineObjectList();
	DeleteAllObjects();
	FreeAllSkeletonFiles(-1);
	DisposeTerrain();
	DeleteAllParticleGroups();
	DeleteAllConfettiGroups();
	DisposeInfobar();
	DisposeParticleSystem();
	DisposeSpriteGroup(SPRITE_GROUP_LEVELSPECIFIC);
	#if PANGEA_SAFE_ITEM_LOADING
	for (int biome = BIOME_FOREST; biome < NUM_BIOMES; biome++)
		DisposeSpriteGroup(GetNanosaur2LevelSpriteGroup(biome));
	#endif
	DisposeSpriteGroup(SPRITE_GROUP_OVERHEADMAP);
	DisposeAllBG3DContainers();
	DisposeContrails();
	FreeAllZaps();

	OGL_DisposeGameView();	// do this last!


		/* SET SOME IMPORTANT GLOBALS BACK TO DEFAULTS */

	gVSMode = VS_MODE_NONE;
	gNumPlayers = 1;
}

#pragma mark -


/******************** MOVE EVERYTHING ************************/

void MoveEverything(void)
{
	MoveObjects();
	MoveSplineObjects();
	UpdateCameras();								// update camera
	UpdateFences();
	UpdateDustDevilUVAnimation();

		/***********************/
		/* MODE-SPECIFIC STUFF */
		/***********************/

	switch(gVSMode)
	{
			/* ADVENTURE MODE */

		case	VS_MODE_NONE:
				break;

			/* RACE MODE */

		case	VS_MODE_RACE:
				gRaceReadySetGoTimer -= gFramesPerSecondFrac;
				CalcPlayerPlaces();									// determinw who is in what place
				break;


	}


}


/***************** START LEVEL COMPLETION ****************/

void StartLevelCompletion(float coolDownTimer)
{
	if (!gLevelCompleted)
	{
		gLevelCompleted = true;
		gLevelCompletedCoolDownTimer = coolDownTimer;
#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_OnLevelComplete(gLevelNum);
#endif
	}
}


#pragma mark -

/******************* PRIME TIME DEMO SPLINE *************************/
//
//
//

Boolean PrimeTimeDemoSpline(long splineNum, SplineItemType *itemPtr)
{
ObjNode			*newObj;
float			x,z,placement;


	if (!gTimeDemo)												// are we in time demo mode?
		return(false);


			/* GET SPLINE INFO */

	placement = itemPtr->placement;
	GetCoordOnSpline(&gSplineList[splineNum], placement, &x, &z);


				/* MAKE DUMMY SPLINE TRACKER OBJECT */

	NewObjectDefinitionType def =
	{
		.genre		= CUSTOM_GENRE,
		.slot 		= 0,
		.moveCall 	= nil,
		.flags 		= 0,
		.scale		= 1,
	};

	newObj = MakeNewObject(&def);


				/* SET SPLINE INFO */

	newObj->StatusBits		|= STATUS_BIT_ONSPLINE;
	newObj->SplineItemPtr 	= itemPtr;
	newObj->SplineNum 		= splineNum;
	newObj->SplinePlacement = placement;
	newObj->SplineMoveCall 	= MoveTimeDemoOnSpline;					// set move call

			/* ADD SPLINE OBJECT TO SPLINE OBJECT LIST */

	DetachObject(newObj, true);										// detach this object from the linked list
	AddToSplineObjectList(newObj, true);

	return(true);
}


/******************* MOVE TIME DEMO ON SPLINE ***********************/

static void MoveTimeDemoOnSpline(ObjNode *theNode)
{
OGLVector3D	v;
float	r;
OGLMatrix4x4	m;
ObjNode	*player = gPlayerInfo[0].objNode;

		/* MOVE ALONG THE SPLINE */

	if (IncreaseSplineIndex(theNode, 450))
		gGameOver = true;

	GetObjectCoordOnSpline(theNode);

	theNode->OldCoord.y = theNode->Coord.y = GetTerrainY(theNode->Coord.x, theNode->Coord.z) + 600.0f;

	v.x = theNode->Coord.x - theNode->OldCoord.x;				// calc aim vector
	v.y = theNode->Coord.y - theNode->OldCoord.y;
	v.z = theNode->Coord.z - theNode->OldCoord.z;
	OGLVector3D_Normalize(&v, &v);

//	from.x = theNode->Coord.x - (v.x * 500.0f);
//	from.y = theNode->Coord.y - (v.y * 500.0f);
//	from.z = theNode->Coord.z - (v.z * 500.0f);


			/* AIM ALONG SPLINE */

	OGL_UpdateCameraFromToUp(&theNode->OldCoord, &theNode->Coord, &gUp, 0);


	gPlayerInfo[0].camera.cameraLocation = theNode->Coord;
	gPlayerInfo[0].coord = theNode->Coord;				// update player coord
	player->Coord =  theNode->Coord;
	player->MotionVector = v;
	r= player->Rot.y = CalcYAngleFromPointToPoint(0, theNode->OldCoord.x, theNode->OldCoord.z,
															theNode->Coord.x, theNode->Coord.z);

	OGLMatrix4x4_SetTranslate(&player->BaseTransformMatrix, theNode->Coord.x, theNode->Coord.y, theNode->Coord.z);
	OGLMatrix4x4_SetRotate_Y(&m, r);
	OGLMatrix4x4_Multiply(&m, &player->BaseTransformMatrix, &player->BaseTransformMatrix);

	theNode->OldCoord = theNode->Coord;			// remember coord also

	if ((MyRandomLong() & 0xff) < 15)
	{
		gPlayerInfo[0].currentWeapon = RandomRange(WEAPON_TYPE_BLASTER, WEAPON_TYPE_BOMB);
		gPlayerInfo[0].weaponQuantity[gPlayerInfo[0].currentWeapon] = 999;
		PlayerFireButtonPressed(player, true);



	}


}



#pragma mark -


/****** LOAD/DISPOSE FONT USED THROUGHOUT THE GAME *****/

void LoadGlobalAssets(void)
{
	LoadSpriteAtlas(ATLAS_GROUP_FONT1, ":Sprites:fonts:font", kAtlasLoadFont | kAtlasLoadFontIsUpperCaseOnly);
	LoadSpriteAtlas(ATLAS_GROUP_FONT2, ":Sprites:fonts:font", kAtlasLoadFont | kAtlasLoadFontIsUpperCaseOnly | kAtlasLoadAltSkin1);
	LoadSpriteGroupFromFile(SPRITE_GROUP_CURSOR, ":Sprites:menu:cursor", 0);
	LoadSpriteGroupFromSeries(SPRITE_GROUP_INFOBAR,		INFOBAR_SObjType_COUNT,		"infobar");
	LoadSpriteGroupFromSeries(SPRITE_GROUP_GLOBAL,		GLOBAL_SObjType_COUNT,		"global");
	LoadSpriteGroupFromSeries(SPRITE_GROUP_SPHEREMAPS,	SPHEREMAP_SObjType_COUNT,	"spheremap");
	LoadSpriteGroupFromSeries(SPRITE_GROUP_PARTICLES,	PARTICLE_SObjType_COUNT,	"particle");
	BlendAllSpritesInGroup(SPRITE_GROUP_PARTICLES);
}

void DisposeGlobalAssets(void)
{
	DisposeSpriteAtlas(ATLAS_GROUP_FONT1);
	DisposeSpriteAtlas(ATLAS_GROUP_FONT2);
	DisposeSpriteGroup(SPRITE_GROUP_CURSOR);
	DisposeSpriteGroup(SPRITE_GROUP_INFOBAR);
	DisposeSpriteGroup(SPRITE_GROUP_GLOBAL);
	DisposeSpriteGroup(SPRITE_GROUP_SPHEREMAPS);
	DisposeSpriteGroup(SPRITE_GROUP_PARTICLES);
}


#pragma mark -


/************************************************************/
/******************** PROGRAM MAIN ENTRY  *******************/
/************************************************************/


void GameMain(void)
{
unsigned long	someLong;


				/**************/
				/* BOOT STUFF */
				/**************/

	ToolBoxInit();


#if !_DEBUG
	SDL_HideCursor();
#endif

	DoWarmUpScreen();



			/* INIT SOME OF MY STUFF */

	LoadLocalizedStrings(gGamePrefs.language);
	InitSpriteManager();
	InitBG3DManager();
	InitWindowStuff();
	InitTerrainManager();
	InitSkeletonManager();
	InitSoundTools();
	InitTwitchSystem();


			/* INIT MORE MY STUFF */

	InitObjectManager();

	GetDateTime ((unsigned long *)(&someLong));		// init random seed
	SetMyRandomSeed((uint32_t) someLong);


			/* PRELOAD SPRITES FOR ENTIRE GAME */

	LoadGlobalAssets();


		/* DIRECT LEVEL LOADING (--level flag / WebAssembly level editor mode) */

#if !SKIPFLUFF
	if (gCmdLevelNum < 0)
	{
		// Show titles only when not jumping directly to a level
		DoLegalScreen();
		DoIntroStoryScreen();
	}
#endif


	// If a level was specified on the command line (or via URL param in WebAssembly),
	// skip all menus and jump directly into that level.
	if (gCmdLevelNum >= 0)
	{
		gLevelNum = (short) gCmdLevelNum;
		LoadLevelMetadata();

#if __EMSCRIPTEN__
		if (PangeaNet_IsEnabled())
		{
			int networkPlayerCount = PangeaNet_GetPlayerCount();
			if (networkPlayerCount < 1)
			{
				networkPlayerCount = 1;
			}
			if (networkPlayerCount > MAX_PLAYERS)
			{
				networkPlayerCount = MAX_PLAYERS;
			}
			gNumPlayers = (Byte) networkPlayerCount;
			if (gVSMode == VS_MODE_NONE)
			{
				gVSMode = GetVSModeForLevel(gLevelNum);
			}
			SDL_Log(
				"Nanosaur2 direct network launch level=%d vsMode=%s(%d) players=%d host=%d",
				gLevelNum,
				Nanosaur2VSModeName(gVSMode),
				gVSMode,
				gNumPlayers,
				PangeaNet_IsHost());
		}
		else
#endif
		{
			gNumPlayers = 1;
			gVSMode = VS_MODE_NONE;
		}
		gPlayingFromSavedGame = false;
		gSkipLevelIntro = true;
#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_Init();
#endif
		InitPlayerInfo_Game();
		PlaySong(gLevelSongs[gLevelNum], true);
#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_LoadLevelConfig(gLevelNum);
#endif
		InitLevel();
#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_OnLevelLoad(gLevelNum);
#endif
		PlayLevel();
#ifdef PANGEA_ENABLE_SCRIPTING
		if (gLevelCompleted)
			Nanosaur2Script_OnLevelComplete(gLevelNum);
		Nanosaur2Script_OnLevelUnload(gLevelNum);
#endif
		CleanupLevel();
#ifdef PANGEA_ENABLE_SCRIPTING
		Nanosaur2Script_Shutdown();
#endif
		return;
	}


		/*************/
		/* MAIN LOOP */
		/*************/

	while(true)
	{
		gTimeDemo = false;

			/* DO MAIN MENU */

		MyFlushEvents();
		DoMainMenuScreen();

			/* PLAY ADVENTURE OR VS. MODE */

		if (gVSMode == VS_MODE_NONE)
			PlayGame_Adventure();
		else
		{
			if (DoLocalGatherScreen())
			{
				gVSMode = VS_MODE_NONE;
				gNumPlayers = 1;
				continue;
			}
			PlayGame_Versus();
		}
	}

}

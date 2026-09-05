/****************************/
/*      LOAD LEVEL.C        */
/* (c)2002 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include "game.h"

/****************************/
/*    PROTOTYPES            */
/****************************/

static void MakeTerrainSpec(FSSpec *spec, const char *defaultRelPath);

static void MakeCurrentAreaTerrainSpec(FSSpec *spec)
{
	static const char *const kAreaTerrainPaths[] =
	{
		":Terrain:town_duel.ter",
		":Terrain:town_shootout.ter",
		":Terrain:town_duel.ter",
		":Terrain:town_stampede.ter",
		":Terrain:town_duel.ter",
		":Terrain:town_duel.ter",
		":Terrain:swamp_duel.ter",
		":Terrain:swamp_shootout.ter",
		":Terrain:swamp_duel.ter",
		":Terrain:swamp_stampede.ter",
		":Terrain:swamp_duel.ter",
		":Terrain:swamp_duel.ter"
	};

	if (gCurrentArea >= 0 && gCurrentArea < (int) SDL_arraysize(kAreaTerrainPaths))
		MakeTerrainSpec(spec, kAreaTerrainPaths[gCurrentArea]);
}


/****************************/
/*    CONSTANTS             */
/****************************/


/**********************/
/*     VARIABLES      */
/**********************/


/***************** MAKE TERRAIN SPEC ***********************/
//
// Build an FSSpec for a terrain file.
// If gDirectTerrainPath is set, use that path instead of the bundled default.
//

static void MakeTerrainSpec(FSSpec *spec, const char *defaultRelPath)
{
	if (gDirectTerrainPath[0] != '\0')
	{
		// Use the overridden terrain file (e.g. supplied by level editor via WebAssembly)
		OSErr err = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, gDirectTerrainPath, spec);
		if (err == noErr)
			return;
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
					"MakeTerrainSpec: custom terrain '%s' not found (err=%d), using default '%s'",
					gDirectTerrainPath, (int)err, defaultRelPath);
	}

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, defaultRelPath, spec);
}



/************************** LOAD DUEL ART ***************************/

void LoadDuelArt(void)
{
FSSpec	spec;



			/*********************/
			/* LOAD COMMNON DATA */
			/*********************/


			/* LOAD GLOBAL BG3D GEOMETRY */
			
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:global.bg3d", &spec);
	ImportBG3D(&spec, MODEL_GROUP_GLOBAL);


			/* LOAD LEVEL BG3D */
			
	if (!IsBillySwampArea())
	{
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:town.bg3d", &spec);
				ImportBG3D(&spec, MODEL_GROUP_LEVELSPECIFIC);

				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:buildings.bg3d", &spec);
				ImportBG3D(&spec, MODEL_GROUP_BUILDINGS);
	}
	else
	{
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:swamp.bg3d", &spec);
				ImportBG3D(&spec, MODEL_GROUP_LEVELSPECIFIC);
	}


			/* LOAD SPRITES */
			
	LoadSpriteGroup(SPRITE_GROUP_INFOBAR);
	LoadSpriteGroup(SPRITE_GROUP_GLOBAL);
	LoadSpriteGroup(SPRITE_GROUP_SPHEREMAPS);
	LoadSpriteGroup(SPRITE_GROUP_FONT);
	LoadSpriteGroup(SPRITE_GROUP_DUEL);


			/* LOAD PLAYER SKELETON */
			
	LoadASkeleton(SKELETON_TYPE_BILLY);
	LoadASkeleton(SKELETON_TYPE_BANDITO);
	LoadASkeleton(SKELETON_TYPE_RYGAR);
	LoadASkeleton(SKELETON_TYPE_SHORTY);




			/* LOAD TERRAIN */
			//
			// must do this after creating the view!
			//
			
	MakeCurrentAreaTerrainSpec(&spec);
	
	LoadPlayfield(&spec);

}


/************************** LOAD SHOOTOUT ART ***************************/

void LoadShootoutArt(void)
{
FSSpec	spec;



			/*********************/
			/* LOAD COMMNON DATA */
			/*********************/
				

			/* LOAD GLOBAL BG3D GEOMETRY */
			
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:global.bg3d", &spec);
	ImportBG3D(&spec, MODEL_GROUP_GLOBAL);


	if (!IsBillySwampArea())
	{
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:town.bg3d", &spec);
				ImportBG3D(&spec, MODEL_GROUP_LEVELSPECIFIC);

				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:buildings.bg3d", &spec);
				ImportBG3D(&spec, MODEL_GROUP_BUILDINGS);
	}
	else
	{
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:swamp.bg3d", &spec);
				ImportBG3D(&spec, MODEL_GROUP_LEVELSPECIFIC);
	}


			/* LOAD SPRITES */
			
	LoadSpriteGroup(SPRITE_GROUP_INFOBAR);
	LoadSpriteGroup(SPRITE_GROUP_GLOBAL);
	LoadSpriteGroup(SPRITE_GROUP_SPHEREMAPS);
	LoadSpriteGroup(SPRITE_GROUP_CURSOR);
	LoadSpriteGroup(SPRITE_GROUP_FONT);


			/* LOAD PLAYER SKELETON */
			
	LoadASkeleton(SKELETON_TYPE_BILLY);

	if (!IsBillySwampArea())
	{
			LoadASkeleton(SKELETON_TYPE_BANDITO);
				LoadASkeleton(SKELETON_TYPE_SHORTY);
				LoadASkeleton(SKELETON_TYPE_WALKER);
				LoadASkeleton(SKELETON_TYPE_KANGACOW);
	}
	else
	{
				LoadASkeleton(SKELETON_TYPE_KANGAREX);
				LoadASkeleton(SKELETON_TYPE_TREMORALIEN);
				LoadASkeleton(SKELETON_TYPE_TREMORGHOST);
				LoadASkeleton(SKELETON_TYPE_FROGMAN);
				LoadASkeleton(SKELETON_TYPE_BANDITO);
				LoadASkeleton(SKELETON_TYPE_SHORTY);
	}



			/* LOAD TERRAIN */
			//
			// must do this after creating the view!
			//
			
	MakeCurrentAreaTerrainSpec(&spec);

	LoadPlayfield(&spec);
	
	
	BG3D_SphereMapGeomteryMaterial(MODEL_GROUP_GLOBAL, GLOBAL_ObjType_PesoPOW,
								0, MULTI_TEXTURE_COMBINE_ADD, SPHEREMAP_SObjType_Sheen);			
}


/************************** LOAD STAMPEDE ART ***************************/

void LoadStampedeArt(void)
{
FSSpec	spec;



			/*********************/
			/* LOAD COMMNON DATA */
			/*********************/
				

			/* LOAD LEVEL BG3D */
			
	if (!IsBillySwampArea())
	{
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:town.bg3d", &spec);
	}
	else
	{
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:swamp.bg3d", &spec);
	}
	ImportBG3D(&spec, MODEL_GROUP_LEVELSPECIFIC);


			/* LOAD GLOBAL BG3D GEOMETRY */
			
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:global.bg3d", &spec);
	ImportBG3D(&spec, MODEL_GROUP_GLOBAL);

	BG3D_SphereMapGeomteryMaterial(MODEL_GROUP_GLOBAL, GLOBAL_ObjType_Boost,
								0, MULTI_TEXTURE_COMBINE_ADD, SPHEREMAP_SObjType_Sheen);			


			/* LOAD SPRITES */

	LoadSpriteGroup(SPRITE_GROUP_INFOBAR);
	LoadSpriteGroup(SPRITE_GROUP_GLOBAL);
	LoadSpriteGroup(SPRITE_GROUP_SPHEREMAPS);
	LoadSpriteGroup(SPRITE_GROUP_STAMPEDE);
	LoadSpriteGroup(SPRITE_GROUP_FONT);


			/* LOAD PLAYER SKELETON */
						
	LoadASkeleton(SKELETON_TYPE_BILLY);
	
	if (!IsBillySwampArea())
	{
				LoadASkeleton(SKELETON_TYPE_KANGACOW);
	}
	else
	{
				LoadASkeleton(SKELETON_TYPE_KANGAREX);
	}
	




			/* LOAD TERRAIN */
			//
			// must do this after creating the view!
			//
			
	MakeCurrentAreaTerrainSpec(&spec);

	LoadPlayfield(&spec);



	BG3D_SphereMapGeomteryMaterial(MODEL_GROUP_GLOBAL, GLOBAL_ObjType_PesoPOW,
								0, MULTI_TEXTURE_COMBINE_ADD, SPHEREMAP_SObjType_Sheen);			

	BG3D_SphereMapGeomteryMaterial(MODEL_GROUP_GLOBAL, GLOBAL_ObjType_Boost,
								0, MULTI_TEXTURE_COMBINE_ADD, SPHEREMAP_SObjType_Sheen);			
}




/************************** LOAD TARGET PRACTICE ART ***************************/

void LoadTargetPracticeArt(void)
{
FSSpec	spec;



			/*********************/
			/* LOAD COMMNON DATA */
			/*********************/

			/* LOAD GLOBAL BG3D GEOMETRY */
			
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:global.bg3d", &spec);
	ImportBG3D(&spec, MODEL_GROUP_GLOBAL);

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:targetpractice.bg3d", &spec);
	ImportBG3D(&spec, MODEL_GROUP_LEVELSPECIFIC);



			/* LOAD SPRITES */
			
	LoadSpriteGroup(SPRITE_GROUP_INFOBAR);
	LoadSpriteGroup(SPRITE_GROUP_GLOBAL);
	LoadSpriteGroup(SPRITE_GROUP_SPHEREMAPS);
	LoadSpriteGroup(SPRITE_GROUP_CURSOR);
	LoadSpriteGroup(SPRITE_GROUP_FONT);


			/* LOAD PLAYER SKELETON */
	
	if (gCurrentArea == AREA_TARGETPRACTICE1)
	{			
		LoadASkeleton(SKELETON_TYPE_KANGACOW);
		LoadASkeleton(SKELETON_TYPE_SHORTY);
	}
	else
	{
		LoadASkeleton(SKELETON_TYPE_FROGMAN);
		LoadASkeleton(SKELETON_TYPE_TREMORGHOST);
	}


	BG3D_SphereMapGeomteryMaterial(MODEL_GROUP_GLOBAL, GLOBAL_ObjType_Boost,
								0, MULTI_TEXTURE_COMBINE_ADD, SPHEREMAP_SObjType_Sheen);			

	BG3D_SphereMapGeomteryMaterial(MODEL_GROUP_GLOBAL, GLOBAL_ObjType_PesoPOW,
								0, MULTI_TEXTURE_COMBINE_ADD, SPHEREMAP_SObjType_Sheen);			

	BG3D_SphereMapGeomteryMaterial(MODEL_GROUP_LEVELSPECIFIC, PRACTICE_ObjType_Bottle,
								0, MULTI_TEXTURE_COMBINE_ADD, SPHEREMAP_SObjType_Sheen);			

	BG3D_SphereMapGeomteryMaterial(MODEL_GROUP_LEVELSPECIFIC, PRACTICE_ObjType_DeathSkull,
								0, MULTI_TEXTURE_COMBINE_ADD, SPHEREMAP_SObjType_Satin);
}



#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "pangea_presentation.h"

static int ScriptPresentation_ActivePlayers(void)
{
	return gNumPlayers;
}

static bool ScriptPresentation_TextAvailable(void)
{
	float width, height;
	return Atlas_MeasureString(ATLAS_GROUP_FONT1,"",0,&width,&height);
}

#define SCRIPT_CONFIRM_SOUND EFFECT_MENUSELECT
#define SCRIPT_PICKUP_SOUND EFFECT_GETPOW
#define SCRIPT_POSITIONAL_AUDIO true
#define SCRIPT_PRESENTATION_PLAYER gCurrentSplitScreenPane
#define SCRIPT_PARTICLE_PANES
#define SCRIPT_PRESENTATION_PLAYERS 2
#define SCRIPT_PRESENTATION_ACTIVE_PLAYERS ScriptPresentation_ActivePlayers
#define SCRIPT_PRESENTATION_TEXT_AVAILABLE ScriptPresentation_TextAvailable
#include "pangea_presentation_particles.inc"

static void ScriptPresentation_DrawLine(const char* text, int row)
{
	float width, height;
	int flags=kTextMeshAlignLeft|kTextMeshAlignTop|kTextMeshKeepCurrentProjection;
	if (!Atlas_MeasureString(ATLAS_GROUP_FONT1,text,flags,&width,&height)) return;
	float zoom=GAME_MAX(.01f,(float)gGamePrefs.hudScale*.01f);
	OGLRect viewport=Get2DLogicalRect(gCurrentSplitScreenPane,zoom);
	float left=gGamePrefs.force4x3HUD ? 0 : viewport.left;
	float top=gGamePrefs.force4x3HUD ? 0 : viewport.top;
	float bottom=gGamePrefs.force4x3HUD ? 480/zoom : viewport.bottom;
	float center=320/zoom;
	// Keep script text below the meters and left of the aiming reticle in every pane.
	float availableWidth=GAME_MAX(32,center-left-40);
	float spacing=GAME_MIN(18,GAME_MAX(1,(bottom-top-190)/PANGEA_PRESENTATION_ROWS));
	float scale=.22f;
	if (width>0) scale=GAME_MIN(scale,availableWidth/width);
	if (height>0) scale=GAME_MIN(scale,spacing*.85f/height);
	OGLColorRGB previousColor=gGlobalColorFilter;
	gGlobalColorFilter=(OGLColorRGB){0,0,0};
	Atlas_DrawString(ATLAS_GROUP_FONT1,text,left+17,top+101+row*spacing,scale,flags);
	gGlobalColorFilter=previousColor;
	Atlas_DrawString(ATLAS_GROUP_FONT1,text,left+16,top+100+row*spacing,scale,flags);
}

static void ScriptPresentation_Input(void)
{
	for (int player=0; player<gNumPlayers; player++)
	{
		if (!PangeaDialogue_IsActiveForPlayer(player)) continue;
		PangeaDialogue_InputForPlayer(player,IsNeedDown(kNeed_UIConfirm,player),IsNeedDown(kNeed_UIBack,player),IsNeedDown(kNeed_UIDown,player)-IsNeedDown(kNeed_UIUp,player));
	}
}

#include "pangea_presentation_native.inc"
#endif

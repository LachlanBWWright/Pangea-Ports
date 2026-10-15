#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "pangea_presentation.h"
#include "menu.h"

#define SCRIPT_CONFIRM_SOUND EFFECT_JUMP
#define SCRIPT_PICKUP_SOUND EFFECT_GETPOW
#define SCRIPT_POSITIONAL_AUDIO true
#define SCRIPT_PRESENTATION_PLAYER 0
#define SCRIPT_PRESENTATION_BEGIN_DIALOGUE BeginScriptDialog
#define SCRIPT_PRESENTATION_END_DIALOGUE EndScriptDialog
#include "pangea_presentation_particles.inc"

static void ScriptPresentation_DrawLine(const char* text, int row)
{
	if (gGamePaused || GetCurrentMenuID() != 0) return;
	float top = gGamePrefs.uiCentering ? 0 : g2DLogicalRect.top;
	GameFont_DrawString(text,150,top+140+row*19,.20f,kTextMeshAlignLeft|kTextMeshAlignTop|kTextMeshKeepCurrentProjection);
}

static void ScriptPresentation_Input(void)
{
	if (gGamePaused || GetCurrentMenuID() != 0) return;
	PangeaDialogue_Input(IsNeedDown(kNeed_UIConfirm),IsNeedDown(kNeed_UIBack),IsNeedDown(kNeed_UIDown)-IsNeedDown(kNeed_UIUp));
}

#include "pangea_presentation_native.inc"
#endif

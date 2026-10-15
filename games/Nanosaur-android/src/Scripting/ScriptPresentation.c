#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "pangea_presentation.h"
#include <stdio.h>
#include <string.h>

#define SCRIPT_CONFIRM_SOUND EFFECT_SELECT
#define SCRIPT_PICKUP_SOUND EFFECT_POWPICKUP
#define SCRIPT_POSITIONAL_AUDIO false
#define SCRIPT_PRESENTATION_PLAYER 0
#define SCRIPT_PRESENTATION_RESET reset_text
#define SCRIPT_BURST_NAME "dust"
#define SCRIPT_PRESENTATION_TEXT_AVAILABLE TextMesh_IsAvailable
#define SCRIPT_TEXT_X 110
#define SCRIPT_TEXT_Y 110
#define SCRIPT_TEXT_SCALE .30f
#define SCRIPT_TEXT_ROW_HEIGHT 22
#include "pangea_presentation_legacy_text.inc"

static PangeaPresentationStatus ScriptPresentation_Burst(const char* id, const PangeaPresentationOptions* options)
{
	if (!strcmp(id,"smoke")) return MakeSmokePuff(options->x,options->y,options->z,options->scale) ? PANGEA_PRESENTATION_OK : PANGEA_PRESENTATION_CAPACITY;
	if (!strcmp(id,"dust")) return MakeDustPuff(options->x,options->y,options->z,options->scale) ? PANGEA_PRESENTATION_OK : PANGEA_PRESENTATION_CAPACITY;
	return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
}

static void ScriptPresentation_Input(void)
{
	PangeaDialogue_Input(GetNewNeedState(kNeed_UIConfirm),GetNewNeedState(kNeed_UIBack),GetNewNeedState(kNeed_UIDown)-GetNewNeedState(kNeed_UIUp));
}

#include "pangea_presentation_native.inc"
#endif

#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "pangea_presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCRIPT_CONFIRM_SOUND EFFECT_SELECT
#define SCRIPT_PICKUP_SOUND EFFECT_GETPOW
#define SCRIPT_POSITIONAL_AUDIO true
#define SCRIPT_PRESENTATION_PLAYER 0
#define SCRIPT_PRESENTATION_RESET reset_text
#define SCRIPT_PRESENTATION_TEXT_AVAILABLE TextMesh_IsAvailable
#include "ScriptPresentationText.inc"

static PangeaPresentationStatus ScriptPresentation_Burst(const char* id, const PangeaPresentationOptions* options)
{
	bool smoke=!strcmp(id,"smoke");
	if (!smoke && strcmp(id,"sparkles")) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	int32_t group=NewParticleGroup(PARTICLE_TYPE_FALLINGSPARKS,0,smoke ? -30 : 80,0,6*options->scale,0,1,smoke ? PARTICLE_TEXTURE_PATCHY : PARTICLE_TEXTURE_WHITE);
	if (group<0) return PANGEA_PRESENTATION_CAPACITY;
	TQ3Point3D where={options->x,options->y,options->z};
	for (int i=0; i<12; i++)
	{
		float angle=i*.5235988f;
		TQ3Vector3D velocity={cosf(angle)*35,60,sinf(angle)*35};
		if (AddParticleToGroup(group,&where,&velocity,1,1)) return PANGEA_PRESENTATION_CAPACITY;
	}
	return PANGEA_PRESENTATION_OK;
}

static void ScriptPresentation_Input(void)
{
	PangeaDialogue_Input(GetNewKeyState(kKey_UI_Confirm)||GetNewKeyState(kKey_UI_PadConfirm),GetNewKeyState(kKey_UI_Cancel)||GetNewKeyState(kKey_UI_PadCancel),GetNewKeyState(kKey_Backward)-GetNewKeyState(kKey_Forward));
}

#include "pangea_presentation_native.inc"
#endif

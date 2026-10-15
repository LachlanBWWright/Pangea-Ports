#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "pangea_presentation.h"
#include "pangea_presentation_layout.h"

#define SCRIPT_CONFIRM_SOUND EFFECT_SELECTCLICK
#define SCRIPT_PICKUP_SOUND EFFECT_GETPOW
#define SCRIPT_POSITIONAL_AUDIO true
#define SCRIPT_PRESENTATION_PLAYER gCurrentSplitScreenPane
#define SCRIPT_PRESENTATION_PLAYERS MAX_LOCAL_PLAYERS
#define SCRIPT_PRESENTATION_ACTIVE_PLAYERS ScriptPresentation_ActivePlayers
#define SCRIPT_PRESENTATION_TEXT_AVAILABLE ScriptPresentation_TextAvailable
#define SCRIPT_SOUND_CATALOG(X) \
	X("ui.confirm", EFFECT_SELECTCLICK) \
	X("pickup.chime", EFFECT_GETPOW) \
	X("ui.reject", EFFECT_BADSELECT) \
	X("vehicle.nitro", EFFECT_NITRO) \
	X("vehicle.crash", EFFECT_CRASH) \
	X("environment.splash", EFFECT_SPLASH)
#include "pangea_presentation_particles.inc"

static int ScriptPresentation_ActivePlayers(void)
{
	return GAME_MAX(1, GAME_MIN(gNumLocalPlayers, MAX_LOCAL_PLAYERS));
}

static bool ScriptPresentation_TextAvailable(void)
{
	return gAtlases[SPRITE_GROUP_FONT] != NULL;
}

static float ScriptPresentation_PrepareLine(const char* text, char output[128])
{
	const Atlas* font = gAtlases[SPRITE_GROUP_FONT];
	size_t remaining = strlen(text), position = 0;
	float width = 0;
	while (remaining && position < 123)
	{
		unsigned int codepoint;
		size_t consumed = PangeaPresentation_UTF8Character((const unsigned char*)text, remaining, &codepoint);
		if (!consumed) { consumed = 1; codepoint = 0; }
		const AtlasGlyph* glyph = codepoint >= 32 && codepoint != 127 ? Atlas_GetGlyph(font, codepoint) : NULL;
		if (glyph && glyph->xadv <= 0 && glyph->w <= 0) glyph = NULL;
		if (glyph)
		{
			memcpy(output + position, text, consumed);
			position += consumed;
		}
		else
		{
			output[position++] = '?';
			glyph = Atlas_GetGlyph(font, '?');
		}
		if (glyph) width += GAME_MAX(glyph->xadv, glyph->xoff + glyph->w);
		text += consumed;
		remaining -= consumed;
	}
	output[position] = 0;
	return width;
}

static void ScriptPresentation_DrawLine(const char* text, int row)
{
	if (!ScriptPresentation_TextAvailable() || gDrawingOverlayPane) return;
	float width = gGameView->panes[gCurrentSplitScreenPane].logicalWidth;
	float height = gGameView->panes[gCurrentSplitScreenPane].logicalHeight;
	float spacing = GAME_MIN(18.0f, (height - 204.0f) / PANGEA_PRESENTATION_ROWS);
	float scale = GAME_MIN(.22f, spacing * .85f / gAtlases[SPRITE_GROUP_FONT]->lineHeight);
	char rendered[128];
	float textWidth = ScriptPresentation_PrepareLine(text, rendered);
	if (textWidth > 0) scale = GAME_MIN(scale, (width - 100.0f) / textWidth);
	// Full-rect coordinates leave room for the top inventory and bottom race HUD.
	Atlas_DrawString(SPRITE_GROUP_FONT, rendered, 76, 84 + row * spacing, scale,
		kTextMeshAlignLeft | kTextMeshAlignTop | kTextMeshKeepCurrentProjection);
}

static void ScriptPresentation_Input(void)
{
	for (int player = 0; player < ScriptPresentation_ActivePlayers(); player++)
	{
		if (!PangeaDialogue_IsActiveForPlayer(player)) continue;
		PangeaDialogue_InputForPlayer(player, GetNewNeedState(kNeed_UIConfirm, player),
			GetNewNeedState(kNeed_UIBack, player),
			GetNewNeedState(kNeed_UIDown, player) - GetNewNeedState(kNeed_UIUp, player));
	}
}

#include "pangea_presentation_native.inc"
#endif

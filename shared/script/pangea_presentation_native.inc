#include "pangea_presentation.h"
#include "pangea_presentation_game.h"
#include <string.h>

#ifndef SCRIPT_PRESENTATION_RESET
#define SCRIPT_PRESENTATION_RESET NULL
#endif
#ifndef SCRIPT_PRESENTATION_DRAW_EFFECTS
#define SCRIPT_PRESENTATION_DRAW_EFFECTS() ((void)0)
#endif
#ifndef SCRIPT_BURST_NAME
#define SCRIPT_BURST_NAME "sparkles"
#endif
#ifndef SCRIPT_PRESENTATION_PLAYERS
#define SCRIPT_PRESENTATION_PLAYERS 1
#endif
#ifndef SCRIPT_PRESENTATION_TEXT_AVAILABLE
#define SCRIPT_PRESENTATION_TEXT_AVAILABLE NULL
#endif
#ifndef SCRIPT_AUDIO_PARAMETERS
#define SCRIPT_AUDIO_PARAMETERS true
#endif
#ifndef SCRIPT_PRESENTATION_ACTIVE_PLAYERS
#define SCRIPT_PRESENTATION_ACTIVE_PLAYERS NULL
#endif
#ifndef SCRIPT_PRESENTATION_BEGIN_DIALOGUE
#define SCRIPT_PRESENTATION_BEGIN_DIALOGUE NULL
#endif
#ifndef SCRIPT_PRESENTATION_END_DIALOGUE
#define SCRIPT_PRESENTATION_END_DIALOGUE NULL
#endif
#ifndef SCRIPT_SOUND_CATALOG
#define SCRIPT_SOUND_CATALOG(X) X("ui.confirm",SCRIPT_CONFIRM_SOUND) X("pickup.chime",SCRIPT_PICKUP_SOUND)
#endif
#ifndef SCRIPT_EFFECT_CATALOG
#define SCRIPT_EFFECT_CATALOG(X) X(SCRIPT_BURST_NAME) X("smoke")
#endif

#define SCRIPT_SOUND_NAME(name,effect) name,
#define SCRIPT_EFFECT_NAME(name) name,
static const char* const kSounds[]={SCRIPT_SOUND_CATALOG(SCRIPT_SOUND_NAME)};
static const char* const kEffects[]={SCRIPT_EFFECT_CATALOG(SCRIPT_EFFECT_NAME)};
#undef SCRIPT_SOUND_NAME
#undef SCRIPT_EFFECT_NAME
static struct { int channel; int effect; uint32_t version; bool used; } gVoices[8];

static PangeaPresentationStatus play_sound(const char* name, const PangeaPresentationOptions* options, int* voice)
{
	if (!SCRIPT_AUDIO_PARAMETERS && (options->volume!=1 || options->pitch!=1)) return PANGEA_PRESENTATION_UNSUPPORTED;
	int effect=-1;
#define SCRIPT_SOUND_MATCH(label,value) if (!strcmp(name,label)) effect=value;
	SCRIPT_SOUND_CATALOG(SCRIPT_SOUND_MATCH)
#undef SCRIPT_SOUND_MATCH
	if (effect<0) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	int slot=-1;
	for (int i=0; i<8; i++) if (!gVoices[i].used) { slot=i; break; }
	if (slot<0) return PANGEA_PRESENTATION_CAPACITY;
	int channel=ScriptPresentation_PlaySound(effect,options);
	if (channel<0) return PANGEA_PRESENTATION_CAPACITY;
	gVoices[slot].channel=channel;
	gVoices[slot].effect=effect;
	gVoices[slot].version=ScriptPresentation_ChannelVersion(channel);
	gVoices[slot].used=true;
	*voice=slot;
	return PANGEA_PRESENTATION_OK;
}

static bool playing(int voice)
{
	if (voice<0 || voice>=8 || !gVoices[voice].used) return false;
	if (gVoices[voice].version==ScriptPresentation_ChannelVersion(gVoices[voice].channel) && ScriptPresentation_SoundPlaying(gVoices[voice].channel,gVoices[voice].effect)) return true;
	gVoices[voice].used=false;
	return false;
}

static void stop(int voice)
{
	if (playing(voice)) ScriptPresentation_StopSound(gVoices[voice].channel,gVoices[voice].effect);
	if (voice>=0 && voice<8) gVoices[voice].used=false;
}

static void position(int voice, float x, float y, float z)
{
	if (playing(voice)) ScriptPresentation_PositionSound(gVoices[voice].channel,gVoices[voice].effect,x,y,z);
}

void ScriptPresentation_Configure(void)
{
	const PangeaPresentationNative native={
		.text=true,.choices=true,.positionalAudio=SCRIPT_POSITIONAL_AUDIO,.attachedEffects=true,
		.players=SCRIPT_PRESENTATION_PLAYERS,
		.activePlayers=SCRIPT_PRESENTATION_ACTIVE_PLAYERS,
		.textAvailable=SCRIPT_PRESENTATION_TEXT_AVAILABLE,
		.beginDialogue=SCRIPT_PRESENTATION_BEGIN_DIALOGUE,
		.endDialogue=SCRIPT_PRESENTATION_END_DIALOGUE,
		.play=play_sound,.stop=stop,.playing=playing,.position=position,
		.burst=ScriptPresentation_Burst,
		.sounds=kSounds,.soundCount=(int)(sizeof(kSounds)/sizeof(kSounds[0])),
		.effects=kEffects,.effectCount=(int)(sizeof(kEffects)/sizeof(kEffects[0])),
		.input=ScriptPresentation_Input,
		.reset=SCRIPT_PRESENTATION_RESET,
	};
	PangeaPresentation_Configure(&native);
	memset(gVoices,0,sizeof(gVoices));
}

void ScriptPresentation_Draw(void)
{
	SCRIPT_PRESENTATION_DRAW_EFFECTS();
	PangeaPresentationRow rows[PANGEA_PRESENTATION_ROWS];
	int count=PangeaPresentation_RowsForPlayer(rows,PANGEA_PRESENTATION_ROWS,SCRIPT_PRESENTATION_PLAYER);
	int displayed=0;
	for (int i=0; i<count; i++)
	{
		if (rows[i].player>=0 && rows[i].player!=SCRIPT_PRESENTATION_PLAYER) continue;
		ScriptPresentation_DrawLine(rows[i].text,displayed++);
	}
}

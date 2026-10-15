#include "pangea_presentation_game.h"

#ifndef NORMAL_CHANNEL_RATE
#define NORMAL_CHANNEL_RATE 0x10000
#endif

static uint32_t gScriptChannelVersions[64];

void ScriptPresentation_ChannelStarted(int channel)
{
	if (channel>=0 && channel<64 && ++gScriptChannelVersions[channel]==0) ++gScriptChannelVersions[channel];
}

uint32_t ScriptPresentation_ChannelVersion(int channel)
{
	return channel>=0 && channel<64 ? gScriptChannelVersions[channel] : 0;
}

bool ScriptPresentation_SoundPlaying(int channel, int effect)
{
	if (channel<0 || channel>=gMaxChannels || SCRIPT_CHANNEL_EFFECT(channel)!=effect) return false;
	SCStatus status;
	return SndChannelStatus(gSndChannel[channel],sizeof(status),&status)==noErr && status.scChannelBusy;
}

void ScriptPresentation_StopSound(int channel, int effect)
{
	if (!ScriptPresentation_SoundPlaying(channel,effect)) return;
	short nativeChannel=(short)channel;
	SCRIPT_STOP_CHANNEL(nativeChannel);
}

int ScriptPresentation_PlaySound(int effect, const PangeaPresentationOptions* options)
{
#ifdef SCRIPT_AUDIO_EFFECT_LOADED
	if (!SCRIPT_AUDIO_EFFECT_LOADED(effect)) return -1;
#endif
	int busy=0;
	for (int channel=0; channel<gMaxChannels; channel++)
	{
		SCStatus status;
		if (SndChannelStatus(gSndChannel[channel],sizeof(status),&status)==noErr && status.scChannelBusy) busy++;
	}
	if (busy>=gMaxChannels-2) return -1;
#ifdef SCRIPT_AUDIO_POSITIONAL
	if (options->positional)
	{
		SCRIPT_AUDIO_POINT where={options->x,options->y,options->z};
		return PlayEffect_Parms3D(effect,&where,(uint32_t)(NORMAL_CHANNEL_RATE*options->pitch),options->volume);
	}
	uint32_t volume=(uint32_t)(FULL_CHANNEL_VOLUME*options->volume);
	return PlayEffect_Parms(effect,volume,volume,(uint32_t)(NORMAL_CHANNEL_RATE*options->pitch));
#elif defined(SCRIPT_AUDIO_NANOSAUR)
	return PlayEffect_Parms(effect,(unsigned char)(FULL_CHANNEL_VOLUME*options->volume),(uint32_t)(NORMAL_CHANNEL_RATE*options->pitch));
#elif defined(SCRIPT_AUDIO_PLAY_CUSTOM)
	return SCRIPT_AUDIO_PLAY_CUSTOM(effect,options);
#else
	if (options->volume!=1 || options->pitch!=1) return -1;
	return PlaySound((short)effect);
#endif
}

void ScriptPresentation_PositionSound(int channel, int effect, float x, float y, float z)
{
#ifdef SCRIPT_AUDIO_POSITIONAL
	if (!ScriptPresentation_SoundPlaying(channel,effect)) return;
	SCRIPT_AUDIO_POINT where={x,y,z};
	short nativeChannel=(short)channel;
	Update3DSoundChannel(effect,&nativeChannel,&where);
#else
	(void)channel; (void)effect; (void)x; (void)y; (void)z;
#endif
}

#pragma once

#ifdef PANGEA_ENABLE_SCRIPTING
#include "pangea_presentation.h"
void ScriptPresentation_Configure(void);
void ScriptPresentation_Draw(void);
int ScriptPresentation_PlaySound(int effect, const PangeaPresentationOptions* options);
bool ScriptPresentation_SoundPlaying(int channel, int effect);
void ScriptPresentation_StopSound(int channel, int effect);
void ScriptPresentation_PositionSound(int channel, int effect, float x, float y, float z);
void ScriptPresentation_ChannelStarted(int channel);
uint32_t ScriptPresentation_ChannelVersion(int channel);
#else
static inline void ScriptPresentation_Configure(void) {}
static inline void ScriptPresentation_Draw(void) {}
#endif

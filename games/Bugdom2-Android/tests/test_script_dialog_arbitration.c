#include <assert.h>
#include "../Source/Screens/Dialog.c"

Boolean gGamePaused;
float gFramesPerSecondFrac = 1.0f;
static int gPlayed, gStopped;
static bool gVoicePlaying = true;

Boolean IsEffectChannelPlaying(short channel)
{
	(void)channel;
	return gVoicePlaying;
}

short PlayEffect(int effect)
{
	(void)effect;
	gPlayed++;
	return 1;
}

short PlayEffect_Parms3D(int effect, OGLPoint3D* where, uint32_t rate, float volume)
{
	(void)where;
	(void)rate;
	(void)volume;
	return PlayEffect(effect);
}

Boolean StopAChannelIfEffectNum(short* channel, short effect)
{
	(void)effect;
	gStopped++;
	*channel = -1;
	return true;
}

Boolean Update3DSoundChannel(int effect, short* channel, OGLPoint3D* where)
{
	(void)effect;
	(void)channel;
	(void)where;
	return false;
}

int main(void)
{
	InitDialogManager();
	DoDialogMessage(DIALOG_MESSAGE_NEEDSNAILSHELL, 10, 5, NULL);
	assert(gPlayed == 1);
	assert(BeginScriptDialog());
	assert(gStopped == 1 && gDialogSoundChannel == -1);
	UpdateDialogMessage();
	assert(gMessageDuration == 5 && gDialogAlpha == 0);
	DoDialogMessage(DIALOG_MESSAGE_GOTSNAILSHELL, 20, 8, NULL);
	DoDialogMessage(DIALOG_MESSAGE_FINDMARBLE, 30, 8, NULL);
	assert(gNextMessageNum == DIALOG_MESSAGE_GOTSNAILSHELL);
	assert(gPlayed == 1);
	EndScriptDialog();
	assert(gPlayed == 2 && gCurrentDialogMessageNum == DIALOG_MESSAGE_NEEDSNAILSHELL);
	assert(gNextMessageNum == DIALOG_MESSAGE_GOTSNAILSHELL);
	EndScriptDialog();
	assert(gPlayed == 2);

	InitDialogManager();
	assert(BeginScriptDialog());
	DoDialogMessage(DIALOG_MESSAGE_FINDMARBLE, 10, 5, NULL);
	assert(gCurrentDialogKey == STR_NULL && gPlayed == 2);
	EndScriptDialog();
	assert(gCurrentDialogMessageNum == DIALOG_MESSAGE_FINDMARBLE && gPlayed == 3);
	assert(gNextMessageNum == -1);

	assert(BeginScriptDialog());
	DoDialogMessage(DIALOG_MESSAGE_DONEBOWLING, 1, 5, NULL);
	assert(gDialogMode == DIALOG_MODE_DISAPPEAR);
	EndScriptDialog();
	assert(gPlayed == 3);
	UpdateDialogMessage();
	assert(gCurrentDialogMessageNum == DIALOG_MESSAGE_DONEBOWLING && gPlayed == 4);
	gVoicePlaying = false;
	assert(BeginScriptDialog());
	EndScriptDialog();
	assert(gPlayed == 4);
	return 0;
}

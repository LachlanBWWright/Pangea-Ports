#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "pangea_presentation.h"
#include "pangea_presentation_layout.h"
#include <math.h>
#include <string.h>

#define SCRIPT_CONFIRM_SOUND EFFECT_DUELKEY
#define SCRIPT_PICKUP_SOUND EFFECT_GETCOIN
#define SCRIPT_POSITIONAL_AUDIO true
#define SCRIPT_PRESENTATION_PLAYER 0
#define SCRIPT_SOUND_CATALOG(X) \
	X("ui.confirm", EFFECT_DUELKEY) \
	X("pickup.chime", EFFECT_GETCOIN) \
	X("ui.success", EFFECT_DUELKEYSDONE) \
	X("ui.error", EFFECT_DUELFAIL) \
	X("timer.chime", EFFECT_TIMERCHIME) \
	X("weapon.reload", EFFECT_RELOAD) \
	X("weapon.empty", EFFECT_EMPTY) \
	X("impact.metal", EFFECT_BULLETHITMETAL) \
	X("impact.ricochet", EFFECT_RICOCHET) \
	X("shield.hit", EFFECT_SHIELDHIT)
#define SCRIPT_EFFECT_CATALOG(X) \
	X("sparkles") X("smoke") X("dust") \
	X("sparks.red") X("sparks.blue") X("sparks.gold")

static PangeaPresentationStatus ScriptPresentation_Burst(const char* name, const PangeaPresentationOptions* options)
{
	int texture;
	bool smoke = !strcmp(name,"smoke");
	bool dust = !strcmp(name,"dust");
	if (smoke) texture = PARTICLE_SObjType_GreySmoke;
	else if (dust) texture = PARTICLE_SObjType_Dust;
	else if (!strcmp(name,"sparkles")) texture = PARTICLE_SObjType_WhiteSpark;
	else if (!strcmp(name,"sparks.red")) texture = PARTICLE_SObjType_RedSpark;
	else if (!strcmp(name,"sparks.blue")) texture = PARTICLE_SObjType_BlueSpark;
	else if (!strcmp(name,"sparks.gold")) texture = PARTICLE_SObjType_YellowGlint;
	else return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	if (texture >= gNumSpritesInGroupList[SPRITE_GROUP_PARTICLES]) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	bool cloud = smoke || dust;
	NewParticleGroupDefType definition = {
		.type=PARTICLE_TYPE_FALLINGSPARKS,.flags=PARTICLE_FLAGS_DONTCHECKGROUND,
		.gravity=smoke ? -30 : dust ? 10 : 80,
		.baseScale=(cloud ? 12 : 5)*options->scale,
		.decayRate=cloud ? -.2f : 0,.fadeRate=cloud ? .8f : 1.5f,
		.particleTextureNum=texture,.srcBlend=GL_SRC_ALPHA,
		.dstBlend=cloud ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE,
	};
	short group = NewParticleGroup(&definition);
	if (group < 0) return PANGEA_PRESENTATION_CAPACITY;
	OGLPoint3D where = {options->x, options->y, options->z};
	for (int i=0; i<12; i++)
	{
		float angle = (float)i*.5235988f;
		OGLVector3D velocity = {cosf(angle)*35, smoke ? 40 : dust ? 15 : 60, sinf(angle)*35};
		NewParticleDefType particle = {.groupNum=group,.where=&where,.delta=&velocity,.scale=1,.alpha=1};
		if (AddParticleToGroup(&particle)) return PANGEA_PRESENTATION_CAPACITY;
	}
	return PANGEA_PRESENTATION_OK;
}

static bool ScriptPresentation_TextAvailable(void)
{
	return gNumSpritesInGroupList[SPRITE_GROUP_FONT] > FONT_SObjType_QuestionMark;
}
#define SCRIPT_PRESENTATION_TEXT_AVAILABLE ScriptPresentation_TextAvailable

static void ScriptPresentation_FontText(const char* text, char output[128])
{
	size_t remaining = strlen(text), position = 0;
	while (remaining && position < 127)
	{
		unsigned int codepoint;
		size_t consumed = PangeaPresentation_UTF8Character((const unsigned char*)text,remaining,&codepoint);
		if (!consumed) { consumed = 1; codepoint = '?'; }
		output[position++] = (char)(codepoint < 128 ? codepoint : '?');
		text += consumed;
		remaining -= consumed;
	}
	output[position] = 0;
}

static void ScriptPresentation_DrawLine(const char* text, int row)
{
	if (!ScriptPresentation_TextAvailable()) return;
	char display[128];
	ScriptPresentation_FontText(text,display);
	float scale = 18;
	float width = GetStringWidth(display,scale);
	if (width > 440) scale *= 440/width;
	float y = 108+row*20;
	OGLColorRGB savedColor = gGlobalColorFilter;
	float savedAlpha = gGlobalTransparency;
	OGL_PushState();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	SetColor4f(.08f,.04f,.02f,.78f);
	glBegin(GL_QUADS);
	glVertex2f(84,y-10); glVertex2f(556,y-10);
	glVertex2f(556,y+10); glVertex2f(84,y+10);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	gGlobalTransparency = 1;
	gGlobalColorFilter = (OGLColorRGB){0,0,0};
	DrawFontString(display,322,y+2,scale,true);
	gGlobalColorFilter = (OGLColorRGB){1,.9f,.7f};
	DrawFontString(display,320,y,scale,true);
	gGlobalColorFilter = savedColor;
	gGlobalTransparency = savedAlpha;
	OGL_PopState();
}

static void ScriptPresentation_Input(void)
{
	PangeaDialogue_Input(GetNewNeedState(kNeed_UIConfirm),GetNewNeedState(kNeed_UIBack),GetNewNeedState(kNeed_UIDown)-GetNewNeedState(kNeed_UIUp));
}

#include "pangea_presentation_native.inc"
#endif

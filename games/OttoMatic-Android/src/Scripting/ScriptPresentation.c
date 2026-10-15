#ifdef PANGEA_ENABLE_SCRIPTING
#include "game.h"
#include "pangea_presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCRIPT_SOUND_CATALOG(X) \
	X("ui.confirm", EFFECT_MENUCHANGE) \
	X("pickup.chime", EFFECT_ATOMCHIME) \
	X("ui.cancel", EFFECT_BADSELECT) \
	X("pickup.life", EFFECT_NEWLIFE) \
	X("checkpoint.activate", EFFECT_CHECKPOINTHIT) \
	X("teleport.arrive", EFFECT_TELEPORTHUMAN) \
	X("weapon.change", EFFECT_CHANGEWEAPON) \
	X("warning.health", EFFECT_HEALTHWARNING)
#define SCRIPT_EFFECT_CATALOG(X) \
	X("sparkles") X("smoke") X("sparks.blue") X("sparks.green") \
	X("snow") X("bubbles") X("teleport.glow")
#define SCRIPT_POSITIONAL_AUDIO true
#define SCRIPT_PRESENTATION_PLAYER 0
#define SCRIPT_PRESENTATION_RESET reset_text

static PangeaPresentationStatus ScriptPresentation_Burst(const char* name, const PangeaPresentationOptions* options)
{
	static const struct
	{
		const char* name;
		short texture;
		float gravity, size, fade, upward;
		bool additive;
	} presets[]={
		{"sparkles", PARTICLE_SObjType_WhiteSpark, 80, 5, 1, 60, true},
		{"smoke", PARTICLE_SObjType_GreySmoke, -30, 12, 1, 40, false},
		{"sparks.blue", PARTICLE_SObjType_BlueSpark, 80, 5, 1.5f, 60, true},
		{"sparks.green", PARTICLE_SObjType_GreenSpark, 80, 5, 1.5f, 60, true},
		{"snow", PARTICLE_SObjType_SnowFlakes, 20, 5, .8f, -20, false},
		{"bubbles", PARTICLE_SObjType_Bubble, -15, 8, .8f, 30, false},
		{"teleport.glow", PARTICLE_SObjType_BlueGlint, -20, 10, 1.2f, 40, true},
	};
	int preset=-1;
	for (unsigned i=0; i<sizeof(presets)/sizeof(presets[0]); i++)
		if (!strcmp(name,presets[i].name)) { preset=(int)i; break; }
	if (preset<0) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	if (!gSpriteGroupList[SPRITE_GROUP_PARTICLES] || presets[preset].texture>=gNumSpritesInGroupList[SPRITE_GROUP_PARTICLES])
		return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	NewParticleGroupDefType definition={
		.type=PARTICLE_TYPE_FALLINGSPARKS,.flags=PARTICLE_FLAGS_DONTCHECKGROUND,
		.gravity=presets[preset].gravity,.baseScale=presets[preset].size*options->scale,
		.fadeRate=presets[preset].fade,.particleTextureNum=presets[preset].texture,
		.srcBlend=GL_SRC_ALPHA,.dstBlend=presets[preset].additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA,
	};
	short group=NewParticleGroup(&definition);
	if (group<0) return PANGEA_PRESENTATION_CAPACITY;
	OGLPoint3D where={options->x,options->y,options->z};
	for (int i=0; i<12; i++)
	{
		float angle=(float)i*.5235988f;
		OGLVector3D velocity={cosf(angle)*35,presets[preset].upward,sinf(angle)*35};
		NewParticleDefType particle={.groupNum=group,.where=&where,.delta=&velocity,.scale=1,.alpha=1};
		if (AddParticleToGroup(&particle)) return PANGEA_PRESENTATION_CAPACITY;
	}
	return PANGEA_PRESENTATION_OK;
}

static ObjNode* gText[PANGEA_PRESENTATION_ROWS];
static char gLabels[PANGEA_PRESENTATION_ROWS][128];

static void reset_text(void)
{
	for (int i=0; i<PANGEA_PRESENTATION_ROWS; i++)
	{
		if (gText[i]) DeleteObject(gText[i]);
		gText[i]=NULL;
		gLabels[i][0]=0;
	}
}

static void ScriptPresentation_DrawLine(const char* text, int row)
{
	if (!gText[row])
	{
		NewObjectDefinitionType definition={.scale=1,.slot=SPRITE_SLOT};
		gText[row]=TextMesh_New(text,kTextMeshAlignLeft,&definition);
		gText[row]->StatusBits|=STATUS_BIT_HIDDEN;
	}
	else if (strcmp(text,gLabels[row])) TextMesh_Update(text,kTextMeshAlignLeft,gText[row]);
	snprintf(gLabels[row],sizeof(gLabels[row]),"%s",text);
	float uiScale=fminf(GetUIScale(),g2DLogicalHeight/480.0f);
	float width=fminf(460*uiScale,g2DLogicalWidth-32);
	float left=(g2DLogicalWidth-width)*.5f;
	float top=(140+row*21)*uiScale;
	float naturalWidth=gText[row]->RightOff-gText[row]->LeftOff;
	float textScale=.6f*uiScale;
	if (naturalWidth*textScale>width-24*uiScale) textScale=(width-24*uiScale)/naturalWidth;
	gText[row]->Scale=(OGLVector3D){textScale,textScale,textScale};
	gText[row]->Coord=(OGLPoint3D){left+12*uiScale,top+7*uiScale,0};
	UpdateObjectTransforms(gText[row]);
	OGL_PushState();
	SetInfobarSpriteState(false);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	SetColor4f(.02f,.05f,.08f,.78f);
	glBegin(GL_QUADS);
	glVertex2f(left,top); glVertex2f(left+width,top);
	glVertex2f(left+width,top+21*uiScale); glVertex2f(left,top+21*uiScale);
	glEnd();
	SetColor4f(.25f,.8f,1,.9f);
	glBegin(GL_QUADS);
	glVertex2f(left,top); glVertex2f(left+2*uiScale,top);
	glVertex2f(left+2*uiScale,top+21*uiScale); glVertex2f(left,top+21*uiScale);
	glEnd();
	SetColor4f(1,1,1,1);
	MO_DrawObject(gText[row]->BaseGroup);
	OGL_PopState();
}

static void ScriptPresentation_Input(void)
{
	PangeaDialogue_Input(GetNewNeedState(kNeed_UIConfirm),GetNewNeedState(kNeed_UIBack),GetNewNeedState(kNeed_UIDown)-GetNewNeedState(kNeed_UIUp));
}

#include "pangea_presentation_native.inc"
#endif

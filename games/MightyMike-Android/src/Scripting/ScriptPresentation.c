#ifdef PANGEA_ENABLE_SCRIPTING
#include "myglobals.h"
#include "externs.h"
#include "object.h"
#include "objecttypes.h"
#include "shape.h"
#include "font.h"
#include "input.h"
#include "sound2.h"
#include "pangea_presentation.h"
#include <math.h>
#include <string.h>

#define SCRIPT_CONFIRM_SOUND SOUND_SELECTCHIME
#define SCRIPT_PICKUP_SOUND SOUND_HEALTHDING
#define SCRIPT_POSITIONAL_AUDIO false
#define SCRIPT_AUDIO_PARAMETERS true
#define SCRIPT_PRESENTATION_PLAYER 0
#define SCRIPT_PRESENTATION_RESET reset_effects
#define SCRIPT_PRESENTATION_DRAW_EFFECTS() draw_effects()

static struct { float x,y,vx,vy,age; bool active,smoke; } gParticles[64];

static void reset_effects(void) { memset(gParticles,0,sizeof(gParticles)); }

void MikeScript_UpdateEffects(float deltaSeconds)
{
	if (gIsGamePaused || !isfinite(deltaSeconds) || deltaSeconds<=0) return;
	for (int i=0; i<64; i++)
	{
		if (!gParticles[i].active) continue;
		gParticles[i].age+=deltaSeconds;
		if (gParticles[i].age>=.7f) gParticles[i].active=false;
	}
}

static PangeaPresentationStatus ScriptPresentation_Burst(const char* id, const PangeaPresentationOptions* options)
{
	bool smoke=!strcmp(id,"smoke");
	if (!smoke && strcmp(id,"sparkles")) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	int available=0;
	for (int i=0; i<64; i++) if (!gParticles[i].active) available++;
	if (available<8) return PANGEA_PRESENTATION_CAPACITY;
	int emitted=0;
	for (int i=0; i<64 && emitted<8; i++)
	{
		if (gParticles[i].active) continue;
		float angle=emitted++*.7853982f;
		gParticles[i].x=options->x; gParticles[i].y=options->y;
		gParticles[i].vx=cosf(angle)*20*options->scale;
		gParticles[i].vy=smoke ? -20 : sinf(angle)*20*options->scale;
		gParticles[i].age=0;
		gParticles[i].active=true;
		gParticles[i].smoke=smoke;
	}
	return PANGEA_PRESENTATION_OK;
}

static void draw_effects(void)
{
	if (!gIndexedFramebuffer) return;
	uint8_t colors[2]={0};
	for (int style=0; style<2; style++)
	{
		uint64_t best=UINT64_MAX;
		int target=style ? 35000 : 65535;
		for (int index=0; index<256; index++)
		{
			RGBColor color=gGamePalette.baseColors[index];
			int64_t red=(int)color.red-target, green=(int)color.green-target, blue=(int)color.blue-target;
			uint64_t distance=(uint64_t)(red*red+green*green+blue*blue);
			if (distance<best) { best=distance; colors[style]=(uint8_t)index; }
		}
	}
	for (int i=0; i<64; i++)
	{
		if (!gParticles[i].active) continue;
		float elapsed=gParticles[i].age;
		int x=(int)(gParticles[i].x+gParticles[i].vx*elapsed)-gScrollX;
		int y=(int)(gParticles[i].y+gParticles[i].vy*elapsed)-gScrollY;
		if (x<0 || y<0 || x>=VISIBLE_WIDTH || y>=VISIBLE_HEIGHT) continue;
		gIndexedFramebuffer[y*VISIBLE_WIDTH+x]=colors[gParticles[i].smoke ? 1 : 0];
	}
}

static void ScriptPresentation_DrawLine(const char* text, int row)
{
	extern Ptr gSHAPE_HEADER_Ptrs[MAX_SHAPE_GROUPS][MAX_SHAPES_IN_FILE];
	if (!gSHAPE_HEADER_Ptrs[GroupNum_BigFont][ObjType_BigFont]) LoadShapeTable(":Shapes:highscore.shapes",GROUP_WIN);
	int x=20, y=80+row*24;
	for (const char* c=text; *c && x<VISIBLE_WIDTH-20; c++)
	{
		if (*c!=' ')
		{
			int frame=ASCIIToBigFont(*c)-1;
			if (frame>=0) DrawFrameToScreen(x,y,GroupNum_BigFont,ObjType_BigFont,frame);
		}
		x+=*c==' ' ? 7 : 14;
	}
}

static void ScriptPresentation_Input(void)
{
	PangeaDialogue_Input(GetNewNeedState(kNeed_UIConfirm),GetNewNeedState(kNeed_UIBack),GetNewNeedState(kNeed_UIDown)-GetNewNeedState(kNeed_UIUp));
}

#include "pangea_presentation_native.inc"
#endif

#include <math.h>
#include <string.h>

static PangeaPresentationStatus ScriptPresentation_Burst(const char* id, const PangeaPresentationOptions* options)
{
	bool smoke=!strcmp(id,"smoke");
	if (!smoke && strcmp(id,"sparkles")) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	NewParticleGroupDefType definition={
		.type=PARTICLE_TYPE_FALLINGSPARKS,.flags=PARTICLE_FLAGS_DONTCHECKGROUND,
		.gravity=smoke ? -30 : 80,.baseScale=(smoke ? 12 : 5)*options->scale,
		.decayRate=smoke ? -.2f : 0,.fadeRate=1,
		.particleTextureNum=smoke ? PARTICLE_SObjType_GreySmoke : PARTICLE_SObjType_WhiteSpark,
		.srcBlend=GL_SRC_ALPHA,.dstBlend=smoke ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE,
	};
	short group=NewParticleGroup(&definition);
	if (group<0) return PANGEA_PRESENTATION_CAPACITY;
#ifdef SCRIPT_PARTICLE_PANES
	SetParticleGroupVisiblePanes(group,options->player<0 || options->player==0,options->player<0 || options->player==1);
#endif
	OGLPoint3D where={options->x,options->y,options->z};
	for (int i=0; i<12; i++)
	{
		float angle=(float)i*.5235988f;
		OGLVector3D velocity={cosf(angle)*35,smoke ? 40 : 60,sinf(angle)*35};
		NewParticleDefType particle={.groupNum=group,.where=&where,.delta=&velocity,.scale=1,.alpha=1};
		if (AddParticleToGroup(&particle)) return PANGEA_PRESENTATION_CAPACITY;
	}
	return PANGEA_PRESENTATION_OK;
}

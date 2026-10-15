#include "pangea_presentation_layout.h"

#ifndef SCRIPT_TEXT_X
#define SCRIPT_TEXT_X 70
#endif
#ifndef SCRIPT_TEXT_Y
#define SCRIPT_TEXT_Y 130
#endif
#ifndef SCRIPT_TEXT_SCALE
#define SCRIPT_TEXT_SCALE .32f
#endif
#ifndef SCRIPT_TEXT_ROW_HEIGHT
#define SCRIPT_TEXT_ROW_HEIGHT 22
#endif

static TQ3TriMeshData* gPresentationMeshes[PANGEA_PRESENTATION_ROWS];
static TQ3Matrix4x4 gPresentationMatrices[PANGEA_PRESENTATION_ROWS];
static TQ3Matrix4x4 gPresentationShadowMatrices[PANGEA_PRESENTATION_ROWS];
static RenderModifiers gPresentationShadowModifiers;
static char gPresentationLabels[PANGEA_PRESENTATION_ROWS][128];

static void reset_text(void)
{
	for (int i=0; i<PANGEA_PRESENTATION_ROWS; i++)
	{
		if (gPresentationMeshes[i]) Q3TriMeshData_Dispose(gPresentationMeshes[i]);
		gPresentationMeshes[i]=NULL;
		gPresentationLabels[i][0]=0;
	}
}

static void ScriptPresentation_DrawLine(const char* text, int row)
{
	char label[128];
	PangeaPresentation_Latin1(text,label);
	text=label;
	if (!gPresentationMeshes[row] || strcmp(text,gPresentationLabels[row]))
	{
		TextMeshDef definition;
		TextMesh_FillDef(&definition);
		definition.align=TEXTMESH_ALIGN_LEFT;
		gPresentationMeshes[row]=TextMesh_SetMesh(&definition,text,gPresentationMeshes[row]);
		snprintf(gPresentationLabels[row],sizeof(gPresentationLabels[row]),"%s",text);
	}
	if (!gPresentationMeshes[row]) return;
	TQ3Matrix4x4* matrix=&gPresentationMatrices[row];
	Q3Matrix4x4_SetScale(matrix,SCRIPT_TEXT_SCALE,-SCRIPT_TEXT_SCALE,1);
	matrix->value[3][0]=SCRIPT_TEXT_X;
	matrix->value[3][1]=SCRIPT_TEXT_Y+row*SCRIPT_TEXT_ROW_HEIGHT;
	gPresentationShadowMatrices[row]=*matrix;
	gPresentationShadowMatrices[row].value[3][0]+=2;
	gPresentationShadowMatrices[row].value[3][1]+=2;
	gPresentationShadowModifiers=kDefaultRenderMods_DebugUI;
	gPresentationShadowModifiers.diffuseColor=(TQ3ColorRGBA){0,0,0,1};
	gPresentationShadowModifiers.sortPriority--;
	Render_SubmitMesh(gPresentationMeshes[row],&gPresentationShadowMatrices[row],&gPresentationShadowModifiers,&kQ3Point3D_Zero,0);
	Render_SubmitMesh(gPresentationMeshes[row],matrix,&kDefaultRenderMods_DebugUI,&kQ3Point3D_Zero,0);
}

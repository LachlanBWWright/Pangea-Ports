#include "pangea_presentation_layout.h"

static TQ3TriMeshData* gPresentationMeshes[PANGEA_PRESENTATION_ROWS];
static TQ3Matrix4x4 gPresentationMatrices[PANGEA_PRESENTATION_ROWS][2];
static char gPresentationLabels[PANGEA_PRESENTATION_ROWS][128];
static RenderModifiers gPresentationTextMods;
static RenderModifiers gPresentationShadowMods;

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
	if (!TextMesh_IsAvailable()) return;
	char label[128];
	PangeaPresentation_Latin1(text,label);
	if (!gPresentationMeshes[row] || strcmp(label,gPresentationLabels[row]))
	{
		TextMeshDef definition;
		TextMesh_FillDef(&definition);
		definition.align=TEXTMESH_ALIGN_LEFT;
		// Recycled text meshes have no capacity metadata and cannot safely grow.
		if (gPresentationMeshes[row]) Q3TriMeshData_Dispose(gPresentationMeshes[row]);
		gPresentationMeshes[row]=TextMesh_CreateMesh(&definition,label);
		snprintf(gPresentationLabels[row],sizeof(gPresentationLabels[row]),"%s",label);
	}
	if (!gPresentationMeshes[row]) return;
	gPresentationTextMods=kDefaultRenderMods_UI;
	gPresentationTextMods.drawOrder=kDrawOrder_ScriptPresentation;
	gPresentationShadowMods=kDefaultRenderMods_UI;
	gPresentationShadowMods.diffuseColor=(TQ3ColorRGBA){0,0,0,1};
	for (int shadow=1; shadow>=0; shadow--)
	{
		TQ3Matrix4x4* matrix=&gPresentationMatrices[row][shadow];
		Q3Matrix4x4_SetScale(matrix,.32f,-.32f,1);
		matrix->value[3][0]=56+shadow*2;
		matrix->value[3][1]=128+row*22+shadow*2;
		Render_SubmitMesh(gPresentationMeshes[row],matrix,shadow ? &gPresentationShadowMods : &gPresentationTextMods,&kQ3Point3D_Zero,0);
	}
}

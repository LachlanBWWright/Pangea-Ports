// RENDERER.C
// OpenGL 3.3 / OpenGL ES 3.0 renderer
// (C)2021 Iliyas Jorio
// This file is part of Bugdom. https://github.com/jorio/bugdom

/****************************/
/*    EXTERNALS             */
/****************************/

// On desktop (non-ES) builds, we need GL 2.0+ shader function prototypes.
// Must be defined before game.h pulls in SDL_opengl.h -> SDL_opengl_glext.h.
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#define GL_GLEXT_PROTOTYPES 1
#endif

#include "game.h"
#include "RenderPolicy.h"

#include <QD3D.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
static PFNGLACTIVETEXTUREPROC gGlActiveTexture;
static PFNGLATTACHSHADERPROC gGlAttachShader;
static PFNGLBINDATTRIBLOCATIONPROC gGlBindAttribLocation;
static PFNGLBINDBUFFERPROC gGlBindBuffer;
static PFNGLBINDFRAMEBUFFERPROC gGlBindFramebuffer;
static PFNGLBINDVERTEXARRAYPROC gGlBindVertexArray;
static PFNGLBLITFRAMEBUFFERPROC gGlBlitFramebuffer;
static PFNGLBUFFERDATAPROC gGlBufferData;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC gGlCheckFramebufferStatus;
static PFNGLCOMPILESHADERPROC gGlCompileShader;
static PFNGLCREATEPROGRAMPROC gGlCreateProgram;
static PFNGLCREATESHADERPROC gGlCreateShader;
static PFNGLDELETESHADERPROC gGlDeleteShader;
static PFNGLDELETEBUFFERSPROC gGlDeleteBuffers;
static PFNGLDELETEFRAMEBUFFERSPROC gGlDeleteFramebuffers;
static PFNGLDELETEVERTEXARRAYSPROC gGlDeleteVertexArrays;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC gGlDisableVertexAttribArray;
static PFNGLDRAWBUFFERSPROC gGlDrawBuffers;
static PFNGLENABLEVERTEXATTRIBARRAYPROC gGlEnableVertexAttribArray;
static PFNGLFRAMEBUFFERTEXTURE2DPROC gGlFramebufferTexture2D;
static PFNGLGENBUFFERSPROC gGlGenBuffers;
static PFNGLGENFRAMEBUFFERSPROC gGlGenFramebuffers;
static PFNGLGENVERTEXARRAYSPROC gGlGenVertexArrays;
static PFNGLGETATTRIBLOCATIONPROC gGlGetAttribLocation;
static PFNGLGETPROGRAMIVPROC gGlGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC gGlGetProgramInfoLog;
static PFNGLGETSHADERIVPROC gGlGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC gGlGetShaderInfoLog;
static PFNGLGETUNIFORMLOCATIONPROC gGlGetUniformLocation;
static PFNGLLINKPROGRAMPROC gGlLinkProgram;
static PFNGLSHADERSOURCEPROC gGlShaderSource;
static PFNGLUSEPROGRAMPROC gGlUseProgram;
static PFNGLUNIFORM1FPROC gGlUniform1f;
static PFNGLUNIFORM1IPROC gGlUniform1i;
static PFNGLUNIFORM2FPROC gGlUniform2f;
static PFNGLUNIFORM3FPROC gGlUniform3f;
static PFNGLUNIFORM4FPROC gGlUniform4f;
static PFNGLUNIFORMMATRIX3FVPROC gGlUniformMatrix3fv;
static PFNGLUNIFORMMATRIX4FVPROC gGlUniformMatrix4fv;
static PFNGLVERTEXATTRIBPOINTERPROC gGlVertexAttribPointer;

#define glActiveTexture gGlActiveTexture
#define glAttachShader gGlAttachShader
#define glBindAttribLocation gGlBindAttribLocation
#define glBindBuffer gGlBindBuffer
#define glBindFramebuffer gGlBindFramebuffer
#define glBindVertexArray gGlBindVertexArray
#define glBlitFramebuffer gGlBlitFramebuffer
#define glBufferData gGlBufferData
#define glCheckFramebufferStatus gGlCheckFramebufferStatus
#define glCompileShader gGlCompileShader
#define glCreateProgram gGlCreateProgram
#define glCreateShader gGlCreateShader
#define glDeleteShader gGlDeleteShader
#define glDeleteBuffers gGlDeleteBuffers
#define glDeleteFramebuffers gGlDeleteFramebuffers
#define glDeleteVertexArrays gGlDeleteVertexArrays
#define glDisableVertexAttribArray gGlDisableVertexAttribArray
#define glDrawBuffers gGlDrawBuffers
#define glEnableVertexAttribArray gGlEnableVertexAttribArray
#define glFramebufferTexture2D gGlFramebufferTexture2D
#define glGenBuffers gGlGenBuffers
#define glGenFramebuffers gGlGenFramebuffers
#define glGenVertexArrays gGlGenVertexArrays
#define glGetAttribLocation gGlGetAttribLocation
#define glGetProgramiv gGlGetProgramiv
#define glGetProgramInfoLog gGlGetProgramInfoLog
#define glGetShaderiv gGlGetShaderiv
#define glGetShaderInfoLog gGlGetShaderInfoLog
#define glGetUniformLocation gGlGetUniformLocation
#define glLinkProgram gGlLinkProgram
#define glShaderSource gGlShaderSource
#define glUseProgram gGlUseProgram
#define glUniform1f gGlUniform1f
#define glUniform1i gGlUniform1i
#define glUniform2f gGlUniform2f
#define glUniform3f gGlUniform3f
#define glUniform4f gGlUniform4f
#define glUniformMatrix3fv gGlUniformMatrix3fv
#define glUniformMatrix4fv gGlUniformMatrix4fv
#define glVertexAttribPointer gGlVertexAttribPointer

static void LoadWindowsGLFunctions(void)
{
	gGlActiveTexture = (PFNGLACTIVETEXTUREPROC) SDL_GL_GetProcAddress("glActiveTexture");
	gGlAttachShader = (PFNGLATTACHSHADERPROC) SDL_GL_GetProcAddress("glAttachShader");
	gGlBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC) SDL_GL_GetProcAddress("glBindAttribLocation");
	gGlBindBuffer = (PFNGLBINDBUFFERPROC) SDL_GL_GetProcAddress("glBindBuffer");
	gGlBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) SDL_GL_GetProcAddress("glBindFramebuffer");
	gGlBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) SDL_GL_GetProcAddress("glBindVertexArray");
	gGlBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) SDL_GL_GetProcAddress("glBlitFramebuffer");
	gGlBufferData = (PFNGLBUFFERDATAPROC) SDL_GL_GetProcAddress("glBufferData");
	gGlCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) SDL_GL_GetProcAddress("glCheckFramebufferStatus");
	gGlCompileShader = (PFNGLCOMPILESHADERPROC) SDL_GL_GetProcAddress("glCompileShader");
	gGlCreateProgram = (PFNGLCREATEPROGRAMPROC) SDL_GL_GetProcAddress("glCreateProgram");
	gGlCreateShader = (PFNGLCREATESHADERPROC) SDL_GL_GetProcAddress("glCreateShader");
	gGlDeleteShader = (PFNGLDELETESHADERPROC) SDL_GL_GetProcAddress("glDeleteShader");
	gGlDeleteBuffers = (PFNGLDELETEBUFFERSPROC) SDL_GL_GetProcAddress("glDeleteBuffers");
	gGlDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) SDL_GL_GetProcAddress("glDeleteFramebuffers");
	gGlDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) SDL_GL_GetProcAddress("glDeleteVertexArrays");
	gGlDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) SDL_GL_GetProcAddress("glDisableVertexAttribArray");
	gGlDrawBuffers = (PFNGLDRAWBUFFERSPROC) SDL_GL_GetProcAddress("glDrawBuffers");
	gGlEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) SDL_GL_GetProcAddress("glEnableVertexAttribArray");
	gGlFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) SDL_GL_GetProcAddress("glFramebufferTexture2D");
	gGlGenBuffers = (PFNGLGENBUFFERSPROC) SDL_GL_GetProcAddress("glGenBuffers");
	gGlGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) SDL_GL_GetProcAddress("glGenFramebuffers");
	gGlGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) SDL_GL_GetProcAddress("glGenVertexArrays");
	gGlGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) SDL_GL_GetProcAddress("glGetAttribLocation");
	gGlGetProgramiv = (PFNGLGETPROGRAMIVPROC) SDL_GL_GetProcAddress("glGetProgramiv");
	gGlGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) SDL_GL_GetProcAddress("glGetProgramInfoLog");
	gGlGetShaderiv = (PFNGLGETSHADERIVPROC) SDL_GL_GetProcAddress("glGetShaderiv");
	gGlGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) SDL_GL_GetProcAddress("glGetShaderInfoLog");
	gGlGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) SDL_GL_GetProcAddress("glGetUniformLocation");
	gGlLinkProgram = (PFNGLLINKPROGRAMPROC) SDL_GL_GetProcAddress("glLinkProgram");
	gGlShaderSource = (PFNGLSHADERSOURCEPROC) SDL_GL_GetProcAddress("glShaderSource");
	gGlUseProgram = (PFNGLUSEPROGRAMPROC) SDL_GL_GetProcAddress("glUseProgram");
	gGlUniform1f = (PFNGLUNIFORM1FPROC) SDL_GL_GetProcAddress("glUniform1f");
	gGlUniform1i = (PFNGLUNIFORM1IPROC) SDL_GL_GetProcAddress("glUniform1i");
	gGlUniform2f = (PFNGLUNIFORM2FPROC) SDL_GL_GetProcAddress("glUniform2f");
	gGlUniform3f = (PFNGLUNIFORM3FPROC) SDL_GL_GetProcAddress("glUniform3f");
	gGlUniform4f = (PFNGLUNIFORM4FPROC) SDL_GL_GetProcAddress("glUniform4f");
	gGlUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC) SDL_GL_GetProcAddress("glUniformMatrix3fv");
	gGlUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) SDL_GL_GetProcAddress("glUniformMatrix4fv");
	gGlVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) SDL_GL_GetProcAddress("glVertexAttribPointer");
}
#endif

#pragma mark -

/****************************/
/*    PROTOTYPES            */
/****************************/

typedef struct RendererState
{
GLuint          shaderProgram;

// Attribute locations
GLint           loc_a_Position;
GLint           loc_a_Normal;
GLint           loc_a_TexCoord;
GLint           loc_a_Color;

// Uniform locations
GLint           loc_u_Projection;
GLint           loc_u_ModelView;
GLint           loc_u_NormalMatrix;
GLint           loc_u_DiffuseColor;
GLint           loc_u_Texture0;
GLint           loc_u_TextureEnabled;
GLint           loc_u_LightingEnabled;
GLint           loc_u_AmbientColor;
GLint           loc_u_NumLights;
GLint           loc_u_LightDir0;
GLint           loc_u_LightColor0;
GLint           loc_u_LightDir1;
GLint           loc_u_LightColor1;
GLint           loc_u_FogEnabled;
GLint           loc_u_FogColor;
GLint           loc_u_FogStart;
GLint           loc_u_FogEnd;
GLint           loc_u_AlphaTestEnabled;
GLint           loc_u_AlphaThreshold;
GLint           loc_u_UseVertexColors;
GLint           loc_u_AutoFadeFactor;
GLint           loc_u_ReflectionEnabled;
GLint           loc_u_ReflectionTransform;
GLint           loc_u_ReflectionNormalMatrix;
GLint           loc_u_ReflectionCamera;
GLint           loc_u_SceneDepth;
GLint           loc_u_SoftParticle;
GLint           loc_u_InvFramebufferSize;

// Tracked GL state
GLuint          boundTexture;
bool            attrib_Position;
bool            attrib_Normal;
bool            attrib_TexCoord;
bool            attrib_Color;
bool            textureEnabled;
bool            lightingEnabled;
bool            fogEnabled;
bool            alphaTestEnabled;
bool            useVertexColors;
bool            reflectionEnabled;
bool            sceneHasFog;
float           fogStart;
float           fogEnd;
float           fogColor[4];
bool            hasFlag_glDepthMask;
bool            blendFuncIsAdditive;
bool            hasState_GL_BLEND;
bool            hasState_GL_DEPTH_TEST;
bool            hasState_GL_CULL_FACE;
GLboolean       wantColorMask;
const TQ3Matrix4x4*  currentTransform;
} RendererState;

typedef struct MeshQueueEntry
{
const TQ3TriMeshData*   mesh;
const TQ3Matrix4x4*     transform;
const RenderModifiers*  mods;
float                   depth;
uint16_t                slot;
bool                    meshIsTransparent;
} MeshQueueEntry;

#define MESHQUEUE_MAX_SIZE 4096

static MeshQueueEntry       gMeshQueueEntryPool[MESHQUEUE_MAX_SIZE];
static MeshQueueEntry*      gMeshQueuePtrs[MESHQUEUE_MAX_SIZE];
static int                  gMeshQueueSize = 0;
static bool                 gFrameStarted = false;

enum { VBO_ATTRIB_POS=0, VBO_ATTRIB_NORM=1, VBO_ATTRIB_COLOR=2, VBO_ATTRIB_TC=3, VBO_ATTRIB_COUNT=4 };
enum { MESH_CACHE_SIZE = 256 };

typedef struct MeshGpuCacheEntry
{
bool                    valid;
uint32_t                lru;
const TQ3TriMeshData*   mesh;
const void*             points;
const void*             normals;
const void*             colors;
const void*             uvs;
const void*             triangles;
int                     numPoints;
int                     numTriangles;
GLuint                  vbo[VBO_ATTRIB_COUNT];
GLuint                  ebo;
} MeshGpuCacheEntry;

static GLuint               s_vertexArray = 0;
static MeshGpuCacheEntry    s_meshCache[MESH_CACHE_SIZE];
static uint32_t             s_meshCacheTick = 1;
static int                  s_forceCacheMiss = -1;

static bool ForceMeshCacheMisses(void)
{
	if (s_forceCacheMiss < 0)
	{
		const char* value = getenv("PANGEA_FORCE_CACHE_MISS");
		s_forceCacheMiss = value && value[0] && value[0] != '0';
	}
	return s_forceCacheMiss != 0;
}

static int DrawOrderComparator(void const* a_void, void const* b_void);

static void BeginDepthPass(const MeshQueueEntry* entry);
static void BeginShadingPass(const MeshQueueEntry* entry, const MeshGpuCacheEntry* cacheEntry);
static void PrepareOpaqueShading(const MeshQueueEntry* entry, const MeshGpuCacheEntry* cacheEntry);
static void PrepareAlphaShading(const MeshQueueEntry* entry, const MeshGpuCacheEntry* cacheEntry);
static void SendGeometry(const MeshQueueEntry* entry, const MeshGpuCacheEntry* cacheEntry);

static bool MeshCacheEntryMatches(const MeshGpuCacheEntry* entry, const TQ3TriMeshData* mesh)
{
	return entry->valid
		&& entry->mesh == mesh
		&& entry->points == mesh->points
		&& entry->normals == (mesh->hasVertexNormals ? mesh->vertexNormals : NULL)
		&& entry->colors == (mesh->hasVertexColors ? mesh->vertexColors : NULL)
		&& entry->uvs == mesh->vertexUVs
		&& entry->triangles == mesh->triangles
		&& entry->numPoints == mesh->numPoints
		&& entry->numTriangles == mesh->numTriangles;
}

static MeshGpuCacheEntry* GetMeshCacheEntry(const TQ3TriMeshData* mesh)
{
	MeshGpuCacheEntry* lruEntry = &s_meshCache[0];
	bool forceCacheMiss = ForceMeshCacheMisses();
	gRenderStats.cacheLookups++;

	for (int i = 0; i < MESH_CACHE_SIZE; i++)
	{
		MeshGpuCacheEntry* entry = &s_meshCache[i];

		if (!forceCacheMiss && MeshCacheEntryMatches(entry, mesh))
		{
			entry->lru = s_meshCacheTick++;
			gRenderStats.cacheHits++;
			return entry;
		}

		if (!entry->valid)
		{
			lruEntry = entry;
			break;
		}

		if (entry->lru < lruEntry->lru)
			lruEntry = entry;
	}

	gRenderStats.cacheMisses++;
	if (lruEntry->valid)
		gRenderStats.cacheEvictions++;

	if (!lruEntry->vbo[VBO_ATTRIB_POS])
		glGenBuffers(VBO_ATTRIB_COUNT, lruEntry->vbo);

	if (!lruEntry->ebo)
		glGenBuffers(1, &lruEntry->ebo);

	lruEntry->valid = true;
	lruEntry->lru = s_meshCacheTick++;
	lruEntry->mesh = mesh;
	lruEntry->points = mesh->points;
	lruEntry->normals = mesh->hasVertexNormals ? mesh->vertexNormals : NULL;
	lruEntry->colors = mesh->hasVertexColors ? mesh->vertexColors : NULL;
	lruEntry->uvs = mesh->vertexUVs;
	lruEntry->triangles = mesh->triangles;
	lruEntry->numPoints = mesh->numPoints;
	lruEntry->numTriangles = mesh->numTriangles;

	GLsizeiptr byteCount = (GLsizeiptr)(mesh->numPoints * sizeof(mesh->points[0]));
	glBindBuffer(GL_ARRAY_BUFFER, lruEntry->vbo[VBO_ATTRIB_POS]);
	glBufferData(GL_ARRAY_BUFFER, byteCount, mesh->points, GL_STATIC_DRAW);
	gRenderStats.bufferUploadCalls++;
	gRenderStats.bufferUploadBytes += (int) byteCount;

	if (mesh->hasVertexNormals)
	{
		byteCount = (GLsizeiptr)(mesh->numPoints * sizeof(mesh->vertexNormals[0]));
		glBindBuffer(GL_ARRAY_BUFFER, lruEntry->vbo[VBO_ATTRIB_NORM]);
		glBufferData(GL_ARRAY_BUFFER, byteCount, mesh->vertexNormals, GL_STATIC_DRAW);
		gRenderStats.bufferUploadCalls++;
		gRenderStats.bufferUploadBytes += (int) byteCount;
	}

	if (mesh->hasVertexColors)
	{
		byteCount = (GLsizeiptr)(mesh->numPoints * sizeof(mesh->vertexColors[0]));
		glBindBuffer(GL_ARRAY_BUFFER, lruEntry->vbo[VBO_ATTRIB_COLOR]);
		glBufferData(GL_ARRAY_BUFFER, byteCount, mesh->vertexColors, GL_STATIC_DRAW);
		gRenderStats.bufferUploadCalls++;
		gRenderStats.bufferUploadBytes += (int) byteCount;
	}

	if (mesh->vertexUVs)
	{
		byteCount = (GLsizeiptr)(mesh->numPoints * sizeof(mesh->vertexUVs[0]));
		glBindBuffer(GL_ARRAY_BUFFER, lruEntry->vbo[VBO_ATTRIB_TC]);
		glBufferData(GL_ARRAY_BUFFER, byteCount, mesh->vertexUVs, GL_STATIC_DRAW);
		gRenderStats.bufferUploadCalls++;
		gRenderStats.bufferUploadBytes += (int) byteCount;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	byteCount = (GLsizeiptr)(mesh->numTriangles * sizeof(mesh->triangles[0]));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lruEntry->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, byteCount, mesh->triangles, GL_STATIC_DRAW);
	gRenderStats.bufferUploadCalls++;
	gRenderStats.bufferUploadBytes += (int) byteCount;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return lruEntry;
}

static void CachedVertexAttrib(GLint attribLoc, const MeshGpuCacheEntry* entry, GLint attrIdx, GLint components)
{
	glBindBuffer(GL_ARRAY_BUFFER, entry->vbo[attrIdx]);
	glVertexAttribPointer(attribLoc, components, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void DrawCachedElements(const MeshGpuCacheEntry* entry)
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry->ebo);
	glDrawElements(GL_TRIANGLES, entry->numTriangles * 3, GL_UNSIGNED_INT, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void DeleteMeshCacheBuffers(void)
{
	for (int i = 0; i < MESH_CACHE_SIZE; i++)
	{
		if (s_meshCache[i].vbo[VBO_ATTRIB_POS])
		{
			glDeleteBuffers(VBO_ATTRIB_COUNT, s_meshCache[i].vbo);
			SDL_memset(s_meshCache[i].vbo, 0, sizeof(s_meshCache[i].vbo));
		}

		if (s_meshCache[i].ebo)
		{
			glDeleteBuffers(1, &s_meshCache[i].ebo);
			s_meshCache[i].ebo = 0;
		}

		s_meshCache[i].valid = false;
	}
}


#pragma mark -

/****************************/
/*    CONSTANTS             */
/****************************/

const TQ3Point3D kQ3Point3D_Zero = {0, 0, 0};

static const RenderModifiers kDefaultRenderMods =
{
.statusBits     = 0,
.diffuseColor   = {1,1,1,1},
.autoFadeFactor = 1.0f,
.drawOrder      = 0,
};

const RenderModifiers kDefaultRenderMods_UI =
{
.statusBits     = STATUS_BIT_NULLSHADER | STATUS_BIT_NOFOG | STATUS_BIT_NOZWRITE,
.diffuseColor   = {1,1,1,1},
.autoFadeFactor = 1.0f,
.drawOrder      = kDrawOrder_UI
};

static const RenderModifiers kDefaultRenderMods_FadeOverlay =
{
.statusBits     = STATUS_BIT_NULLSHADER | STATUS_BIT_NOFOG | STATUS_BIT_NOZWRITE,
.diffuseColor   = {1,1,1,1},
.autoFadeFactor = 1.0f,
.drawOrder      = kDrawOrder_FadeOverlay
};

const RenderModifiers kDefaultRenderMods_DebugUI =
{
.statusBits     = STATUS_BIT_NULLSHADER | STATUS_BIT_NOFOG | STATUS_BIT_NOZWRITE | STATUS_BIT_KEEPBACKFACES | STATUS_BIT_DONTCULL,
.diffuseColor   = {1,1,1,1},
.autoFadeFactor = 1.0f,
.drawOrder      = kDrawOrder_DebugUI
};

const RenderModifiers kDefaultRenderMods_Pillarbox =
{
.statusBits     = STATUS_BIT_NULLSHADER | STATUS_BIT_NOFOG | STATUS_BIT_NOZWRITE | STATUS_BIT_KEEPBACKFACES | STATUS_BIT_DONTCULL,
.diffuseColor   = {0,0,0,1},
.autoFadeFactor = 1.0f,
.drawOrder      = kDrawOrder_DebugUI
};

#pragma mark -

/****************************/
/*    SHADER SOURCE         */
/****************************/

static const char* kVertexShaderSource =
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
"#version 300 es\nprecision highp float;\nprecision mediump int;\n"
#else
"#version 330 core\n"
#endif
"in vec3 a_Position;\n"
"in vec3 a_Normal;\n"
"in vec2 a_TexCoord;\n"
"in vec4 a_Color;\n"
"uniform mat4 u_Projection;\n"
"uniform mat4 u_ModelView;\n"
"uniform mat3 u_NormalMatrix;\n"
"uniform int  u_LightingEnabled;\n"
"uniform vec4 u_AmbientColor;\n"
"uniform int  u_NumLights;\n"
"uniform vec3 u_LightDir0;\n"
"uniform vec4 u_LightColor0;\n"
"uniform vec3 u_LightDir1;\n"
"uniform vec4 u_LightColor1;\n"
"uniform int  u_UseVertexColors;\n"
"uniform vec4 u_DiffuseColor;\n"
"uniform float u_AutoFadeFactor;\n"
"uniform int u_ReflectionEnabled;\n"
"uniform mat3 u_ReflectionTransform;\n"
"uniform mat3 u_ReflectionNormalMatrix;\n"
"uniform vec3 u_ReflectionCamera;\n"
"uniform int  u_FogEnabled;\n"
"uniform float u_FogStart;\n"
"uniform float u_FogEnd;\n"
"out vec2 v_TexCoord;\n"
"out vec4 v_Color;\n"
"out float v_FogFactor;\n"
"void main() {\n"
"    vec4 eyePos = u_ModelView * vec4(a_Position, 1.0);\n"
"    gl_Position = u_Projection * eyePos;\n"
"    if (u_ReflectionEnabled != 0) {\n"
"        vec3 normal = normalize(u_ReflectionNormalMatrix * a_Normal);\n"
"        vec3 viewVector = u_ReflectionTransform * a_Position - u_ReflectionCamera;\n"
"        vec3 reflected = normalize(normal * (2.0 * dot(normal, viewVector)) - viewVector);\n"
"        v_TexCoord = reflected.xy * 0.5 + 0.5;\n"
"    } else {\n"
"        v_TexCoord = a_TexCoord;\n"
"    }\n"
"    vec4 baseColor = (u_UseVertexColors != 0) ? a_Color : u_DiffuseColor;\n"
"    baseColor.a *= u_AutoFadeFactor;\n"
"    if (u_LightingEnabled != 0) {\n"
"        vec3 eyeNormal = normalize(u_NormalMatrix * a_Normal);\n"
"        vec3 lit = u_AmbientColor.rgb;\n"
"        if (u_NumLights > 0) { lit += u_LightColor0.rgb * max(dot(eyeNormal, u_LightDir0), 0.0); }\n"
"        if (u_NumLights > 1) { lit += u_LightColor1.rgb * max(dot(eyeNormal, u_LightDir1), 0.0); }\n"
"        v_Color = vec4(baseColor.rgb * clamp(lit, 0.0, 1.0), baseColor.a);\n"
"    } else {\n"
"        v_Color = baseColor;\n"
"    }\n"
"    if (u_FogEnabled != 0) {\n"
"        float d = length(eyePos.xyz);\n"
"        v_FogFactor = clamp((u_FogEnd - d) / (u_FogEnd - u_FogStart), 0.0, 1.0);\n"
"    } else {\n"
"        v_FogFactor = 1.0;\n"
"    }\n"
"}\n";

static const char* kFragmentShaderSource =
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
"#version 300 es\nprecision highp float;\nprecision mediump int;\n"
#else
"#version 330 core\n"
#endif
"uniform sampler2D u_Texture0;\n"
"uniform sampler2D u_SceneDepth;\n"
"uniform int u_SoftParticle;\n"
"uniform vec2 u_InvFramebufferSize;\n"
"uniform mat4 u_Projection;\n"
"uniform int u_TextureEnabled;\n"
"uniform int u_AlphaTestEnabled;\n"
"uniform float u_AlphaThreshold;\n"
"uniform vec4 u_FogColor;\n"
"uniform int u_FogEnabled;\n"
"in vec2 v_TexCoord;\n"
"in vec4 v_Color;\n"
"in float v_FogFactor;\n"
"out vec4 fragColor;\n"
"float eyeDepth(float depth) {\n"
"    return u_Projection[3][2] / (depth * 2.0 - 1.0 + u_Projection[2][2]);\n"
"}\n"
"void main() {\n"
"    vec4 color;\n"
"    if (u_TextureEnabled != 0) {\n"
"        color = v_Color * texture(u_Texture0, v_TexCoord);\n"
"    } else {\n"
"        color = v_Color;\n"
"    }\n"
"    if (u_AlphaTestEnabled != 0 && color.a < u_AlphaThreshold) discard;\n"
"    if (u_FogEnabled != 0) { color.rgb = mix(u_FogColor.rgb, color.rgb, v_FogFactor); }\n"
"    if (u_SoftParticle != 0) {\n"
"        float sceneDepth = texture(u_SceneDepth, gl_FragCoord.xy * u_InvFramebufferSize).r;\n"
"        if (sceneDepth < 1.0) {\n"
"            color.a *= clamp((eyeDepth(sceneDepth) - eyeDepth(gl_FragCoord.z)) / 60.0, 0.0, 1.0);\n"
"        }\n"
"    }\n"
"    fragColor = color;\n"
"}\n";

#pragma mark -

/****************************/
/*    VARIABLES             */
/****************************/

static SDL_GLContext gGLContext = NULL;
static RendererState gState;
static GLuint gSceneDepthTexture = 0;
static GLuint gSceneDepthFramebuffer = 0;
static int gSceneDepthWidth = 0;
static int gSceneDepthHeight = 0;
static bool gCanCopySceneDepth = false;
float gGammaFadeFactor = 1.0f;
static TQ3TriMeshData* gFullscreenQuad = nil;

// Current matrices (set by camera / 2D mode)
static TQ3Matrix4x4 gCurrentProjection;
static TQ3Matrix4x4 gCurrentModelView;

// Matrix stack for 2D mode push/pop
#define MATRIX_STACK_DEPTH 4
static TQ3Matrix4x4 gProjectionStack[MATRIX_STACK_DEPTH];
static TQ3Matrix4x4 gModelViewStack[MATRIX_STACK_DEPTH];
static int gMatrixStackTop = 0;

#pragma mark -

/****************************/
/*    HELPERS               */
/****************************/

#if _DEBUG
void DoFatalGLError(GLenum error, const char* file, int line)
{
static char alertbuf[1024];
SDL_snprintf(alertbuf, sizeof(alertbuf), "OpenGL error 0x%x\nin %s:%d", error, file, line);
DoFatalAlert(alertbuf);
}
#endif

static GLuint CompileShader(GLenum type, const char* source)
{
GLuint shader = glCreateShader(type);
glShaderSource(shader, 1, &source, NULL);
glCompileShader(shader);

GLint status;
glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
if (!status)
{
char log[1024];
glGetShaderInfoLog(shader, sizeof(log), NULL, log);
SDL_Log("Shader compile error:\n%s\n", log);
GAME_ASSERT_MESSAGE(false, "Shader compile failed");
}
return shader;
}

static void CreateShaderProgram(void)
{
GLuint vert = CompileShader(GL_VERTEX_SHADER,   kVertexShaderSource);
GLuint frag = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);

gState.shaderProgram = glCreateProgram();
glAttachShader(gState.shaderProgram, vert);
glAttachShader(gState.shaderProgram, frag);

// Bind attribute locations before link
glBindAttribLocation(gState.shaderProgram, 0, "a_Position");
glBindAttribLocation(gState.shaderProgram, 1, "a_Normal");
glBindAttribLocation(gState.shaderProgram, 2, "a_TexCoord");
glBindAttribLocation(gState.shaderProgram, 3, "a_Color");

glLinkProgram(gState.shaderProgram);

GLint status;
glGetProgramiv(gState.shaderProgram, GL_LINK_STATUS, &status);
if (!status)
{
char log[1024];
glGetProgramInfoLog(gState.shaderProgram, sizeof(log), NULL, log);
SDL_Log("Shader link error:\n%s\n", log);
GAME_ASSERT_MESSAGE(false, "Shader link failed");
}

glDeleteShader(vert);
glDeleteShader(frag);

glUseProgram(gState.shaderProgram);

// Cache attribute locations
gState.loc_a_Position        = glGetAttribLocation(gState.shaderProgram, "a_Position");
gState.loc_a_Normal          = glGetAttribLocation(gState.shaderProgram, "a_Normal");
gState.loc_a_TexCoord        = glGetAttribLocation(gState.shaderProgram, "a_TexCoord");
gState.loc_a_Color           = glGetAttribLocation(gState.shaderProgram, "a_Color");

// Cache uniform locations
gState.loc_u_Projection      = glGetUniformLocation(gState.shaderProgram, "u_Projection");
gState.loc_u_ModelView       = glGetUniformLocation(gState.shaderProgram, "u_ModelView");
gState.loc_u_NormalMatrix    = glGetUniformLocation(gState.shaderProgram, "u_NormalMatrix");
gState.loc_u_DiffuseColor    = glGetUniformLocation(gState.shaderProgram, "u_DiffuseColor");
gState.loc_u_Texture0        = glGetUniformLocation(gState.shaderProgram, "u_Texture0");
gState.loc_u_TextureEnabled  = glGetUniformLocation(gState.shaderProgram, "u_TextureEnabled");
gState.loc_u_LightingEnabled = glGetUniformLocation(gState.shaderProgram, "u_LightingEnabled");
gState.loc_u_AmbientColor    = glGetUniformLocation(gState.shaderProgram, "u_AmbientColor");
gState.loc_u_NumLights       = glGetUniformLocation(gState.shaderProgram, "u_NumLights");
gState.loc_u_LightDir0       = glGetUniformLocation(gState.shaderProgram, "u_LightDir0");
gState.loc_u_LightColor0     = glGetUniformLocation(gState.shaderProgram, "u_LightColor0");
gState.loc_u_LightDir1       = glGetUniformLocation(gState.shaderProgram, "u_LightDir1");
gState.loc_u_LightColor1     = glGetUniformLocation(gState.shaderProgram, "u_LightColor1");
gState.loc_u_FogEnabled      = glGetUniformLocation(gState.shaderProgram, "u_FogEnabled");
gState.loc_u_FogColor        = glGetUniformLocation(gState.shaderProgram, "u_FogColor");
gState.loc_u_FogStart        = glGetUniformLocation(gState.shaderProgram, "u_FogStart");
gState.loc_u_FogEnd          = glGetUniformLocation(gState.shaderProgram, "u_FogEnd");
gState.loc_u_AlphaTestEnabled= glGetUniformLocation(gState.shaderProgram, "u_AlphaTestEnabled");
gState.loc_u_AlphaThreshold  = glGetUniformLocation(gState.shaderProgram, "u_AlphaThreshold");
gState.loc_u_UseVertexColors = glGetUniformLocation(gState.shaderProgram, "u_UseVertexColors");
gState.loc_u_AutoFadeFactor = glGetUniformLocation(gState.shaderProgram, "u_AutoFadeFactor");
gState.loc_u_ReflectionEnabled = glGetUniformLocation(gState.shaderProgram, "u_ReflectionEnabled");
gState.loc_u_ReflectionTransform = glGetUniformLocation(gState.shaderProgram, "u_ReflectionTransform");
gState.loc_u_ReflectionNormalMatrix = glGetUniformLocation(gState.shaderProgram, "u_ReflectionNormalMatrix");
gState.loc_u_ReflectionCamera = glGetUniformLocation(gState.shaderProgram, "u_ReflectionCamera");
gState.loc_u_SceneDepth = glGetUniformLocation(gState.shaderProgram, "u_SceneDepth");
gState.loc_u_SoftParticle = glGetUniformLocation(gState.shaderProgram, "u_SoftParticle");
gState.loc_u_InvFramebufferSize = glGetUniformLocation(gState.shaderProgram, "u_InvFramebufferSize");

// Set texture sampler to unit 0
glUniform1i(gState.loc_u_Texture0, 0);
glUniform1i(gState.loc_u_SceneDepth, 1);
glUniform1i(gState.loc_u_SoftParticle, 0);
glUniform1i(gState.loc_u_ReflectionEnabled, 0);
glUniform1f(gState.loc_u_AutoFadeFactor, 1.0f);

// Set initial defaults that never change
glUniform1f(gState.loc_u_AlphaThreshold, 0.4999f);
glUniform1i(gState.loc_u_NumLights, 0);
glUniform4f(gState.loc_u_AmbientColor, 1, 1, 1, 1);
}

static void UploadMatrix4x4(GLint loc, const TQ3Matrix4x4* m)
{
// TQ3Matrix4x4 is column-major (value[col][row]), which matches OpenGL's expected layout.
// ES 3 requires transpose=GL_FALSE.
glUniformMatrix4fv(loc, 1, GL_FALSE, (const float*) m->value);
}

static void UploadMatrix3x3NormalFromMV(const TQ3Matrix4x4* mv)
{
// Normal matrix = upper-left 3x3 of modelview (valid for rigid/uniform-scale transforms).
// Supply column-major (GL_FALSE) by treating TQ3 rows as GL columns.
float nm[9] = {
mv->value[0][0], mv->value[0][1], mv->value[0][2],
mv->value[1][0], mv->value[1][1], mv->value[1][2],
mv->value[2][0], mv->value[2][1], mv->value[2][2],
};
glUniformMatrix3fv(gState.loc_u_NormalMatrix, 1, GL_FALSE, nm);
}

static void UploadReflectionMatrix(GLint location, const TQ3Matrix4x4* matrix)
{
float values[9] = {
matrix->value[0][0], matrix->value[0][1], matrix->value[0][2],
matrix->value[1][0], matrix->value[1][1], matrix->value[1][2],
matrix->value[2][0], matrix->value[2][1], matrix->value[2][2],
};
glUniformMatrix3fv(location, 1, GL_FALSE, values);
}

static void UploadReflectionState(const TQ3Matrix4x4* transform)
{
TQ3Matrix4x4 inverseTranspose;
Q3Matrix4x4_Invert(transform, &inverseTranspose);
Q3Matrix4x4_Transpose(&inverseTranspose, &inverseTranspose);
UploadReflectionMatrix(gState.loc_u_ReflectionTransform, transform);
UploadReflectionMatrix(gState.loc_u_ReflectionNormalMatrix, &inverseTranspose);
const TQ3Point3D* camera = &gGameViewInfoPtr->currentCameraCoords;
glUniform3f(gState.loc_u_ReflectionCamera, camera->x, camera->y, camera->z);
}

static inline void SetAttrib(int loc, bool* flag, bool enable)
{
if (enable != *flag)
{
if (enable)
glEnableVertexAttribArray(loc);
else
glDisableVertexAttribArray(loc);
*flag = enable;
}
}

#define EnableAttrib(name)  SetAttrib(gState.loc_a_##name, &gState.attrib_##name, true)
#define DisableAttrib(name) SetAttrib(gState.loc_a_##name, &gState.attrib_##name, false)

static inline void SetStateGL(GLenum s, bool* flag, bool enable)
{
if (enable != *flag)
{
if (enable)
glEnable(s);
else
glDisable(s);
*flag = enable;
}
}

#define EnableState(s)  SetStateGL(s, &gState.hasState_##s, true)
#define DisableState(s) SetStateGL(s, &gState.hasState_##s, false)
#define SetState(s, v)  SetStateGL(s, &gState.hasState_##s, (v))

#define SetFlag(glFn, val) do { \
if ((val) != gState.hasFlag_##glFn) { \
glFn((val) ? GL_TRUE : GL_FALSE); \
gState.hasFlag_##glFn = (val); \
} } while(0)

static inline void SetColorMask(GLboolean enable)
{
if (enable != gState.wantColorMask)
{
glColorMask(enable, enable, enable, enable);
gState.wantColorMask = enable;
}
}

static inline void SetUniformBool(GLint loc, bool* tracked, bool val)
{
if (val != *tracked)
{
glUniform1i(loc, val ? 1 : 0);
*tracked = val;
}
}

#pragma mark -

//=======================================================================================================

/****************************/
/*    API IMPLEMENTATION    */
/****************************/

void Render_CreateContext(void)
{
gGLContext = SDL_GL_CreateContext(gSDLWindow);
GAME_ASSERT(gGLContext);
SDL_Log("Bugdom GL context: %s; GLSL: %s", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
}

void Render_DeleteContext(void)
{
if (gGLContext)
{
if (gSceneDepthTexture) { glDeleteTextures(1, &gSceneDepthTexture); gSceneDepthTexture = 0; }
if (gSceneDepthFramebuffer) { glDeleteFramebuffers(1, &gSceneDepthFramebuffer); gSceneDepthFramebuffer = 0; }
gSceneDepthWidth = gSceneDepthHeight = 0;
DeleteMeshCacheBuffers();
if (s_vertexArray) { glDeleteVertexArrays(1, &s_vertexArray); s_vertexArray = 0; }
SDL_GL_DestroyContext(gGLContext);
gGLContext = NULL;
}
}

void Render_SetDefaultModifiers(RenderModifiers* dest)
{
SDL_memcpy(dest, &kDefaultRenderMods, sizeof(RenderModifiers));
}

void Render_SetProjectionMatrix(const TQ3Matrix4x4* m)
{
gCurrentProjection = *m;
UploadMatrix4x4(gState.loc_u_Projection, &gCurrentProjection);
}

void Render_SetModelViewMatrix(const TQ3Matrix4x4* m)
{
gCurrentModelView = *m;
UploadMatrix4x4(gState.loc_u_ModelView, &gCurrentModelView);
UploadMatrix3x3NormalFromMV(&gCurrentModelView);
}

void Render_SetLights(const QD3DLightDefType* lights, const TQ3Matrix4x4* viewMatrix)
{
// Ambient
glUniform4f(gState.loc_u_AmbientColor,
lights->ambientBrightness * lights->ambientColor.r,
lights->ambientBrightness * lights->ambientColor.g,
lights->ambientBrightness * lights->ambientColor.b,
1.0f);

int numLights = (int) lights->numFillLights;
if (numLights > 2)
numLights = 2;
glUniform1i(gState.loc_u_NumLights, numLights);

for (int i = 0; i < numLights; i++)
{
// Negate direction (toward-light convention)
TQ3Vector3D dir = {
-lights->fillDirection[i].x,
-lights->fillDirection[i].y,
-lights->fillDirection[i].z
};

// Transform direction to view space (transpose of upper-left 3x3 of viewMatrix)
TQ3Vector3D dirView;
dirView.x = viewMatrix->value[0][0]*dir.x + viewMatrix->value[1][0]*dir.y + viewMatrix->value[2][0]*dir.z;
dirView.y = viewMatrix->value[0][1]*dir.x + viewMatrix->value[1][1]*dir.y + viewMatrix->value[2][1]*dir.z;
dirView.z = viewMatrix->value[0][2]*dir.x + viewMatrix->value[1][2]*dir.y + viewMatrix->value[2][2]*dir.z;

// Normalize
float len = sqrtf(dirView.x*dirView.x + dirView.y*dirView.y + dirView.z*dirView.z);
if (len > 0.0001f)
{
dirView.x /= len;
dirView.y /= len;
dirView.z /= len;
}

GLint locDir   = (i == 0) ? gState.loc_u_LightDir0  : gState.loc_u_LightDir1;
GLint locColor = (i == 0) ? gState.loc_u_LightColor0 : gState.loc_u_LightColor1;

glUniform3f(locDir, dirView.x, dirView.y, dirView.z);
glUniform4f(locColor,
lights->fillColor[i].r * lights->fillBrightness[i],
lights->fillColor[i].g * lights->fillBrightness[i],
lights->fillColor[i].b * lights->fillBrightness[i],
1.0f);
}
}

void Render_InitState(const TQ3ColorRGBA* clearColor)
{
#if defined(_WIN32)
LoadWindowsGLFunctions();
#endif
// Compile & link shaders
CreateShaderProgram();
GLint sampleCount = 0;
glGetIntegerv(GL_SAMPLES, &sampleCount);
gCanCopySceneDepth = sampleCount == 0;
if (!gCanCopySceneDepth)
SDL_Log("Soft particles disabled for multisampled default framebuffer (%d samples)", sampleCount);

// Depth / blend / face culling
if (!s_vertexArray) { glGenVertexArrays(1, &s_vertexArray); }
glBindVertexArray(s_vertexArray);
glEnable(GL_DEPTH_TEST);    gState.hasState_GL_DEPTH_TEST = true;
glEnable(GL_CULL_FACE);     gState.hasState_GL_CULL_FACE  = true;
glDisable(GL_BLEND);        gState.hasState_GL_BLEND       = false;

glDepthMask(GL_TRUE);
gState.hasFlag_glDepthMask = true;

glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
gState.blendFuncIsAdditive = false;

glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
gState.wantColorMask = true;

glFrontFace(GL_CCW);

// Position attrib always enabled; others off
glEnableVertexAttribArray(0);   gState.attrib_Position = true;
glDisableVertexAttribArray(1);  gState.attrib_Normal   = false;
glDisableVertexAttribArray(2);  gState.attrib_TexCoord = false;
glDisableVertexAttribArray(3);  gState.attrib_Color    = false;

// Shader uniform initial state
gState.textureEnabled   = false;    glUniform1i(gState.loc_u_TextureEnabled,   0);
gState.lightingEnabled  = true;     glUniform1i(gState.loc_u_LightingEnabled,  1);
gState.fogEnabled       = false;    glUniform1i(gState.loc_u_FogEnabled,       0);
gState.alphaTestEnabled = false;    glUniform1i(gState.loc_u_AlphaTestEnabled, 0);
gState.useVertexColors  = false;    glUniform1i(gState.loc_u_UseVertexColors,  0);
gState.reflectionEnabled = false;   glUniform1i(gState.loc_u_ReflectionEnabled, 0);
glUniform1f(gState.loc_u_AutoFadeFactor, 1.0f);

glUniform4f(gState.loc_u_DiffuseColor, 1, 1, 1, 1);
glUniform4f(gState.loc_u_FogColor, 0, 0, 0, 1);

gState.sceneHasFog      = false;
gState.boundTexture     = 0;
gState.currentTransform = NULL;

glClearColor(clearColor->r, clearColor->g, clearColor->b, 1.0f);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

// Set up mesh queue & fullscreen overlay quad
gMeshQueueSize = 0;
SDL_memset(gMeshQueuePtrs, 0, sizeof(gMeshQueuePtrs));

if (!gFullscreenQuad)
{
gFullscreenQuad = MakeQuadMesh_UI(0, 0, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT, 0, 0, 1, 1);
}


CHECK_GL_ERROR();
}

void Render_InvalidateMeshCachePtr(const void* ptr)
{
	if (!ptr)
		return;

	for (int i = 0; i < MESH_CACHE_SIZE; i++)
	{
		MeshGpuCacheEntry* entry = &s_meshCache[i];

		if (entry->points == ptr
			|| entry->normals == ptr
			|| entry->colors == ptr
			|| entry->uvs == ptr
			|| entry->triangles == ptr)
		{
			if (entry->valid)
				gRenderStats.cacheInvalidations++;
			entry->valid = false;
		}
	}
}

void Render_InvalidateMeshCacheForMesh(const TQ3TriMeshData* mesh)
{
	if (!mesh)
		return;

	for (int i = 0; i < MESH_CACHE_SIZE; i++)
	{
		if (s_meshCache[i].mesh == mesh)
		{
			if (s_meshCache[i].valid)
				gRenderStats.cacheInvalidations++;
			s_meshCache[i].valid = false;
		}
	}
}

void Render_EndScene(void)
{
if (gFullscreenQuad)
{
Q3TriMeshData_Dispose(gFullscreenQuad);
gFullscreenQuad = NULL;
}
}

void Render_EnableFog(
float camHither,
float camYon,
float fogHither,
float fogYon,
TQ3ColorRGBA fogColor)
{
(void) camHither;

float fogRefYon = (camYon > 4200.0f) ? 4200.0f : camYon;
gState.fogStart    = fogHither * fogRefYon;
gState.fogEnd      = fogYon   * fogRefYon;
gState.fogColor[0] = fogColor.r;
gState.fogColor[1] = fogColor.g;
gState.fogColor[2] = fogColor.b;
gState.fogColor[3] = fogColor.a;

glUniform1f(gState.loc_u_FogStart, gState.fogStart);
glUniform1f(gState.loc_u_FogEnd,   gState.fogEnd);
glUniform4f(gState.loc_u_FogColor, fogColor.r, fogColor.g, fogColor.b, fogColor.a);

gState.sceneHasFog = true;
}

void Render_DisableFog(void)
{
gState.sceneHasFog = false;
}

#pragma mark -

void Render_BindTexture(GLuint textureName)
{
if (gState.boundTexture != textureName)
{
glBindTexture(GL_TEXTURE_2D, textureName);
gState.boundTexture = textureName;
}
}

#pragma mark - Texture Loading

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
static void* ConvertBGRA32ToRGBA(const void* src, int width, int height)
{
int n = width * height;
uint8_t* dst = (uint8_t*) AllocPtr(n * 4);
const uint8_t* s = (const uint8_t*) src;
for (int i = 0; i < n; i++)
{
dst[i*4+0] = s[i*4+2];     // R
dst[i*4+1] = s[i*4+1];     // G
dst[i*4+2] = s[i*4+0];     // B
dst[i*4+3] = s[i*4+3];     // A
}
return dst;
}

static void* ConvertBGR24ToRGB(const void* src, int width, int height)
{
int n = width * height;
uint8_t* dst = (uint8_t*) AllocPtr(n * 3);
const uint8_t* s = (const uint8_t*) src;
for (int i = 0; i < n; i++)
{
dst[i*3+0] = s[i*3+2];     // R
dst[i*3+1] = s[i*3+1];     // G
dst[i*3+2] = s[i*3+0];     // B
}
return dst;
}

// Convert 16-bit 1555 (GL_UNSIGNED_SHORT_1_5_5_5_REV + GL_BGRA) to RGBA8
static void* Convert1555ToRGBA8(const void* src, int width, int height, bool hasAlpha)
{
int n = width * height;
uint8_t* dst = (uint8_t*) AllocPtr(n * 4);
const uint16_t* s = (const uint16_t*) src;
for (int i = 0; i < n; i++)
{
uint16_t v = s[i];
uint8_t b = (v & 0x001F);
uint8_t g = (v >> 5)  & 0x1F;
uint8_t r = (v >> 10) & 0x1F;
uint8_t a = (v >> 15) & 1;
dst[i*4+0] = (r << 3) | (r >> 2);
dst[i*4+1] = (g << 3) | (g >> 2);
dst[i*4+2] = (b << 3) | (b >> 2);
dst[i*4+3] = hasAlpha ? (a ? 255 : 0) : 255;
}
return dst;
}
#endif

/********************* CLASSIFY PIXMAP OPACITY ************************/
//
// Inspect the 3DMF pixmap's alpha usage so we can choose the least-expensive
// correct texturing mode:
//   - fully opaque alpha    -> kQ3TexturingModeOpaque
//   - binary 0/255 alpha    -> kQ3TexturingModeAlphaTest
//   - intermediate alpha    -> kQ3TexturingModeAlphaBlend
//
static TQ3TexturingMode ClassifyPixmapOpacity(const TQ3TextureShader* textureShader)
{
	switch (textureShader->pixmap->pixelType)
	{
		case kQ3PixelTypeRGB32:
		case kQ3PixelTypeRGB16:
		case kQ3PixelTypeRGB24:
			return kQ3TexturingModeOpaque;

		case kQ3PixelTypeARGB16:
			return kQ3TexturingModeAlphaTest;

		case kQ3PixelTypeARGB32:
		{
			bool sawTransparentPixel = false;
			const uint8_t* pixels = (const uint8_t*) textureShader->pixmap->image;
			int width = textureShader->pixmap->width;
			int height = textureShader->pixmap->height;
			int rowBytes = textureShader->pixmap->rowBytes;
			if (rowBytes <= 0)
				rowBytes = width * 4;

			for (int y = 0; y < height; y++)
			{
				const uint8_t* row = pixels + y * rowBytes;
				for (int x = 0; x < width; x++)
				{
					uint8_t alpha = row[x * 4 + 3];
					if (alpha == 255)
						continue;

					sawTransparentPixel = true;
					if (alpha != 0)
						return kQ3TexturingModeAlphaBlend;
				}
			}

			return sawTransparentPixel ? kQ3TexturingModeAlphaTest : kQ3TexturingModeOpaque;
		}

		default:
			return kQ3TexturingModeOff;
	}
}

GLuint Render_LoadTexture(
GLenum internalFormat,
int width,
int height,
GLenum bufferFormat,
GLenum bufferType,
const GLvoid* pixels,
RendererTextureFlags flags)
{
GAME_ASSERT(gGLContext);

GLuint textureName;
glGenTextures(1, &textureName);
CHECK_GL_ERROR();

Render_BindTexture(textureName);
CHECK_GL_ERROR();

glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, !gGamePrefs.lowDetail ? GL_LINEAR : GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, !gGamePrefs.lowDetail ? GL_LINEAR : GL_NEAREST);

if (flags & kRendererTextureFlags_ClampU)
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
if (flags & kRendererTextureFlags_ClampV)
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
// Desktop pixel layouts are converted to formats accepted by GLES 3 and WebGL 2.
void* converted = NULL;
GLenum glFormat = GL_RGBA;
GLenum glType   = GL_UNSIGNED_BYTE;

if (bufferFormat == GL_BGRA && bufferType == GL_UNSIGNED_INT_8_8_8_8_REV)
{
converted = ConvertBGRA32ToRGBA(pixels, width, height);
glFormat  = GL_RGBA;
// RGB32 textures (internalFormat==GL_RGB) have no real alpha; the source
// byte is zero (unused Mac padding). Force alpha=255 so the WebGL canvas
// does not write transparent pixels and show the HTML background through
// opaque geometry. (Same principle as the 1555 fix in Render_UpdateTexture.)
if (internalFormat == GL_RGB)
{
	uint8_t* p = (uint8_t*) converted;
	for (int i = 0; i < width * height; i++)
		p[i*4+3] = 255;
}
}
else if (bufferFormat == GL_BGR && bufferType == GL_UNSIGNED_BYTE)
{
converted = ConvertBGR24ToRGB(pixels, width, height);
glFormat  = GL_RGB;
}
else if (bufferFormat == GL_BGRA && bufferType == GL_UNSIGNED_SHORT_1_5_5_5_REV)
{
bool hasAlpha = (internalFormat == GL_RGBA);
converted = Convert1555ToRGBA8(pixels, width, height, hasAlpha);
glFormat  = GL_RGBA;
}
else
{
glFormat = bufferFormat;
glType   = bufferType;
}

glTexImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0,
glFormat, glType, converted ? converted : pixels);

if (converted)
DisposePtr((Ptr) converted);
#else
glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
bufferFormat, bufferType, pixels);
#endif

CHECK_GL_ERROR();
return textureName;
}

void Render_UpdateTexture(
GLuint textureName,
int x,
int y,
int width,
int height,
GLenum bufferFormat,
GLenum bufferType,
const GLvoid* pixels,
int rowBytesInInput)
{
Render_BindTexture(textureName);

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
// Convert legacy pixel formats, same as Render_LoadTexture.
void* converted = NULL;
GLenum glFormat = bufferFormat;
GLenum glType   = bufferType;

// Strip row padding before conversion.
const GLvoid* srcPixels = pixels;
uint8_t* rowStripped = NULL;
if (rowBytesInInput > 0)
{
int bpp = 4;
if (bufferFormat == GL_RGB || bufferFormat == GL_BGR) bpp = 3;
else if (bufferFormat == GL_ALPHA) bpp = 1;
else if (bufferType == GL_UNSIGNED_SHORT_1_5_5_5_REV) bpp = 2;
int lineBytes = width * bpp;
int strideBytes = rowBytesInInput * bpp;  // rowBytesInInput is in pixels, convert to bytes
rowStripped = (uint8_t*) AllocPtr(height * lineBytes);
const uint8_t* src = (const uint8_t*) pixels;
for (int row = 0; row < height; row++)
SDL_memcpy(rowStripped + row * lineBytes, src + row * strideBytes, lineBytes);
srcPixels = rowStripped;
}

if (bufferFormat == GL_BGRA && bufferType == GL_UNSIGNED_INT_8_8_8_8_REV)
{
	converted = ConvertBGRA32ToRGBA(srcPixels, width, height);
	glFormat  = GL_RGBA;
	glType    = GL_UNSIGNED_BYTE;
}
else if (bufferFormat == GL_BGR && bufferType == GL_UNSIGNED_BYTE)
{
	converted = ConvertBGR24ToRGB(srcPixels, width, height);
	glFormat  = GL_RGB;
}
else if (bufferFormat == GL_BGRA && bufferType == GL_UNSIGNED_SHORT_1_5_5_5_REV)
{
	// Terrain supertile textures are GL_RGB (fully opaque). Force alpha=255 so that
	// the WebGL canvas (alpha:true) does not composite tile pixels as transparent,
	// which would show the page background through the terrain.
	converted = Convert1555ToRGBA8(srcPixels, width, height, false);
	glFormat  = GL_RGBA;
	glType    = GL_UNSIGNED_BYTE;
}

const GLvoid* uploadPixels = converted ? converted : srcPixels;
glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, glFormat, glType, uploadPixels);

if (converted)
DisposePtr((Ptr) converted);
if (rowStripped)
DisposePtr((Ptr) rowStripped);
#else
GLint pUnpackRowLength = 0;
if (rowBytesInInput > 0)
{
glGetIntegerv(GL_UNPACK_ROW_LENGTH, &pUnpackRowLength);
glPixelStorei(GL_UNPACK_ROW_LENGTH, rowBytesInInput);
}
glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, bufferFormat, bufferType, pixels);
if (rowBytesInInput > 0)
glPixelStorei(GL_UNPACK_ROW_LENGTH, pUnpackRowLength);
#endif

CHECK_GL_ERROR();
}

void Render_Load3DMFTextures(TQ3MetaFile* metaFile, GLuint* outTextureNames, bool forceClampUVs)
{
for (int i = 0; i < metaFile->numTextures; i++)
{
TQ3TextureShader* textureShader = &metaFile->textures[i];
GAME_ASSERT(textureShader->pixmap);
GAME_ASSERT(textureShader->pixmap->image);

TQ3TexturingMode meshTexturingMode = kQ3TexturingModeOff;
GLenum internalFormat;
GLenum format;
GLenum type;

	switch (textureShader->pixmap->pixelType)
	{
		case kQ3PixelTypeRGB32:
			meshTexturingMode = ClassifyPixmapOpacity(textureShader);
			internalFormat = GL_RGB;
			format = GL_BGRA;
			type   = GL_UNSIGNED_INT_8_8_8_8_REV;
			break;

		case kQ3PixelTypeARGB32:
			meshTexturingMode = ClassifyPixmapOpacity(textureShader);
			internalFormat = GL_RGBA;
			format = GL_BGRA;
			type   = GL_UNSIGNED_INT_8_8_8_8_REV;
			break;

		case kQ3PixelTypeRGB16:
			meshTexturingMode = ClassifyPixmapOpacity(textureShader);
			internalFormat = GL_RGB;
			format = GL_BGRA;
			type   = GL_UNSIGNED_SHORT_1_5_5_5_REV;
			break;

		case kQ3PixelTypeARGB16:
			meshTexturingMode = ClassifyPixmapOpacity(textureShader);
			internalFormat = GL_RGBA;
			format = GL_BGRA;
			type   = GL_UNSIGNED_SHORT_1_5_5_5_REV;
			break;

		case kQ3PixelTypeRGB24:
			meshTexturingMode = ClassifyPixmapOpacity(textureShader);
			internalFormat = GL_RGB;
			format = GL_BGR;
			type   = GL_UNSIGNED_BYTE;
			break;

		default:
			DoAlert("3DMF texture: Unsupported kQ3PixelType");
			continue;
	}

int clampFlags = forceClampUVs ? kRendererTextureFlags_ClampBoth : 0;
if (textureShader->boundaryU == kQ3ShaderUVBoundaryClamp)
clampFlags |= kRendererTextureFlags_ClampU;
if (textureShader->boundaryV == kQ3ShaderUVBoundaryClamp)
clampFlags |= kRendererTextureFlags_ClampV;

outTextureNames[i] = Render_LoadTexture(
internalFormat,
textureShader->pixmap->width,
textureShader->pixmap->height,
format,
type,
textureShader->pixmap->image,
clampFlags);

for (int j = 0; j < metaFile->numMeshes; j++)
{
if (metaFile->meshes[j]->internalTextureID == i)
{
metaFile->meshes[j]->glTextureName = outTextureNames[i];
metaFile->meshes[j]->texturingMode = meshTexturingMode;
}
}
}
}

#pragma mark -

void Render_StartFrame(void)
{
bool didMakeCurrent = SDL_GL_MakeCurrent(gSDLWindow, gGLContext);
GAME_ASSERT_MESSAGE(didMakeCurrent, SDL_GetError());

glUseProgram(gState.shaderProgram);

// Clear rendering statistics
SDL_memset(&gRenderStats, 0, sizeof(gRenderStats));

// Clear mesh queue
gMeshQueueSize = 0;

// The depth mask must be re-enabled so we can clear the depth buffer.
SetFlag(glDepthMask, true);

GLbitfield clearWhat = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
#if OSXPPC
if (gIsInGame && gLevelType == LEVEL_TYPE_LAWN && gCyclorama && gDebugMode != DEBUG_MODE_WIREFRAME)
clearWhat &= ~GL_COLOR_BUFFER_BIT;
#endif
glClear(clearWhat);

GAME_ASSERT(gState.currentTransform == NULL);
GAME_ASSERT(!gFrameStarted);
gFrameStarted = true;
}

bool BugdomScript_MakeRendererCurrent(void)
{
	return SDL_GL_MakeCurrent(gSDLWindow, gGLContext);
}

void Render_SetViewport(int x, int y, int w, int h)
{
glViewport(x, y, w, h);
}

static bool SnapshotOpaqueDepth(void)
{
	if (!gCanCopySceneDepth)
		return false;

	int width = 0;
	int height = 0;
	SDL_GetWindowSizeInPixels(gSDLWindow, &width, &height);
	if (width <= 0 || height <= 0)
		return false;

	if (width != gSceneDepthWidth || height != gSceneDepthHeight)
	{
		if (!gSceneDepthTexture)
			glGenTextures(1, &gSceneDepthTexture);
		if (!gSceneDepthFramebuffer)
			glGenFramebuffers(1, &gSceneDepthFramebuffer);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gSceneDepthTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
			GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, gSceneDepthFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
			gSceneDepthTexture, 0);
		const GLenum none = GL_NONE;
		glDrawBuffers(1, &none);
		glReadBuffer(GL_NONE);
		gCanCopySceneDepth = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glActiveTexture(GL_TEXTURE0);
		if (!gCanCopySceneDepth)
		{
			SDL_Log("Soft particles disabled: depth framebuffer incomplete");
			return false;
		}
		gSceneDepthWidth = width;
		gSceneDepthHeight = height;
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gSceneDepthFramebuffer);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
		GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (glGetError() != GL_NO_ERROR)
	{
		gCanCopySceneDepth = false;
		SDL_Log("Soft particles disabled: opaque depth copy failed");
		return false;
	}

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gSceneDepthTexture);
	glActiveTexture(GL_TEXTURE0);
	glUniform2f(gState.loc_u_InvFramebufferSize, 1.0f / width, 1.0f / height);
	return true;
}

void Render_FlushQueue(void)
{
	GAME_ASSERT(gFrameStarted);

	if (gMeshQueueSize == 0)
		return;

	//--------------------------------------------------------------
	// SORT DRAW QUEUE ENTRIES
	// Opaque meshes are sorted front-to-back,
	// followed by transparent meshes sorted back-to-front.
	SDL_qsort(
			gMeshQueuePtrs,
			gMeshQueueSize,
			sizeof(gMeshQueuePtrs[0]),
			DrawOrderComparator);

	//--------------------------------------------------------------
	// PASS 1: OPAQUE COLOR + DEPTH
	// Draw opaque meshes to color AND depth buffers.

	int numDeferredColorMeshes = 0;

	SetFlag(glDepthMask, true);
	glDepthFunc(GL_LEQUAL);
	DisableState(GL_BLEND);
	glUniform1i(gState.loc_u_SoftParticle, 0);

	for (int i = 0; i < gMeshQueueSize; i++)
	{
		MeshQueueEntry* entry = gMeshQueuePtrs[i];

		if (!entry->meshIsTransparent)
		{
			const MeshGpuCacheEntry* cacheEntry = GetMeshCacheEntry(entry->mesh);
			BeginShadingPass(entry, cacheEntry);
			PrepareOpaqueShading(entry, cacheEntry);
			SendGeometry(entry, cacheEntry);
		}
		else
		{
			// Defer transparent mesh to Pass 2
			GAME_ASSERT(numDeferredColorMeshes <= i);
			gMeshQueuePtrs[numDeferredColorMeshes++] = entry;
		}
	}

	//--------------------------------------------------------------
	// PASS 2: ALPHA-BLENDED COLOR
	// Draw transparent meshes to color buffer only.

	gRenderStats.meshesPass2 += numDeferredColorMeshes;

	if (numDeferredColorMeshes > 0)
	{
		bool hasSoftParticles = false;
		for (int i = 0; i < numDeferredColorMeshes; i++)
			hasSoftParticles |= (gMeshQueuePtrs[i]->mods->statusBits & STATUS_BIT_SOFT_PARTICLE) != 0;
		const bool depthReady = hasSoftParticles && SnapshotOpaqueDepth();

		EnableState(GL_BLEND);
		gState.alphaTestEnabled = false;
		glUniform1i(gState.loc_u_AlphaTestEnabled, 0);
		SetFlag(glDepthMask, false);
		glDepthFunc(GL_LEQUAL);     // depth info already written in pass 1

		for (int i = 0; i < numDeferredColorMeshes; i++)
		{
			const MeshQueueEntry* entry = gMeshQueuePtrs[i];
			glUniform1i(gState.loc_u_SoftParticle,
				depthReady && (entry->mods->statusBits & STATUS_BIT_SOFT_PARTICLE) != 0);
			const MeshGpuCacheEntry* cacheEntry = GetMeshCacheEntry(entry->mesh);
			BeginShadingPass(entry, cacheEntry);
			PrepareAlphaShading(entry, cacheEntry);
			SendGeometry(entry, cacheEntry);
		}
		glUniform1i(gState.loc_u_SoftParticle, 0);
	}

	//--------------------------------------------------------------
	// CLEAN UP

	// Restore camera-only modelview if a per-object transform was left active
	if (gState.currentTransform != NULL)
	{
		UploadMatrix4x4(gState.loc_u_ModelView, &gCurrentModelView);
		UploadMatrix3x3NormalFromMV(&gCurrentModelView);
		gState.currentTransform = NULL;
	}

	gMeshQueueSize = 0;
}

void Render_EndFrame(void)
{
GAME_ASSERT(gFrameStarted);
Render_FlushQueue();
gFrameStarted = false;
}

#pragma mark -

static inline float WorldPointToDepth(const TQ3Point3D p)
{
// Simplified depth estimation relative to the camera frustum.
#define M(x,y) gCameraWorldToFrustumMatrix.value[x][y]
return p.x*M(0,2) + p.y*M(1,2) + p.z*M(2,2);
#undef M
}

static float GetDepth(
int                 numMeshes,
TQ3TriMeshData**    meshList,
const TQ3Point3D*   centerCoord)
{
if (centerCoord)
return WorldPointToDepth(*centerCoord);

// Average centers of all bounding boxes
float mult = (float) numMeshes / 2.0f;
TQ3Point3D center = {0, 0, 0};
for (int i = 0; i < numMeshes; i++)
{
center.x += (meshList[i]->bBox.min.x + meshList[i]->bBox.max.x) * mult;
center.y += (meshList[i]->bBox.min.y + meshList[i]->bBox.max.y) * mult;
center.z += (meshList[i]->bBox.min.z + meshList[i]->bBox.max.z) * mult;
}
return WorldPointToDepth(center);
}

static bool IsMeshTransparent(const TQ3TriMeshData* mesh, const RenderModifiers* mods)
{
return RenderPolicy_IsMeshTransparent(
	mesh->texturingMode == kQ3TexturingModeAlphaBlend,
	mesh->diffuseColor.a,
	mods->diffuseColor.a,
	mods->autoFadeFactor,
	(mods->statusBits & STATUS_BIT_GLOW) != 0);
}

static MeshQueueEntry* NewMeshQueueEntry(void)
{
MeshQueueEntry* entry = &gMeshQueueEntryPool[gMeshQueueSize];
gMeshQueuePtrs[gMeshQueueSize] = entry;
gMeshQueueSize++;
return entry;
}

void Render_SubmitMeshList(
		int                     numMeshes,
		TQ3TriMeshData**        meshList,
		const TQ3Matrix4x4*     transform,
		const RenderModifiers*  mods,
		const TQ3Point3D*       centerCoord,
		uint16_t                slot)
{
	if (numMeshes <= 0)
		SDL_Log("not drawing this!\n");

	GAME_ASSERT(gFrameStarted);
	GAME_ASSERT(gMeshQueueSize + numMeshes <= MESHQUEUE_MAX_SIZE);

	float depth = GetDepth(numMeshes, meshList, centerCoord);

	for (int i = 0; i < numMeshes; i++)
	{
		MeshQueueEntry* entry       = NewMeshQueueEntry();
		entry->mesh                 = meshList[i];
		entry->transform            = transform;
		entry->mods                 = mods ? mods : &kDefaultRenderMods;
		entry->depth                = depth;
		entry->slot                 = slot;
		entry->meshIsTransparent    = IsMeshTransparent(entry->mesh, entry->mods);

		gRenderStats.meshesPass1++;
		gRenderStats.triangles += entry->mesh->numTriangles;

		GAME_ASSERT(!(entry->mods->statusBits & STATUS_BIT_HIDDEN));
	}
}

void Render_SubmitMesh(
		const TQ3TriMeshData*   mesh,
		const TQ3Matrix4x4*     transform,
		const RenderModifiers*  mods,
		const TQ3Point3D*       centerCoord,
		uint16_t                slot)
{
	GAME_ASSERT(gFrameStarted);
	GAME_ASSERT(gMeshQueueSize < MESHQUEUE_MAX_SIZE);

	MeshQueueEntry* entry       = NewMeshQueueEntry();
	entry->mesh                 = mesh;
	entry->transform            = transform;
	entry->mods                 = mods ? mods : &kDefaultRenderMods;
	entry->depth                = GetDepth(1, (TQ3TriMeshData **) &mesh, centerCoord);
	entry->slot                 = slot;
	entry->meshIsTransparent    = IsMeshTransparent(entry->mesh, entry->mods);

	gRenderStats.meshesPass1++;
	gRenderStats.triangles += entry->mesh->numTriangles;

	GAME_ASSERT(!(entry->mods->statusBits & STATUS_BIT_HIDDEN));
}

#pragma mark -

static int DrawOrderComparator(const void* a_void, const void* b_void)
{
	static const int AFirst   = -1;
	static const int BFirst   = +1;
	static const int DontCare =  0;

	const MeshQueueEntry* a = *(MeshQueueEntry**) a_void;
	const MeshQueueEntry* b = *(MeshQueueEntry**) b_void;

	// 1. Manual draw order (e.g. Terrain < World < UI)
	if (a->mods->drawOrder < b->mods->drawOrder)    return AFirst;
	if (a->mods->drawOrder > b->mods->drawOrder)    return BFirst;

	// 2. Opaque meshes before transparent meshes
	if (a->meshIsTransparent != b->meshIsTransparent)
		return b->meshIsTransparent ? AFirst : BFirst;

	// 3. Depth sort
	if (!a->meshIsTransparent)
	{
		// Opaque: Front-to-Back (optimization)
		if (a->depth < b->depth)    return AFirst;
		if (a->depth > b->depth)    return BFirst;
	}
	else
	{
		// Transparent: Back-to-Front (required for blending)
		if (a->depth < b->depth)    return BFirst;
		if (a->depth > b->depth)    return AFirst;
	}

	// 4. Slot tie-breaker (linked list order)
	// If depth is identical, use Slot to maintain a consistent order.
	if (a->slot < b->slot) return AFirst;
	if (a->slot > b->slot) return BFirst;

	return DontCare;
}

#pragma mark -

static void SendGeometry(const MeshQueueEntry* entry, const MeshGpuCacheEntry* cacheEntry)
{
uint32_t statusBits = entry->mods->statusBits;

// Cull backfaces unless explicitly kept
SetState(GL_CULL_FACE, !(statusBits & STATUS_BIT_KEEPBACKFACES));

// Two-pass for transparent backfaces: draw backfaces first, then frontfaces
if (statusBits & STATUS_BIT_KEEPBACKFACES_2PASS)
glCullFace(GL_FRONT);       // pass 1: draw backfaces

// Submit vertex positions
CachedVertexAttrib(gState.loc_a_Position, cacheEntry, VBO_ATTRIB_POS, 3);

// Upload combined modelview if the per-object transform changed
if (gState.currentTransform != entry->transform)
{
if (entry->transform)
{
TQ3Matrix4x4 combined;
Q3Matrix4x4_Multiply(entry->transform, &gCurrentModelView, &combined);
UploadMatrix4x4(gState.loc_u_ModelView, &combined);
UploadMatrix3x3NormalFromMV(&combined);
}
else
{
UploadMatrix4x4(gState.loc_u_ModelView, &gCurrentModelView);
UploadMatrix3x3NormalFromMV(&gCurrentModelView);
}
gState.currentTransform = entry->transform;
}

DrawCachedElements(cacheEntry);
CHECK_GL_ERROR();

// Pass 2: draw frontfaces (improves look of translucent spheres etc.)
if (statusBits & STATUS_BIT_KEEPBACKFACES_2PASS)
{
glCullFace(GL_BACK);        // pass 2: draw frontfaces
DrawCachedElements(cacheEntry);
CHECK_GL_ERROR();
}
}

static void BeginDepthPass(const MeshQueueEntry* entry)
{
const TQ3TriMeshData* mesh = entry->mesh;
const MeshGpuCacheEntry* cacheEntry = GetMeshCacheEntry(mesh);

GAME_ASSERT(!(entry->mods->statusBits & STATUS_BIT_NOZWRITE));

// Never write to color buffer in this pass
SetColorMask(GL_FALSE);
SetFlag(glDepthMask, true);
EnableState(GL_DEPTH_TEST);

// No lighting or fog needed for depth-only pass
SetUniformBool(gState.loc_u_LightingEnabled, &gState.lightingEnabled, false);
SetUniformBool(gState.loc_u_FogEnabled,      &gState.fogEnabled,      false);
SetUniformBool(gState.loc_u_UseVertexColors, &gState.useVertexColors, false);
SetUniformBool(gState.loc_u_ReflectionEnabled, &gState.reflectionEnabled, false);
glUniform1f(gState.loc_u_AutoFadeFactor, 1.0f);
glUniform4f(gState.loc_u_DiffuseColor, 1, 1, 1, 1);

DisableAttrib(Normal);
DisableAttrib(Color);

if (gDebugMode != DEBUG_MODE_NOTEXTURES &&
(mesh->texturingMode & kQ3TexturingModeExt_OpacityModeMask) != kQ3TexturingModeOff)
{
SetUniformBool(gState.loc_u_TextureEnabled,   &gState.textureEnabled,   true);
SetUniformBool(gState.loc_u_AlphaTestEnabled, &gState.alphaTestEnabled, true);
EnableAttrib(TexCoord);
Render_BindTexture(mesh->glTextureName);
CachedVertexAttrib(gState.loc_a_TexCoord, cacheEntry, VBO_ATTRIB_TC, 2);
CHECK_GL_ERROR();
}
else
{
SetUniformBool(gState.loc_u_TextureEnabled,   &gState.textureEnabled,   false);
SetUniformBool(gState.loc_u_AlphaTestEnabled, &gState.alphaTestEnabled, false);
DisableAttrib(TexCoord);
}
}

static void BeginShadingPass(const MeshQueueEntry* entry, const MeshGpuCacheEntry* cacheEntry)
{
const TQ3TriMeshData* mesh = entry->mesh;
uint32_t statusBits = entry->mods->statusBits;

// Always write to color buffer in this pass
SetColorMask(GL_TRUE);

bool reflection = (statusBits & STATUS_BIT_REFLECTIONMAP) != 0;
SetUniformBool(gState.loc_u_ReflectionEnabled, &gState.reflectionEnabled, reflection);
if (reflection)
{
GAME_ASSERT(entry->transform && mesh->hasVertexNormals);
UploadReflectionState(entry->transform);
}

// Lighting
bool wantLighting = !((statusBits & STATUS_BIT_NULLSHADER)
|| (mesh->texturingMode & kQ3TexturingModeExt_NullShaderFlag));
SetUniformBool(gState.loc_u_LightingEnabled, &gState.lightingEnabled, wantLighting);

// Fog
bool wantFog = gState.sceneHasFog && !(statusBits & STATUS_BIT_NOFOG);
SetUniformBool(gState.loc_u_FogEnabled, &gState.fogEnabled, wantFog);

// Texture mapping
if (gDebugMode != DEBUG_MODE_NOTEXTURES &&
(mesh->texturingMode & kQ3TexturingModeExt_OpacityModeMask) != kQ3TexturingModeOff)
{
SetUniformBool(gState.loc_u_TextureEnabled, &gState.textureEnabled, true);
Render_BindTexture(mesh->glTextureName);
if (reflection)
DisableAttrib(TexCoord);
else
{
EnableAttrib(TexCoord);
CachedVertexAttrib(gState.loc_a_TexCoord, cacheEntry, VBO_ATTRIB_TC, 2);
}
CHECK_GL_ERROR();
}
else
{
SetUniformBool(gState.loc_u_TextureEnabled, &gState.textureEnabled, false);
DisableAttrib(TexCoord);
}

// Normal data (only when lighting is active)
if (mesh->hasVertexNormals && (wantLighting || reflection))
{
EnableAttrib(Normal);
CachedVertexAttrib(gState.loc_a_Normal, cacheEntry, VBO_ATTRIB_NORM, 3);
}
else
{
DisableAttrib(Normal);
}
}

static void PrepareOpaqueShading(const MeshQueueEntry* entry, const MeshGpuCacheEntry* cacheEntry)
{
const TQ3TriMeshData* mesh = entry->mesh;
const uint32_t statusBits  = entry->mods->statusBits;
TQ3TexturingMode texMode   = (mesh->texturingMode & kQ3TexturingModeExt_OpacityModeMask);

SetFlag(glDepthMask, !(statusBits & STATUS_BIT_NOZWRITE));
glUniform1f(gState.loc_u_AutoFadeFactor, 1.0f);

bool wantAlphaTest = (texMode == kQ3TexturingModeAlphaTest);
SetUniformBool(gState.loc_u_AlphaTestEnabled, &gState.alphaTestEnabled, wantAlphaTest);

if (mesh->hasVertexColors)
{
SetUniformBool(gState.loc_u_UseVertexColors, &gState.useVertexColors, true);
EnableAttrib(Color);
CachedVertexAttrib(gState.loc_a_Color, cacheEntry, VBO_ATTRIB_COLOR, 4);
}
else
{
SetUniformBool(gState.loc_u_UseVertexColors, &gState.useVertexColors, false);
DisableAttrib(Color);
glUniform4f(gState.loc_u_DiffuseColor,
mesh->diffuseColor.r * entry->mods->diffuseColor.r,
mesh->diffuseColor.g * entry->mods->diffuseColor.g,
mesh->diffuseColor.b * entry->mods->diffuseColor.b,
1.0f);
}
}

static void PrepareAlphaShading(const MeshQueueEntry* entry, const MeshGpuCacheEntry* cacheEntry)
{
	const TQ3TriMeshData* mesh = entry->mesh;
	const uint32_t statusBits  = entry->mods->statusBits;

	// In multi-pass rendering, transparent objects should NOT write to depth
	// to avoid blocking other transparent objects behind them.
	SetFlag(glDepthMask, false);
	glUniform1f(gState.loc_u_AutoFadeFactor, entry->mods->autoFadeFactor);

	// Additive blending for glow meshes
bool wantAdditive = !!(statusBits & STATUS_BIT_GLOW);
if (gState.blendFuncIsAdditive != wantAdditive)
{
if (wantAdditive)
glBlendFunc(GL_SRC_ALPHA, GL_ONE);
else
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
gState.blendFuncIsAdditive = wantAdditive;
}

if (mesh->hasVertexColors)
{
SetUniformBool(gState.loc_u_UseVertexColors, &gState.useVertexColors, true);
EnableAttrib(Color);

CachedVertexAttrib(gState.loc_a_Color, cacheEntry, VBO_ATTRIB_COLOR, 4);
}
else
{
SetUniformBool(gState.loc_u_UseVertexColors, &gState.useVertexColors, false);
DisableAttrib(Color);
glUniform4f(gState.loc_u_DiffuseColor,
mesh->diffuseColor.r * entry->mods->diffuseColor.r,
mesh->diffuseColor.g * entry->mods->diffuseColor.g,
mesh->diffuseColor.b * entry->mods->diffuseColor.b,
mesh->diffuseColor.a * entry->mods->diffuseColor.a);
}
}

void Render_ResetColor(void)
{
glUniform1f(gState.loc_u_AutoFadeFactor, 1.0f);
SetUniformBool(gState.loc_u_ReflectionEnabled, &gState.reflectionEnabled, false);
DisableState(GL_BLEND);
SetUniformBool(gState.loc_u_AlphaTestEnabled, &gState.alphaTestEnabled, false);
SetUniformBool(gState.loc_u_LightingEnabled,  &gState.lightingEnabled,  false);
SetUniformBool(gState.loc_u_TextureEnabled,   &gState.textureEnabled,   false);
DisableAttrib(Normal);
DisableAttrib(Color);
glUniform4f(gState.loc_u_DiffuseColor, 1, 1, 1, 1);
}

#pragma mark -

//=======================================================================================================

TQ3Vector2D FitRectKeepAR(
int logicalWidth,
int logicalHeight,
float displayWidth,
float displayHeight)
{
float displayAR = (float) displayWidth  / (float) displayHeight;
float logicalAR = (float) logicalWidth  / (float) logicalHeight;

if (displayAR >= logicalAR)
return (TQ3Vector2D) { displayHeight * logicalAR, displayHeight };
else
return (TQ3Vector2D) { displayWidth, displayWidth / logicalAR };
}

/****************************/
/*    2D MODE               */
/****************************/

static void MakeOrthoMatrix(
TQ3Matrix4x4* m,
float left, float right,
float bottom, float top,
float nearZ, float farZ)
{
float rl = right - left;
float tb = top   - bottom;
float fn = farZ  - nearZ;
SDL_memset(m->value, 0, sizeof(m->value));
m->value[0][0] =  2.0f / rl;
m->value[1][1] =  2.0f / tb;
m->value[2][2] = -2.0f / fn;
m->value[3][0] = -(right + left)   / rl;
m->value[3][1] = -(top   + bottom) / tb;
m->value[3][2] = -(farZ  + nearZ)  / fn;
m->value[3][3] = 1.0f;
}

static void MakeIdentityMatrix(TQ3Matrix4x4* m)
{
SDL_memset(m->value, 0, sizeof(m->value));
m->value[0][0] = m->value[1][1] = m->value[2][2] = m->value[3][3] = 1.0f;
}

void Render_Enter2D_Full640x480(void)
{
if (gGamePrefs.force4x3AspectRatio)
{
TQ3Vector2D fitted = FitRectKeepAR(GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT, gWindowWidth, gWindowHeight);
glViewport(
(GLint) (0.5f * (gWindowWidth  - fitted.x)),
(GLint) (0.5f * (gWindowHeight - fitted.y)),
(GLint) ceilf(fitted.x),
(GLint) ceilf(fitted.y));
}
else
{
glViewport(0, 0, gWindowWidth, gWindowHeight);
}

GAME_ASSERT(gMatrixStackTop < MATRIX_STACK_DEPTH);
gProjectionStack[gMatrixStackTop] = gCurrentProjection;
gModelViewStack[gMatrixStackTop]  = gCurrentModelView;
gMatrixStackTop++;

TQ3Matrix4x4 proj, mv;
MakeOrthoMatrix(&proj, 0, 640, 480, 0, 0, 1000);
MakeIdentityMatrix(&mv);
Render_SetProjectionMatrix(&proj);
Render_SetModelViewMatrix(&mv);
}

void Render_Enter2D_NormalizedCoordinates(float aspect)
{
GAME_ASSERT(gMatrixStackTop < MATRIX_STACK_DEPTH);
gProjectionStack[gMatrixStackTop] = gCurrentProjection;
gModelViewStack[gMatrixStackTop]  = gCurrentModelView;
gMatrixStackTop++;

TQ3Matrix4x4 proj, mv;
MakeOrthoMatrix(&proj, -aspect, aspect, -1, 1, 0, 1000);
MakeIdentityMatrix(&mv);
Render_SetProjectionMatrix(&proj);
Render_SetModelViewMatrix(&mv);
}

void Render_Enter2D_NativeResolution(void)
{
GAME_ASSERT(gMatrixStackTop < MATRIX_STACK_DEPTH);
gProjectionStack[gMatrixStackTop] = gCurrentProjection;
gModelViewStack[gMatrixStackTop]  = gCurrentModelView;
gMatrixStackTop++;

TQ3Matrix4x4 proj, mv;
MakeOrthoMatrix(&proj, 0, gWindowWidth, gWindowHeight, 0, 0, 1000);
MakeIdentityMatrix(&mv);
Render_SetProjectionMatrix(&proj);
Render_SetModelViewMatrix(&mv);
}

void Render_Exit2D(void)
{
GAME_ASSERT(gMatrixStackTop > 0);
gMatrixStackTop--;
Render_SetProjectionMatrix(&gProjectionStack[gMatrixStackTop]);
Render_SetModelViewMatrix(&gModelViewStack[gMatrixStackTop]);
}

#pragma mark -

//=======================================================================================================

/*******************************************/
/*    BACKDROP/OVERLAY (COVER WINDOW)      */
/*******************************************/

void Render_DrawFadeOverlay(float opacity)
{
GAME_ASSERT(gFullscreenQuad);
gFullscreenQuad->texturingMode = kQ3TexturingModeOff;
gFullscreenQuad->diffuseColor  = (TQ3ColorRGBA) { 0, 0, 0, 1.0f - opacity };
Render_SubmitMesh(gFullscreenQuad, NULL, &kDefaultRenderMods_FadeOverlay, &kQ3Point3D_Zero, 0);
}

#pragma mark -

TQ3Area Render_GetAdjustedViewportRect(Rect paneClip, int logicalWidth, int logicalHeight)
{
float scaleX, scaleY, xoff = 0, yoff = 0;

if (!gGamePrefs.force4x3AspectRatio)
{
scaleX = gWindowWidth  / (float) logicalWidth;
scaleY = gWindowHeight / (float) logicalHeight;
}
else
{
TQ3Vector2D fitted = FitRectKeepAR(logicalWidth, logicalHeight, gWindowWidth, gWindowHeight);
xoff   = (gWindowWidth  - fitted.x) * 0.5f;
yoff   = (gWindowHeight - fitted.y) * 0.5f;
scaleX = fitted.x / (float) logicalWidth;
scaleY = fitted.y / (float) logicalHeight;
}

float left   = floorf(xoff + scaleX * paneClip.left);
float right  = ceilf (xoff + scaleX * (logicalWidth  - paneClip.right));
float top    = floorf(yoff + scaleY * paneClip.top);
float bottom = ceilf (yoff + scaleY * (logicalHeight - paneClip.bottom));

return (TQ3Area) {{left, top}, {right, bottom}};
}

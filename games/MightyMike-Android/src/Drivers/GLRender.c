// OPENGL RENDERING DRIVER
// (C) 2022 Iliyas Jorio
// This file is part of Mighty Mike. https://github.com/jorio/mightymike
//
// On PowerPC Macs, SDL 2.0.3's 2D renderer doesn't produce very fast results,
// especially if Altivec isn't available (G3 CPUs).
// So, on PPC, we bypass their renderer and use our own.
//
// Newer platforms may also benefit from this renderer
// as it is typically more performant than SDL's default renderer
// (e.g. up to 3x the framerate as SDL's metal renderer on M1 MBA, SDL 2.26)
//
// Additionally, people have reported issues with SDL's renderer with decent
// hardware on Windows (https://github.com/jorio/MightyMike/issues/13).
// The custom GL renderer has solved their issue.

#if GLRENDER

#include <SDL3/SDL.h>
#include "myglobals.h"
#include "externs.h"
#include "misc.h"
#include "renderdrivers.h"
#include "framebufferfilter.h"

#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
PFNGLGENBUFFERSARBPROC pglGenBuffersARB;
PFNGLDELETEBUFFERSARBPROC pglDeleteBuffersARB;
PFNGLBINDBUFFERARBPROC pglBindBufferARB;
PFNGLMAPBUFFERARBPROC pglMapBufferARB;
PFNGLBUFFERDATAARBPROC pglBufferDataARB;
PFNGLUNMAPBUFFERARBPROC pglUnmapBufferARB;

// Marginal FPS increase at the cost of 1 frame of latency
#define DEFERRED_TEX_UPDATE 0

// RGB 5-6-5 appears to be the fastest format for streaming textures
// on graphics cards that ship with ancient PPC hardware
#define kFramePixelType GL_UNSIGNED_SHORT_5_6_5

#define kFrameTextureWidth 1024
#define kFrameTextureHeight 512

#if (kFramePixelType == GL_UNSIGNED_SHORT_5_6_5)
	#define kFrameInternalFormat	GL_RGB
	#define kFramePixelFormat		GL_RGB
	#define kFramePixelType			GL_UNSIGNED_SHORT_5_6_5
	#define kFrameBytesPerPixel		2
#elif (kFramePixelType == GL_UNSIGNED_SHORT_5_5_5_1)
	#define kFrameInternalFormat	GL_RGBA
	#define kFramePixelFormat		GL_RGBA
	#define kFramePixelType			GL_UNSIGNED_SHORT_5_5_5_1
	#define kFrameBytesPerPixel		2
#else
	#define kFrameInternalFormat	GL_RGBA
	#define kFramePixelFormat		GL_RGBA
	#define kFramePixelType			GL_UNSIGNED_BYTE
	#define kFrameBytesPerPixel		4
#endif

static SDL_GLContext gGLContext = NULL;
static GLuint gFrameTexture = 0;
static GLuint gFramePBO = 0;
static GLuint gQuadVAO = 0;
static GLuint gQuadVBO = 0;
static GLuint gBlitProgram = 0;
static GLint gMaxTextureSize = 0;

const char* gRendererName = "NULL";
Boolean gCanDoHQStretch = true;
float gFramebufferConvertMs = 0;
float gFramebufferUpdateTextureMs = 0;
float gFramebufferRenderTextureMs = 0;
float gFramebufferPresentMs = 0;
int gFramebufferUploadBytes = 0;

static float GLRender_ElapsedMs(uint64_t start, uint64_t end)
{
	return (float)((double)(end - start) * 1000.0 / (double)SDL_GetPerformanceFrequency());
}

#if _DEBUG
#define CHECK_GL_ERROR()												\
	do {					 											\
		GLenum err = glGetError();										\
		if (err != GL_NO_ERROR)											\
			DoFatalGLError(err, __func__, __LINE__);					\
	} while(0)

static void DoFatalGLError(GLenum error, const char* file, int line)
{
	static char alertbuf[1024];
	SDL_snprintf(alertbuf, sizeof(alertbuf), "OpenGL error 0x%x\nin %s:%d", error, file, line);
	DoFatalAlert(alertbuf);
}
#else
#define CHECK_GL_ERROR() do {} while(0)
#endif

SDL_Point FitRectKeepAR(
		int logicalWidth,
		int logicalHeight,
		int displayWidth,
		int displayHeight)
{
	float displayAR = (float)displayWidth / (float)displayHeight;
	float logicalAR = (float)logicalWidth / (float)logicalHeight;

	if (displayAR >= logicalAR)
	{
		return (SDL_Point) { displayHeight * logicalAR, displayHeight };
	}
	else
	{
		return (SDL_Point) { displayWidth, displayWidth / logicalAR };
	}
}

static GLuint CompileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		char log[1024] = {0};
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Blit shader compile failed: %s", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static bool InitBlitter(void)
{
	static const char* vertexSource =
		"#version 330 core\n"
		"layout(location=0) in vec2 position;\n"
		"layout(location=1) in vec2 texcoord;\n"
		"uniform vec2 uvScale;\n"
		"out vec2 uv;\n"
		"void main() { uv = texcoord * uvScale; gl_Position = vec4(position, 0.0, 1.0); }\n";
	static const char* fragmentSource =
		"#version 330 core\n"
		"in vec2 uv;\n"
		"uniform sampler2D frameTexture;\n"
		"out vec4 color;\n"
		"void main() { color = texture(frameTexture, uv); }\n";
	GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
	GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
	if (!vertexShader || !fragmentShader)
	{
		if (vertexShader) glDeleteShader(vertexShader);
		if (fragmentShader) glDeleteShader(fragmentShader);
		return false;
	}
	gBlitProgram = glCreateProgram();
	glAttachShader(gBlitProgram, vertexShader);
	glAttachShader(gBlitProgram, fragmentShader);
	glLinkProgram(gBlitProgram);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	GLint linked = GL_FALSE;
	glGetProgramiv(gBlitProgram, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		char log[1024] = {0};
		glGetProgramInfoLog(gBlitProgram, sizeof(log), NULL, log);
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Blit shader link failed: %s", log);
		glDeleteProgram(gBlitProgram);
		gBlitProgram = 0;
		return false;
	}
	const GLfloat vertices[] = {
		-1, -1, 0, 1,  1, -1, 1, 1,  1, 1, 1, 0,
		-1, -1, 0, 1,  1, 1, 1, 0,  -1, 1, 0, 0,
	};
	glGenVertexArrays(1, &gQuadVAO);
	glGenBuffers(1, &gQuadVBO);
	glBindVertexArray(gQuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, gQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*) 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*) (2 * sizeof(GLfloat)));
	glBindVertexArray(0);
	glUseProgram(gBlitProgram);
	glUniform1i(glGetUniformLocation(gBlitProgram, "frameTexture"), 0);
	glUseProgram(0);
	return true;
}

#define GL_GET_PROC_ADDRESS(t, proc) \
do { \
    (proc) = (t) SDL_GL_GetProcAddress(#proc); \
    GAME_ASSERT_MESSAGE((proc), "Missing OpenGL procedure " #proc); \
} while(0)

static void InitTextureAndPBO(int pixelZoom)
{
	glGenTextures(1, &gFrameTexture);
	CHECK_GL_ERROR();

	pglGenBuffersARB(1, &gFramePBO);
	CHECK_GL_ERROR();

#if 0
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, gFramePBO);
	// perhaps we don't need to do this everytime?
	glBufferDataARB(
		GL_PIXEL_UNPACK_BUFFER_ARB,
		kFrameTextureWidth * kFrameTextureHeight * kFrameBytesPerPixel * (pixelZoom*pixelZoom),
		NULL,
		GL_STREAM_DRAW);
	CHECK_GL_ERROR();
#endif

	glBindTexture(GL_TEXTURE_2D, gFrameTexture);
	CHECK_GL_ERROR();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(
			GL_TEXTURE_2D,
			0,
			kFrameInternalFormat,
			kFrameTextureWidth * pixelZoom,
			kFrameTextureHeight * pixelZoom,
			0,
			kFramePixelFormat,
			kFramePixelType,
			NULL // need initial call with NULL so glTexSubImage2D works later on
	);
	CHECK_GL_ERROR();
}

static void DeleteTextureAndPBO(void)
{
	if (gFrameTexture != 0)
	{
		glDeleteTextures(1, &gFrameTexture);
		gFrameTexture = 0;
	}

	if (gFramePBO != 0)
	{
		pglDeleteBuffersARB(1, &gFramePBO);
		gFramePBO = 0;
	}
}

void GLRender_Init(void)
{
	SDL_Log("Using special PPC renderer!");

#if FRAMEBUFFER_COLOR_DEPTH == 32
	gRendererName = "fastgl32";
#elif FRAMEBUFFER_COLOR_DEPTH == 16
	gRendererName = "fastgl16";
#else
	gRendererName = "gl??";
#endif

	gGLContext = SDL_GL_CreateContext(gSDLWindow);
	GAME_ASSERT(gGLContext);

	bool didMakeCurrent = SDL_GL_MakeCurrent(gSDLWindow, gGLContext);
	GAME_ASSERT_MESSAGE(didMakeCurrent, SDL_GetError());
	SDL_Log("OpenGL context: %s; GLSL: %s", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gMaxTextureSize);
	SDL_Log("Max texture size: %d", (int) gMaxTextureSize);

	if (gMaxTextureSize < kFrameTextureWidth)
	{
		char message[128];
		SDL_snprintf(message, sizeof(message), "Your graphics card's max texture size (%d)\nis below the game's requirements (%d).", (int) gMaxTextureSize, kFrameTextureWidth);
		DoAlert(message);
	}

	pglGenBuffersARB = (PFNGLGENBUFFERSARBPROC) SDL_GL_GetProcAddress("glGenBuffersARB");
	pglDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC) SDL_GL_GetProcAddress("glDeleteBuffersARB");
	pglBindBufferARB = (PFNGLBINDBUFFERARBPROC) SDL_GL_GetProcAddress("glBindBufferARB");
	pglUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC) SDL_GL_GetProcAddress("glUnmapBufferARB");
	pglMapBufferARB = (PFNGLMAPBUFFERARBPROC) SDL_GL_GetProcAddress("glMapBufferARB");
	pglBufferDataARB = (PFNGLBUFFERDATAARBPROC) SDL_GL_GetProcAddress("glBufferDataARB");
	GAME_ASSERT_MESSAGE(pglGenBuffersARB && pglDeleteBuffersARB && pglBindBufferARB
		&& pglUnmapBufferARB && pglMapBufferARB && pglBufferDataARB,
		"Missing OpenGL buffer procedures.");

#if OSXPPC
	gCanDoHQStretch = false;
#else
	gCanDoHQStretch = gMaxTextureSize >= 2*kFrameTextureWidth;
#endif


#if !(NOVSYNC)
	SDL_GL_SetSwapInterval(1);
#else
	SDL_GL_SetSwapInterval(0);
#endif

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDepthMask(false);

	//glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	CHECK_GL_ERROR();

	InitTextureAndPBO(1);
	GAME_ASSERT_MESSAGE(InitBlitter(), "Couldn't initialize OpenGL blit shaders.");
}

void GLRender_Shutdown(void)
{
	ShutdownRenderThreads();

	DeleteTextureAndPBO();
	if (gQuadVBO) glDeleteBuffers(1, &gQuadVBO);
	if (gQuadVAO) glDeleteVertexArrays(1, &gQuadVAO);
	if (gBlitProgram) glDeleteProgram(gBlitProgram);

	if (gGLContext)
	{
        SDL_GL_DestroyContext(gGLContext);
		gGLContext = NULL;
	}
}

static SDL_Rect GetViewportSize(void)
{
	const int vw = VISIBLE_WIDTH;
	const int vh = VISIBLE_HEIGHT;

	int dw = 0;
	int dh = 0;
//	SDL_GL_GetDrawableSize(gSDLWindow, &dw, &dh);	// DON'T use SDL_GetWindowSize as it returns fake scaled pixels in HiDPI displays
	SDL_GetWindowSizeInPixels(gSDLWindow, &dw, &dh);

	SDL_Point size;
	if (gEffectiveScalingType == kScaling_PixelPerfect)
	{
		int zoom = GetMaxIntegerZoom(dw, dh);
		size = (SDL_Point) { zoom*vw, zoom*vh };
	}
	else
	{
		size = FitRectKeepAR(vw, vh, dw, dh);
	}

	return (SDL_Rect)
	{
		.x = (dw-size.x) / 2,
		.y = (dh-size.y) / 2,
		.w = size.x,
		.h = size.y
	};
}

void GLRender_PresentFramebuffer(void)
{
	static SDL_Rect previousViewportRect = {0};
	static int previousEffectiveScalingType = kScaling_Unspecified;
	static int needClear = 60;

	const int vw = VISIBLE_WIDTH;
	const int vh = VISIBLE_HEIGHT;

	bool didMakeCurrent = SDL_GL_MakeCurrent(gSDLWindow, gGLContext);
	GAME_ASSERT_MESSAGE(didMakeCurrent, SDL_GetError());

	//-------------------------------------------------------------------------
	// Update dimensions

	SDL_Rect viewportRect = GetViewportSize();
    if (!SDL_RectsEqual(&viewportRect, &previousViewportRect))
	{
		previousViewportRect = viewportRect;
		glViewport(viewportRect.x, viewportRect.y, viewportRect.w, viewportRect.h);
		needClear = 60;
	}

	bool isHQ = gEffectiveScalingType == kScaling_HQStretch;
	bool wasHQ = previousEffectiveScalingType == kScaling_HQStretch;
	if (wasHQ ^ isHQ)
	{
		DeleteTextureAndPBO();
		InitTextureAndPBO(isHQ? 2: 1);
	}
	previousEffectiveScalingType = gEffectiveScalingType;

	int zvw = (isHQ ? 2 : 1) * vw;
	int zvh = (isHQ ? 2 : 1) * vh;

	//-------------------------------------------------------------------------
	// Update PBO

	pglBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, gFramePBO);
	CHECK_GL_ERROR();

	// get new PBO
	int numBytes = zvw * zvh * kFrameBytesPerPixel;
	pglBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, numBytes, NULL, GL_STREAM_DRAW);
	CHECK_GL_ERROR();

	void* mappedBuffer = pglMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);
	CHECK_GL_ERROR();
	GAME_ASSERT(mappedBuffer);

	// now write data into the buffer, possibly in another thread
	uint64_t startTicks = SDL_GetPerformanceCounter();
	ConvertFramebufferMT(mappedBuffer);
	uint64_t endTicks = SDL_GetPerformanceCounter();
	gFramebufferConvertMs = GLRender_ElapsedMs(startTicks, endTicks);

	pglUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
	CHECK_GL_ERROR();

	//-------------------------------------------------------------------------
	// Draw the quad

	// On a Mini G4, NOT clearing the screen increases the framerate by 8%
	// so don't do it unless the viewport rectangle has recently changed.
	if (needClear > 0)
	{
		needClear--;
		glClearColor(0,0,0,1);
#if _DEBUG
		if (needClear > 4)
			glClearColor(0,0,1,1);
#endif
		glClear(GL_COLOR_BUFFER_BIT);
	}

	glBindTexture(GL_TEXTURE_2D, gFrameTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					gEffectiveScalingType == kScaling_PixelPerfect ? GL_NEAREST : GL_LINEAR);

#if !DEFERRED_TEX_UPDATE
	// Update the texture
	startTicks = SDL_GetPerformanceCounter();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, zvw, zvh, kFramePixelFormat, kFramePixelType, NULL);
	endTicks = SDL_GetPerformanceCounter();
	gFramebufferUpdateTextureMs = GLRender_ElapsedMs(startTicks, endTicks);
	gFramebufferUploadBytes = numBytes;
	CHECK_GL_ERROR();
#endif

	const float umax = vw * (1.0f / kFrameTextureWidth);
	const float vmax = vh * (1.0f / kFrameTextureHeight);

	startTicks = SDL_GetPerformanceCounter();
	glUseProgram(gBlitProgram);
	glUniform2f(glGetUniformLocation(gBlitProgram, "uvScale"), umax, vmax);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(gQuadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	glUseProgram(0);
	endTicks = SDL_GetPerformanceCounter();
	gFramebufferRenderTextureMs = GLRender_ElapsedMs(startTicks, endTicks);
	CHECK_GL_ERROR();

	startTicks = SDL_GetPerformanceCounter();
	SDL_GL_SwapWindow(gSDLWindow);
	endTicks = SDL_GetPerformanceCounter();
	gFramebufferPresentMs = GLRender_ElapsedMs(startTicks, endTicks);

#if DEFERRED_TEX_UPDATE
	//-------------------------------------------------------------------------
	// Update texture

	startTicks = SDL_GetPerformanceCounter();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, zvw, zvh, kFramePixelFormat, kFramePixelType, NULL);
	endTicks = SDL_GetPerformanceCounter();
	gFramebufferUpdateTextureMs = GLRender_ElapsedMs(startTicks, endTicks);
	gFramebufferUploadBytes = numBytes;
	CHECK_GL_ERROR();
#endif
}

#endif // GLRENDER

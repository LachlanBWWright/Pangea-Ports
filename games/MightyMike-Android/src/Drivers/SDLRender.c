#if !(GLRENDER)

#include <SDL3/SDL.h>
#include "myglobals.h"
#include "externs.h"
#include "misc.h"
#include "renderdrivers.h"
#include "framebufferfilter.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#if _DEBUG
#define CHECK_SDL_ERROR(success)										\
	do {					 											\
		if (!success)													\
			DoFatalSDLError(success, __func__, __LINE__);				\
	} while(0)

static void DoFatalSDLError(int error, const char* file, int line)
{
	static char alertbuf[1024];
	SDL_snprintf(alertbuf, sizeof(alertbuf), "SDL error %d\nin %s:%d\n%s", error, file, line, SDL_GetError());
	DoFatalAlert(alertbuf);
}
#else
#define CHECK_SDL_ERROR(success) (void) (success)
#endif

static SDL_Renderer*	gSDLRenderer		= NULL;
static SDL_Texture*		gSDLTexture			= NULL;
static color_t*			gFinalFramebuffer	= NULL;
const char*				gRendererName		= "NULL";
Boolean					gCanDoHQStretch		= true;
float					gFramebufferConvertMs = 0;
float					gFramebufferUpdateTextureMs = 0;
float					gFramebufferRenderTextureMs = 0;
float					gFramebufferPresentMs = 0;
int						gFramebufferUploadBytes = 0;

static float SDLRender_ElapsedMs(uint64_t start, uint64_t end)
{
	return (float)((double)(end - start) * 1000.0 / (double)SDL_GetPerformanceFrequency());
}

Boolean SDLRender_Init(void)
{
	gSDLRenderer = SDL_CreateRenderer(gSDLWindow, NULL);
	if (!gSDLRenderer)
		return false;
	// The texture bound to the renderer is created in-game after loading the prefs.

#if !(NOVSYNC)
	SDL_SetRenderVSync(gSDLRenderer, 1);
#endif
	
	const char* sdlRendererName = SDL_GetRendererName(gSDLRenderer);
	if (sdlRendererName)
	{
		static char rendererName[32];
		SDL_snprintf(rendererName, sizeof(rendererName), "sdl-%s-%d", sdlRendererName, (int) sizeof(color_t) * 8);
		gRendererName = rendererName;
	}
	
	SDL_SetRenderLogicalPresentation(gSDLRenderer, VISIBLE_WIDTH, VISIBLE_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	return true;
}

static void SDLRender_NukeTextureAndBuffers(void)
{
	if (gFinalFramebuffer)
	{
		DisposePtr((Ptr) gFinalFramebuffer);
		gFinalFramebuffer = NULL;
	}

	if (gSDLTexture)
	{
		SDL_DestroyTexture(gSDLTexture);
		gSDLTexture = NULL;
	}
}

void SDLRender_Shutdown(void)
{
	ShutdownRenderThreads();

	SDLRender_NukeTextureAndBuffers();

	if (gSDLRenderer)
	{
		SDL_DestroyRenderer(gSDLRenderer);
		gSDLRenderer = NULL;
	}
}

void SDLRender_InitTexture(void)
{
	// Nuke old texture and RGBA buffers
	SDLRender_NukeTextureAndBuffers();

	bool crisp = (gEffectiveScalingType == kScaling_PixelPerfect);
	int textureSizeMultiplier = (gEffectiveScalingType == kScaling_HQStretch) ? 2 : 1;

	// Allocate buffer
	gFinalFramebuffer = (color_t*) NewPtrClear((VISIBLE_WIDTH * 2) * (VISIBLE_HEIGHT * 2) * (int) sizeof(color_t));
	GAME_ASSERT(gFinalFramebuffer);

	// Recreate texture
	gSDLTexture = SDL_CreateTexture(
			gSDLRenderer,
#if FRAMEBUFFER_COLOR_DEPTH == 16
			SDL_PIXELFORMAT_RGB565,
#else
			SDL_PIXELFORMAT_RGBA8888,
#endif
			SDL_TEXTUREACCESS_STREAMING,
			VISIBLE_WIDTH * textureSizeMultiplier,
			VISIBLE_HEIGHT * textureSizeMultiplier);
	GAME_ASSERT(gSDLTexture);

	// Set scaling quality
	SDL_SetTextureScaleMode(gSDLTexture, crisp ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);

	// Set logical size
	SDL_SetRenderLogicalPresentation(gSDLRenderer, VISIBLE_WIDTH, VISIBLE_HEIGHT, crisp ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

void SDLRender_PresentFramebuffer(void)
{
	bool success = true;

	//-------------------------------------------------------------------------
	// Convert indexed to RGBA, with optional post-processing

	uint64_t startTicks = SDL_GetPerformanceCounter();
	ConvertFramebufferMT(gFinalFramebuffer);
	uint64_t endTicks = SDL_GetPerformanceCounter();
	gFramebufferConvertMs = SDLRender_ElapsedMs(startTicks, endTicks);

	//-------------------------------------------------------------------------
	// Update SDL texture

	int pitch = VISIBLE_WIDTH * (int) sizeof(color_t);

	if (gEffectiveScalingType == kScaling_HQStretch)
		pitch *= 2;

	startTicks = SDL_GetPerformanceCounter();
	success = SDL_UpdateTexture(gSDLTexture, NULL, gFinalFramebuffer, pitch);
	endTicks = SDL_GetPerformanceCounter();
	gFramebufferUpdateTextureMs = SDLRender_ElapsedMs(startTicks, endTicks);
	gFramebufferUploadBytes = pitch * VISIBLE_HEIGHT * ((gEffectiveScalingType == kScaling_HQStretch) ? 2 : 1);
	CHECK_SDL_ERROR(success);

	//-------------------------------------------------------------------------
	// Present it

	SDL_RenderClear(gSDLRenderer);
	startTicks = SDL_GetPerformanceCounter();
	success = SDL_RenderTexture(gSDLRenderer, gSDLTexture, NULL, NULL);
	endTicks = SDL_GetPerformanceCounter();
	gFramebufferRenderTextureMs = SDLRender_ElapsedMs(startTicks, endTicks);
	CHECK_SDL_ERROR(success);
	startTicks = SDL_GetPerformanceCounter();
	SDL_RenderPresent(gSDLRenderer);
	endTicks = SDL_GetPerformanceCounter();
	gFramebufferPresentMs = SDLRender_ElapsedMs(startTicks, endTicks);

#ifdef __EMSCRIPTEN__
	// Yield control to the browser so it can update the canvas and
	// process input events.  emscripten_sleep(0) suspends the C stack
	// via Asyncify without a real delay.
	emscripten_sleep(0);
#endif
}

#endif

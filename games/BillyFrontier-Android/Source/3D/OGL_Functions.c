#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "game.h"

#ifndef __EMSCRIPTEN__
// On Emscripten/WebGL, glActiveTexture and glClientActiveTexture are available
// as core or LEGACY_GL_EMULATION functions -- no proc-address lookup needed.
// See ogl_functions.h for the Emscripten macro definitions.

PFNGLACTIVETEXTUREARBPROC			procptr_glActiveTextureARB			= NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC		procptr_glClientActiveTextureARB	= NULL;

void OGL_InitFunctions(void)
{
	procptr_glActiveTextureARB			= (PFNGLACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glActiveTextureARB");
	procptr_glClientActiveTextureARB	= (PFNGLCLIENTACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glClientActiveTextureARB");

	GAME_ASSERT(procptr_glActiveTextureARB);
	GAME_ASSERT(procptr_glClientActiveTextureARB);
}

#endif /* !__EMSCRIPTEN__ */

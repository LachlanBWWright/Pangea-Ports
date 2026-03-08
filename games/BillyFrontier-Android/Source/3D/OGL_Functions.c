#include <SDL3/SDL.h>
#ifndef __EMSCRIPTEN__
#include <SDL3/SDL_opengl.h>
#endif

#include "game.h"

#ifndef __EMSCRIPTEN__
// On Emscripten/WebGL, glActiveTexture and glClientActiveTexture are available
// as core GLES2 functions.  See ogl_functions.h for the Emscripten definitions.

PFNGLACTIVETEXTUREARBPROC			procptr_glActiveTextureARB			= NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC		procptr_glClientActiveTextureARB	= NULL;

void OGL_InitFunctions(void)
{
	procptr_glActiveTextureARB			= (PFNGLACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glActiveTextureARB");
	procptr_glClientActiveTextureARB	= (PFNGLCLIENTACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glClientActiveTextureARB");

	GAME_ASSERT(procptr_glActiveTextureARB);
	GAME_ASSERT(procptr_glClientActiveTextureARB);
}

#else /* __EMSCRIPTEN__ */

// On Emscripten, glActiveTexture is a core GLES2 function.
// state_compat.c references procptr_glActiveTextureARB directly (extern),
// so we must provide a definition here, pointing to the real function.
PFNGLACTIVETEXTUREARBPROC procptr_glActiveTextureARB =
    (PFNGLACTIVETEXTUREARBPROC) glActiveTexture;

#endif /* !__EMSCRIPTEN__ */

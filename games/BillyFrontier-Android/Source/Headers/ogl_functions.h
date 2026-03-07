#pragma once

#include <SDL3/SDL_opengl.h>

#ifdef __EMSCRIPTEN__
// In WebGL/OpenGL ES, glActiveTexture and glClientActiveTexture are core or
// emulated functions -- no ARB proc-address lookup needed at runtime.
#define glActiveTextureARB					glActiveTexture
#define glClientActiveTextureARB			glClientActiveTexture
static inline void OGL_InitFunctions(void) {}
#else
extern PFNGLACTIVETEXTUREARBPROC			procptr_glActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC		procptr_glClientActiveTextureARB;

#define glActiveTextureARB					procptr_glActiveTextureARB
#define glClientActiveTextureARB			procptr_glClientActiveTextureARB

void OGL_InitFunctions(void);
#endif

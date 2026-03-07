// EmscriptenGLStubs.c
// Provides no-op stubs for OpenGL 1.x fixed-function functions that are
// declared in SDL_opengl.h but NOT implemented by Emscripten's LEGACY_GL_EMULATION.
// These stubs are compiled only for Emscripten/WebAssembly builds.

#ifdef __EMSCRIPTEN__

#include <SDL3/SDL_opengl.h>

// glColorMaterial: sets how vertex colors interact with material properties.
// In LEGACY_GL_EMULATION the default (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE)
// is used automatically, so this function is a no-op.
void glColorMaterial(GLenum face, GLenum mode)
{
	(void)face;
	(void)mode;
}

#endif /* __EMSCRIPTEN__ */

#ifdef _WIN32

#include <SDL3/SDL.h>
#include "modern_gl_api.h"

#define DEFINE_DESKTOP_GL(name, suffix) PFNGL##suffix##PROC bf_gl##name;
BILLY_DESKTOP_GL_FUNCTIONS(DEFINE_DESKTOP_GL)
#undef DEFINE_DESKTOP_GL

bool Billy_LoadDesktopGL(void)
{
    bool loaded = true;
#define LOAD_DESKTOP_GL(name, suffix) \
    bf_gl##name = (PFNGL##suffix##PROC) SDL_GL_GetProcAddress("gl" #name); \
    if (!bf_gl##name) { SDL_Log("OpenGL 3.3 function missing: gl" #name); loaded = false; }
    BILLY_DESKTOP_GL_FUNCTIONS(LOAD_DESKTOP_GL)
#undef LOAD_DESKTOP_GL
    return loaded;
}

#endif

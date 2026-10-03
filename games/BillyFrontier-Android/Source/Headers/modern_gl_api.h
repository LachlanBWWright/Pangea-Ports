#pragma once

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#ifdef _WIN32
#include "desktop_gl_loader.h"
#endif
#endif

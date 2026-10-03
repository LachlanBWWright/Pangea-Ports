#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <stdbool.h>

#define DESKTOP_GL_PROCS(X) \
    X(ActiveTexture, ACTIVETEXTURE) \
    X(AttachShader, ATTACHSHADER) \
    X(BindAttribLocation, BINDATTRIBLOCATION) \
    X(BindBuffer, BINDBUFFER) \
    X(BindFramebuffer, BINDFRAMEBUFFER) \
    X(BindVertexArray, BINDVERTEXARRAY) \
    X(BlitFramebuffer, BLITFRAMEBUFFER) \
    X(BufferData, BUFFERDATA) \
    X(CheckFramebufferStatus, CHECKFRAMEBUFFERSTATUS) \
    X(CompileShader, COMPILESHADER) \
    X(CreateProgram, CREATEPROGRAM) \
    X(CreateShader, CREATESHADER) \
    X(DeleteBuffers, DELETEBUFFERS) \
    X(DeleteFramebuffers, DELETEFRAMEBUFFERS) \
    X(DeleteProgram, DELETEPROGRAM) \
    X(DeleteShader, DELETESHADER) \
    X(DeleteVertexArrays, DELETEVERTEXARRAYS) \
    X(DisableVertexAttribArray, DISABLEVERTEXATTRIBARRAY) \
    X(DrawBuffers, DRAWBUFFERS) \
    X(EnableVertexAttribArray, ENABLEVERTEXATTRIBARRAY) \
    X(FramebufferTexture2D, FRAMEBUFFERTEXTURE2D) \
    X(GenBuffers, GENBUFFERS) \
    X(GenFramebuffers, GENFRAMEBUFFERS) \
    X(GenVertexArrays, GENVERTEXARRAYS) \
    X(GetProgramInfoLog, GETPROGRAMINFOLOG) \
    X(GetProgramiv, GETPROGRAMIV) \
    X(GetShaderInfoLog, GETSHADERINFOLOG) \
    X(GetShaderiv, GETSHADERIV) \
    X(GetUniformLocation, GETUNIFORMLOCATION) \
    X(LinkProgram, LINKPROGRAM) \
    X(ShaderSource, SHADERSOURCE) \
    X(Uniform1f, UNIFORM1F) \
    X(Uniform1i, UNIFORM1I) \
    X(Uniform2f, UNIFORM2F) \
    X(Uniform4fv, UNIFORM4FV) \
    X(UniformMatrix3fv, UNIFORMMATRIX3FV) \
    X(UniformMatrix4fv, UNIFORMMATRIX4FV) \
    X(UseProgram, USEPROGRAM) \
    X(VertexAttribPointer, VERTEXATTRIBPOINTER)

#define DECLARE_DESKTOP_GL_PROC(name, suffix) static PFNGL##suffix##PROC desktop_gl##name;
DESKTOP_GL_PROCS(DECLARE_DESKTOP_GL_PROC)
#undef DECLARE_DESKTOP_GL_PROC

static bool LoadDesktopGL(void)
{
#define LOAD_DESKTOP_GL_PROC(name, suffix) \
    desktop_gl##name = (PFNGL##suffix##PROC) SDL_GL_GetProcAddress("gl" #name); \
    if (!desktop_gl##name) { SDL_Log("Missing OpenGL 3.3 function: gl" #name); return false; }
    DESKTOP_GL_PROCS(LOAD_DESKTOP_GL_PROC)
#undef LOAD_DESKTOP_GL_PROC
    return true;
}

#define glActiveTexture desktop_glActiveTexture
#define glAttachShader desktop_glAttachShader
#define glBindAttribLocation desktop_glBindAttribLocation
#define glBindBuffer desktop_glBindBuffer
#define glBindFramebuffer desktop_glBindFramebuffer
#define glBindVertexArray desktop_glBindVertexArray
#define glBlitFramebuffer desktop_glBlitFramebuffer
#define glBufferData desktop_glBufferData
#define glCheckFramebufferStatus desktop_glCheckFramebufferStatus
#define glCompileShader desktop_glCompileShader
#define glCreateProgram desktop_glCreateProgram
#define glCreateShader desktop_glCreateShader
#define glDeleteBuffers desktop_glDeleteBuffers
#define glDeleteFramebuffers desktop_glDeleteFramebuffers
#define glDeleteProgram desktop_glDeleteProgram
#define glDeleteShader desktop_glDeleteShader
#define glDeleteVertexArrays desktop_glDeleteVertexArrays
#define glDisableVertexAttribArray desktop_glDisableVertexAttribArray
#define glDrawBuffers desktop_glDrawBuffers
#define glEnableVertexAttribArray desktop_glEnableVertexAttribArray
#define glFramebufferTexture2D desktop_glFramebufferTexture2D
#define glGenBuffers desktop_glGenBuffers
#define glGenFramebuffers desktop_glGenFramebuffers
#define glGenVertexArrays desktop_glGenVertexArrays
#define glGetProgramInfoLog desktop_glGetProgramInfoLog
#define glGetProgramiv desktop_glGetProgramiv
#define glGetShaderInfoLog desktop_glGetShaderInfoLog
#define glGetShaderiv desktop_glGetShaderiv
#define glGetUniformLocation desktop_glGetUniformLocation
#define glLinkProgram desktop_glLinkProgram
#define glShaderSource desktop_glShaderSource
#define glUniform1f desktop_glUniform1f
#define glUniform1i desktop_glUniform1i
#define glUniform2f desktop_glUniform2f
#define glUniform4fv desktop_glUniform4fv
#define glUniformMatrix3fv desktop_glUniformMatrix3fv
#define glUniformMatrix4fv desktop_glUniformMatrix4fv
#define glUseProgram desktop_glUseProgram
#define glVertexAttribPointer desktop_glVertexAttribPointer

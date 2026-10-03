#pragma once

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)

#include <stdbool.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

typedef struct ModernGLFunctions
{
	PFNGLACTIVETEXTUREPROC ActiveTexture;
	PFNGLATTACHSHADERPROC AttachShader;
	PFNGLBINDATTRIBLOCATIONPROC BindAttribLocation;
	PFNGLBINDBUFFERPROC BindBuffer;
	PFNGLBINDFRAMEBUFFERPROC BindFramebuffer;
	PFNGLBINDRENDERBUFFERPROC BindRenderbuffer;
	PFNGLBINDVERTEXARRAYPROC BindVertexArray;
	PFNGLBLITFRAMEBUFFERPROC BlitFramebuffer;
	PFNGLBUFFERDATAPROC BufferData;
	PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus;
	PFNGLCOMPILESHADERPROC CompileShader;
	PFNGLCREATEPROGRAMPROC CreateProgram;
	PFNGLCREATESHADERPROC CreateShader;
	PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers;
	PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers;
	PFNGLDELETESHADERPROC DeleteShader;
	PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;
	PFNGLDRAWBUFFERSPROC DrawBuffers;
	PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
	PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer;
	PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D;
	PFNGLGENBUFFERSPROC GenBuffers;
	PFNGLGENFRAMEBUFFERSPROC GenFramebuffers;
	PFNGLGENRENDERBUFFERSPROC GenRenderbuffers;
	PFNGLGENVERTEXARRAYSPROC GenVertexArrays;
	PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
	PFNGLGETPROGRAMIVPROC GetProgramiv;
	PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
	PFNGLGETSHADERIVPROC GetShaderiv;
	PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
	PFNGLLINKPROGRAMPROC LinkProgram;
	PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage;
	PFNGLSHADERSOURCEPROC ShaderSource;
	PFNGLUNIFORM1FPROC Uniform1f;
	PFNGLUNIFORM1IPROC Uniform1i;
	PFNGLUNIFORM2FPROC Uniform2f;
	PFNGLUNIFORM4FVPROC Uniform4fv;
	PFNGLUNIFORMMATRIX3FVPROC UniformMatrix3fv;
	PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;
	PFNGLUSEPROGRAMPROC UseProgram;
	PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
} ModernGLFunctions;

#ifdef __cplusplus
extern "C" {
#endif
extern ModernGLFunctions gModernGL;
bool ModernGL_LoadFunctions(void);
#ifdef __cplusplus
}
#endif

#define glActiveTexture gModernGL.ActiveTexture
#define glAttachShader gModernGL.AttachShader
#define glBindAttribLocation gModernGL.BindAttribLocation
#define glBindBuffer gModernGL.BindBuffer
#define glBindFramebuffer gModernGL.BindFramebuffer
#define glBindRenderbuffer gModernGL.BindRenderbuffer
#define glBindVertexArray gModernGL.BindVertexArray
#define glBlitFramebuffer gModernGL.BlitFramebuffer
#define glBufferData gModernGL.BufferData
#define glCheckFramebufferStatus gModernGL.CheckFramebufferStatus
#define glCompileShader gModernGL.CompileShader
#define glCreateProgram gModernGL.CreateProgram
#define glCreateShader gModernGL.CreateShader
#define glDeleteFramebuffers gModernGL.DeleteFramebuffers
#define glDeleteRenderbuffers gModernGL.DeleteRenderbuffers
#define glDeleteShader gModernGL.DeleteShader
#define glDisableVertexAttribArray gModernGL.DisableVertexAttribArray
#define glDrawBuffers gModernGL.DrawBuffers
#define glEnableVertexAttribArray gModernGL.EnableVertexAttribArray
#define glFramebufferRenderbuffer gModernGL.FramebufferRenderbuffer
#define glFramebufferTexture2D gModernGL.FramebufferTexture2D
#define glGenBuffers gModernGL.GenBuffers
#define glGenFramebuffers gModernGL.GenFramebuffers
#define glGenRenderbuffers gModernGL.GenRenderbuffers
#define glGenVertexArrays gModernGL.GenVertexArrays
#define glGetProgramInfoLog gModernGL.GetProgramInfoLog
#define glGetProgramiv gModernGL.GetProgramiv
#define glGetShaderInfoLog gModernGL.GetShaderInfoLog
#define glGetShaderiv gModernGL.GetShaderiv
#define glGetUniformLocation gModernGL.GetUniformLocation
#define glLinkProgram gModernGL.LinkProgram
#define glRenderbufferStorage gModernGL.RenderbufferStorage
#define glShaderSource gModernGL.ShaderSource
#define glUniform1f gModernGL.Uniform1f
#define glUniform1i gModernGL.Uniform1i
#define glUniform2f gModernGL.Uniform2f
#define glUniform4fv gModernGL.Uniform4fv
#define glUniformMatrix3fv gModernGL.UniformMatrix3fv
#define glUniformMatrix4fv gModernGL.UniformMatrix4fv
#define glUseProgram gModernGL.UseProgram
#define glVertexAttribPointer gModernGL.VertexAttribPointer

#endif

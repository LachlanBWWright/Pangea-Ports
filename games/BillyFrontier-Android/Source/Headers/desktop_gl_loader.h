#pragma once

#ifdef _WIN32

#include <stdbool.h>

#define BILLY_DESKTOP_GL_FUNCTIONS(X) \
    X(ActiveTexture, ACTIVETEXTURE) \
    X(AttachShader, ATTACHSHADER) \
    X(BindAttribLocation, BINDATTRIBLOCATION) \
    X(BindBuffer, BINDBUFFER) \
    X(BindFramebuffer, BINDFRAMEBUFFER) \
    X(BindRenderbuffer, BINDRENDERBUFFER) \
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
    X(DeleteRenderbuffers, DELETERENDERBUFFERS) \
    X(DeleteShader, DELETESHADER) \
    X(DeleteVertexArrays, DELETEVERTEXARRAYS) \
    X(DisableVertexAttribArray, DISABLEVERTEXATTRIBARRAY) \
    X(DrawBuffers, DRAWBUFFERS) \
    X(EnableVertexAttribArray, ENABLEVERTEXATTRIBARRAY) \
    X(FramebufferRenderbuffer, FRAMEBUFFERRENDERBUFFER) \
    X(FramebufferTexture2D, FRAMEBUFFERTEXTURE2D) \
    X(GenBuffers, GENBUFFERS) \
    X(GenFramebuffers, GENFRAMEBUFFERS) \
    X(GenRenderbuffers, GENRENDERBUFFERS) \
    X(GenVertexArrays, GENVERTEXARRAYS) \
    X(GetProgramInfoLog, GETPROGRAMINFOLOG) \
    X(GetProgramiv, GETPROGRAMIV) \
    X(GetShaderInfoLog, GETSHADERINFOLOG) \
    X(GetShaderiv, GETSHADERIV) \
    X(GetUniformLocation, GETUNIFORMLOCATION) \
    X(LinkProgram, LINKPROGRAM) \
    X(RenderbufferStorage, RENDERBUFFERSTORAGE) \
    X(ShaderSource, SHADERSOURCE) \
    X(Uniform1f, UNIFORM1F) \
    X(Uniform1i, UNIFORM1I) \
    X(Uniform2f, UNIFORM2F) \
    X(Uniform3fv, UNIFORM3FV) \
    X(Uniform4fv, UNIFORM4FV) \
    X(UniformMatrix3fv, UNIFORMMATRIX3FV) \
    X(UniformMatrix4fv, UNIFORMMATRIX4FV) \
    X(UseProgram, USEPROGRAM) \
    X(ValidateProgram, VALIDATEPROGRAM) \
    X(VertexAttrib2f, VERTEXATTRIB2F) \
    X(VertexAttrib3f, VERTEXATTRIB3F) \
    X(VertexAttrib4f, VERTEXATTRIB4F) \
    X(VertexAttribPointer, VERTEXATTRIBPOINTER)

#ifdef __cplusplus
extern "C" {
#endif

#define DECLARE_DESKTOP_GL(name, suffix) extern PFNGL##suffix##PROC bf_gl##name;
BILLY_DESKTOP_GL_FUNCTIONS(DECLARE_DESKTOP_GL)
#undef DECLARE_DESKTOP_GL

bool Billy_LoadDesktopGL(void);

#ifdef __cplusplus
}
#endif

#define glActiveTexture bf_glActiveTexture
#define glAttachShader bf_glAttachShader
#define glBindAttribLocation bf_glBindAttribLocation
#define glBindBuffer bf_glBindBuffer
#define glBindFramebuffer bf_glBindFramebuffer
#define glBindRenderbuffer bf_glBindRenderbuffer
#define glBindVertexArray bf_glBindVertexArray
#define glBlitFramebuffer bf_glBlitFramebuffer
#define glBufferData bf_glBufferData
#define glCheckFramebufferStatus bf_glCheckFramebufferStatus
#define glCompileShader bf_glCompileShader
#define glCreateProgram bf_glCreateProgram
#define glCreateShader bf_glCreateShader
#define glDeleteBuffers bf_glDeleteBuffers
#define glDeleteFramebuffers bf_glDeleteFramebuffers
#define glDeleteProgram bf_glDeleteProgram
#define glDeleteRenderbuffers bf_glDeleteRenderbuffers
#define glDeleteShader bf_glDeleteShader
#define glDeleteVertexArrays bf_glDeleteVertexArrays
#define glDisableVertexAttribArray bf_glDisableVertexAttribArray
#define glDrawBuffers bf_glDrawBuffers
#define glEnableVertexAttribArray bf_glEnableVertexAttribArray
#define glFramebufferRenderbuffer bf_glFramebufferRenderbuffer
#define glFramebufferTexture2D bf_glFramebufferTexture2D
#define glGenBuffers bf_glGenBuffers
#define glGenFramebuffers bf_glGenFramebuffers
#define glGenRenderbuffers bf_glGenRenderbuffers
#define glGenVertexArrays bf_glGenVertexArrays
#define glGetProgramInfoLog bf_glGetProgramInfoLog
#define glGetProgramiv bf_glGetProgramiv
#define glGetShaderInfoLog bf_glGetShaderInfoLog
#define glGetShaderiv bf_glGetShaderiv
#define glGetUniformLocation bf_glGetUniformLocation
#define glLinkProgram bf_glLinkProgram
#define glRenderbufferStorage bf_glRenderbufferStorage
#define glShaderSource bf_glShaderSource
#define glUniform1f bf_glUniform1f
#define glUniform1i bf_glUniform1i
#define glUniform2f bf_glUniform2f
#define glUniform3fv bf_glUniform3fv
#define glUniform4fv bf_glUniform4fv
#define glUniformMatrix3fv bf_glUniformMatrix3fv
#define glUniformMatrix4fv bf_glUniformMatrix4fv
#define glUseProgram bf_glUseProgram
#define glValidateProgram bf_glValidateProgram
#define glVertexAttrib2f bf_glVertexAttrib2f
#define glVertexAttrib3f bf_glVertexAttrib3f
#define glVertexAttrib4f bf_glVertexAttrib4f
#define glVertexAttribPointer bf_glVertexAttribPointer

#endif

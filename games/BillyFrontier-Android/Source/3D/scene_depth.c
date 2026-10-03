#include "game.h"
#include "scene_depth.h"

#undef glDrawArrays
#undef glActiveTexture
#undef glEnable
#undef glDisable
#undef glIsEnabled

#ifdef _WIN32
#define SceneDepth_ActiveTexture bf_glActiveTexture
#else
#define SceneDepth_ActiveTexture glActiveTexture
#endif

static GLuint sceneFBO;
static GLuint sceneColor;
static GLuint sceneDepth;
static GLuint depthFBO;
static GLuint depthTexture;
static GLuint presentProgram;
static GLuint presentVAO;
static int sceneWidth;
static int sceneHeight;
static Boolean ready;

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#define PRESENT_GLSL_VERSION "#version 300 es\n"
#define PRESENT_GLSL_PRECISION "precision highp float;\n"
#else
#define PRESENT_GLSL_VERSION "#version 330 core\n"
#define PRESENT_GLSL_PRECISION ""
#endif

static const char* presentVertexSource =
    PRESENT_GLSL_VERSION
    PRESENT_GLSL_PRECISION
    "out vec2 uv;\n"
    "void main() {\n"
    "  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "  uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

static const char* presentFragmentSource =
    PRESENT_GLSL_VERSION
    PRESENT_GLSL_PRECISION
    "uniform sampler2D scene;\n"
    "in vec2 uv; out vec4 color;\n"
    "void main() { color = texture(scene, uv); }\n";

static GLuint CompilePresentShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled) return shader;
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    SDL_Log("Scene presentation shader failed: %s", log);
    glDeleteShader(shader);
    return 0;
}

static Boolean CreatePresentProgram(void)
{
    GLuint vertex = CompilePresentShader(GL_VERTEX_SHADER, presentVertexSource);
    if (!vertex) return false;
    GLuint fragment = CompilePresentShader(GL_FRAGMENT_SHADER, presentFragmentSource);
    if (!fragment)
    {
        glDeleteShader(vertex);
        return false;
    }
    presentProgram = glCreateProgram();
    glAttachShader(presentProgram, vertex);
    glAttachShader(presentProgram, fragment);
    glLinkProgram(presentProgram);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(presentProgram, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[1024];
        glGetProgramInfoLog(presentProgram, sizeof(log), NULL, log);
        SDL_Log("Scene presentation program failed: %s", log);
        glDeleteProgram(presentProgram);
        presentProgram = 0;
        return false;
    }
    glGenVertexArrays(1, &presentVAO);
    return true;
}

void SceneDepth_Shutdown(void)
{
    if (sceneFBO) glDeleteFramebuffers(1, &sceneFBO);
    if (depthFBO) glDeleteFramebuffers(1, &depthFBO);
    if (sceneColor) glDeleteTextures(1, &sceneColor);
    if (depthTexture) glDeleteTextures(1, &depthTexture);
    if (sceneDepth) glDeleteRenderbuffers(1, &sceneDepth);
    if (presentVAO) glDeleteVertexArrays(1, &presentVAO);
    if (presentProgram) glDeleteProgram(presentProgram);
    sceneFBO = depthFBO = sceneColor = depthTexture = sceneDepth = 0;
    presentVAO = presentProgram = 0;
    sceneWidth = sceneHeight = 0;
    ready = false;
}

void SceneDepth_BeginFrame(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    if (ready && width == sceneWidth && height == sceneHeight)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
        return;
    }

    GLint oldTexture = 0;
    GLint oldRenderbuffer = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &oldRenderbuffer);
    SceneDepth_Shutdown();
    sceneWidth = width;
    sceneHeight = height;
    if (!CreatePresentProgram()) return;

    glGenTextures(1, &sceneColor);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &sceneDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glGenFramebuffers(1, &sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepth);
    GLenum sceneStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
        GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &depthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none);
    glReadBuffer(GL_NONE);
    GLenum depthStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, oldTexture);
    glBindRenderbuffer(GL_RENDERBUFFER, oldRenderbuffer);
    ready = sceneStatus == GL_FRAMEBUFFER_COMPLETE && depthStatus == GL_FRAMEBUFFER_COMPLETE;
    if (!ready)
    {
        SDL_Log("Scene depth unavailable: scene 0x%x, depth 0x%x", sceneStatus, depthStatus);
        SceneDepth_Shutdown();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
}

void SceneDepth_Capture(void)
{
    if (!ready) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthFBO);
    glBlitFramebuffer(0, 0, sceneWidth, sceneHeight, 0, 0, sceneWidth, sceneHeight,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    SceneDepth_ActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    SceneDepth_ActiveTexture(activeTexture);
}

void SceneDepth_Present(void)
{
    if (!ready) return;
    GLint oldProgram, oldVAO, activeTexture, oldTexture;
    GLboolean colorMask[4];
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVAO);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendingEnabled = glIsEnabled(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    SceneDepth_ActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    glUseProgram(presentProgram);
    glUniform1i(glGetUniformLocation(presentProgram, "scene"), 2);
    glBindVertexArray(presentVAO);
    glViewport(0, 0, sceneWidth, sceneHeight);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(oldVAO);
    glUseProgram(oldProgram);
    glBindTexture(GL_TEXTURE_2D, oldTexture);
    SceneDepth_ActiveTexture(activeTexture);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (depthTest) glEnable(GL_DEPTH_TEST);
    if (blendingEnabled) glEnable(GL_BLEND);
}

GLuint SceneDepth_Texture(void) { return ready ? depthTexture : 0; }
int SceneDepth_Width(void) { return sceneWidth; }
int SceneDepth_Height(void) { return sceneHeight; }

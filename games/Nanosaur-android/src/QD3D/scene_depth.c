#include <SDL3/SDL.h>
#include "gl_compat.h"
#include "scene_depth.h"

static GLuint s_sceneFramebuffer;
static GLuint s_sceneColor;
static GLuint s_sceneDepth;
static GLuint s_copyFramebuffer;
static GLuint s_depthCopy;
static int s_width;
static int s_height;
static bool s_active;
static bool s_failed;
static bool s_captured;

void SceneDepth_Shutdown(void)
{
	COMPAT_GL_SetSoftParticleDepth(0, 0, 0);
	if (s_depthCopy) glDeleteTextures(1, &s_depthCopy);
	if (s_copyFramebuffer) glDeleteFramebuffers(1, &s_copyFramebuffer);
	if (s_sceneDepth) glDeleteRenderbuffers(1, &s_sceneDepth);
	if (s_sceneColor) glDeleteTextures(1, &s_sceneColor);
	if (s_sceneFramebuffer) glDeleteFramebuffers(1, &s_sceneFramebuffer);
	s_depthCopy = s_copyFramebuffer = s_sceneDepth = s_sceneColor = s_sceneFramebuffer = 0;
	s_width = s_height = 0;
	s_active = false;
	s_captured = false;
	s_failed = false;
}

static bool CreateFramebuffers(int width, int height)
{
	GLint activeTexture;
	GLint boundTexture;
	GLint boundRenderbuffer;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &boundRenderbuffer);
	glGenFramebuffers(1, &s_sceneFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, s_sceneFramebuffer);

	glGenTextures(1, &s_sceneColor);
	glBindTexture(GL_TEXTURE_2D, s_sceneColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_sceneColor, 0);

	glGenRenderbuffers(1, &s_sceneDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, s_sceneDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_sceneDepth);
	GLenum sceneStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	glGenFramebuffers(1, &s_copyFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, s_copyFramebuffer);
	glGenTextures(1, &s_depthCopy);
	glBindTexture(GL_TEXTURE_2D, s_depthCopy);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_depthCopy, 0);
	const GLenum none = GL_NONE;
	glDrawBuffers(1, &none);
	glReadBuffer(GL_NONE);
	GLenum copyStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindTexture(GL_TEXTURE_2D, boundTexture);
	glActiveTexture(activeTexture);
	glBindRenderbuffer(GL_RENDERBUFFER, boundRenderbuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (sceneStatus != GL_FRAMEBUFFER_COMPLETE || copyStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		SDL_Log("Scene depth framebuffer incomplete: scene=0x%x copy=0x%x", sceneStatus, copyStatus);
		SceneDepth_Shutdown();
		return false;
	}

	s_width = width;
	s_height = height;
	return true;
}

bool SceneDepth_BeginFrame(int width, int height, bool enabled)
{
	s_active = false;
	s_captured = false;
	COMPAT_GL_SetSoftParticleDepth(0, 0, 0);
	if (!enabled || width <= 0 || height <= 0 || s_failed)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}
	if (width != s_width || height != s_height)
	{
		SceneDepth_Shutdown();
		if (!CreateFramebuffers(width, height))
		{
			s_failed = true;
			return false;
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, s_sceneFramebuffer);
	s_active = true;
	return true;
}

void SceneDepth_CaptureOpaque(void)
{
	if (!s_active || s_captured) return;
	GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
	if (scissor) glDisable(GL_SCISSOR_TEST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, s_sceneFramebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_copyFramebuffer);
	glBlitFramebuffer(0, 0, s_width, s_height, 0, 0, s_width, s_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, s_sceneFramebuffer);
	if (scissor) glEnable(GL_SCISSOR_TEST);
	COMPAT_GL_SetSoftParticleDepth(s_depthCopy, s_width, s_height);
	s_captured = true;
}

void SceneDepth_EndFrame(void)
{
	if (!s_active) return;
	COMPAT_GL_SetSoftParticleDepth(0, 0, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, s_sceneFramebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, s_width, s_height, 0, 0, s_width, s_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	s_active = false;
}

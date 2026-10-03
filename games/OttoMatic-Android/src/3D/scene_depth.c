#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)

#include "game.h"
#include <GLES3/gl3.h>
#include <stdlib.h>

#undef glActiveTexture

static GLuint sDepthTexture;
static GLuint sDepthFramebuffer;
static int sWidth;
static int sHeight;
static Boolean sDepthReady;
static Boolean sDepthSupported = true;

static void DisposeDepthTarget(void)
{
	if (sDepthFramebuffer)
		glDeleteFramebuffers(1, &sDepthFramebuffer);
	if (sDepthTexture)
		glDeleteTextures(1, &sDepthTexture);
	sDepthFramebuffer = 0;
	sDepthTexture = 0;
	sWidth = 0;
	sHeight = 0;
}

void ModernGL_BeginDepthPass(void)
{
	sDepthReady = false;
}

void ModernGL_DisposeDepthTarget(void)
{
	DisposeDepthTarget();
	sDepthSupported = true;
}

static Boolean CreateDepthTarget(int width, int height)
{
	GLint depthBits = 0;
	glGetIntegerv(GL_DEPTH_BITS, &depthBits);
	if (depthBits != 16 && depthBits != 24)
		return false;

	GLint previousTexture = 0;
	GLint previousFramebuffer = 0;
	GLint previousActiveTexture = 0;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
	glActiveTexture(GL_TEXTURE2);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);

	glGenTextures(1, &sDepthTexture);
	glBindTexture(GL_TEXTURE_2D, sDepthTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0,
		depthBits == 24 ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16,
		width, height, 0, GL_DEPTH_COMPONENT,
		depthBits == 24 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, NULL);
	glGenFramebuffers(1, &sDepthFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, sDepthFramebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sDepthTexture, 0);
	GLenum noColor = GL_NONE;
	glDrawBuffers(1, &noColor);
	glReadBuffer(GL_NONE);
	Boolean complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
	glBindTexture(GL_TEXTURE_2D, previousTexture);
	glActiveTexture(previousActiveTexture);
	if (!complete)
	{
		SDL_Log("Scene depth framebuffer incomplete; soft particles disabled");
		DisposeDepthTarget();
		return false;
	}
	sWidth = width;
	sHeight = height;
	return true;
}

static Boolean CaptureSceneDepth(void)
{
	if (sDepthReady)
		return true;
	if (!sDepthSupported)
		return false;
	const char* setting = getenv("PANGEA_SOFT_PARTICLES");
	if (setting && setting[0] == '0')
		return false;

	GLint samples = 0;
	glGetIntegerv(GL_SAMPLES, &samples);
	if (samples > 0)
		return false;

	int width = 0;
	int height = 0;
	SDL_GetWindowSizeInPixels(gSDLWindow, &width, &height);
	if (width <= 0 || height <= 0)
		return false;
	if (width != sWidth || height != sHeight)
	{
		DisposeDepthTarget();
		if (!CreateDepthTarget(width, height))
		{
			sDepthSupported = false;
			return false;
		}
	}

	GLint readFramebuffer = 0;
	GLint drawFramebuffer = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sDepthFramebuffer);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
		GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	GLenum error = glGetError();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
	if (error != GL_NO_ERROR)
	{
		SDL_Log("Scene depth copy failed (0x%x); soft particles disabled", error);
		sDepthSupported = false;
		return false;
	}
	sDepthReady = true;
	return true;
}

Boolean ModernGL_BeginSoftParticle(float nearPlane, float farPlane, float distance)
{
	if (nearPlane <= 0 || farPlane <= nearPlane || distance <= 0 || !CaptureSceneDepth())
		return false;
	ModernGL_UseShader();
	GLint previousActiveTexture = 0;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, sDepthTexture);
	glActiveTexture(previousActiveTexture);
	glUniform1i(gModernGLShader.uSceneDepth, 2);
	glUniform1i(gModernGLShader.uSoftParticle, 1);
	glUniform2f(gModernGLShader.uDepthSize, (float)sWidth, (float)sHeight);
	glUniform2f(gModernGLShader.uDepthRange, nearPlane, farPlane);
	glUniform1f(gModernGLShader.uFeatherDistance, distance);
	return true;
}

void ModernGL_EndSoftParticle(void)
{
	glUniform1i(gModernGLShader.uSoftParticle, 0);
}

#endif

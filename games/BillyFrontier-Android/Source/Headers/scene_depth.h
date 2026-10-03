#pragma once

#include "modern_gl_api.h"

void SceneDepth_BeginFrame(int width, int height);
void SceneDepth_Capture(void);
void SceneDepth_Present(void);
void SceneDepth_Shutdown(void);
GLuint SceneDepth_Texture(void);
int SceneDepth_Width(void);
int SceneDepth_Height(void);

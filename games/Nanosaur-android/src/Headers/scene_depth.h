#pragma once

#include <stdbool.h>

bool SceneDepth_BeginFrame(int width, int height, bool enabled);
void SceneDepth_CaptureOpaque(void);
void SceneDepth_EndFrame(void);
void SceneDepth_Shutdown(void);

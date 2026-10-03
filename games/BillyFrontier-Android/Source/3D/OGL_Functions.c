#include "game.h"

void OGL_InitFunctions(void)
{
#ifdef _WIN32
	GAME_ASSERT_MESSAGE(Billy_LoadDesktopGL(), "Required OpenGL 3.3 functions are unavailable");
#endif
	ModernGL_Init();
}

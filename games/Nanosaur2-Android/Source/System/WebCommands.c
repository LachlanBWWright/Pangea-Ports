// NANOSAUR 2 WEB COMMANDS
// JavaScript <-> C interop for WebAssembly builds.
// Exposes cheat/debug commands callable from the browser console or level editor.

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include "game.h"
#include "profiling.h"

/****************************/
/*   FENCE COLLISION CHEAT  */
/****************************/

// Called from JavaScript: Module.ccall('Nanosaur2_SetFenceCollisionsEnabled', null, ['number'], [0]);
EMSCRIPTEN_KEEPALIVE void Nanosaur2_SetFenceCollisionsEnabled(int enabled)
{
	gFenceCollisionsDisabled = !enabled;
	SDL_Log("Fence collisions %s", enabled ? "enabled" : "disabled");
}

EMSCRIPTEN_KEEPALIVE int Nanosaur2_GetFenceCollisionsEnabled(void)
{
	return gFenceCollisionsDisabled ? 0 : 1;
}

/****************************/
/*   LEVEL MANAGEMENT       */
/****************************/

// Returns the current level number (0-based)
EMSCRIPTEN_KEEPALIVE int Nanosaur2_GetCurrentLevel(void)
{
	return (int)gLevelNum;
}

// Set a terrain override file path for the next level load.
// Call this before the level loads (e.g., before clicking "Play" in a wrapper page).
// The path should point to a .ter file that has already been written into the
// Emscripten virtual filesystem (e.g., via FS.writeFile).
EMSCRIPTEN_KEEPALIVE void Nanosaur2_SetTerrainOverridePath(const char* path)
{
	if (path && path[0] != '\0')
	{
		SDL_strlcpy(gCmdTerrainOverridePath, path, sizeof(gCmdTerrainOverridePath));
		// Note: gCmdTerrainOverrideSpec will be set when LoadLevelArt() is called
		// because Pomme::Files::HostPathToFSSpec is C++ and not directly callable here.
		// We defer conversion to the C++ side in LoadLevelArt via a wrapper.
		SDL_Log("Terrain override path set: %s", gCmdTerrainOverridePath);
	}
	else
	{
		gCmdTerrainOverridePath[0] = '\0';
		SDL_memset(&gCmdTerrainOverrideSpec, 0, sizeof(gCmdTerrainOverrideSpec));
	}
}

/****************************/
/*   PROFILING / DEBUG      */
/****************************/

// Returns profiling data as a JSON string into the provided buffer.
// Call from JS: Module.ccall('Nanosaur2_GetProfilingJSON', 'string', [], [])
// or: Module.ccall('Nanosaur2_GetProfilingJSON', 'string', ['number','number'], [ptr, len])
EMSCRIPTEN_KEEPALIVE void Nanosaur2_GetProfilingJSON(char* buf, int bufLen)
{
	if (!buf || bufLen <= 0) return;

	int offset = SDL_snprintf(buf, bufLen, "{");
	for (int i = 0; i < NUM_PROFILE_PHASES && offset < bufLen - 2; i++)
	{
		const char* name = gProfilePhases[i].name ? gProfilePhases[i].name : "?";
		float ms = GetProfilePhaseAvgMs((ProfilePhaseType)i);
		offset += SDL_snprintf(buf + offset, bufLen - offset,
			"%s\"%s\":%.3f",
			(i == 0) ? "" : ",",
			name, (double)ms);
	}
	// also include current fps
	offset += SDL_snprintf(buf + offset, bufLen - offset,
		",\"fps\":%.1f", (double)gFramesPerSecond);
	SDL_snprintf(buf + offset, bufLen - offset, "}");
}

// Toggle the in-game debug overlay (cycles 0→1→2→3→0).
// Press F8 in-game or call this from JS: Module.ccall('Nanosaur2_ToggleDebugMode', null, [], [])
EMSCRIPTEN_KEEPALIVE void Nanosaur2_ToggleDebugMode(void)
{
	if (++gDebugMode > 2)
		gDebugMode = 0;
}

// Returns the current debug mode level (0 = off, 1 = basic, 2 = full).
EMSCRIPTEN_KEEPALIVE int Nanosaur2_GetDebugMode(void)
{
	return (int)gDebugMode;
}

// Returns current FPS as a float.
EMSCRIPTEN_KEEPALIVE float Nanosaur2_GetFPS(void)
{
	return gFramesPerSecond;
}

#endif // __EMSCRIPTEN__

// CRO-MAG RALLY ENTRY POINT
// (C) 2025 Iliyas Jorio
// This file is part of Cro-Mag Rally. https://github.com/jorio/cromagrally

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "Pomme.h"
#include "PommeInit.h"
#include "PommeFiles.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#endif

extern "C"
{
	#include "game.h"
	#include "pangea_net.h"

	SDL_Window* gSDLWindow = nullptr;
	FSSpec gDataSpec;
	CommandLineOptions gCommandLine;
	int gCurrentAntialiasingLevel;

#ifdef __EMSCRIPTEN__
	static int gPangeaNetEnabled = 0;
	static int gPangeaNetIsHost = 1;
	static int gPangeaNetLocalPlayerIndex = 0;
	static int gPangeaNetHostPlayerIndex = 0;
	static int gPangeaNetPlayerCount = 1;
	static uint32_t gPangeaNetMatchSeed = 1;
	static uint32_t gPangeaNetMatchIdLow = 1;
	static uint32_t gPangeaNetMatchIdHigh = 0;
	static char gPangeaNetLobbyId[64] = "00000000-0000-0000-0000-000000000000";
	static char gPangeaNetMatchId[64] = "00000000-0000-0000-0000-000000000000";
	static uint32_t gPangeaDebugFrameNumber = 0;
	static uint32_t gPangeaDebugLastSyncHash = 0;
	static int gPangeaDebugHasDesync = 0;

	EM_JS(int, JS_PangeaNet_SendReliable, (const void* bytes, int byteCount), {
		const net = globalThis.PangeaNet;
		if (!net || typeof net.sendReliable !== "function" || byteCount <= 0)
		{
			return 0;
		}
		const packet = HEAPU8.slice(bytes, bytes + byteCount);
		return net.sendReliable(packet.buffer) ? 1 : 0;
	});

	EM_JS(int, JS_PangeaNet_SendUnreliable, (const void* bytes, int byteCount), {
		const net = globalThis.PangeaNet;
		if (!net || typeof net.sendUnreliable !== "function" || byteCount <= 0)
		{
			return 0;
		}
		const packet = HEAPU8.slice(bytes, bytes + byteCount);
		return net.sendUnreliable(packet.buffer) ? 1 : 0;
	});

	EM_JS(int, JS_PangeaNet_PollMessage, (void* outBytes, int maxByteCount), {
		const net = globalThis.PangeaNet;
		if (!net || typeof net.pollMessage !== "function" || maxByteCount <= 0)
		{
			return 0;
		}
		const packet = net.pollMessage(maxByteCount);
		if (!(packet instanceof ArrayBuffer))
		{
			return 0;
		}
		const payload = new Uint8Array(packet);
		if (payload.byteLength <= 0 || payload.byteLength > maxByteCount)
		{
			return 0;
		}
		HEAPU8.set(payload, outBytes);
		return payload.byteLength;
	});

	EM_JS(double, JS_PangeaNet_NowMilliseconds, (void), {
		const net = globalThis.PangeaNet;
		if (!net || typeof net.nowMilliseconds !== "function")
		{
			return -1;
		}
		return net.nowMilliseconds();
	});

	EM_JS(void, JS_PangeaNet_ReportDesync, (uint32_t frame, uint32_t localHash, uint32_t remoteHash), {
		const net = globalThis.PangeaNet;
		if (net && typeof net.reportDesync === "function")
		{
			net.reportDesync(frame, localHash, remoteHash);
		}
	});

	EM_JS(void, JS_PangeaNet_ReportMatchEnded, (int reason), {
		const net = globalThis.PangeaNet;
		if (net && typeof net.reportMatchEnded === "function")
		{
			net.reportMatchEnded(reason);
		}
	});

	EM_JS(void, JS_PangeaNet_ReportMatchResult, (const char* json), {
		const net = globalThis.PangeaNet;
		if (net && typeof net.reportMatchResult === "function")
		{
			net.reportMatchResult(UTF8ToString(json));
		}
	});

	static int ParseJsonInt(const char* json, const char* key, int fallback)
	{
		if (!json || !key)
		{
			return fallback;
		}

		const char* keyAt = SDL_strstr(json, key);
		if (!keyAt)
		{
			return fallback;
		}

		const char* colon = SDL_strchr(keyAt, ':');
		if (!colon)
		{
			return fallback;
		}

		char* end = nullptr;
		const long parsed = strtol(colon + 1, &end, 10);
		if (end == colon + 1)
		{
			return fallback;
		}

		return (int) parsed;
	}

	static uint32_t ParseJsonU32(const char* json, const char* key, uint32_t fallback)
	{
		if (!json || !key)
		{
			return fallback;
		}

		const char* keyAt = SDL_strstr(json, key);
		if (!keyAt)
		{
			return fallback;
		}

		const char* colon = SDL_strchr(keyAt, ':');
		if (!colon)
		{
			return fallback;
		}

		char* end = nullptr;
		const unsigned long parsed = strtoul(colon + 1, &end, 10);
		if (end == colon + 1)
		{
			return fallback;
		}

		return (uint32_t) parsed;
	}

	static void ParseJsonString(const char* json, const char* key, char* outValue, size_t outValueSize)
	{
		if (!outValue || outValueSize == 0)
		{
			return;
		}

		outValue[0] = '\0';
		if (!json || !key)
		{
			return;
		}

		const char* keyAt = SDL_strstr(json, key);
		if (!keyAt)
		{
			return;
		}

		const char* colon = SDL_strchr(keyAt, ':');
		if (!colon)
		{
			return;
		}

		const char* quoteStart = SDL_strchr(colon, '"');
		if (!quoteStart)
		{
			return;
		}

		quoteStart++;
		const char* quoteEnd = SDL_strchr(quoteStart, '"');
		if (!quoteEnd || quoteEnd <= quoteStart)
		{
			return;
		}

		size_t copyLength = (size_t) (quoteEnd - quoteStart);
		if (copyLength >= outValueSize)
		{
			copyLength = outValueSize - 1;
		}

		SDL_memcpy(outValue, quoteStart, copyLength);
		outValue[copyLength] = '\0';
	}

	static int ParseCroMagTrackNumber(const char* json, int fallback)
	{
		char trackOrLevel[64];
		ParseJsonString(json, "\"trackOrLevel\"", trackOrLevel, sizeof(trackOrLevel));
		if (trackOrLevel[0] == '\0')
		{
			return fallback;
		}

		char* end = nullptr;
		const long parsed = strtol(trackOrLevel, &end, 10);
		if (end == trackOrLevel)
		{
			return fallback;
		}

		const int parsedTrack = (int) parsed;
		return parsedTrack > 0 ? parsedTrack - 1 : fallback;
	}

	static const char* CroMagGameModeName(int gameMode)
	{
		switch (gameMode)
		{
			case GAME_MODE_PRACTICE:
				return "practice";

			case GAME_MODE_TOURNAMENT:
				return "tournament";

			case GAME_MODE_MULTIPLAYERRACE:
				return "multiplayerRace";

			case GAME_MODE_TAG1:
				return "multiplayerTag1";

			case GAME_MODE_TAG2:
				return "multiplayerTag2";

			case GAME_MODE_SURVIVAL:
				return "multiplayerSurvival";

			case GAME_MODE_CAPTUREFLAG:
				return "multiplayerQuestForFire";

			default:
				return "unknown";
		}
	}

	static int FallbackCroMagModeFromTrack(int trackNumber, const char* fallbackReason)
	{
		const int fallbackMode = trackNumber >= NUM_RACE_TRACKS
			? GAME_MODE_SURVIVAL
			: GAME_MODE_MULTIPLAYERRACE;
		SDL_Log(
			"Cro-Mag network mode fallback reason=%s track=%d resolvedMode=%s(%d)",
			fallbackReason,
			trackNumber + 1,
			CroMagGameModeName(fallbackMode),
			fallbackMode);
		return fallbackMode;
	}

	static int ParseCroMagMode(const char* json, int trackNumber)
	{
		char mode[64];
		ParseJsonString(json, "\"mode\"", mode, sizeof(mode));

		if (mode[0] == '\0')
		{
			return FallbackCroMagModeFromTrack(trackNumber, "missing-mode");
		}

		if (SDL_strcasecmp(mode, "multiplayerRace") == 0 || SDL_strcasecmp(mode, "race") == 0)
		{
			return GAME_MODE_MULTIPLAYERRACE;
		}
		if (SDL_strcasecmp(mode, "multiplayerTag1") == 0 || SDL_strcasecmp(mode, "tagKeepAway") == 0 || SDL_strcasecmp(mode, "tag1") == 0)
		{
			return GAME_MODE_TAG1;
		}
		if (SDL_strcasecmp(mode, "multiplayerTag2") == 0 || SDL_strcasecmp(mode, "tagStampede") == 0 || SDL_strcasecmp(mode, "tag2") == 0)
		{
			return GAME_MODE_TAG2;
		}
		if (SDL_strcasecmp(mode, "multiplayerSurvival") == 0 || SDL_strcasecmp(mode, "survival") == 0 || SDL_strcasecmp(mode, "multiplayerBattle") == 0 || SDL_strcasecmp(mode, "battle") == 0)
		{
			return GAME_MODE_SURVIVAL;
		}
		if (SDL_strcasecmp(mode, "multiplayerQuestForFire") == 0 || SDL_strcasecmp(mode, "multiplayerFlag") == 0 || SDL_strcasecmp(mode, "captureFlag") == 0 || SDL_strcasecmp(mode, "questForFire") == 0)
		{
			return GAME_MODE_CAPTUREFLAG;
		}

		return FallbackCroMagModeFromTrack(trackNumber, mode);
	}

	// Called once per browser frame when running in WASM mode (legacy path, kept for reference)
	void GameMain_RunFrame(void);
	void GameMain_InitEmscripten(void);
	Boolean GameMain_IsEmscriptenDone(void);
	// New unified entry point: reads URL params into gCommandLine, then calls GameMain()
	void GameMain_ReadURLParams(void);

	// JavaScript-callable cheat command handler
	EMSCRIPTEN_KEEPALIVE
	void WASM_SetFenceCollision(int enable)
	{
		gDisableFenceCollision = !enable;
	}

	EMSCRIPTEN_KEEPALIVE
	int WASM_GetFenceCollision(void)
	{
		return gDisableFenceCollision ? 0 : 1;
	}

	EMSCRIPTEN_KEEPALIVE
	void PangeaGame_SetNetworkMatchConfig(const char* json, int byteCount)
	{
		(void) byteCount;
		SDL_Log("PangeaGame_SetNetworkMatchConfig json=%s", json ? json : "(null)");
		gPangeaNetEnabled = 1;
		const int explicitIsHost = ParseJsonInt(json, "\"isHost\"", -1);
		gPangeaNetLocalPlayerIndex = ParseJsonInt(json, "\"localPlayerIndex\"", 0);
		gPangeaNetHostPlayerIndex = ParseJsonInt(json, "\"hostPlayerIndex\"", 0);
		gPangeaNetPlayerCount = ParseJsonInt(json, "\"playerCount\"", 2);
		gPangeaNetMatchSeed = ParseJsonU32(json, "\"seed\"", 1);
		gPangeaNetMatchIdLow = ParseJsonU32(json, "\"matchIdLow\"", gPangeaNetMatchSeed);
		gPangeaNetMatchIdHigh = ParseJsonU32(json, "\"matchIdHigh\"", 0);
		ParseJsonString(json, "\"lobbyId\"", gPangeaNetLobbyId, sizeof(gPangeaNetLobbyId));
		ParseJsonString(json, "\"matchId\"", gPangeaNetMatchId, sizeof(gPangeaNetMatchId));
		if (gPangeaNetLobbyId[0] == '\0')
		{
			SDL_strlcpy(gPangeaNetLobbyId, "00000000-0000-0000-0000-000000000000", sizeof(gPangeaNetLobbyId));
		}
		if (gPangeaNetMatchId[0] == '\0')
		{
			SDL_strlcpy(gPangeaNetMatchId, "00000000-0000-0000-0000-000000000000", sizeof(gPangeaNetMatchId));
		}
		const int trackNumber = ParseCroMagTrackNumber(json, gTrackNum);
		const int tagDurationMinutes = ParseJsonInt(json, "\"tagDurationMinutes\"", gGamePrefs.tagDuration);

		if (gPangeaNetPlayerCount < 1)
		{
			gPangeaNetPlayerCount = 1;
		}
		if (gPangeaNetHostPlayerIndex < 0 || gPangeaNetHostPlayerIndex >= gPangeaNetPlayerCount)
		{
			gPangeaNetHostPlayerIndex = 0;
		}
		if (gPangeaNetLocalPlayerIndex < 0)
		{
			gPangeaNetLocalPlayerIndex = 0;
		}
		if (gPangeaNetLocalPlayerIndex >= gPangeaNetPlayerCount)
		{
			gPangeaNetLocalPlayerIndex = gPangeaNetPlayerCount - 1;
		}
		if (explicitIsHost == 0 || explicitIsHost == 1)
		{
			gPangeaNetIsHost = explicitIsHost;
		}
		else
		{
			gPangeaNetIsHost = gPangeaNetLocalPlayerIndex == gPangeaNetHostPlayerIndex ? 1 : 0;
		}

		if (gPangeaNetIsHost)
		{
			gPangeaNetLocalPlayerIndex = gPangeaNetHostPlayerIndex;
		}
		else if (gPangeaNetPlayerCount > 1 && gPangeaNetLocalPlayerIndex == gPangeaNetHostPlayerIndex)
		{
			gPangeaNetLocalPlayerIndex = gPangeaNetHostPlayerIndex == 0 ? 1 : 0;
		}

		gNetGameInProgress = true;
		gIsNetworkHost = gPangeaNetIsHost != 0;
		gIsNetworkClient = gPangeaNetIsHost == 0;
		gTrackNum = trackNumber;
		gGameMode = ParseCroMagMode(json, trackNumber);
		gGamePrefs.tagDuration = (Byte)SDL_clamp(tagDurationMinutes, 2, 4);
		gNumRealPlayers = (short) gPangeaNetPlayerCount;
		gMyNetworkPlayerNum = (short) gPangeaNetLocalPlayerIndex;
		PangeaNetBridge_SetRuntimeMatchIdentity(gPangeaNetMatchIdLow, gPangeaNetMatchIdHigh);
		SDL_Log(
			"PangeaGame_SetNetworkMatchConfig resolved host=%d localPlayer=%d hostPlayer=%d playerCount=%d seed=%u matchId=%u:%u mode=%s(%d) track=%d raceMode=%d tagDuration=%d",
			gPangeaNetIsHost,
			gPangeaNetLocalPlayerIndex,
			gPangeaNetHostPlayerIndex,
			gPangeaNetPlayerCount,
			(unsigned) gPangeaNetMatchSeed,
			(unsigned) gPangeaNetMatchIdHigh,
			(unsigned) gPangeaNetMatchIdLow,
			CroMagGameModeName(gGameMode),
			gGameMode,
			gTrackNum + 1,
			IsRaceMode(),
			gGamePrefs.tagDuration);
	}

	EMSCRIPTEN_KEEPALIVE
	void PangeaGame_StartNetworkMatch(void)
	{
		SDL_Log(
			"PangeaGame_StartNetworkMatch called mode=%s(%d) track=%d players=%d host=%d raceMode=%d",
			CroMagGameModeName(gGameMode),
			gGameMode,
			gTrackNum + 1,
			gPangeaNetPlayerCount,
			gPangeaNetIsHost,
			IsRaceMode());
		gPangeaNetEnabled = 1;
		gNetGameInProgress = true;
		gPangeaDebugFrameNumber = 0;
		gPangeaDebugLastSyncHash = gPangeaNetMatchSeed;
		gPangeaDebugHasDesync = 0;
	}

	EMSCRIPTEN_KEEPALIVE int PangeaNet_IsEnabled(void) { return gPangeaNetEnabled; }
	EMSCRIPTEN_KEEPALIVE int PangeaNet_IsHost(void) { return gPangeaNetIsHost; }
	EMSCRIPTEN_KEEPALIVE int PangeaNet_GetLocalPlayerIndex(void) { return gPangeaNetLocalPlayerIndex; }
	EMSCRIPTEN_KEEPALIVE int PangeaNet_GetPlayerCount(void) { return gPangeaNetPlayerCount; }
	EMSCRIPTEN_KEEPALIVE uint32_t PangeaNet_GetMatchSeed(void) { return gPangeaNetMatchSeed; }
	EMSCRIPTEN_KEEPALIVE uint32_t PangeaNet_GetMatchIdLow(void) { return gPangeaNetMatchIdLow; }
	EMSCRIPTEN_KEEPALIVE uint32_t PangeaNet_GetMatchIdHigh(void) { return gPangeaNetMatchIdHigh; }
	EMSCRIPTEN_KEEPALIVE const char* PangeaNet_GetLobbyIdString(void) { return gPangeaNetLobbyId; }
	EMSCRIPTEN_KEEPALIVE const char* PangeaNet_GetMatchIdString(void) { return gPangeaNetMatchId; }
	EMSCRIPTEN_KEEPALIVE int PangeaNet_SendReliable(const void* bytes, int byteCount)
	{
		return JS_PangeaNet_SendReliable(bytes, byteCount);
	}
	EMSCRIPTEN_KEEPALIVE int PangeaNet_SendUnreliable(const void* bytes, int byteCount)
	{
		return JS_PangeaNet_SendUnreliable(bytes, byteCount);
	}
	EMSCRIPTEN_KEEPALIVE int PangeaNet_PollMessage(void* outBytes, int maxByteCount)
	{
		return JS_PangeaNet_PollMessage(outBytes, maxByteCount);
	}
	EMSCRIPTEN_KEEPALIVE double PangeaNet_NowMilliseconds(void)
	{
		double bridgedNow = JS_PangeaNet_NowMilliseconds();
		return bridgedNow >= 0 ? bridgedNow : emscripten_get_now();
	}
	EMSCRIPTEN_KEEPALIVE void PangeaNet_ReportDesync(uint32_t frame, uint32_t localHash, uint32_t remoteHash)
	{
		JS_PangeaNet_ReportDesync(frame, localHash, remoteHash);
		gPangeaDebugFrameNumber = frame;
		gPangeaDebugLastSyncHash = remoteHash;
		gPangeaDebugHasDesync = 1;
	}
	EMSCRIPTEN_KEEPALIVE void PangeaNet_ReportMatchEnded(int reason)
	{
		JS_PangeaNet_ReportMatchEnded(reason);
	}
	EMSCRIPTEN_KEEPALIVE void PangeaNet_ReportMatchResult(const char* json)
	{
		if (!json)
		{
			return;
		}
		JS_PangeaNet_ReportMatchResult(json);
	}
	EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetFrameNumber(void) { return gPangeaDebugFrameNumber; }
	EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetLocalPlayerIndex(void) { return gPangeaNetLocalPlayerIndex; }
	EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetPlayerCount(void) { return gPangeaNetPlayerCount; }
	EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugIsNetworkMatchRunning(void) { return gPangeaNetEnabled; }
	EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugHasDesync(void) { return gPangeaDebugHasDesync; }
	EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetLastSyncHash(void) { return gPangeaDebugLastSyncHash; }
	EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetLastMatchEndReason(void) { return PangeaNetBridge_GetLastMatchEndReason(); }
	EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugHasMatchResult(void) { return PangeaNetBridge_HasMatchResult(); }
	EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetPlayerPosition(int playerIndex, float* outX, float* outY, float* outZ)
	{
		if (!outX || !outY || !outZ)
		{
			return 0;
		}
		if (playerIndex < 0 || playerIndex >= gPangeaNetPlayerCount)
		{
			return 0;
		}
		*outX = 0.0f;
		*outY = 0.0f;
		*outZ = 0.0f;
		return 1;
	}
	EMSCRIPTEN_KEEPALIVE void PangeaGame_DebugSetInputScript(const char* json, int byteCount)
	{
		(void) json;
		(void) byteCount;
	}
#endif
}

static fs::path FindGameData(const char* executablePath)
{
	fs::path dataPath;

	int attemptNum = 0;

#if !(__APPLE__)
	attemptNum++;		// skip macOS special case #0
#endif

	if (!executablePath)
		attemptNum = 2;

tryAgain:
	switch (attemptNum)
	{
		case 0:			// special case for macOS app bundles
			dataPath = executablePath;
			dataPath = dataPath.parent_path().parent_path() / "Resources";
			break;

		case 1:
			dataPath = executablePath;
			dataPath = dataPath.parent_path() / "Data";
			break;

		case 2:
			dataPath = "Data";
			break;

		default:
			throw std::runtime_error("Couldn't find the Data folder.");
	}

	attemptNum++;

	dataPath = dataPath.lexically_normal();

	// Set data spec -- Lets the game know where to find its asset files
	gDataSpec = Pomme::Files::HostPathToFSSpec(dataPath / "System");

	FSSpec someDataFileSpec;
	OSErr iErr = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":System:gamecontrollerdb.txt", &someDataFileSpec);
	if (iErr)
	{
		goto tryAgain;
	}

	return dataPath;
}

static void ParseCommandLine(int argc, char** argv)
{
	SDL_memset(&gCommandLine, 0, sizeof(gCommandLine));
	gCommandLine.vsync = 1;

	for (int i = 1; i < argc; i++)
	{
		std::string argument = argv[i];

		if (argument == "--track")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "practice track # unspecified");
			gCommandLine.bootToTrack = atoi(argv[i + 1]);
			i += 1;
		}
		else if (argument == "--car")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "car # unspecified");
			gCommandLine.car = atoi(argv[i + 1]);
			i += 1;
		}
		else if (argument == "--level-override")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "level override path unspecified");
			SDL_strlcpy(gCommandLine.levelOverridePath, argv[i + 1], sizeof(gCommandLine.levelOverridePath));
			i += 1;
		}
		else if (argument == "--no-fence-collision")
			gCommandLine.noFenceCollision = 1;
		else if (argument == "--stats")
			gDebugMode = 1;
		else if (argument == "--no-vsync")
			gCommandLine.vsync = 0;
		else if (argument == "--vsync")
			gCommandLine.vsync = 1;
		else if (argument == "--adaptive-vsync")
			gCommandLine.vsync = -1;
#if 0
		else if (argument == "--fullscreen-resolution")
		{
			GAME_ASSERT_MESSAGE(i + 2 < argc, "fullscreen width & height unspecified");
			gCommandLine.fullscreenWidth = atoi(argv[i + 1]);
			gCommandLine.fullscreenHeight = atoi(argv[i + 2]);
			i += 2;
		}
		else if (argument == "--fullscreen-refresh-rate")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "fullscreen refresh rate unspecified");
			gCommandLine.fullscreenRefreshRate = atoi(argv[i + 1]);
			i += 1;
		}
#endif
	}
}

static void Boot(int argc, char** argv)
{
	SDL_SetAppMetadata(GAME_FULL_NAME, GAME_VERSION, GAME_IDENTIFIER);
#if _DEBUG
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#elif defined(__EMSCRIPTEN__)
	// Verbose logging helps debug WASM issues in the browser console
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#else
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
#endif

	ParseCommandLine(argc, argv);

	// Start our "machine"
	Pomme::Init();

	// Find path to game data folder
	const char* executablePath = argc > 0 ? argv[0] : NULL;
	fs::path dataPath = FindGameData(executablePath);

	// Load game prefs before starting
	LoadPrefs();

retryVideo:
	// Initialize SDL video subsystem
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		throw std::runtime_error("Couldn't initialize SDL video subsystem.");
	}

	// Create window
#ifdef __EMSCRIPTEN__
	// Request GLES3 so SDL/Emscripten creates a WebGL2 context.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

	gCurrentAntialiasingLevel = gGamePrefs.antialiasingLevel;
#ifdef __EMSCRIPTEN__
	gCurrentAntialiasingLevel = 0;
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
#endif
	if (gCurrentAntialiasingLevel != 0)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 1 << gCurrentAntialiasingLevel);
	}

	// Determine initial window size
	int initialWidth  = 640;
	int initialHeight = 480;
	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef __EMSCRIPTEN__
	// Use a fixed game resolution for WebAssembly builds.
	// SDL_GetDisplayUsableBounds() may return spuriously small values in
	// headless browsers before the page layout is fully computed.
	initialWidth  = 1280;
	initialHeight = 720;
	windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#endif

	gSDLWindow = SDL_CreateWindow(
		GAME_FULL_NAME " " GAME_VERSION, initialWidth, initialHeight,
		windowFlags);

#ifdef __EMSCRIPTEN__
	// Force the WebGL canvas to the target resolution.
	// SDL3 on Emscripten may set the canvas to 3×3 before the page layout settles.
	if (gSDLWindow)
	{
		SDL_SetWindowSize(gSDLWindow, initialWidth, initialHeight);
		SDL_SyncWindow(gSDLWindow);
	}
#endif

	if (!gSDLWindow)
	{
		if (gCurrentAntialiasingLevel != 0)
		{
			SDL_Log("Couldn't create SDL window with the requested MSAA level. Retrying without MSAA...");

			// retry without MSAA
			gGamePrefs.antialiasingLevel = 0;
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			goto retryVideo;
		}
		else
		{
			throw std::runtime_error("Couldn't create SDL window.");
		}
	}

	// Init gamepad subsystem
	SDL_Init(SDL_INIT_GAMEPAD);
	auto gamecontrollerdbPath8 = (dataPath / "System" / "gamecontrollerdb.txt").u8string();
	if (-1 == SDL_AddGamepadMappingsFromFile((const char*)gamecontrollerdbPath8.c_str()))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, GAME_FULL_NAME, "Couldn't load gamecontrollerdb.txt!", gSDLWindow);
	}
}

#ifndef __EMSCRIPTEN__
static void Shutdown()
{
	// Always restore the user's mouse acceleration before exiting.
	// SetMacLinearMouse(false);

	Pomme::Shutdown();

	if (gSDLWindow)
	{
		SDL_DestroyWindow(gSDLWindow);
		gSDLWindow = NULL;
	}

	SDL_Quit();
}
#endif

int main(int argc, char** argv)
{
	bool success = true;
	std::string uncaught = "";

	try
	{
		Boot(argc, argv);

#ifdef __EMSCRIPTEN__
		// Read URL query params into gCommandLine BEFORE GameMain so that
		// ?track=N skips the menu (same as --track N on the command line).
		// When ?track is absent, gCommandLine.bootToTrack stays 0 and
		// GameMain() will show the normal main-menu flow instead of
		// launching directly into a race.
		GameMain_ReadURLParams();
		GameMain();
#else
		GameMain();
#endif
	}
	catch (Pomme::QuitRequest&)
	{
		// no-op, the game may throw this exception to shut us down cleanly
	}
#if !(_DEBUG)
	// In release builds, catch anything that might be thrown by GameMain
	// so we can show an error dialog to the user.
	catch (std::exception& ex)		// Last-resort catch
	{
		success = false;
		uncaught = ex.what();
	}
	catch (...)						// Last-resort catch
	{
		success = false;
		uncaught = "unknown";
	}
#endif

#ifndef __EMSCRIPTEN__
	Shutdown();
#endif

	if (!success)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Uncaught exception: %s", uncaught.c_str());
		SDL_ShowSimpleMessageBox(0, GAME_FULL_NAME, uncaught.c_str(), nullptr);
	}

	return success ? 0 : 1;
}

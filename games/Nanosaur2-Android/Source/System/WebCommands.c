// NANOSAUR 2 WEB COMMANDS
// JavaScript <-> C interop for WebAssembly builds.
// Exposes cheat/debug commands callable from the browser console or level editor.

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <stdlib.h>
#include <math.h>
#include "game.h"

static int gPangeaNetEnabled = 0;
static int gPangeaNetIsHost = 1;
static int gPangeaNetLocalPlayerIndex = 0;
static int gPangeaNetHostPlayerIndex = 0;
static int gPangeaNetPlayerCount = 1;
static uint32_t gPangeaNetMatchSeed = 1;
static uint32_t gPangeaNetMatchIdLow = 1;
static uint32_t gPangeaNetMatchIdHigh = 0;
static uint32_t gPangeaNetLifecycleSequence = 0;
static int gPangeaNetLastSentLifecycleReason = 0;
static int gPangeaNetRemoteLifecycleReason = 0;
static uint32_t gPangeaNetRemoteLifecycleSequence = 0;
static int gPangeaNetLastMatchEndReason = 0;
static int gPangeaNetHasMatchResult = 0;
static int gPangeaNetResultPublished = 0;
static char gPangeaNetLobbyId[64] = "00000000-0000-0000-0000-000000000000";
static char gPangeaNetMatchId[64] = "00000000-0000-0000-0000-000000000000";
static uint32_t gPangeaDebugFrameNumber = 0;
static uint32_t gPangeaDebugLastSyncHash = 0;
static int gPangeaDebugHasDesync = 0;

static const char* Nanosaur2VSModeName(int vsMode)
{
	switch (vsMode)
	{
		case VS_MODE_NONE:
			return "adventure";

		case VS_MODE_RACE:
			return "race";

		case VS_MODE_BATTLE:
			return "battle";

		case VS_MODE_CAPTURETHEFLAG:
			return "capture-the-flag";

		default:
			return "unknown";
	}
}

/***
 * Sequence tracking for disruption resilience
 * Host sends strictly-increasing sequences for lifecycle packets
 * Clients must accept only valid, non-duplicate sequences
 ***/
static uint32_t gGameplaySequenceTracker = 0;  // Last accepted sequence number for lifecycle packets
static int gConsecutiveMissingLifecyclePackets = 0;

static int ValidateLifecycleSequence(uint32_t incomingSequence, int isFromHost)
{
	// Zero is reserved
	if (incomingSequence == 0)
	{
		SDL_Log("Nanosaur2 lifecycle: rejected zero sequence");
		return 0;
	}

	// Check for duplicate
	if (incomingSequence == gGameplaySequenceTracker)
	{
		SDL_Log("Nanosaur2 lifecycle: duplicate sequence %u", (unsigned)incomingSequence);
		return 0;
	}

	// In strict mode (host packets), sequences must be strictly increasing
	if (isFromHost)
	{
		if (incomingSequence <= gGameplaySequenceTracker)
		{
			SDL_Log(
				"Nanosaur2 lifecycle: out-of-order host packet sequence expected > %u, got %u",
				(unsigned)gGameplaySequenceTracker,
				(unsigned)incomingSequence);
			return 0;
		}

		// Check for gaps
		if (incomingSequence > gGameplaySequenceTracker + 1)
		{
			SDL_Log(
				"Nanosaur2 lifecycle: gap in host sequence, expected %u got %u (missing %u packets)",
				(unsigned)(gGameplaySequenceTracker + 1),
				(unsigned)incomingSequence,
				(unsigned)(incomingSequence - gGameplaySequenceTracker - 1));
			gConsecutiveMissingLifecyclePackets++;
		}
		else
		{
			gConsecutiveMissingLifecyclePackets = 0;
		}
	}

	// Accept packet and update tracker
	gGameplaySequenceTracker = incomingSequence;
	gConsecutiveMissingLifecyclePackets = 0;
	return 1;
}

#define PANGEA_NET_LIFECYCLE_MAGIC 0x504E4D53u
#define PANGEA_NET_LIFECYCLE_VERSION 1u
#define PANGEA_NET_LIFECYCLE_MATCH_END 1u
#define PANGEA_NET_LIFECYCLE_MATCH_RESULT 2u
#define PANGEA_NET_MATCH_STATE_NONE 0
#define PANGEA_NET_MATCH_STATE_GAME_OVER 1
#define PANGEA_NET_MATCH_STATE_LEVEL_COMPLETED 2
#define PANGEA_NET_BUFFER_SIZE 4096
#define PANGEA_NET_BACKLOG_CAPACITY 64

typedef struct
{
	uint32_t magic;
	uint32_t version;
	uint32_t messageType;
	uint32_t sequence;
	int32_t reason;
	int32_t fromHost;
} PangeaNetLifecyclePacket;

#define PANGEA_NET_RESULT_PLAYERS 2

typedef struct
{
	uint8_t playerIndex;
	uint8_t placement;
	uint8_t finished;
	uint8_t eliminated;
	int16_t score;
	uint16_t lapsCompleted;
	uint16_t checkpoint;
	uint16_t reserved;
} PangeaNetResultPlayer;

typedef struct
{
	uint32_t magic;
	uint32_t version;
	uint32_t messageType;
	uint32_t sequence;
	uint32_t matchIdLow;
	uint32_t matchIdHigh;
	uint32_t seed;
	int32_t endReason;
	int32_t winnerPlayerIndex;
	int32_t winningTeam;
	int32_t mode;
	int32_t trackOrLevel;
	int32_t playerCount;
	PangeaNetResultPlayer players[PANGEA_NET_RESULT_PLAYERS];
	int32_t fromHost;
} PangeaNetMatchResultPacket;

typedef struct
{
	int byteCount;
	uint8_t bytes[PANGEA_NET_BUFFER_SIZE];
} PangeaNetQueuedPacket;

static PangeaNetQueuedPacket gPangeaNetBacklog[PANGEA_NET_BACKLOG_CAPACITY];
static int gPangeaNetBacklogHead = 0;
static int gPangeaNetBacklogTail = 0;

static bool PangeaNetBacklogIsFull(void)
{
	return ((gPangeaNetBacklogTail + 1) % PANGEA_NET_BACKLOG_CAPACITY) == gPangeaNetBacklogHead;
}

static bool PangeaNetBacklogIsEmpty(void)
{
	return gPangeaNetBacklogHead == gPangeaNetBacklogTail;
}

static void PangeaNetBacklogPush(const void* bytes, int byteCount)
{
	if (!bytes || byteCount <= 0 || byteCount > PANGEA_NET_BUFFER_SIZE)
	{
		return;
	}

	if (PangeaNetBacklogIsFull())
	{
		gPangeaNetBacklogHead = (gPangeaNetBacklogHead + 1) % PANGEA_NET_BACKLOG_CAPACITY;
	}

	PangeaNetQueuedPacket* slot = &gPangeaNetBacklog[gPangeaNetBacklogTail];
	slot->byteCount = byteCount;
	SDL_memcpy(slot->bytes, bytes, (size_t)byteCount);
	gPangeaNetBacklogTail = (gPangeaNetBacklogTail + 1) % PANGEA_NET_BACKLOG_CAPACITY;
}

static int PangeaNetBacklogPop(void* outBytes, int maxByteCount)
{
	if (PangeaNetBacklogIsEmpty() || !outBytes || maxByteCount <= 0)
	{
		return 0;
	}

	const PangeaNetQueuedPacket* slot = &gPangeaNetBacklog[gPangeaNetBacklogHead];
	if (slot->byteCount <= 0 || slot->byteCount > maxByteCount)
	{
		return 0;
	}

	SDL_memcpy(outBytes, slot->bytes, (size_t)slot->byteCount);
	gPangeaNetBacklogHead = (gPangeaNetBacklogHead + 1) % PANGEA_NET_BACKLOG_CAPACITY;
	return slot->byteCount;
}

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

	char* end = NULL;
	const long parsed = strtol(colon + 1, &end, 10);
	if (end == colon + 1)
	{
		return fallback;
	}

	return (int)parsed;
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

	char* end = NULL;
	const unsigned long parsed = strtoul(colon + 1, &end, 10);
	if (end == colon + 1)
	{
		return fallback;
	}

	return (uint32_t)parsed;
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

	size_t copyLength = (size_t)(quoteEnd - quoteStart);
	if (copyLength >= outValueSize)
	{
		copyLength = outValueSize - 1;
	}

	SDL_memcpy(outValue, quoteStart, copyLength);
	outValue[copyLength] = '\0';
}

static int ParseNanosaur2LevelNumber(const char* json, int fallback)
{
	char trackOrLevel[64];
	ParseJsonString(json, "\"trackOrLevel\"", trackOrLevel, sizeof(trackOrLevel));
	if (trackOrLevel[0] == '\0')
	{
		return fallback;
	}

	char* end = NULL;
	const long parsed = strtol(trackOrLevel, &end, 10);
	if (end == trackOrLevel)
	{
		return fallback;
	}

	return (int)parsed;
}

static int ParseNanosaur2Mode(const char* json, int levelNumber)
{
	char mode[64];
	ParseJsonString(json, "\"mode\"", mode, sizeof(mode));

	if (mode[0] != '\0')
	{
		if (SDL_strcasecmp(mode, "multiplayerRace") == 0 || SDL_strcasecmp(mode, "race") == 0)
		{
			return VS_MODE_RACE;
		}
		if (SDL_strcasecmp(mode, "multiplayerBattle") == 0 || SDL_strcasecmp(mode, "battle") == 0)
		{
			return VS_MODE_BATTLE;
		}
		if (SDL_strcasecmp(mode, "multiplayerFlag") == 0 || SDL_strcasecmp(mode, "captureEggs") == 0 || SDL_strcasecmp(mode, "captureTheFlag") == 0 || SDL_strcasecmp(mode, "flag") == 0)
		{
			return VS_MODE_CAPTURETHEFLAG;
		}
	}

	const int fallbackMode = GetVSModeForLevel((short)levelNumber);
	SDL_Log(
		"Nanosaur2 network mode fallback reason=%s level=%d resolvedMode=%s(%d)",
		mode[0] == '\0' ? "missing-mode" : mode,
		levelNumber,
		Nanosaur2VSModeName(fallbackMode),
		fallbackMode);
	return fallbackMode;
}

static const char* Nanosaur2ResultModeName(int vsMode)
{
	switch (vsMode)
	{
		case VS_MODE_BATTLE:
			return "multiplayerBattle";
		case VS_MODE_CAPTURETHEFLAG:
			return "multiplayerFlag";
		case VS_MODE_RACE:
		default:
			return "multiplayerRace";
	}
}

static const char* Nanosaur2EndReasonName(int reason)
{
	return reason == PANGEA_NET_MATCH_STATE_GAME_OVER ? "game-over" : "level-completed";
}

static int Nanosaur2PlayerTeam(int playerIndex)
{
	return playerIndex & 1;
}

static int PangeaNetBuildMatchResultPacket(int reason, PangeaNetMatchResultPacket* outPacket)
{
	if (!outPacket)
	{
		return 0;
	}

	const int playerCount = SDL_clamp(gPangeaNetPlayerCount, 1, PANGEA_NET_RESULT_PLAYERS);
	int winnerPlayerIndex = -1;
	if (gVSMode == VS_MODE_RACE)
	{
		int bestPlace = PANGEA_NET_RESULT_PLAYERS;
		for (int i = 0; i < playerCount; i++)
		{
			if (gPlayerInfo[i].place < bestPlace)
			{
				bestPlace = gPlayerInfo[i].place;
				winnerPlayerIndex = i;
			}
		}
	}
	else if (gVSMode == VS_MODE_CAPTURETHEFLAG)
	{
		winnerPlayerIndex = gNumEggsSaved[1] >= gNumEggsToSave[1] ? 0
			: gNumEggsSaved[0] >= gNumEggsToSave[0] ? 1
			: -1;
	}
	else
	{
		for (int i = 0; i < playerCount; i++)
		{
			if (gPlayerInfo[i].health > 0.0f)
			{
				winnerPlayerIndex = i;
				break;
			}
		}
	}

	SDL_memset(outPacket, 0, sizeof(*outPacket));
	outPacket->magic = PANGEA_NET_LIFECYCLE_MAGIC;
	outPacket->version = PANGEA_NET_LIFECYCLE_VERSION;
	outPacket->messageType = PANGEA_NET_LIFECYCLE_MATCH_RESULT;
	outPacket->sequence = ++gPangeaNetLifecycleSequence;
	outPacket->matchIdLow = gPangeaNetMatchIdLow;
	outPacket->matchIdHigh = gPangeaNetMatchIdHigh;
	outPacket->seed = gPangeaNetMatchSeed;
	outPacket->endReason = reason;
	outPacket->winnerPlayerIndex = winnerPlayerIndex;
	outPacket->winningTeam = -1;
	outPacket->mode = gVSMode;
	outPacket->trackOrLevel = gLevelNum;
	outPacket->playerCount = playerCount;
	outPacket->fromHost = 1;

	for (int i = 0; i < playerCount; i++)
	{
		PangeaNetResultPlayer* resultPlayer = &outPacket->players[i];
		const int lapsCompleted = SDL_max(0, (int) gPlayerInfo[i].lapNum);
		resultPlayer->playerIndex = (uint8_t) i;
		resultPlayer->placement = (uint8_t) (gVSMode == VS_MODE_RACE
			? SDL_clamp((int) gPlayerInfo[i].place + 1, 1, playerCount)
			: i == winnerPlayerIndex ? 1 : 2);
		resultPlayer->finished = gPlayerInfo[i].raceComplete ? 1u : 0u;
		resultPlayer->eliminated = gPlayerInfo[i].health <= 0.0f ? 1u : 0u;
		resultPlayer->score = (int16_t) (gVSMode == VS_MODE_CAPTURETHEFLAG
			? gNumEggsSaved[Nanosaur2PlayerTeam(i) ^ 1]
			: i == winnerPlayerIndex ? 1 : 0);
		resultPlayer->lapsCompleted = (uint16_t) lapsCompleted;
		resultPlayer->checkpoint = 0;
	}

	return 1;
}

static void PangeaNetEmitMatchResultJson(const PangeaNetMatchResultPacket* packet)
{
	if (!packet || packet->playerCount <= 0)
	{
		return;
	}

	char playersJson[768];
	char placementsJson[128];
	char json[1024];
	char trackOrLevel[64];
	playersJson[0] = '\0';
	placementsJson[0] = '\0';
	SDL_snprintf(trackOrLevel, (int) sizeof(trackOrLevel), "%d", packet->trackOrLevel);

	for (int i = 0; i < packet->playerCount && i < PANGEA_NET_RESULT_PLAYERS; i++)
	{
		const PangeaNetResultPlayer* player = &packet->players[i];
		char entry[192];
		char placementEntry[32];
		SDL_snprintf(
			entry,
			(int) sizeof(entry),
			"%s{\"participantId\":\"player%d\",\"playerIndex\":%d,\"displayName\":\"Player %d\",\"team\":\"%d\",\"placement\":%d,\"finished\":%s,\"eliminated\":%s,\"score\":%d,\"timeMs\":0,\"lapsCompleted\":%d,\"checkpoint\":%d}",
			i > 0 ? "," : "",
			i,
			(int) player->playerIndex,
			i + 1,
			Nanosaur2PlayerTeam(i),
			(int) player->placement,
			player->finished ? "true" : "false",
			player->eliminated ? "true" : "false",
			(int) player->score,
			(int) player->lapsCompleted,
			(int) player->checkpoint);
		SDL_strlcat(playersJson, entry, sizeof(playersJson));
		SDL_snprintf(placementEntry, (int) sizeof(placementEntry), "%s%d", i > 0 ? "," : "", (int) player->placement);
		SDL_strlcat(placementsJson, placementEntry, sizeof(placementsJson));
	}

	SDL_snprintf(
		json,
		(int) sizeof(json),
		"{\"lobbyId\":\"%s\",\"matchId\":\"%s\",\"gameId\":\"nanosaur2\",\"mode\":\"%s\",\"trackOrLevel\":\"%s\",\"seed\":%u,\"endedAt\":\"1970-01-01T00:00:00Z\",\"endReason\":\"%s\",\"winnerPlayerIndex\":%d,\"winningTeam\":\"none\",\"placements\":[%s],\"players\":[%s]}",
		gPangeaNetLobbyId,
		gPangeaNetMatchId,
		Nanosaur2ResultModeName(packet->mode),
		trackOrLevel,
		(unsigned) packet->seed,
		Nanosaur2EndReasonName(packet->endReason),
		packet->winnerPlayerIndex,
		placementsJson,
		playersJson);

	gPangeaNetHasMatchResult = 1;
	gPangeaNetLastMatchEndReason = packet->endReason;
	PangeaNet_ReportMatchResult(json);
}

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

EMSCRIPTEN_KEEPALIVE void PangeaGame_SetNetworkMatchConfig(const char* json, int byteCount)
{
	(void)byteCount;
	SDL_Log("PangeaGame_SetNetworkMatchConfig json=%s", json ? json : "(null)");
	gPangeaNetEnabled = 1;
	const int explicitIsHost = ParseJsonInt(json, "\"isHost\"", -1);
	gPangeaNetLocalPlayerIndex = ParseJsonInt(json, "\"localPlayerIndex\"", 0);
	gPangeaNetHostPlayerIndex = ParseJsonInt(json, "\"hostPlayerIndex\"", 0);
	gPangeaNetPlayerCount = SDL_clamp(ParseJsonInt(json, "\"playerCount\"", 2), 1, 2);
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
	const int levelNumber = ParseNanosaur2LevelNumber(json, gLevelNum);

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

	gNumPlayers = (Byte)gPangeaNetPlayerCount;
	gLevelNum = (short)levelNumber;
	gVSMode = (short)ParseNanosaur2Mode(json, levelNumber);
	SDL_Log(
		"PangeaGame_SetNetworkMatchConfig resolved host=%d localPlayer=%d hostPlayer=%d playerCount=%d seed=%u matchId=%u:%u level=%d vsMode=%s(%d)",
		gPangeaNetIsHost,
		gPangeaNetLocalPlayerIndex,
		gPangeaNetHostPlayerIndex,
		gPangeaNetPlayerCount,
		(unsigned)gPangeaNetMatchSeed,
		(unsigned)gPangeaNetMatchIdHigh,
		(unsigned)gPangeaNetMatchIdLow,
		gLevelNum,
		Nanosaur2VSModeName(gVSMode),
		gVSMode);
}

EMSCRIPTEN_KEEPALIVE void PangeaGame_StartNetworkMatch(void)
{
	SDL_Log(
		"PangeaGame_StartNetworkMatch called level=%d vsMode=%s(%d) players=%d host=%d",
		gLevelNum,
		Nanosaur2VSModeName(gVSMode),
		gVSMode,
		gPangeaNetPlayerCount,
		gPangeaNetIsHost);
	gPangeaNetEnabled = 1;
	gPangeaNetLifecycleSequence = 0;
	gPangeaNetLastSentLifecycleReason = 0;
	gPangeaNetRemoteLifecycleReason = 0;
	gPangeaNetRemoteLifecycleSequence = 0;
	gPangeaNetLastMatchEndReason = 0;
	gPangeaNetHasMatchResult = 0;
	gPangeaNetResultPublished = 0;
	gPangeaNetBacklogHead = 0;
	gPangeaNetBacklogTail = 0;
	gPangeaDebugFrameNumber = 0;
	gPangeaDebugLastSyncHash = gPangeaNetMatchSeed;
	gPangeaDebugHasDesync = 0;
}

EMSCRIPTEN_KEEPALIVE int PangeaNet_IsEnabled(void) { return gPangeaNetEnabled; }
EMSCRIPTEN_KEEPALIVE int PangeaNet_IsHost(void) { return gPangeaNetIsHost; }
EMSCRIPTEN_KEEPALIVE int PangeaNet_GetLocalPlayerIndex(void) { return gPangeaNetLocalPlayerIndex; }
EMSCRIPTEN_KEEPALIVE int PangeaNet_GetPlayerCount(void) { return gPangeaNetPlayerCount; }
EMSCRIPTEN_KEEPALIVE int PangeaNet_IsOnlineMatch(void) { return gPangeaNetEnabled; }
EMSCRIPTEN_KEEPALIVE int PangeaNet_IsLocalPlayer(int playerNum)
{
	return gPangeaNetEnabled ? playerNum == gPangeaNetLocalPlayerIndex : 1;
}
EMSCRIPTEN_KEEPALIVE int PangeaNet_IsRemotePlayer(int playerNum)
{
	return gPangeaNetEnabled && playerNum >= 0 && playerNum < gPangeaNetPlayerCount && playerNum != gPangeaNetLocalPlayerIndex;
}
EMSCRIPTEN_KEEPALIVE int PangeaNet_ShouldSimulateGameplayForPlayer(int playerNum)
{
	return !gPangeaNetEnabled || gPangeaNetIsHost || playerNum == gPangeaNetLocalPlayerIndex;
}
EMSCRIPTEN_KEEPALIVE int PangeaNet_ShouldRenderReplicatedPlayer(int playerNum)
{
	return gPangeaNetEnabled && !gPangeaNetIsHost && playerNum != gPangeaNetLocalPlayerIndex;
}
EMSCRIPTEN_KEEPALIVE uint32_t PangeaNet_GetMatchSeed(void) { return gPangeaNetMatchSeed; }
EMSCRIPTEN_KEEPALIVE uint32_t PangeaNet_GetMatchIdLow(void) { return gPangeaNetMatchIdLow; }
EMSCRIPTEN_KEEPALIVE uint32_t PangeaNet_GetMatchIdHigh(void) { return gPangeaNetMatchIdHigh; }
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
	const int queued = PangeaNetBacklogPop(outBytes, maxByteCount);
	if (queued > 0)
	{
		return queued;
	}

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

EMSCRIPTEN_KEEPALIVE int PangeaNet_GetRemoteLifecycleReason(void)
{
	return gPangeaNetRemoteLifecycleReason;
}

EMSCRIPTEN_KEEPALIVE int PangeaNet_GetLastMatchEndReason(void)
{
	return gPangeaNetLastMatchEndReason;
}

EMSCRIPTEN_KEEPALIVE int PangeaNet_HasMatchResult(void)
{
	return gPangeaNetHasMatchResult;
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_ResetNetworkSequenceTracking(void)
{
	gGameplaySequenceTracker = 0;
	gConsecutiveMissingLifecyclePackets = 0;
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_UpdateMatchLifecycle(void)
{
	if (!gPangeaNetEnabled)
	{
		return;
	}

	uint8_t payload[PANGEA_NET_BUFFER_SIZE];
	for (;;)
	{
		const int byteCount = PangeaNet_PollMessage(payload, (int)sizeof(payload));
		if (byteCount <= 0)
		{
			break;
		}

		if (byteCount == (int)sizeof(PangeaNetLifecyclePacket))
		{
			const PangeaNetLifecyclePacket* packet = (const PangeaNetLifecyclePacket*)payload;
			if (packet->magic == PANGEA_NET_LIFECYCLE_MAGIC
				&& packet->version == PANGEA_NET_LIFECYCLE_VERSION
				&& packet->messageType == PANGEA_NET_LIFECYCLE_MATCH_END)
			{
				if (!gPangeaNetIsHost && packet->fromHost != 0)
				{
					if (ValidateLifecycleSequence(packet->sequence, packet->fromHost))
					{
						gPangeaNetRemoteLifecycleSequence = packet->sequence;
						gPangeaNetRemoteLifecycleReason = packet->reason;
						gPangeaNetLastMatchEndReason = packet->reason;
						PangeaNet_ReportMatchEnded(packet->reason);
						SDL_Log(
							"PangeaNet lifecycle received host match-end reason=%d sequence=%u",
							packet->reason,
							(unsigned)packet->sequence);
					}
				}
				continue;
			}
		}
		if (byteCount == (int) sizeof(PangeaNetMatchResultPacket))
		{
			const PangeaNetMatchResultPacket* packet = (const PangeaNetMatchResultPacket*) payload;
			if (packet->magic == PANGEA_NET_LIFECYCLE_MAGIC
				&& packet->version == PANGEA_NET_LIFECYCLE_VERSION
				&& packet->messageType == PANGEA_NET_LIFECYCLE_MATCH_RESULT)
			{
				if (!gPangeaNetIsHost && packet->fromHost != 0)
				{
					if (ValidateLifecycleSequence(packet->sequence, packet->fromHost))
					{
						gPangeaNetRemoteLifecycleSequence = packet->sequence;
						PangeaNetEmitMatchResultJson(packet);
					}
				}
				continue;
			}
		}

		PangeaNetBacklogPush(payload, byteCount);
		break;
	}
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_PublishLocalMatchLifecycle(void)
{
	if (!gPangeaNetEnabled || !gPangeaNetIsHost)
	{
		return;
	}

	int reason = PANGEA_NET_MATCH_STATE_NONE;
	if (gGameOver)
	{
		reason = PANGEA_NET_MATCH_STATE_GAME_OVER;
	}
	else if (gLevelCompleted)
	{
		reason = PANGEA_NET_MATCH_STATE_LEVEL_COMPLETED;
	}

	if (reason == PANGEA_NET_MATCH_STATE_NONE || reason == gPangeaNetLastSentLifecycleReason)
	{
		return;
	}

	PangeaNetLifecyclePacket packet;
	packet.magic = PANGEA_NET_LIFECYCLE_MAGIC;
	packet.version = PANGEA_NET_LIFECYCLE_VERSION;
	packet.messageType = PANGEA_NET_LIFECYCLE_MATCH_END;
	packet.sequence = ++gPangeaNetLifecycleSequence;
	packet.reason = reason;
	packet.fromHost = 1;

	if (JS_PangeaNet_SendReliable(&packet, (int)sizeof(packet)) != 0)
	{
		gPangeaNetLastSentLifecycleReason = reason;
		gPangeaNetLastMatchEndReason = reason;
		SDL_Log(
			"PangeaNet lifecycle sent host match-end reason=%d sequence=%u",
			reason,
			(unsigned)packet.sequence);
		if (!gPangeaNetResultPublished)
		{
			PangeaNetMatchResultPacket resultPacket;
			if (PangeaNetBuildMatchResultPacket(reason, &resultPacket))
			{
				if (JS_PangeaNet_SendReliable(&resultPacket, (int) sizeof(resultPacket)) != 0)
				{
					gPangeaNetResultPublished = 1;
					PangeaNetEmitMatchResultJson(&resultPacket);
				}
			}
		}
		PangeaNet_ReportMatchEnded(reason);
	}
}
EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetFrameNumber(void) { return gPangeaDebugFrameNumber; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetLocalPlayerIndex(void) { return gPangeaNetLocalPlayerIndex; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetPlayerCount(void) { return gPangeaNetPlayerCount; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugIsNetworkMatchRunning(void) { return gPangeaNetEnabled; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugHasDesync(void) { return gPangeaDebugHasDesync; }
EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetLastSyncHash(void) { return gPangeaDebugLastSyncHash; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetLastMatchEndReason(void) { return gPangeaNetLastMatchEndReason; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugHasMatchResult(void) { return gPangeaNetHasMatchResult; }
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
	(void)json;
	(void)byteCount;
}

/****************************/
/*   GAMEPLAY NETWORKING    */
/****************************/

/*
 * Per-frame input commands and authoritative snapshots.
 * Design: clients send their local input to the host each frame; the host
 * runs the authoritative simulation and broadcasts a full-player-state
 * snapshot back every frame.
 */

#define PANGEA_NET_GAMEPLAY_MAGIC 0x54454E50u /* 'PNET' */
#define PANGEA_NET_GAMEPLAY_VERSION 6u
#define PANGEA_NET_GAMEPLAY_PLAYER_NA 0xFFFFu
#define PANGEA_NET_GAMEPLAY_INPUT_TIMEOUT_MS 750.0
#define PANGEA_NET_GAMEPLAY_INTERP_ALPHA 0.32f
#define PANGEA_NET_GAMEPLAY_LOCAL_SNAP_DIST 5.0f
#define PANGEA_NET_GAMEPLAY_REMOTE_SNAP_DIST 120.0f
#define PANGEA_NET_GAMEPLAY_BUFFER_SIZE 4096
#define PANGEA_NET_GAMEPLAY_INPUT_HISTORY_CAP 64
#define PANGEA_NET_GAMEPLAY_REMOTE_BUFFER_CAP 8
#define PANGEA_NET_GAMEPLAY_REMOTE_INTERP_DELAY 2
#define PANGEA_NET_GAMEPLAY_KEYFRAME_INTERVAL 20u
#define PANGEA_NET_GAMEPLAY_KEYFRAME_TIMEOUT_MS 1200.0

enum
{
	kPangeaNetPacketMatchConfig = 1,
	kPangeaNetPacketClientInput = 2,
	kPangeaNetPacketHostSnapshot = 3,
	kPangeaNetPacketReliableEvent = 4,
	kPangeaNetPacketClientAck = 5,
	kPangeaNetPacketPause = 6,
	kPangeaNetPacketResume = 7,
	kPangeaNetPacketDisconnect = 8,
	kPangeaNetPacketProtocolError = 9,
	kPangeaNetPacketKeyframeResendRequest = 10
};

enum
{
	kNS2SnapshotDelta = 0,
	kNS2SnapshotKeyframe = 1,
	kNS2SnapshotCorrection = 2,
	kNS2SnapshotMatchEnd = 3
};

enum
{
	kNS2InputBitFire = 1 << 0,
	kNS2InputBitJetpack = 1 << 1,
	kNS2InputBitNextWeapon = 1 << 2,
	kNS2InputBitPrevWeapon = 1 << 3,
	kNS2InputBitDrop = 1 << 4
};

enum
{
	kNS2ReliableEventRaceComplete = 1,
	kNS2ReliableEventEliminated = 2,
	kNS2ReliableEventPlayerExploded = 3,
	kNS2ReliableEventPlayerDeathDiveImpact = 4,
	kNS2ReliableEventWeaponFired = 5,
	kNS2ReliableEventWeaponHit = 6,
	kNS2ReliableEventJetpackIgnited = 7,
	kNS2ReliableEventJetpackShutoff = 8,
	kNS2ReliableEventShieldHit = 9,
	kNS2ReliableEventWormholeEntered = 10,
	kNS2ReliableEventWormholeExited = 11,
	kNS2ReliableEventDustDevilCaptured = 12,
	kNS2ReliableEventDustDevilReleased = 13,
	kNS2ReliableEventEggPickedUp = 14,
	kNS2ReliableEventEggDropped = 15,
	kNS2ReliableEventEggRetrieved = 16
};

enum
{
	kNS2DeathPhaseAlive = 0,
	kNS2DeathPhaseDeathDive = 1,
	kNS2DeathPhaseExploded = 2,
	kNS2DeathPhaseWaitingToRespawn = 3,
	kNS2DeathPhaseRespawned = 4
};

typedef struct
{
	uint32_t magic;
	uint16_t version;
	uint16_t packetType;
	uint32_t matchIdLow;
	uint32_t matchIdHigh;
	uint32_t tick;
	uint32_t sequence;
	uint16_t playerIndex;
	uint16_t reserved;
} PangeaNetGameplayHeader;

typedef struct
{
	float coordX, coordY, coordZ;
	float rotX, rotY, rotZ;
	float deltaX, deltaY, deltaZ;
	float health;
	float shieldPower;
	float jetpackFuel;
	float weaponCharge;
	uint8_t jetpackActive;
	uint8_t currentWeapon;
	uint8_t place;
	uint8_t raceComplete;
	uint8_t deathPhase;
	uint8_t shieldVisible;
	uint8_t hiddenState;
	uint16_t lapNum;
	uint16_t raceCheckpointNum;
	uint16_t weaponQuantity0;
	uint16_t weaponQuantity1;
	uint16_t weaponQuantity2;
	float alpha;
	float distToNextCheckpoint;
	float invincibilityTimer;
	float deathTimer;
	float currentAnimTime;
	uint32_t checkpointBits[4];
	uint32_t lastProcessedInputSequence;
	uint16_t numFreeLives;
	uint8_t wrongWay;
	uint8_t movingBackwards;
	uint8_t currentAnimNum;
	uint8_t reserved0;
} NS2SnapshotPlayerState;

typedef struct
{
	uint32_t sequence;
	float analogX;
	float analogZ;
	uint32_t bitsHeld;
	uint32_t bitsNew;
} NS2InputHistoryEntry;

typedef struct
{
	uint8_t* bytes;
	int byteCount;
	int cursor;
	int ok;
} NS2Writer;

typedef struct
{
	const uint8_t* bytes;
	int byteCount;
	int cursor;
	int ok;
} NS2Reader;

typedef struct
{
	uint32_t lastInputTick;
	uint32_t lastAckTick;
	uint32_t lastSentSequence;
	uint32_t lastReceivedSequence;
	float rttMs;
	float jitterMs;
	uint32_t inputBufferLength;
	uint8_t timeoutState;
	double lastInputTimeMs;
} NS2PlayerConnState;

static float gNetInputAnalogX[2] = {0.0f, 0.0f};
static float gNetInputAnalogZ[2] = {0.0f, 0.0f};
static uint32_t gNetInputBitsHeld[2] = {0u, 0u};
static uint32_t gNetInputBitsNew[2] = {0u, 0u};
static NS2PlayerConnState gNS2ConnState[2];
static NS2SnapshotPlayerState gNS2PendingSnapshotPlayers[2];
static int gNS2HavePendingSnapshot = 0;
static uint32_t gNS2PendingSnapshotTick = 0;
static uint32_t gNS2PendingSnapshotSequence = 0;
static uint8_t gNS2PendingSnapshotPlayerCount = 0;
static uint32_t gNS2MatchId = 1;
static uint32_t gNS2MatchIdHigh = 0;
static uint32_t gNS2Tick = 0;
static uint32_t gNS2Sequence = 1;
static uint32_t gNS2LastDroppedPackets = 0;
static uint32_t gNS2LastReorderedPackets = 0;
static float gNS2LastCorrectionDistance = 0.0f;
static uint32_t gNS2LastHostStateHash = 0;
static uint32_t gNS2LastHostKeyframeSeq = 0;
static uint32_t gNS2LastHostDeltaSeq = 0;
static uint8_t gNS2ForceKeyframe = 0;
static double gNS2LastReceivedKeyframeAtMs = 0.0;
static uint8_t gNS2PendingSnapshotKind = kNS2SnapshotDelta;
static uint32_t gNS2InitSeed = 0;
static uint32_t gNS2InitMatchId = 0;
static uint32_t gNS2InitMatchIdHigh = 0;
static int gNS2InitLocalPlayerIndex = -1;
static int gNS2InitIsHost = -1;
static NS2InputHistoryEntry gNS2InputHistory[PANGEA_NET_GAMEPLAY_INPUT_HISTORY_CAP];
static uint32_t gNS2InputHistoryHead = 0;
static uint32_t gNS2InputHistoryCount = 0;
static NS2SnapshotPlayerState gNS2RemoteSnapshotBuffer[2][PANGEA_NET_GAMEPLAY_REMOTE_BUFFER_CAP];
static uint32_t gNS2RemoteSnapshotHead[2] = {0u, 0u};
static uint32_t gNS2RemoteSnapshotCount[2] = {0u, 0u};
static uint8_t gNS2ReliableRaceCompleteSent[2] = {0u, 0u};
static uint8_t gNS2ReliableEliminatedSent[2] = {0u, 0u};
static uint32_t gNS2LastVisualEventSequence = 0;
static uint32_t gNS2LastAppliedVisualEventSequence = 0;
static uint32_t gNS2DuplicateVisualEventCount = 0;
static uint32_t gNS2StaleVisualEventCount = 0;

extern short gNumEggs;
extern ObjNode *gEggObjs[];

#define NS2_NET_MAX_EGGS MAX_NET_EGGS

static uint8_t gNS2PendingEggCount = 0;
static uint8_t gNS2PendingEggState[NS2_NET_MAX_EGGS];
static uint8_t gNS2PendingEggCarrier[NS2_NET_MAX_EGGS];
static float gNS2PendingEggCoordX[NS2_NET_MAX_EGGS];
static float gNS2PendingEggCoordY[NS2_NET_MAX_EGGS];
static float gNS2PendingEggCoordZ[NS2_NET_MAX_EGGS];
static Byte gNS2PendingNumEggsSaved[NUM_EGG_TYPES];
static Boolean gNS2HavePendingEggState = false;

EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugForcePlayerDeath(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= gPangeaNetPlayerCount || playerIndex >= 2)
	{
		return 0;
	}
	if (gPangeaNetEnabled && !gPangeaNetIsHost && playerIndex != gPangeaNetLocalPlayerIndex)
	{
		return 0;
	}

	ObjNode* player = gPlayerInfo[playerIndex].objNode;
	if (!player)
	{
		return 0;
	}

	KillPlayer((short)playerIndex, PLAYER_DEATH_TYPE_EXPLODE, &player->Coord);
	if (gPangeaNetEnabled && gPangeaNetIsHost)
	{
		gNS2ForceKeyframe = 1;
	}
	return 1;
}

static uint32_t gNS2LastRemoteExplosionTick[2] = {0u, 0u};

static void NS2DebugLogEarlyNetPhase(const char* phase)
{
	if (!gPangeaNetEnabled || gNS2Tick > 12)
	{
		return;
	}
	SDL_Log(
		"Nanosaur2 pnet tick %u phase %s host=%d local=%d match=%u:%u pending=%d",
		(unsigned)gNS2Tick,
		phase,
		gPangeaNetIsHost,
		gPangeaNetLocalPlayerIndex,
		(unsigned)gNS2MatchIdHigh,
		(unsigned)gNS2MatchId,
		gNS2HavePendingSnapshot);
}

static void NS2RecordLocalInput(uint32_t sequence, int playerIndex, float analogX, float analogZ, uint32_t bitsHeld, uint32_t bitsNew)
{
	if (playerIndex < 0 || playerIndex >= 2)
	{
		return;
	}
	uint32_t slot = (gNS2InputHistoryHead + gNS2InputHistoryCount) % PANGEA_NET_GAMEPLAY_INPUT_HISTORY_CAP;
	if (gNS2InputHistoryCount >= PANGEA_NET_GAMEPLAY_INPUT_HISTORY_CAP)
	{
		gNS2InputHistoryHead = (gNS2InputHistoryHead + 1) % PANGEA_NET_GAMEPLAY_INPUT_HISTORY_CAP;
		slot = (gNS2InputHistoryHead + gNS2InputHistoryCount - 1) % PANGEA_NET_GAMEPLAY_INPUT_HISTORY_CAP;
	}
	else
	{
		gNS2InputHistoryCount++;
	}
	gNS2InputHistory[slot].sequence = sequence;
	gNS2InputHistory[slot].analogX = analogX;
	gNS2InputHistory[slot].analogZ = analogZ;
	gNS2InputHistory[slot].bitsHeld = bitsHeld;
	gNS2InputHistory[slot].bitsNew = bitsNew;
}

static void NS2ReapplyUnackedLocalInput(uint32_t lastProcessedInputSequence, int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= 2)
	{
		return;
	}
	for (uint32_t i = 0; i < gNS2InputHistoryCount; i++)
	{
		const uint32_t slot = (gNS2InputHistoryHead + i) % PANGEA_NET_GAMEPLAY_INPUT_HISTORY_CAP;
		const NS2InputHistoryEntry* entry = &gNS2InputHistory[slot];
		if (entry->sequence <= lastProcessedInputSequence)
		{
			continue;
		}
		gPlayerInfo[playerIndex].analogControlX = entry->analogX;
		gPlayerInfo[playerIndex].analogControlZ = entry->analogZ;
	}
}

static void NS2PackCheckpointBits(int playerIndex, uint32_t outBits[4])
{
	if (playerIndex < 0 || playerIndex >= 2 || !outBits)
	{
		return;
	}
	for (int word = 0; word < 4; word++)
	{
		outBits[word] = 0u;
	}
	for (int marker = 0; marker < MAX_LINEMARKERS; marker++)
	{
		if (gPlayerInfo[playerIndex].raceCheckpointTagged[marker])
		{
			outBits[marker / 32] |= 1u << (marker % 32);
		}
	}
}

static void NS2UnpackCheckpointBits(const uint32_t checkpointBits[4], int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= 2 || !checkpointBits)
	{
		return;
	}
	for (int marker = 0; marker < MAX_LINEMARKERS; marker++)
	{
		gPlayerInfo[playerIndex].raceCheckpointTagged[marker] =
			(checkpointBits[marker / 32] & (1u << (marker % 32))) != 0u;
	}
}

static uint8_t NS2GetPlayerDeathPhase(int playerIndex, const ObjNode* obj)
{
	if (playerIndex < 0 || playerIndex >= 2)
	{
		return kNS2DeathPhaseAlive;
	}
	if (!gPlayerIsDead[playerIndex])
	{
		if (obj && obj->Skeleton && obj->Skeleton->AnimNum == PLAYER_ANIM_APPEARWORMHOLE)
		{
			return kNS2DeathPhaseRespawned;
		}
		return kNS2DeathPhaseAlive;
	}
	if (obj && obj->Skeleton && obj->Skeleton->AnimNum == PLAYER_ANIM_DEATHDIVE)
	{
		return kNS2DeathPhaseDeathDive;
	}
	if (gDeathTimer[playerIndex] > 0.0f)
	{
		return kNS2DeathPhaseWaitingToRespawn;
	}
	return kNS2DeathPhaseExploded;
}

static void NS2ApplySnapshotDeathState(int playerIndex, ObjNode* obj, const NS2SnapshotPlayerState* snapshot)
{
	if (playerIndex < 0 || playerIndex >= 2 || !snapshot)
	{
		return;
	}

	const uint8_t deathPhase = snapshot->deathPhase;
	gPlayerIsDead[playerIndex] = deathPhase != kNS2DeathPhaseAlive && deathPhase != kNS2DeathPhaseRespawned;
	gCameraInDeathDiveMode[playerIndex] = deathPhase == kNS2DeathPhaseDeathDive;
	gDeathTimer[playerIndex] = snapshot->deathTimer;

	switch (deathPhase)
	{
		case kNS2DeathPhaseAlive:
		case kNS2DeathPhaseRespawned:
			if (gDeathTimer[playerIndex] < 0.0f)
			{
				gDeathTimer[playerIndex] = 0.0f;
			}
			break;

		case kNS2DeathPhaseDeathDive:
		case kNS2DeathPhaseWaitingToRespawn:
			if (gDeathTimer[playerIndex] <= 0.0f)
			{
				gDeathTimer[playerIndex] = 1.0f;
			}
			break;

		case kNS2DeathPhaseExploded:
			gDeathTimer[playerIndex] = -1.0f;
			break;

		default:
			break;
	}

	if (!obj)
	{
		return;
	}

	if (snapshot->hiddenState != 0)
	{
		HidePlayer(obj);
	}
	else
	{
		ShowPlayer(obj);
	}
}

static void NS2SetPlayerAlpha(ObjNode* obj, float alpha)
{
	if (!obj)
	{
		return;
	}
	if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}
	if (alpha > 1.0f)
	{
		alpha = 1.0f;
	}
	for (ObjNode* cursor = obj; cursor != NULL; cursor = cursor->ChainNode)
	{
		cursor->ColorFilter.a = alpha;
	}
}

static void NS2SetShieldVisibility(int playerIndex, int visible)
{
	if (playerIndex < 0 || playerIndex >= 2)
	{
		return;
	}
	if (visible && gPlayerInfo[playerIndex].shieldPower > 0.0f && gPlayerInfo[playerIndex].shieldObj == NULL)
	{
		CreatePlayerShield(playerIndex);
	}
	ObjNode* shield = gPlayerInfo[playerIndex].shieldObj;
	if (!shield)
	{
		return;
	}
	if (visible)
	{
		shield->StatusBits &= ~STATUS_BIT_HIDDEN;
	}
	else
	{
		shield->StatusBits |= STATUS_BIT_HIDDEN;
	}
}

static int NS2CanEmitVisualEvent(short playerNum)
{
	return gPangeaNetEnabled
		&& gPangeaNetIsHost
		&& playerNum >= 0
		&& playerNum < gPangeaNetPlayerCount
		&& playerNum < 2;
}

static int NS2ShouldTreatVisualEventAsStale(uint8_t eventType, int playerIndex, uint32_t tick)
{
	if (playerIndex < 0 || playerIndex >= 2)
	{
		return 1;
	}
	const ObjNode* obj = gPlayerInfo[playerIndex].objNode;
	const uint8_t currentDeathPhase = NS2GetPlayerDeathPhase(playerIndex, obj);
	const bool playerAliveNow = currentDeathPhase == kNS2DeathPhaseAlive || currentDeathPhase == kNS2DeathPhaseRespawned;

	switch (eventType)
	{
		case kNS2ReliableEventPlayerExploded:
		case kNS2ReliableEventPlayerDeathDiveImpact:
			return playerAliveNow ? 1 : 0;

		case kNS2ReliableEventWeaponFired:
		case kNS2ReliableEventWeaponHit:
		case kNS2ReliableEventJetpackIgnited:
		case kNS2ReliableEventJetpackShutoff:
		case kNS2ReliableEventShieldHit:
			if (!playerAliveNow)
			{
				return 1;
			}
			if (gNS2LastRemoteExplosionTick[playerIndex] != 0u && tick < gNS2LastRemoteExplosionTick[playerIndex])
			{
				return 1;
			}
			return 0;

		default:
			return 0;
	}
}

static bool NS2RemoteAnimIsEventDriven(const ObjNode* obj)
{
	if (!obj || !obj->Skeleton)
	{
		return false;
	}

	switch (obj->Skeleton->AnimNum)
	{
		case PLAYER_ANIM_APPEARWORMHOLE:
		case PLAYER_ANIM_ENTERWORMHOLE:
		case PLAYER_ANIM_DUSTDEVIL:
			return true;

		default:
			return false;
	}
}

static short NS2DeriveRemoteFlightAnim(ObjNode* obj)
{
	if (!obj || !obj->Skeleton)
	{
		return PLAYER_ANIM_FLAP;
	}

	if (obj->Rot.z < (-PI / 7))
	{
		return PLAYER_ANIM_BANKRIGHT;
	}

	if (obj->Rot.z > (PI / 7))
	{
		return PLAYER_ANIM_BANKLEFT;
	}

	switch (obj->Skeleton->AnimNum)
	{
		case PLAYER_ANIM_FLAP:
			obj->SpecialF[3] -= gFramesPerSecondFrac;
			if (obj->SpecialF[3] <= 0.0f)
			{
				obj->SpecialF[3] = 2.0f + RandomFloat() * 3.0f;
				return PLAYER_ANIM_COASTING;
			}
			return PLAYER_ANIM_FLAP;

		case PLAYER_ANIM_COASTING:
			obj->SpecialF[3] -= gFramesPerSecondFrac;
			if (obj->SpecialF[3] <= 0.0f)
			{
				obj->SpecialF[3] = 1.0f + RandomFloat() * 3.0f;
				return PLAYER_ANIM_FLAP;
			}
			return PLAYER_ANIM_COASTING;

		default:
			obj->SpecialF[3] = 1.0f + RandomFloat() * 3.0f;
			return PLAYER_ANIM_FLAP;
	}
}

static void NS2ApplyRemotePlayerAnimation(int playerIndex, ObjNode* obj, const NS2SnapshotPlayerState* snapshot)
{
	if (playerIndex < 0 || playerIndex >= 2 || !obj || !snapshot || !obj->Skeleton)
	{
		return;
	}

	const uint8_t deathPhase = snapshot->deathPhase;
	const uint8_t currentAnimNum = snapshot->currentAnimNum;
	if (currentAnimNum <= PLAYER_ANIM_COASTING)
	{
		if (obj->Skeleton->AnimNum != currentAnimNum)
		{
			SetSkeletonAnim(obj->Skeleton, currentAnimNum);
		}
		obj->Skeleton->CurrentAnimTime = snapshot->currentAnimTime;
	}
	else if (deathPhase == kNS2DeathPhaseDeathDive)
	{
		if (obj->Skeleton->AnimNum != PLAYER_ANIM_DEATHDIVE)
		{
			SetSkeletonAnim(obj->Skeleton, PLAYER_ANIM_DEATHDIVE);
		}
	}
	else if (!NS2RemoteAnimIsEventDriven(obj))
	{
		const short desiredAnim = NS2DeriveRemoteFlightAnim(obj);
		if (obj->Skeleton->AnimNum != desiredAnim)
		{
			MorphToSkeletonAnim(obj->Skeleton, desiredAnim, 3.0f);
		}
	}

}

static void NS2RestoreRemotePlayerRenderState(int playerIndex, ObjNode* obj, const NS2SnapshotPlayerState* snapshot)
{
	if (playerIndex < 0 || playerIndex >= 2 || !obj || !snapshot)
	{
		return;
	}
	if (snapshot->hiddenState != 0)
	{
		HidePlayer(obj);
		NS2SetShieldVisibility(playerIndex, 0);
		return;
	}

	obj->Health = snapshot->health;
	obj->CType = CTYPE_PLAYER1 << playerIndex;
	ShowPlayer(obj);
	NS2SetPlayerAlpha(obj, snapshot->alpha);
	NS2SetShieldVisibility(playerIndex, snapshot->shieldVisible != 0);
	UpdateObjectTransforms(obj);
	CalcObjectBoxFromNode(obj);
	UpdateShadow(obj);
}

static void NS2PushRemoteSnapshot(int playerIndex, const NS2SnapshotPlayerState* snapshot)
{
	if (playerIndex < 0 || playerIndex >= 2 || !snapshot)
	{
		return;
	}
	uint32_t slot = (gNS2RemoteSnapshotHead[playerIndex] + gNS2RemoteSnapshotCount[playerIndex]) % PANGEA_NET_GAMEPLAY_REMOTE_BUFFER_CAP;
	if (gNS2RemoteSnapshotCount[playerIndex] >= PANGEA_NET_GAMEPLAY_REMOTE_BUFFER_CAP)
	{
		gNS2RemoteSnapshotHead[playerIndex] = (gNS2RemoteSnapshotHead[playerIndex] + 1) % PANGEA_NET_GAMEPLAY_REMOTE_BUFFER_CAP;
		slot = (gNS2RemoteSnapshotHead[playerIndex] + gNS2RemoteSnapshotCount[playerIndex] - 1) % PANGEA_NET_GAMEPLAY_REMOTE_BUFFER_CAP;
	}
	else
	{
		gNS2RemoteSnapshotCount[playerIndex]++;
	}
	gNS2RemoteSnapshotBuffer[playerIndex][slot] = *snapshot;
}

static int NS2GetDelayedRemoteSnapshot(int playerIndex, NS2SnapshotPlayerState* outSnapshot)
{
	if (playerIndex < 0 || playerIndex >= 2 || !outSnapshot)
	{
		return 0;
	}
	const uint32_t count = gNS2RemoteSnapshotCount[playerIndex];
	if (count == 0)
	{
		return 0;
	}
	const uint32_t delayedOffset = count > PANGEA_NET_GAMEPLAY_REMOTE_INTERP_DELAY
		? (count - 1 - PANGEA_NET_GAMEPLAY_REMOTE_INTERP_DELAY)
		: (count - 1);
	const uint32_t slot = (gNS2RemoteSnapshotHead[playerIndex] + delayedOffset) % PANGEA_NET_GAMEPLAY_REMOTE_BUFFER_CAP;
	*outSnapshot = gNS2RemoteSnapshotBuffer[playerIndex][slot];
	return 1;
}

static void NS2EnsureMatchStateInitialized(void)
{
	const uint32_t seed = gPangeaNetMatchSeed;
	if (gNS2InitSeed == seed
		&& gNS2InitMatchId == gPangeaNetMatchIdLow
		&& gNS2InitMatchIdHigh == gPangeaNetMatchIdHigh
		&& gNS2InitLocalPlayerIndex == gPangeaNetLocalPlayerIndex
		&& gNS2InitIsHost == gPangeaNetIsHost)
	{
		return;
	}

	gNS2InitSeed = seed;
	gNS2InitMatchId = gPangeaNetMatchIdLow;
	gNS2InitMatchIdHigh = gPangeaNetMatchIdHigh;
	gNS2InitLocalPlayerIndex = gPangeaNetLocalPlayerIndex;
	gNS2InitIsHost = gPangeaNetIsHost;
	gNS2MatchId = gPangeaNetMatchIdLow;
	gNS2MatchIdHigh = gPangeaNetMatchIdHigh;
	gNS2Tick = 0;
	gNS2Sequence = 1;
	gNS2PendingSnapshotTick = 0;
	gNS2PendingSnapshotSequence = 0;
	gNS2PendingSnapshotPlayerCount = 0;
	gNS2HavePendingSnapshot = 0;
	gNS2LastDroppedPackets = 0;
	gNS2LastReorderedPackets = 0;
	gNS2LastCorrectionDistance = 0.0f;
	gNS2LastHostStateHash = 0;
	gNS2LastHostKeyframeSeq = 0;
	gNS2LastHostDeltaSeq = 0;
	gNS2ForceKeyframe = 0;
	gNS2LastReceivedKeyframeAtMs = 0.0;
	gNS2PendingSnapshotKind = kNS2SnapshotDelta;
	gNS2InputHistoryHead = 0;
	gNS2InputHistoryCount = 0;
	gNS2RemoteSnapshotHead[0] = 0;
	gNS2RemoteSnapshotHead[1] = 0;
	gNS2RemoteSnapshotCount[0] = 0;
	gNS2RemoteSnapshotCount[1] = 0;
	gNS2ReliableRaceCompleteSent[0] = 0;
	gNS2ReliableRaceCompleteSent[1] = 0;
	gNS2ReliableEliminatedSent[0] = 0;
	gNS2ReliableEliminatedSent[1] = 0;
	gNS2LastVisualEventSequence = 0;
	gNS2LastAppliedVisualEventSequence = 0;
	gNS2DuplicateVisualEventCount = 0;
	gNS2StaleVisualEventCount = 0;
	gNS2LastRemoteExplosionTick[0] = 0;
	gNS2LastRemoteExplosionTick[1] = 0;
	for (int i = 0; i < 2; i++)
	{
		gNetInputAnalogX[i] = 0.0f;
		gNetInputAnalogZ[i] = 0.0f;
		gNetInputBitsHeld[i] = 0u;
		gNetInputBitsNew[i] = 0u;
		gNS2ConnState[i].lastInputTick = 0;
		gNS2ConnState[i].lastAckTick = 0;
		gNS2ConnState[i].lastSentSequence = 0;
		gNS2ConnState[i].lastReceivedSequence = 0;
		gNS2ConnState[i].rttMs = 0.0f;
		gNS2ConnState[i].jitterMs = 0.0f;
		gNS2ConnState[i].inputBufferLength = 0;
		gNS2ConnState[i].timeoutState = 0;
		gNS2ConnState[i].lastInputTimeMs = 0.0;
	}
}

static void NS2Writer_Init(NS2Writer* writer, void* bytes, int byteCount)
{
	writer->bytes = (uint8_t*)bytes;
	writer->byteCount = byteCount;
	writer->cursor = 0;
	writer->ok = 1;
}

static void NS2Reader_Init(NS2Reader* reader, const void* bytes, int byteCount)
{
	reader->bytes = (const uint8_t*)bytes;
	reader->byteCount = byteCount;
	reader->cursor = 0;
	reader->ok = 1;
}

static void NS2Writer_U8(NS2Writer* writer, uint8_t value)
{
	if (!writer->ok || writer->cursor + 1 > writer->byteCount)
	{
		writer->ok = 0;
		return;
	}
	writer->bytes[writer->cursor++] = value;
}

static void NS2Writer_U16(NS2Writer* writer, uint16_t value)
{
	NS2Writer_U8(writer, (uint8_t)(value & 0xFFu));
	NS2Writer_U8(writer, (uint8_t)((value >> 8) & 0xFFu));
}

static void NS2Writer_U32(NS2Writer* writer, uint32_t value)
{
	NS2Writer_U8(writer, (uint8_t)(value & 0xFFu));
	NS2Writer_U8(writer, (uint8_t)((value >> 8) & 0xFFu));
	NS2Writer_U8(writer, (uint8_t)((value >> 16) & 0xFFu));
	NS2Writer_U8(writer, (uint8_t)((value >> 24) & 0xFFu));
}

static void NS2Writer_F32(NS2Writer* writer, float value)
{
	uint32_t bits = 0;
	SDL_memcpy(&bits, &value, sizeof(bits));
	NS2Writer_U32(writer, bits);
}

static uint8_t NS2Reader_U8(NS2Reader* reader)
{
	if (!reader->ok || reader->cursor + 1 > reader->byteCount)
	{
		reader->ok = 0;
		return 0;
	}
	return reader->bytes[reader->cursor++];
}

static uint16_t NS2Reader_U16(NS2Reader* reader)
{
	const uint16_t lo = NS2Reader_U8(reader);
	const uint16_t hi = NS2Reader_U8(reader);
	return (uint16_t)(lo | (hi << 8));
}

static uint32_t NS2Reader_U32(NS2Reader* reader)
{
	const uint32_t b0 = NS2Reader_U8(reader);
	const uint32_t b1 = NS2Reader_U8(reader);
	const uint32_t b2 = NS2Reader_U8(reader);
	const uint32_t b3 = NS2Reader_U8(reader);
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

static float NS2Reader_F32(NS2Reader* reader)
{
	const uint32_t bits = NS2Reader_U32(reader);
	float value = 0.0f;
	SDL_memcpy(&value, &bits, sizeof(value));
	return value;
}

static int NS2WriteHeader(NS2Writer* writer, const PangeaNetGameplayHeader* header)
{
	NS2Writer_U32(writer, header->magic);
	NS2Writer_U16(writer, header->version);
	NS2Writer_U16(writer, header->packetType);
	NS2Writer_U32(writer, header->matchIdLow);
	NS2Writer_U32(writer, header->matchIdHigh);
	NS2Writer_U32(writer, header->tick);
	NS2Writer_U32(writer, header->sequence);
	NS2Writer_U16(writer, header->playerIndex);
	NS2Writer_U16(writer, header->reserved);
	return writer->ok;
}

static int NS2ReadHeader(NS2Reader* reader, PangeaNetGameplayHeader* outHeader)
{
	outHeader->magic = NS2Reader_U32(reader);
	outHeader->version = NS2Reader_U16(reader);
	outHeader->packetType = NS2Reader_U16(reader);
	outHeader->matchIdLow = NS2Reader_U32(reader);
	outHeader->matchIdHigh = NS2Reader_U32(reader);
	outHeader->tick = NS2Reader_U32(reader);
	outHeader->sequence = NS2Reader_U32(reader);
	outHeader->playerIndex = NS2Reader_U16(reader);
	outHeader->reserved = NS2Reader_U16(reader);
	return reader->ok;
}

static uint32_t NS2ComputeAuthoritativeStateHash(uint8_t playerCount)
{
	uint32_t hash = 2166136261u;
	for (int i = 0; i < playerCount && i < 2; i++)
	{
		const ObjNode* obj = gPlayerInfo[i].objNode;
		const SkeletonObjDataType* skeleton = obj ? obj->Skeleton : NULL;
		const float coordX = obj ? obj->Coord.x : 0.0f;
		const float coordY = obj ? obj->Coord.y : 0.0f;
		const float coordZ = obj ? obj->Coord.z : 0.0f;
		const float health = gPlayerInfo[i].health;
		const uint32_t packedX = (uint32_t)(coordX * 10.0f);
		const uint32_t packedY = (uint32_t)(coordY * 10.0f);
		const uint32_t packedZ = (uint32_t)(coordZ * 10.0f);
		const uint32_t packedHealth = (uint32_t)(health * 100.0f);
		hash ^= packedX + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= packedY + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= packedZ + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= packedHealth + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)gPlayerInfo[i].lapNum + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)gPlayerInfo[i].raceCheckpointNum + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)gPlayerInfo[i].place + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)gPlayerInfo[i].numFreeLives + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)(gPlayerInfo[i].wrongWay ? 1 : 0) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)(gPlayerInfo[i].movingBackwards ? 1 : 0) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)(gPlayerInfo[i].distToNextCheckpoint * 10.0f) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)(gPlayerInfo[i].invincibilityTimer * 10.0f) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= (uint32_t)(skeleton ? skeleton->AnimNum : 0) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		uint32_t checkpointBits[4];
		NS2PackCheckpointBits(i, checkpointBits);
		for (int word = 0; word < 4; word++)
		{
			hash ^= checkpointBits[word] + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		}
		hash ^= gPlayerInfo[i].raceComplete ? 0xA5A5A5A5u : 0x5A5A5A5Au;
	}
	{
		Byte eggState[NS2_NET_MAX_EGGS];
		Byte eggCarrier[NS2_NET_MAX_EGGS];
		float eggX[NS2_NET_MAX_EGGS], eggY[NS2_NET_MAX_EGGS], eggZ[NS2_NET_MAX_EGGS];
		const int eggCount = PangeaNet_GetEggSnapshotData(eggState, eggCarrier, eggX, eggY, eggZ, NS2_NET_MAX_EGGS);
		for (int ei = 0; ei < eggCount; ei++)
		{
			hash ^= (uint32_t)eggState[ei] + 0x9e3779b9u + (hash << 6) + (hash >> 2);
			hash ^= (uint32_t)eggCarrier[ei] + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		}
	}
	for (int k = 0; k < NUM_EGG_TYPES; k++)
	{
		hash ^= (uint32_t)gNumEggsSaved[k] + 0x9e3779b9u + (hash << 6) + (hash >> 2);
	}
	return hash;
}

static void NS2SendReliableEvent(uint8_t eventType, int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= 2)
	{
		return;
	}
	uint8_t packet[PANGEA_NET_GAMEPLAY_BUFFER_SIZE];
	NS2Writer writer;
	PangeaNetGameplayHeader header;
	header.magic = PANGEA_NET_GAMEPLAY_MAGIC;
	header.version = PANGEA_NET_GAMEPLAY_VERSION;
	header.packetType = kPangeaNetPacketReliableEvent;
	header.matchIdLow = gNS2MatchId;
	header.matchIdHigh = gNS2MatchIdHigh;
	header.tick = gNS2Tick;
	header.sequence = gNS2Sequence++;
	header.playerIndex = PANGEA_NET_GAMEPLAY_PLAYER_NA;
	header.reserved = 0;
	NS2Writer_Init(&writer, packet, (int)sizeof(packet));
	NS2WriteHeader(&writer, &header);
	NS2Writer_U8(&writer, eventType);
	NS2Writer_U8(&writer, (uint8_t)playerIndex);
	NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[playerIndex].lapNum);
	NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[playerIndex].place);
	NS2Writer_U8(&writer, gPlayerInfo[playerIndex].raceComplete ? 1 : 0);
	NS2Writer_U8(&writer, gPlayerInfo[playerIndex].health <= 0.0f ? 1 : 0);
	if (!writer.ok)
	{
		return;
	}
	JS_PangeaNet_SendReliable(packet, writer.cursor);
}

static void NS2SendVisualEvent(uint8_t eventType, short playerNum, short aux0, short aux1, const OGLPoint3D* where)
{
	if (!NS2CanEmitVisualEvent(playerNum))
	{
		return;
	}
	const ObjNode* obj = gPlayerInfo[playerNum].objNode;
	const OGLPoint3D zeroPoint = {0.0f, 0.0f, 0.0f};
	const OGLPoint3D eventPoint = where ? *where : (obj ? obj->Coord : zeroPoint);
	uint8_t packet[PANGEA_NET_GAMEPLAY_BUFFER_SIZE];
	NS2Writer writer;
	PangeaNetGameplayHeader header;
	header.magic = PANGEA_NET_GAMEPLAY_MAGIC;
	header.version = PANGEA_NET_GAMEPLAY_VERSION;
	header.packetType = kPangeaNetPacketReliableEvent;
	header.matchIdLow = gNS2MatchId;
	header.matchIdHigh = gNS2MatchIdHigh;
	header.tick = gNS2Tick;
	header.sequence = gNS2Sequence++;
	header.playerIndex = PANGEA_NET_GAMEPLAY_PLAYER_NA;
	header.reserved = 0;
	NS2Writer_Init(&writer, packet, (int)sizeof(packet));
	NS2WriteHeader(&writer, &header);
	NS2Writer_U8(&writer, eventType);
	NS2Writer_U8(&writer, (uint8_t)playerNum);
	NS2Writer_F32(&writer, eventPoint.x);
	NS2Writer_F32(&writer, eventPoint.y);
	NS2Writer_F32(&writer, eventPoint.z);
	NS2Writer_U8(&writer, (uint8_t)aux0);
	NS2Writer_U8(&writer, (uint8_t)aux1);
	NS2Writer_U8(&writer, NS2GetPlayerDeathPhase(playerNum, obj));
	if (!writer.ok)
	{
		return;
	}
	gNS2LastVisualEventSequence = header.sequence;
	JS_PangeaNet_SendReliable(packet, writer.cursor);
}

static void NS2SendEggEvent(uint8_t eventType, uint8_t eggIndex, uint8_t playerNum, uint8_t kind)
{
	uint8_t packet[PANGEA_NET_GAMEPLAY_BUFFER_SIZE];
	NS2Writer writer;
	PangeaNetGameplayHeader header;
	header.magic = PANGEA_NET_GAMEPLAY_MAGIC;
	header.version = PANGEA_NET_GAMEPLAY_VERSION;
	header.packetType = kPangeaNetPacketReliableEvent;
	header.matchIdLow = gNS2MatchId;
	header.matchIdHigh = gNS2MatchIdHigh;
	header.tick = gNS2Tick;
	header.sequence = gNS2Sequence++;
	header.playerIndex = PANGEA_NET_GAMEPLAY_PLAYER_NA;
	header.reserved = 0;
	NS2Writer_Init(&writer, packet, (int)sizeof(packet));
	NS2WriteHeader(&writer, &header);
	NS2Writer_U8(&writer, eventType);
	NS2Writer_U8(&writer, eggIndex);
	NS2Writer_U8(&writer, playerNum);
	NS2Writer_U8(&writer, kind);
	if (!writer.ok)
	{
		return;
	}
	JS_PangeaNet_SendReliable(packet, writer.cursor);
}

void PangeaNet_SendEggPickedUp(int eggIndex, int playerNum)
{
	NS2SendEggEvent(kNS2ReliableEventEggPickedUp, (uint8_t)eggIndex, (uint8_t)playerNum, 0);
}

void PangeaNet_SendEggDropped(int eggIndex, int playerNum)
{
	NS2SendEggEvent(kNS2ReliableEventEggDropped, (uint8_t)eggIndex, (uint8_t)playerNum, 0);
}

void PangeaNet_SendEggRetrieved(int eggIndex, int kind)
{
	NS2SendEggEvent(kNS2ReliableEventEggRetrieved, (uint8_t)eggIndex, 0, (uint8_t)kind);
}

static void NS2PumpGameplayMessages(void)
{
	NS2EnsureMatchStateInitialized();
	NS2DebugLogEarlyNetPhase("pump-begin");
	uint8_t payload[PANGEA_NET_GAMEPLAY_BUFFER_SIZE];
	for (;;)
	{
		const int byteCount = PangeaNet_PollMessage(payload, (int)sizeof(payload));
		if (byteCount <= 0)
		{
			NS2DebugLogEarlyNetPhase("pump-empty");
			break;
		}

		NS2Reader reader;
		PangeaNetGameplayHeader header;
		NS2Reader_Init(&reader, payload, byteCount);
		if (!NS2ReadHeader(&reader, &header))
		{
			NS2DebugLogEarlyNetPhase("pump-bad-header");
			continue;
		}

		if (header.magic != PANGEA_NET_GAMEPLAY_MAGIC || header.version != PANGEA_NET_GAMEPLAY_VERSION)
		{
			NS2DebugLogEarlyNetPhase("pump-backlog-non-gameplay");
			PangeaNetBacklogPush(payload, byteCount);
			break;
		}

		if (header.matchIdLow != gNS2MatchId || header.matchIdHigh != gNS2MatchIdHigh)
		{
			NS2DebugLogEarlyNetPhase("pump-drop-match-id");
			gNS2LastDroppedPackets++;
			continue;
		}

		if (header.sequence != 0 && header.sequence <= gNS2ConnState[gPangeaNetLocalPlayerIndex].lastReceivedSequence)
		{
			gNS2LastReorderedPackets++;
		}
		gNS2ConnState[gPangeaNetLocalPlayerIndex].lastReceivedSequence = header.sequence;

		if (header.packetType == kPangeaNetPacketClientInput)
		{
			if (!gPangeaNetIsHost)
			{
				NS2DebugLogEarlyNetPhase("pump-skip-client-input-on-client");
				continue;
			}

			const int idx = (int)header.playerIndex;
			if (idx < 0 || idx >= gPangeaNetPlayerCount || idx >= 2)
			{
				continue;
			}

			gNetInputAnalogX[idx] = NS2Reader_F32(&reader);
			gNetInputAnalogZ[idx] = NS2Reader_F32(&reader);
			gNetInputBitsHeld[idx] = NS2Reader_U32(&reader);
			gNetInputBitsNew[idx] = NS2Reader_U32(&reader);
			if (!reader.ok)
			{
				NS2DebugLogEarlyNetPhase("pump-bad-client-input");
				continue;
			}

			NS2DebugLogEarlyNetPhase("pump-client-input");
			gNS2ConnState[idx].lastInputTick = header.tick;
			gNS2ConnState[idx].lastReceivedSequence = header.sequence;
			gNS2ConnState[idx].inputBufferLength = 1;
			gNS2ConnState[idx].timeoutState = 0;
			gNS2ConnState[idx].lastInputTimeMs = (double)SDL_GetTicks();
			continue;
		}

		if (header.packetType == kPangeaNetPacketHostSnapshot)
		{
			if (gPangeaNetIsHost)
			{
				NS2DebugLogEarlyNetPhase("pump-skip-snapshot-on-host");
				continue;
			}
			if (header.sequence <= gNS2PendingSnapshotSequence)
			{
				NS2DebugLogEarlyNetPhase("pump-drop-old-snapshot");
				gNS2LastDroppedPackets++;
				continue;
			}

			const uint8_t snapshotKind = NS2Reader_U8(&reader);
			const uint32_t stateHash = NS2Reader_U32(&reader);
			const uint32_t lastKeyframeSeq = NS2Reader_U32(&reader);
			const uint32_t lastDeltaSeq = NS2Reader_U32(&reader);
			const uint8_t playerCount = NS2Reader_U8(&reader);
			for (int i = 0; i < 2; i++)
			{
				NS2SnapshotPlayerState* s = &gNS2PendingSnapshotPlayers[i];
				s->coordX = NS2Reader_F32(&reader);
				s->coordY = NS2Reader_F32(&reader);
				s->coordZ = NS2Reader_F32(&reader);
				s->rotX = NS2Reader_F32(&reader);
				s->rotY = NS2Reader_F32(&reader);
				s->rotZ = NS2Reader_F32(&reader);
				s->deltaX = NS2Reader_F32(&reader);
				s->deltaY = NS2Reader_F32(&reader);
				s->deltaZ = NS2Reader_F32(&reader);
				s->health = NS2Reader_F32(&reader);
				s->shieldPower = NS2Reader_F32(&reader);
				s->jetpackFuel = NS2Reader_F32(&reader);
				s->weaponCharge = NS2Reader_F32(&reader);
				s->jetpackActive = NS2Reader_U8(&reader);
				s->currentWeapon = NS2Reader_U8(&reader);
				s->place = NS2Reader_U8(&reader);
				s->raceComplete = NS2Reader_U8(&reader);
				s->deathPhase = NS2Reader_U8(&reader);
				s->shieldVisible = NS2Reader_U8(&reader);
				s->hiddenState = NS2Reader_U8(&reader);
				s->lapNum = NS2Reader_U16(&reader);
				s->raceCheckpointNum = NS2Reader_U16(&reader);
				s->weaponQuantity0 = NS2Reader_U16(&reader);
				s->weaponQuantity1 = NS2Reader_U16(&reader);
				s->weaponQuantity2 = NS2Reader_U16(&reader);
				s->alpha = NS2Reader_F32(&reader);
				s->distToNextCheckpoint = NS2Reader_F32(&reader);
				s->invincibilityTimer = NS2Reader_F32(&reader);
				s->deathTimer = NS2Reader_F32(&reader);
				s->currentAnimTime = NS2Reader_F32(&reader);
				for (int word = 0; word < 4; word++)
				{
					s->checkpointBits[word] = NS2Reader_U32(&reader);
				}
				s->lastProcessedInputSequence = NS2Reader_U32(&reader);
				s->numFreeLives = NS2Reader_U16(&reader);
				s->wrongWay = NS2Reader_U8(&reader);
				s->movingBackwards = NS2Reader_U8(&reader);
				s->currentAnimNum = NS2Reader_U8(&reader);
				s->reserved0 = NS2Reader_U8(&reader);
			}

			/* Read egg objective state */
			{
				const uint8_t pendingCount = NS2Reader_U8(&reader);
				gNS2PendingEggCount = pendingCount < NS2_NET_MAX_EGGS ? pendingCount : NS2_NET_MAX_EGGS;
				for (int k = 0; k < NUM_EGG_TYPES; k++)
					gNS2PendingNumEggsSaved[k] = NS2Reader_U8(&reader);
				for (uint8_t ei = 0; ei < gNS2PendingEggCount; ei++)
				{
					gNS2PendingEggState[ei] = NS2Reader_U8(&reader);
					gNS2PendingEggCarrier[ei] = NS2Reader_U8(&reader);
					gNS2PendingEggCoordX[ei] = NS2Reader_F32(&reader);
					gNS2PendingEggCoordY[ei] = NS2Reader_F32(&reader);
					gNS2PendingEggCoordZ[ei] = NS2Reader_F32(&reader);
				}
				gNS2HavePendingEggState = reader.ok;
			}

			if (!reader.ok)
			{
				NS2DebugLogEarlyNetPhase("pump-bad-snapshot");
				continue;
			}

			NS2DebugLogEarlyNetPhase("pump-host-snapshot");
			gNS2PendingSnapshotPlayerCount = playerCount > 2 ? 2 : playerCount;
			gNS2PendingSnapshotKind = snapshotKind;
			gNS2PendingSnapshotTick = header.tick;
			gNS2PendingSnapshotSequence = header.sequence;
			gNS2HavePendingSnapshot = 1;
			gNS2LastHostStateHash = stateHash;
			gNS2LastHostKeyframeSeq = lastKeyframeSeq;
			gNS2LastHostDeltaSeq = lastDeltaSeq;
			if (snapshotKind == kNS2SnapshotKeyframe)
			{
				gNS2LastReceivedKeyframeAtMs = (double)SDL_GetTicks();
			}
			gNS2ConnState[gPangeaNetLocalPlayerIndex].lastAckTick = header.tick;
			for (int i = 0; i < playerCount && i < 2; i++)
			{
				if (i == gPangeaNetLocalPlayerIndex)
				{
					continue;
				}
				NS2PushRemoteSnapshot(i, &gNS2PendingSnapshotPlayers[i]);
			}
			continue;
		}

		if (header.packetType == kPangeaNetPacketClientAck)
		{
			if (!gPangeaNetIsHost)
			{
				continue;
			}
			const int idx = (int)header.playerIndex;
			if (idx >= 0 && idx < gPangeaNetPlayerCount && idx < 2)
			{
				gNS2ConnState[idx].lastAckTick = header.tick;
				gNS2ConnState[idx].lastReceivedSequence = NS2Reader_U32(&reader);
				(void)NS2Reader_U32(&reader);
				(void)NS2Reader_U32(&reader);
			}
			continue;
		}

		if (header.packetType == kPangeaNetPacketKeyframeResendRequest)
		{
			if (gPangeaNetIsHost)
			{
				gNS2ForceKeyframe = 1;
			}
			continue;
		}

		if (header.packetType == kPangeaNetPacketReliableEvent)
		{
			if (gPangeaNetIsHost)
			{
				continue;
			}
			const uint8_t eventType = NS2Reader_U8(&reader);
			const uint8_t eventArg = NS2Reader_U8(&reader);
			if (!reader.ok)
			{
				continue;
			}
			if (eventType == kNS2ReliableEventEggPickedUp)
			{
				const uint8_t playerNum = NS2Reader_U8(&reader);
				(void)NS2Reader_U8(&reader);
				if (reader.ok && eventArg < NS2_NET_MAX_EGGS && playerNum < 2)
				{
					PangeaNet_ApplyEggNetworkState(eventArg, 1, playerNum, 0.0f, 0.0f, 0.0f);
				}
				continue;
			}
			if (eventType == kNS2ReliableEventEggDropped)
			{
				(void)NS2Reader_U8(&reader);
				(void)NS2Reader_U8(&reader);
				if (reader.ok && eventArg < NS2_NET_MAX_EGGS)
				{
					PangeaNet_ApplyEggNetworkState(eventArg, 0, 0xFF, 0.0f, 0.0f, 0.0f);
				}
				continue;
			}
			if (eventType == kNS2ReliableEventEggRetrieved)
			{
				(void)NS2Reader_U8(&reader);
				(void)NS2Reader_U8(&reader);
				if (reader.ok && eventArg < NS2_NET_MAX_EGGS)
				{
					PangeaNet_ApplyEggNetworkState(eventArg, 3, 0xFF, 0.0f, 0.0f, 0.0f);
				}
				continue;
			}
			const uint8_t eventPlayer = eventArg;
			if (eventPlayer >= 2)
			{
				continue;
			}
			if (eventType == kNS2ReliableEventRaceComplete)
			{
				const uint16_t lapNum = NS2Reader_U16(&reader);
				const uint16_t place = NS2Reader_U16(&reader);
				const uint8_t raceComplete = NS2Reader_U8(&reader);
				(void)NS2Reader_U8(&reader);
				gPlayerInfo[eventPlayer].lapNum = (int)lapNum;
				gPlayerInfo[eventPlayer].place = (int)place;
				gPlayerInfo[eventPlayer].raceComplete = raceComplete != 0;
			}
			else if (eventType == kNS2ReliableEventEliminated)
			{
				(void)NS2Reader_U16(&reader);
				(void)NS2Reader_U16(&reader);
				(void)NS2Reader_U8(&reader);
				const uint8_t eliminated = NS2Reader_U8(&reader);
				if (eliminated != 0)
				{
					gPlayerInfo[eventPlayer].health = 0.0f;
				}
			}
			else
			{
				OGLPoint3D eventPoint =
				{
					NS2Reader_F32(&reader),
					NS2Reader_F32(&reader),
					NS2Reader_F32(&reader),
				};
				const uint8_t aux0 = NS2Reader_U8(&reader);
				const uint8_t aux1 = NS2Reader_U8(&reader);
				(void)NS2Reader_U8(&reader);
				if (!reader.ok)
				{
					continue;
				}
				if (header.sequence <= gNS2LastAppliedVisualEventSequence)
				{
					gNS2DuplicateVisualEventCount++;
					continue;
				}
				if (NS2ShouldTreatVisualEventAsStale(eventType, eventPlayer, header.tick))
				{
					gNS2StaleVisualEventCount++;
					continue;
				}
				ObjNode* obj = gPlayerInfo[eventPlayer].objNode;
				switch (eventType)
				{
					case kNS2ReliableEventPlayerExploded:
					case kNS2ReliableEventPlayerDeathDiveImpact:
						if (obj)
						{
							ExplodePlayer(obj, eventPlayer, &eventPoint);
							gNS2LastRemoteExplosionTick[eventPlayer] = header.tick;
						}
						break;

					case kNS2ReliableEventWeaponFired:
						PangeaNet_PlayRemoteWeaponFire((short)aux0, &eventPoint);
						break;

					case kNS2ReliableEventWeaponHit:
						PangeaNet_PlayRemoteWeaponHit((short)aux0, aux1 != 0, &eventPoint);
						break;

					case kNS2ReliableEventJetpackIgnited:
						PlayEffect_Parms3D(EFFECT_JETPACKIGNITE, &eventPoint, NORMAL_CHANNEL_RATE, .7f);
						break;

					case kNS2ReliableEventJetpackShutoff:
						if (obj)
						{
							StopAChannelIfEffectNum(&obj->EffectChannel, EFFECT_JETPACKHUM);
						}
						break;

					case kNS2ReliableEventShieldHit:
						PlayEffect_Parms3D(EFFECT_SHIELD, &eventPoint, NORMAL_CHANNEL_RATE, .8f);
						break;

					case kNS2ReliableEventWormholeEntered:
						if (obj && obj->Skeleton)
						{
							SetSkeletonAnim(obj->Skeleton, PLAYER_ANIM_ENTERWORMHOLE);
						}
						break;

					case kNS2ReliableEventWormholeExited:
						if (obj && obj->Skeleton)
						{
							SetSkeletonAnim(obj->Skeleton, PLAYER_ANIM_APPEARWORMHOLE);
						}
						break;

					case kNS2ReliableEventDustDevilCaptured:
						if (obj && obj->Skeleton)
						{
							MorphToSkeletonAnim(obj->Skeleton, PLAYER_ANIM_DUSTDEVIL, 2.0f);
						}
						break;

					case kNS2ReliableEventDustDevilReleased:
						if (obj && obj->Skeleton)
						{
							MorphToSkeletonAnim(obj->Skeleton, PLAYER_ANIM_FLAP, 2.0f);
						}
						break;

					default:
						break;
				}
				gNS2LastAppliedVisualEventSequence = header.sequence;
			}
		}
	}
}

EMSCRIPTEN_KEEPALIVE int PangeaNet_HostIsRemoteNeedActive(int playerNum, int needID)
{
	NS2EnsureMatchStateInitialized();
	if (!gPangeaNetEnabled || !gPangeaNetIsHost || playerNum < 0 || playerNum >= gPangeaNetPlayerCount || playerNum >= 2)
	{
		return 0;
	}

	switch (needID)
	{
		case kNeed_Fire:
			return (gNetInputBitsHeld[playerNum] & kNS2InputBitFire) != 0;
		case kNeed_Jetpack:
			return (gNetInputBitsHeld[playerNum] & kNS2InputBitJetpack) != 0;
		case kNeed_NextWeapon:
			return (gNetInputBitsHeld[playerNum] & kNS2InputBitNextWeapon) != 0;
		case kNeed_PrevWeapon:
			return (gNetInputBitsHeld[playerNum] & kNS2InputBitPrevWeapon) != 0;
		case kNeed_Drop:
			return (gNetInputBitsHeld[playerNum] & kNS2InputBitDrop) != 0;
		default:
			return 0;
	}
}

EMSCRIPTEN_KEEPALIVE int PangeaNet_HostIsRemoteNeedDown(int playerNum, int needID)
{
	NS2EnsureMatchStateInitialized();
	if (!gPangeaNetEnabled || !gPangeaNetIsHost || playerNum < 0 || playerNum >= gPangeaNetPlayerCount || playerNum >= 2)
	{
		return 0;
	}

	switch (needID)
	{
		case kNeed_NextWeapon:
			return (gNetInputBitsNew[playerNum] & kNS2InputBitNextWeapon) != 0;
		case kNeed_PrevWeapon:
			return (gNetInputBitsNew[playerNum] & kNS2InputBitPrevWeapon) != 0;
		case kNeed_Fire:
			return (gNetInputBitsNew[playerNum] & kNS2InputBitFire) != 0;
		case kNeed_Jetpack:
			return (gNetInputBitsNew[playerNum] & kNS2InputBitJetpack) != 0;
		case kNeed_Drop:
			return (gNetInputBitsNew[playerNum] & kNS2InputBitDrop) != 0;
		default:
			return 0;
	}
}

EMSCRIPTEN_KEEPALIVE float PangeaNet_HostGetRemoteAnalogX(int playerNum)
{
	NS2EnsureMatchStateInitialized();
	if (!gPangeaNetEnabled || !gPangeaNetIsHost || playerNum < 0 || playerNum >= gPangeaNetPlayerCount || playerNum >= 2)
	{
		return 0.0f;
	}
	return gNetInputAnalogX[playerNum];
}

EMSCRIPTEN_KEEPALIVE float PangeaNet_HostGetRemoteAnalogZ(int playerNum)
{
	NS2EnsureMatchStateInitialized();
	if (!gPangeaNetEnabled || !gPangeaNetIsHost || playerNum < 0 || playerNum >= gPangeaNetPlayerCount || playerNum >= 2)
	{
		return 0.0f;
	}
	return gNetInputAnalogZ[playerNum];
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_ClientSendInput(void)
{
	NS2EnsureMatchStateInitialized();
	if (!gPangeaNetEnabled || gPangeaNetIsHost)
	{
		return;
	}

	const int me = gPangeaNetLocalPlayerIndex;
	uint8_t packet[PANGEA_NET_GAMEPLAY_BUFFER_SIZE];
	NS2Writer writer;
	PangeaNetGameplayHeader header;

	gNS2Tick++;
	header.magic = PANGEA_NET_GAMEPLAY_MAGIC;
	header.version = PANGEA_NET_GAMEPLAY_VERSION;
	header.packetType = kPangeaNetPacketClientInput;
	header.matchIdLow = gNS2MatchId;
	header.matchIdHigh = gNS2MatchIdHigh;
	header.tick = gNS2Tick;
	header.sequence = gNS2Sequence++;
	header.playerIndex = (uint16_t)me;
	header.reserved = 0;

	NS2Writer_Init(&writer, packet, (int)sizeof(packet));
	NS2WriteHeader(&writer, &header);

	uint32_t heldBits = 0;
	uint32_t newBits = 0;
	if (IsNeedActive(kNeed_Fire, me)) heldBits |= kNS2InputBitFire;
	if (IsNeedActive(kNeed_Jetpack, me)) heldBits |= kNS2InputBitJetpack;
	if (IsNeedActive(kNeed_NextWeapon, me)) heldBits |= kNS2InputBitNextWeapon;
	if (IsNeedActive(kNeed_PrevWeapon, me)) heldBits |= kNS2InputBitPrevWeapon;
	if (IsNeedActive(kNeed_Drop, me)) heldBits |= kNS2InputBitDrop;
	if (IsNeedDown(kNeed_Fire, me)) newBits |= kNS2InputBitFire;
	if (IsNeedDown(kNeed_Jetpack, me)) newBits |= kNS2InputBitJetpack;
	if (IsNeedDown(kNeed_NextWeapon, me)) newBits |= kNS2InputBitNextWeapon;
	if (IsNeedDown(kNeed_PrevWeapon, me)) newBits |= kNS2InputBitPrevWeapon;
	if (IsNeedDown(kNeed_Drop, me)) newBits |= kNS2InputBitDrop;

	NS2Writer_F32(&writer, gPlayerInfo[me].analogControlX);
	NS2Writer_F32(&writer, gPlayerInfo[me].analogControlZ);
	NS2Writer_U32(&writer, heldBits);
	NS2Writer_U32(&writer, newBits);
	if (!writer.ok)
	{
		return;
	}

	gNS2ConnState[me].lastSentSequence = header.sequence;
	NS2RecordLocalInput(header.sequence, me, gPlayerInfo[me].analogControlX, gPlayerInfo[me].analogControlZ, heldBits, newBits);
	JS_PangeaNet_SendUnreliable(packet, writer.cursor);
	for (int i = 0; i < 2; i++)
	{
		if (gPlayerInfo[i].raceComplete && !gNS2ReliableRaceCompleteSent[i])
		{
			NS2SendReliableEvent(kNS2ReliableEventRaceComplete, i);
			gNS2ReliableRaceCompleteSent[i] = 1;
		}
		if (gPlayerInfo[i].health <= 0.0f && !gNS2ReliableEliminatedSent[i])
		{
			NS2SendReliableEvent(kNS2ReliableEventEliminated, i);
			gNS2ReliableEliminatedSent[i] = 1;
		}
	}
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_HostReceiveInputs(void)
{
	NS2EnsureMatchStateInitialized();
	if (!gPangeaNetEnabled || !gPangeaNetIsHost)
	{
		return;
	}

	for (int i = 0; i < 2; i++)
	{
		gNetInputBitsNew[i] = 0;
	}

	NS2PumpGameplayMessages();

	const double nowMs = (double)SDL_GetTicks();
	for (int i = 0; i < gPangeaNetPlayerCount && i < 2; i++)
	{
		if (i == gPangeaNetLocalPlayerIndex)
		{
			continue;
		}

		const double ageMs = nowMs - gNS2ConnState[i].lastInputTimeMs;
		if (gNS2ConnState[i].lastInputTimeMs <= 0.0)
		{
			gNS2ConnState[i].timeoutState = 1;
			continue;
		}
		if (ageMs > PANGEA_NET_GAMEPLAY_INPUT_TIMEOUT_MS)
		{
			gNS2ConnState[i].timeoutState = 2;
		}
		else
		{
			gNS2ConnState[i].timeoutState = 0;
		}

		gPlayerInfo[i].analogControlX = gNetInputAnalogX[i];
		gPlayerInfo[i].analogControlZ = gNetInputAnalogZ[i];
	}
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_HostSendSnapshot(void)
{
	NS2EnsureMatchStateInitialized();
	if (!gPangeaNetEnabled || !gPangeaNetIsHost)
	{
		return;
	}

	uint8_t packet[PANGEA_NET_GAMEPLAY_BUFFER_SIZE];
	NS2Writer writer;
	PangeaNetGameplayHeader header;
	const uint8_t playerCount = (uint8_t)gPangeaNetPlayerCount;
	const uint8_t sendKeyframe = (gNS2ForceKeyframe != 0 || (gNS2Tick % PANGEA_NET_GAMEPLAY_KEYFRAME_INTERVAL) == 0)
		? (uint8_t)kNS2SnapshotKeyframe
		: (uint8_t)kNS2SnapshotDelta;
	const uint32_t stateHash = NS2ComputeAuthoritativeStateHash(playerCount);

	gNS2Tick++;
	header.magic = PANGEA_NET_GAMEPLAY_MAGIC;
	header.version = PANGEA_NET_GAMEPLAY_VERSION;
	header.packetType = kPangeaNetPacketHostSnapshot;
	header.matchIdLow = gNS2MatchId;
	header.matchIdHigh = gNS2MatchIdHigh;
	header.tick = gNS2Tick;
	header.sequence = gNS2Sequence++;
	header.playerIndex = PANGEA_NET_GAMEPLAY_PLAYER_NA;
	header.reserved = 0;

	NS2Writer_Init(&writer, packet, (int)sizeof(packet));
	NS2WriteHeader(&writer, &header);
	NS2Writer_U8(&writer, sendKeyframe);
	NS2Writer_U32(&writer, stateHash);
	if (sendKeyframe == kNS2SnapshotKeyframe)
	{
		gNS2LastHostKeyframeSeq = header.sequence;
	}
	else
	{
		gNS2LastHostDeltaSeq = header.sequence;
	}
	NS2Writer_U32(&writer, gNS2LastHostKeyframeSeq);
	NS2Writer_U32(&writer, gNS2LastHostDeltaSeq);
	NS2Writer_U8(&writer, playerCount);

	for (int i = 0; i < 2; i++)
	{
		const ObjNode* obj = (i < gPangeaNetPlayerCount) ? gPlayerInfo[i].objNode : NULL;
		const OGLVector3D zeroVec = {0.0f, 0.0f, 0.0f};
		const OGLPoint3D zeroPoint = {0.0f, 0.0f, 0.0f};
		const OGLPoint3D coord = obj ? obj->Coord : zeroPoint;
		const OGLVector3D rot = obj ? obj->Rot : zeroVec;
		const OGLVector3D delta = obj ? obj->Delta : zeroVec;
		uint32_t checkpointBits[4];
		NS2PackCheckpointBits(i, checkpointBits);

		NS2Writer_F32(&writer, coord.x);
		NS2Writer_F32(&writer, coord.y);
		NS2Writer_F32(&writer, coord.z);
		NS2Writer_F32(&writer, rot.x);
		NS2Writer_F32(&writer, rot.y);
		NS2Writer_F32(&writer, rot.z);
		NS2Writer_F32(&writer, delta.x);
		NS2Writer_F32(&writer, delta.y);
		NS2Writer_F32(&writer, delta.z);
		NS2Writer_F32(&writer, gPlayerInfo[i].health);
		NS2Writer_F32(&writer, gPlayerInfo[i].shieldPower);
		NS2Writer_F32(&writer, gPlayerInfo[i].jetpackFuel);
		NS2Writer_F32(&writer, gPlayerInfo[i].weaponCharge);
		NS2Writer_U8(&writer, gPlayerInfo[i].jetpackActive ? 1 : 0);
		NS2Writer_U8(&writer, (uint8_t)gPlayerInfo[i].currentWeapon);
		NS2Writer_U8(&writer, (uint8_t)gPlayerInfo[i].place);
		NS2Writer_U8(&writer, gPlayerInfo[i].raceComplete ? 1 : 0);
		NS2Writer_U8(&writer, NS2GetPlayerDeathPhase(i, obj));
		NS2Writer_U8(&writer, gPlayerInfo[i].shieldObj && (gPlayerInfo[i].shieldObj->StatusBits & STATUS_BIT_HIDDEN) == 0 ? 1 : 0);
		NS2Writer_U8(&writer, obj && (obj->StatusBits & STATUS_BIT_HIDDEN) != 0 ? 1 : 0);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].lapNum);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].raceCheckpointNum);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].weaponQuantity[0]);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].weaponQuantity[1]);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].weaponQuantity[2]);
		NS2Writer_F32(&writer, obj ? obj->ColorFilter.a : 1.0f);
		NS2Writer_F32(&writer, gPlayerInfo[i].distToNextCheckpoint);
		NS2Writer_F32(&writer, gPlayerInfo[i].invincibilityTimer);
		NS2Writer_F32(&writer, gDeathTimer[i]);
		NS2Writer_F32(&writer, obj && obj->Skeleton ? obj->Skeleton->CurrentAnimTime : 0.0f);
		for (int word = 0; word < 4; word++)
		{
			NS2Writer_U32(&writer, checkpointBits[word]);
		}
		NS2Writer_U32(&writer, gNS2ConnState[i].lastReceivedSequence);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].numFreeLives);
		NS2Writer_U8(&writer, gPlayerInfo[i].wrongWay ? 1 : 0);
		NS2Writer_U8(&writer, gPlayerInfo[i].movingBackwards ? 1 : 0);
		NS2Writer_U8(&writer, obj && obj->Skeleton ? obj->Skeleton->AnimNum : 0);
		NS2Writer_U8(&writer, 0);
	}

	/* Write egg objective state */
	{
		Byte eggState[NS2_NET_MAX_EGGS];
		Byte eggCarrier[NS2_NET_MAX_EGGS];
		float eggX[NS2_NET_MAX_EGGS], eggY[NS2_NET_MAX_EGGS], eggZ[NS2_NET_MAX_EGGS];
		const int eggCount = PangeaNet_GetEggSnapshotData(eggState, eggCarrier, eggX, eggY, eggZ, NS2_NET_MAX_EGGS);
		NS2Writer_U8(&writer, (uint8_t)eggCount);
		for (int k = 0; k < NUM_EGG_TYPES; k++)
			NS2Writer_U8(&writer, gNumEggsSaved[k]);
		for (int ei = 0; ei < eggCount; ei++)
		{
			NS2Writer_U8(&writer, eggState[ei]);
			NS2Writer_U8(&writer, eggCarrier[ei]);
			NS2Writer_F32(&writer, eggX[ei]);
			NS2Writer_F32(&writer, eggY[ei]);
			NS2Writer_F32(&writer, eggZ[ei]);
		}
	}

	if (!writer.ok)
	{
		return;
	}

	if (sendKeyframe == kNS2SnapshotKeyframe)
	{
		JS_PangeaNet_SendReliable(packet, writer.cursor);
		gNS2ForceKeyframe = 0;
	}
	else
	{
		JS_PangeaNet_SendUnreliable(packet, writer.cursor);
	}
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_ClientApplySnapshot(void)
{
	NS2EnsureMatchStateInitialized();
	if (!gPangeaNetEnabled || gPangeaNetIsHost)
	{
		return;
	}

	NS2PumpGameplayMessages();
	const int havePendingSnapshot = gNS2HavePendingSnapshot;
	if (!havePendingSnapshot)
	{
		const double nowMs = (double)SDL_GetTicks();
		if (gNS2LastReceivedKeyframeAtMs > 0.0 && (nowMs - gNS2LastReceivedKeyframeAtMs) > PANGEA_NET_GAMEPLAY_KEYFRAME_TIMEOUT_MS)
		{
			uint8_t packet[64];
			NS2Writer writer;
			PangeaNetGameplayHeader header;
			header.magic = PANGEA_NET_GAMEPLAY_MAGIC;
			header.version = PANGEA_NET_GAMEPLAY_VERSION;
			header.packetType = kPangeaNetPacketKeyframeResendRequest;
			header.matchIdLow = gNS2MatchId;
			header.matchIdHigh = gNS2MatchIdHigh;
			header.tick = gNS2Tick;
			header.sequence = gNS2Sequence++;
			header.playerIndex = (uint16_t)gPangeaNetLocalPlayerIndex;
			header.reserved = 0;
			NS2Writer_Init(&writer, packet, (int)sizeof(packet));
			NS2WriteHeader(&writer, &header);
			NS2Writer_U32(&writer, gNS2LastHostKeyframeSeq);
			if (writer.ok)
			{
				JS_PangeaNet_SendReliable(packet, writer.cursor);
			}
			PangeaNet_ReportDesync(gNS2Tick, 0u, gNS2LastHostStateHash);
		}
	}

	const int me = gPangeaNetLocalPlayerIndex;
	for (int i = 0; i < gPangeaNetPlayerCount && i < 2; i++)
	{
		NS2SnapshotPlayerState appliedSnapshot = {0};
		const NS2SnapshotPlayerState* s = NULL;
		ObjNode* obj = gPlayerInfo[i].objNode;
		if (!obj)
		{
			continue;
		}

		if (i == me)
		{
			if (!havePendingSnapshot)
			{
				continue;
			}
			appliedSnapshot = gNS2PendingSnapshotPlayers[i];
			s = &appliedSnapshot;
			const float dx = gPlayerInfo[i].coord.x - s->coordX;
			const float dy = gPlayerInfo[i].coord.y - s->coordY;
			const float dz = gPlayerInfo[i].coord.z - s->coordZ;
			const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
			gNS2LastCorrectionDistance = dist;

			if (dist > PANGEA_NET_GAMEPLAY_LOCAL_SNAP_DIST)
			{
				gPlayerInfo[i].coord.x = s->coordX;
				gPlayerInfo[i].coord.y = s->coordY;
				gPlayerInfo[i].coord.z = s->coordZ;
			}
			else
			{
				gPlayerInfo[i].coord.x += (s->coordX - gPlayerInfo[i].coord.x) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
				gPlayerInfo[i].coord.y += (s->coordY - gPlayerInfo[i].coord.y) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
				gPlayerInfo[i].coord.z += (s->coordZ - gPlayerInfo[i].coord.z) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
			}
			NS2ReapplyUnackedLocalInput(s->lastProcessedInputSequence, i);
		}
		else
		{
			if (havePendingSnapshot)
			{
				appliedSnapshot = gNS2PendingSnapshotPlayers[i];
			}
			const int haveRemoteSnapshot = NS2GetDelayedRemoteSnapshot(i, &appliedSnapshot);
			if (!haveRemoteSnapshot && !havePendingSnapshot)
			{
				continue;
			}
			s = &appliedSnapshot;
			if (haveRemoteSnapshot)
			{
				s = &appliedSnapshot;
			}
			const float dx = gPlayerInfo[i].coord.x - s->coordX;
			const float dy = gPlayerInfo[i].coord.y - s->coordY;
			const float dz = gPlayerInfo[i].coord.z - s->coordZ;
			const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
			if (dist > PANGEA_NET_GAMEPLAY_REMOTE_SNAP_DIST)
			{
				gPlayerInfo[i].coord.x = s->coordX;
				gPlayerInfo[i].coord.y = s->coordY;
				gPlayerInfo[i].coord.z = s->coordZ;
			}
			else
			{
				gPlayerInfo[i].coord.x += (s->coordX - gPlayerInfo[i].coord.x) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
				gPlayerInfo[i].coord.y += (s->coordY - gPlayerInfo[i].coord.y) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
				gPlayerInfo[i].coord.z += (s->coordZ - gPlayerInfo[i].coord.z) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
			}
		}

		obj->Coord = gPlayerInfo[i].coord;
		obj->Rot.x += (s->rotX - obj->Rot.x) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
		obj->Rot.y += (s->rotY - obj->Rot.y) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
		obj->Rot.z += (s->rotZ - obj->Rot.z) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
		obj->Delta.x += (s->deltaX - obj->Delta.x) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
		obj->Delta.y += (s->deltaY - obj->Delta.y) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;
		obj->Delta.z += (s->deltaZ - obj->Delta.z) * PANGEA_NET_GAMEPLAY_INTERP_ALPHA;

		gPlayerInfo[i].health = s->health;
		gPlayerInfo[i].shieldPower = s->shieldPower;
		gPlayerInfo[i].jetpackFuel = s->jetpackFuel;
		gPlayerInfo[i].weaponCharge = s->weaponCharge;
		gPlayerInfo[i].jetpackActive = s->jetpackActive != 0;
		gPlayerInfo[i].currentWeapon = s->currentWeapon;
		gPlayerInfo[i].numFreeLives = (short)s->numFreeLives;
		gPlayerInfo[i].wrongWay = s->wrongWay != 0;
		gPlayerInfo[i].movingBackwards = s->movingBackwards != 0;
		gPlayerInfo[i].place = s->place;
		gPlayerInfo[i].raceComplete = s->raceComplete != 0;
		gPlayerInfo[i].distToNextCheckpoint = s->distToNextCheckpoint;
		gPlayerInfo[i].invincibilityTimer = s->invincibilityTimer;
		NS2ApplySnapshotDeathState(i, obj, s);
		if (i != me)
		{
			NS2ApplyRemotePlayerAnimation(i, obj, s);
		}
		gPlayerInfo[i].lapNum = (short)s->lapNum;
		gPlayerInfo[i].raceCheckpointNum = (short)s->raceCheckpointNum;
		NS2UnpackCheckpointBits(s->checkpointBits, i);
		gPlayerInfo[i].weaponQuantity[0] = (short)s->weaponQuantity0;
		gPlayerInfo[i].weaponQuantity[1] = (short)s->weaponQuantity1;
		gPlayerInfo[i].weaponQuantity[2] = (short)s->weaponQuantity2;

		if (i != me)
		{
			NS2RestoreRemotePlayerRenderState(i, obj, s);
		}
	}

	/* Apply egg objective state from snapshot */
	if (gNS2HavePendingEggState)
	{
		for (int k = 0; k < NUM_EGG_TYPES; k++)
			gNumEggsSaved[k] = gNS2PendingNumEggsSaved[k];
		for (int ei = 0; ei < gNS2PendingEggCount; ei++)
		{
			PangeaNet_ApplyEggNetworkState(ei, gNS2PendingEggState[ei], gNS2PendingEggCarrier[ei],
				gNS2PendingEggCoordX[ei], gNS2PendingEggCoordY[ei], gNS2PendingEggCoordZ[ei]);
		}
		gNS2HavePendingEggState = false;
	}

	if (havePendingSnapshot)
	{
		const uint32_t localHash = NS2ComputeAuthoritativeStateHash(gNS2PendingSnapshotPlayerCount);
		if (gNS2PendingSnapshotKind == kNS2SnapshotKeyframe && localHash != gNS2LastHostStateHash)
		{
			PangeaNet_ReportDesync(gNS2PendingSnapshotTick, localHash, gNS2LastHostStateHash);
		}
		uint8_t ackPacket[64];
		NS2Writer writer;
		PangeaNetGameplayHeader ack;
		ack.magic = PANGEA_NET_GAMEPLAY_MAGIC;
		ack.version = PANGEA_NET_GAMEPLAY_VERSION;
		ack.packetType = kPangeaNetPacketClientAck;
		ack.matchIdLow = gNS2MatchId;
		ack.matchIdHigh = gNS2MatchIdHigh;
		ack.tick = gNS2PendingSnapshotTick;
		ack.sequence = gNS2Sequence++;
		ack.playerIndex = (uint16_t)gPangeaNetLocalPlayerIndex;
		ack.reserved = 0;
		NS2Writer_Init(&writer, ackPacket, (int)sizeof(ackPacket));
		NS2WriteHeader(&writer, &ack);
		NS2Writer_U32(&writer, gNS2PendingSnapshotSequence);
		NS2Writer_U32(&writer, gNS2LastHostKeyframeSeq);
		NS2Writer_U32(&writer, gNS2LastHostDeltaSeq);
		if (writer.ok)
		{
			JS_PangeaNet_SendReliable(ackPacket, writer.cursor);
		}
	}

	gNS2HavePendingSnapshot = 0;
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendPlayerExploded(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventPlayerExploded, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendPlayerDeathDiveImpact(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventPlayerDeathDiveImpact, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendWeaponFired(short playerNum, short weaponType, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventWeaponFired, playerNum, weaponType, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendWeaponHit(short playerNum, short weaponType, int terrainHit, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventWeaponHit, playerNum, weaponType, terrainHit ? 1 : 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendJetpackIgnited(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventJetpackIgnited, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendJetpackShutoff(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventJetpackShutoff, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendShieldHit(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventShieldHit, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendWormholeEntered(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventWormholeEntered, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendWormholeExited(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventWormholeExited, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendDustDevilCaptured(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventDustDevilCaptured, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE void PangeaNet_SendDustDevilReleased(short playerNum, const OGLPoint3D* where)
{
	NS2EnsureMatchStateInitialized();
	NS2SendVisualEvent(kNS2ReliableEventDustDevilReleased, playerNum, 0, 0, where);
}

EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetLastVisualEventSequence(void)
{
	return gNS2LastVisualEventSequence;
}

EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetAppliedVisualEventSequence(void)
{
	return gNS2LastAppliedVisualEventSequence;
}

EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetDuplicateVisualEventCount(void)
{
	return gNS2DuplicateVisualEventCount;
}

EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetStaleVisualEventCount(void)
{
	return gNS2StaleVisualEventCount;
}

#endif // __EMSCRIPTEN__

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
static uint32_t gPangeaDebugFrameNumber = 0;
static uint32_t gPangeaDebugLastSyncHash = 0;
static int gPangeaDebugHasDesync = 0;

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
	gPangeaNetPlayerCount = ParseJsonInt(json, "\"playerCount\"", 2);
	gPangeaNetMatchSeed = ParseJsonU32(json, "\"seed\"", 1);
	gPangeaNetMatchIdLow = ParseJsonU32(json, "\"matchIdLow\"", gPangeaNetMatchSeed);
	gPangeaNetMatchIdHigh = ParseJsonU32(json, "\"matchIdHigh\"", 0);

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

	gNumPlayers = (Byte)gPangeaNetPlayerCount;
	gVSMode = VS_MODE_RACE;
	SDL_Log(
		"PangeaGame_SetNetworkMatchConfig resolved host=%d localPlayer=%d hostPlayer=%d playerCount=%d seed=%u matchId=%u:%u",
		gPangeaNetIsHost,
		gPangeaNetLocalPlayerIndex,
		gPangeaNetHostPlayerIndex,
		gPangeaNetPlayerCount,
		(unsigned)gPangeaNetMatchSeed,
		(unsigned)gPangeaNetMatchIdHigh,
		(unsigned)gPangeaNetMatchIdLow);
}

EMSCRIPTEN_KEEPALIVE void PangeaGame_StartNetworkMatch(void)
{
	SDL_Log("PangeaGame_StartNetworkMatch called");
	gPangeaNetEnabled = 1;
	gPangeaNetLifecycleSequence = 0;
	gPangeaNetLastSentLifecycleReason = 0;
	gPangeaNetRemoteLifecycleReason = 0;
	gPangeaNetRemoteLifecycleSequence = 0;
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

EMSCRIPTEN_KEEPALIVE int PangeaNet_GetRemoteLifecycleReason(void)
{
	return gPangeaNetRemoteLifecycleReason;
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
						SDL_Log(
							"PangeaNet lifecycle received host match-end reason=%d sequence=%u",
							packet->reason,
							(unsigned)packet->sequence);
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
		SDL_Log(
			"PangeaNet lifecycle sent host match-end reason=%d sequence=%u",
			reason,
			(unsigned)packet.sequence);
	}
}
EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetFrameNumber(void) { return gPangeaDebugFrameNumber; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetLocalPlayerIndex(void) { return gPangeaNetLocalPlayerIndex; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugGetPlayerCount(void) { return gPangeaNetPlayerCount; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugIsNetworkMatchRunning(void) { return gPangeaNetEnabled; }
EMSCRIPTEN_KEEPALIVE int PangeaGame_DebugHasDesync(void) { return gPangeaDebugHasDesync; }
EMSCRIPTEN_KEEPALIVE uint32_t PangeaGame_DebugGetLastSyncHash(void) { return gPangeaDebugLastSyncHash; }
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
#define PANGEA_NET_GAMEPLAY_VERSION 2u
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
	kNS2InputBitPrevWeapon = 1 << 3
};

enum
{
	kNS2ReliableEventRaceComplete = 1,
	kNS2ReliableEventEliminated = 2
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
	uint8_t animNum;
	uint8_t isDead;
	uint16_t lapNum;
	uint16_t raceCheckpointNum;
	uint16_t weaponQuantity0;
	uint16_t weaponQuantity1;
	uint16_t weaponQuantity2;
	uint32_t lastProcessedInputSequence;
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

static void NS2ApplyRemotePlayerAnimation(int playerIndex, ObjNode* obj, const NS2SnapshotPlayerState* snapshot)
{
	if (playerIndex < 0 || playerIndex >= 2 || !obj || !snapshot || !obj->Skeleton)
	{
		return;
	}
	if (snapshot->animNum >= obj->Skeleton->skeletonDefinition->NumAnims)
	{
		return;
	}

	const bool wasDead = gPlayerIsDead[playerIndex];
	const bool isDead = snapshot->isDead != 0;
	if (!wasDead && isDead && snapshot->animNum != PLAYER_ANIM_DEATHDIVE)
	{
		ExplodePlayer(obj, playerIndex, &obj->Coord);
		return;
	}

	if (obj->Skeleton->AnimNum != snapshot->animNum)
	{
		MorphToSkeletonAnim(obj->Skeleton, snapshot->animNum, 3.0f);
	}

	gPlayerIsDead[playerIndex] = isDead;
	gCameraInDeathDiveMode[playerIndex] = snapshot->animNum == PLAYER_ANIM_DEATHDIVE;
}

static void NS2RestoreRemotePlayerRenderState(int playerIndex, ObjNode* obj, const NS2SnapshotPlayerState* snapshot)
{
	if (playerIndex < 0 || playerIndex >= 2 || !obj || !snapshot)
	{
		return;
	}
	const bool shouldBeVisible = snapshot->health > 0.0f || snapshot->animNum == PLAYER_ANIM_DEATHDIVE;
	if (!shouldBeVisible)
	{
		return;
	}

	obj->Health = snapshot->health;
	obj->CType = CTYPE_PLAYER1 << playerIndex;
	ShowPlayer(obj);
	FadePlayer(obj, 1.0f);
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
		hash ^= gPlayerInfo[i].raceComplete ? 0xA5A5A5A5u : 0x5A5A5A5Au;
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
				s->animNum = NS2Reader_U8(&reader);
				s->isDead = NS2Reader_U8(&reader);
				s->lapNum = NS2Reader_U16(&reader);
				s->raceCheckpointNum = NS2Reader_U16(&reader);
				s->weaponQuantity0 = NS2Reader_U16(&reader);
				s->weaponQuantity1 = NS2Reader_U16(&reader);
				s->weaponQuantity2 = NS2Reader_U16(&reader);
				s->lastProcessedInputSequence = NS2Reader_U32(&reader);
			}

			if (!reader.ok)
			{
				NS2DebugLogEarlyNetPhase("pump-bad-snapshot");
				continue;
			}

			NS2DebugLogEarlyNetPhase("pump-host-snapshot");
			gNS2PendingSnapshotPlayerCount = playerCount;
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
			const uint8_t eventPlayer = NS2Reader_U8(&reader);
			const uint16_t lapNum = NS2Reader_U16(&reader);
			const uint16_t place = NS2Reader_U16(&reader);
			const uint8_t raceComplete = NS2Reader_U8(&reader);
			const uint8_t eliminated = NS2Reader_U8(&reader);
			if (!reader.ok || eventPlayer >= 2)
			{
				continue;
			}
			if (eventType == kNS2ReliableEventRaceComplete)
			{
				gPlayerInfo[eventPlayer].lapNum = (int)lapNum;
				gPlayerInfo[eventPlayer].place = (int)place;
				gPlayerInfo[eventPlayer].raceComplete = raceComplete != 0;
			}
			else if (eventType == kNS2ReliableEventEliminated)
			{
				if (eliminated != 0)
				{
					gPlayerInfo[eventPlayer].health = 0.0f;
				}
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
	if (IsNeedDown(kNeed_Fire, me)) newBits |= kNS2InputBitFire;
	if (IsNeedDown(kNeed_Jetpack, me)) newBits |= kNS2InputBitJetpack;
	if (IsNeedDown(kNeed_NextWeapon, me)) newBits |= kNS2InputBitNextWeapon;
	if (IsNeedDown(kNeed_PrevWeapon, me)) newBits |= kNS2InputBitPrevWeapon;

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
		NS2Writer_U8(&writer, obj && obj->Skeleton ? (uint8_t)obj->Skeleton->AnimNum : (uint8_t)0);
		NS2Writer_U8(&writer, gPlayerIsDead[i] ? 1 : 0);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].lapNum);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].raceCheckpointNum);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].weaponQuantity[0]);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].weaponQuantity[1]);
		NS2Writer_U16(&writer, (uint16_t)gPlayerInfo[i].weaponQuantity[2]);
		NS2Writer_U32(&writer, gNS2ConnState[i].lastReceivedSequence);
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
	if (!gNS2HavePendingSnapshot)
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
		return;
	}

	const int me = gPangeaNetLocalPlayerIndex;
	for (int i = 0; i < gNS2PendingSnapshotPlayerCount && i < 2; i++)
	{
		const NS2SnapshotPlayerState* s = &gNS2PendingSnapshotPlayers[i];
		ObjNode* obj = gPlayerInfo[i].objNode;
		if (!obj)
		{
			continue;
		}

		if (i == me)
		{
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
			NS2SnapshotPlayerState delayedRemote = *s;
			if (NS2GetDelayedRemoteSnapshot(i, &delayedRemote))
			{
				s = &delayedRemote;
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
		gPlayerInfo[i].place = s->place;
		gPlayerInfo[i].raceComplete = s->raceComplete != 0;
		if (i != me)
		{
			NS2ApplyRemotePlayerAnimation(i, obj, s);
		}
		gPlayerInfo[i].lapNum = (short)s->lapNum;
		gPlayerInfo[i].raceCheckpointNum = (short)s->raceCheckpointNum;
		gPlayerInfo[i].weaponQuantity[0] = (short)s->weaponQuantity0;
		gPlayerInfo[i].weaponQuantity[1] = (short)s->weaponQuantity1;
		gPlayerInfo[i].weaponQuantity[2] = (short)s->weaponQuantity2;

		if (i != me)
		{
			NS2RestoreRemotePlayerRenderState(i, obj, s);
		}
	}

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

#endif // __EMSCRIPTEN__

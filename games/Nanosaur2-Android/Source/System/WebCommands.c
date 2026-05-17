// NANOSAUR 2 WEB COMMANDS
// JavaScript <-> C interop for WebAssembly builds.
// Exposes cheat/debug commands callable from the browser console or level editor.

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <stdlib.h>
#include "game.h"

static int gPangeaNetEnabled = 0;
static int gPangeaNetIsHost = 1;
static int gPangeaNetLocalPlayerIndex = 0;
static int gPangeaNetPlayerCount = 1;
static uint32_t gPangeaNetMatchSeed = 1;
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
	gPangeaNetLocalPlayerIndex = ParseJsonInt(json, "\"localPlayerIndex\"", 0);
	gPangeaNetPlayerCount = ParseJsonInt(json, "\"playerCount\"", 2);
	gPangeaNetMatchSeed = (uint32_t)ParseJsonInt(json, "\"seed\"", 1);
	gPangeaNetIsHost = gPangeaNetLocalPlayerIndex == 0 ? 1 : 0;

	if (gPangeaNetPlayerCount < 1)
	{
		gPangeaNetPlayerCount = 1;
	}
	if (gPangeaNetLocalPlayerIndex < 0)
	{
		gPangeaNetLocalPlayerIndex = 0;
	}
	if (gPangeaNetLocalPlayerIndex >= gPangeaNetPlayerCount)
	{
		gPangeaNetLocalPlayerIndex = gPangeaNetPlayerCount - 1;
	}

	gNumPlayers = (Byte)gPangeaNetPlayerCount;
	gVSMode = VS_MODE_RACE;
	SDL_Log(
		"PangeaGame_SetNetworkMatchConfig resolved host=%d localPlayer=%d playerCount=%d seed=%u",
		gPangeaNetIsHost,
		gPangeaNetLocalPlayerIndex,
		gPangeaNetPlayerCount,
		(unsigned)gPangeaNetMatchSeed);
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
EMSCRIPTEN_KEEPALIVE uint32_t PangeaNet_GetMatchSeed(void) { return gPangeaNetMatchSeed; }
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
		const int byteCount = JS_PangeaNet_PollMessage(payload, (int)sizeof(payload));
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

#define PANGEA_NET_GAMEPLAY_MAGIC   0x504E4750u  /* 'PNGP' */
#define PANGEA_NET_GAMEPLAY_VERSION 1u

enum
{
	kNS2PacketInputCommand = 2001,
	kNS2PacketHostSnapshot = 2002
};

/* Input command sent by each client every frame */
typedef struct
{
	uint32_t magic;
	uint32_t version;
	uint32_t packetType;
	uint8_t  playerIndex;
	uint8_t  pad[3];
	float    analogX;        /* yaw */
	float    analogZ;        /* pitch */
	uint8_t  fireHeld;
	uint8_t  jetpackHeld;
	uint8_t  nextWeapon;
	uint8_t  prevWeapon;
} NS2InputCommandPacket;

/* Per-player state inside a snapshot */
typedef struct
{
	float    coordX, coordY, coordZ;
	float    rotX,   rotY,   rotZ;
	float    deltaX, deltaY, deltaZ;
	float    health;
	float    jetpackFuel;
	uint8_t  jetpackActive;
	short    lapNum;
	short    raceCheckpointNum;
	short    place;
	uint8_t  raceComplete;
	uint8_t  pad[2];
} NS2PlayerState;

/* Snapshot sent by the host after each MoveEverything() */
typedef struct
{
	uint32_t      magic;
	uint32_t      version;
	uint32_t      packetType;
	uint32_t      frameSeq;
	uint8_t       playerCount;
	uint8_t       pad[3];
	NS2PlayerState players[2];   /* MAX_PLAYERS == 2 */
} NS2HostSnapshotPacket;

/* Latest input received per player (maintained by host) */
static float    gNetInputAnalogX[2]       = {0, 0};
static float    gNetInputAnalogZ[2]       = {0, 0};
static uint8_t  gNetInputFireHeld[2]      = {0, 0};
static uint8_t  gNetInputJetpackHeld[2]   = {0, 0};
static uint8_t  gNetInputNextWeapon[2]    = {0, 0};
static uint8_t  gNetInputPrevWeapon[2]    = {0, 0};

/* Latest snapshot received by client */
static NS2HostSnapshotPacket gNS2PendingSnapshot;
static int gNS2HavePendingSnapshot = 0;
static uint32_t gNS2SnapshotSeq = 0;

/* Per-frame gameplay packet sequence number */
static uint32_t gNS2FrameSeq = 0;

/* Drain the PangeaNet backlog and route gameplay packets */
static void NS2PumpGameplayMessages(void)
{
	uint8_t buffer[sizeof(NS2HostSnapshotPacket) + 16];
	for (;;)
	{
		const int n = JS_PangeaNet_PollMessage(buffer, (int)sizeof(buffer));
		if (n <= 0)
		{
			break;
		}

		if ((size_t)n < sizeof(uint32_t) * 3)
		{
			continue;
		}

		const uint32_t magic   = *(const uint32_t*)(buffer);
		const uint32_t version = *(const uint32_t*)(buffer + 4);
		const uint32_t ptype   = *(const uint32_t*)(buffer + 8);

		if (magic != PANGEA_NET_GAMEPLAY_MAGIC || version != PANGEA_NET_GAMEPLAY_VERSION)
		{
			/* Lifecycle packets (different magic) are handled by UpdateMatchLifecycle.
			 * Push unrecognised packets back so they don't get lost. */
			PangeaNetBacklogPush(buffer, n);
			break;
		}

		if (ptype == kNS2PacketInputCommand)
		{
			if (n != (int)sizeof(NS2InputCommandPacket))
			{
				continue;
			}

			const NS2InputCommandPacket* cmd = (const NS2InputCommandPacket*)buffer;
			const int idx = (int)cmd->playerIndex;
			if (idx < 0 || idx >= gPangeaNetPlayerCount)
			{
				continue;
			}

			gNetInputAnalogX[idx]     = cmd->analogX;
			gNetInputAnalogZ[idx]     = cmd->analogZ;
			gNetInputFireHeld[idx]    = cmd->fireHeld;
			gNetInputJetpackHeld[idx] = cmd->jetpackHeld;
			gNetInputNextWeapon[idx]  = cmd->nextWeapon;
			gNetInputPrevWeapon[idx]  = cmd->prevWeapon;
		}
		else if (ptype == kNS2PacketHostSnapshot)
		{
			if (n != (int)sizeof(NS2HostSnapshotPacket))
			{
				continue;
			}

			const NS2HostSnapshotPacket* snap = (const NS2HostSnapshotPacket*)buffer;
			if (snap->frameSeq <= gNS2SnapshotSeq && gNS2HavePendingSnapshot)
			{
				continue;
			}

			gNS2PendingSnapshot  = *snap;
			gNS2HavePendingSnapshot = 1;
			gNS2SnapshotSeq = snap->frameSeq;
		}
	}
}

/*
 * PangeaNet_ClientSendInput
 * Called each frame by clients (non-host) after UpdatePlayerSteering().
 * Reads the local player's current input state and sends it to the host.
 */
EMSCRIPTEN_KEEPALIVE void PangeaNet_ClientSendInput(void)
{
	if (!gPangeaNetEnabled || gPangeaNetIsHost)
	{
		return;
	}

	const int me = gPangeaNetLocalPlayerIndex;

	NS2InputCommandPacket cmd;
	cmd.magic        = PANGEA_NET_GAMEPLAY_MAGIC;
	cmd.version      = PANGEA_NET_GAMEPLAY_VERSION;
	cmd.packetType   = kNS2PacketInputCommand;
	cmd.playerIndex  = (uint8_t)me;
	cmd.pad[0]       = cmd.pad[1] = cmd.pad[2] = 0;
	cmd.analogX      = gPlayerInfo[me].analogControlX;
	cmd.analogZ      = gPlayerInfo[me].analogControlZ;
	cmd.fireHeld     = (uint8_t)(IsNeedActive(kNeed_Fire, me)    ? 1 : 0);
	cmd.jetpackHeld  = (uint8_t)(IsNeedActive(kNeed_Jetpack, me) ? 1 : 0);
	cmd.nextWeapon   = (uint8_t)(IsNeedDown(kNeed_NextWeapon, me) ? 1 : 0);
	cmd.prevWeapon   = (uint8_t)(IsNeedDown(kNeed_PrevWeapon, me) ? 1 : 0);

	JS_PangeaNet_SendUnreliable(&cmd, (int)sizeof(cmd));
}

/*
 * PangeaNet_HostReceiveInputs
 * Called each frame by the host before MoveEverything().
 * Drains the message queue and applies received client inputs to gPlayerInfo.
 */
EMSCRIPTEN_KEEPALIVE void PangeaNet_HostReceiveInputs(void)
{
	if (!gPangeaNetEnabled || !gPangeaNetIsHost)
	{
		return;
	}

	NS2PumpGameplayMessages();

	int i;
	for (i = 0; i < gPangeaNetPlayerCount; i++)
	{
		if (i == gPangeaNetLocalPlayerIndex)
		{
			continue;
		}

		gPlayerInfo[i].analogControlX = gNetInputAnalogX[i];
		gPlayerInfo[i].analogControlZ = gNetInputAnalogZ[i];
	}
}

/*
 * PangeaNet_HostSendSnapshot
 * Called each frame by the host after MoveEverything().
 * Broadcasts authoritative player state to all clients.
 */
EMSCRIPTEN_KEEPALIVE void PangeaNet_HostSendSnapshot(void)
{
	if (!gPangeaNetEnabled || !gPangeaNetIsHost)
	{
		return;
	}

	NS2HostSnapshotPacket snap;
	snap.magic       = PANGEA_NET_GAMEPLAY_MAGIC;
	snap.version     = PANGEA_NET_GAMEPLAY_VERSION;
	snap.packetType  = kNS2PacketHostSnapshot;
	snap.frameSeq    = ++gNS2FrameSeq;
	snap.playerCount = (uint8_t)gPangeaNetPlayerCount;
	snap.pad[0] = snap.pad[1] = snap.pad[2] = 0;

	int i;
	for (i = 0; i < 2; i++)
	{
		NS2PlayerState* s = &snap.players[i];
		const ObjNode* obj = (i < gPangeaNetPlayerCount) ? gPlayerInfo[i].objNode : NULL;

		if (obj)
		{
			s->coordX = obj->Coord.x;
			s->coordY = obj->Coord.y;
			s->coordZ = obj->Coord.z;
			s->rotX   = obj->Rot.x;
			s->rotY   = obj->Rot.y;
			s->rotZ   = obj->Rot.z;
			s->deltaX = obj->Delta.x;
			s->deltaY = obj->Delta.y;
			s->deltaZ = obj->Delta.z;
		}
		else
		{
			s->coordX = s->coordY = s->coordZ = 0.0f;
			s->rotX   = s->rotY   = s->rotZ   = 0.0f;
			s->deltaX = s->deltaY = s->deltaZ = 0.0f;
		}

		if (i < gPangeaNetPlayerCount)
		{
			s->health           = gPlayerInfo[i].health;
			s->jetpackFuel      = gPlayerInfo[i].jetpackFuel;
			s->jetpackActive    = gPlayerInfo[i].jetpackActive ? 1 : 0;
			s->lapNum           = gPlayerInfo[i].lapNum;
			s->raceCheckpointNum = gPlayerInfo[i].raceCheckpointNum;
			s->place            = gPlayerInfo[i].place;
			s->raceComplete     = gPlayerInfo[i].raceComplete ? 1 : 0;
		}
		else
		{
			s->health = s->jetpackFuel = 0.0f;
			s->jetpackActive = s->lapNum = s->raceCheckpointNum = s->place = s->raceComplete = 0;
		}
		s->pad[0] = s->pad[1] = 0;
	}

	JS_PangeaNet_SendUnreliable(&snap, (int)sizeof(snap));
}

/*
 * PangeaNet_ClientApplySnapshot
 * Called each frame by clients before MoveEverything().
 * Applies the latest host snapshot to all remote players.
 * The local player's position is NOT overridden (client-side prediction preserved).
 */
EMSCRIPTEN_KEEPALIVE void PangeaNet_ClientApplySnapshot(void)
{
	if (!gPangeaNetEnabled || gPangeaNetIsHost)
	{
		return;
	}

	NS2PumpGameplayMessages();

	if (!gNS2HavePendingSnapshot)
	{
		return;
	}

	const int me = gPangeaNetLocalPlayerIndex;
	int i;
	for (i = 0; i < gPangeaNetPlayerCount && i < 2; i++)
	{
		if (i == me)
		{
			continue;
		}

		ObjNode* obj = gPlayerInfo[i].objNode;
		if (!obj)
		{
			continue;
		}

		const NS2PlayerState* s = &gNS2PendingSnapshot.players[i];
		obj->Coord.x = s->coordX;
		obj->Coord.y = s->coordY;
		obj->Coord.z = s->coordZ;
		obj->Rot.x   = s->rotX;
		obj->Rot.y   = s->rotY;
		obj->Rot.z   = s->rotZ;
		obj->Delta.x = s->deltaX;
		obj->Delta.y = s->deltaY;
		obj->Delta.z = s->deltaZ;

		gPlayerInfo[i].coord.x      = s->coordX;
		gPlayerInfo[i].coord.y      = s->coordY;
		gPlayerInfo[i].coord.z      = s->coordZ;
		gPlayerInfo[i].health       = s->health;
		gPlayerInfo[i].jetpackFuel  = s->jetpackFuel;
		gPlayerInfo[i].jetpackActive = s->jetpackActive != 0;
		gPlayerInfo[i].lapNum       = s->lapNum;
		gPlayerInfo[i].raceCheckpointNum = s->raceCheckpointNum;
		gPlayerInfo[i].place        = s->place;
		gPlayerInfo[i].raceComplete = s->raceComplete != 0;
	}

	gNS2HavePendingSnapshot = 0;
}

#endif // __EMSCRIPTEN__

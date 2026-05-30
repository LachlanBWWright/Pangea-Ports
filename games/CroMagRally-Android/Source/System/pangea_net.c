#include "game.h"
#include "pangea_net.h"

#ifdef __EMSCRIPTEN__
extern int PangeaNet_IsEnabled(void);
extern int PangeaNet_IsHost(void);
extern int PangeaNet_GetLocalPlayerIndex(void);
extern int PangeaNet_GetPlayerCount(void);
extern uint32_t PangeaNet_GetMatchSeed(void);
extern uint32_t PangeaNet_GetMatchIdLow(void);
extern uint32_t PangeaNet_GetMatchIdHigh(void);
extern const char* PangeaNet_GetLobbyIdString(void);
extern const char* PangeaNet_GetMatchIdString(void);
extern int PangeaNet_SendReliable(const void* bytes, int byteCount);
extern int PangeaNet_SendUnreliable(const void* bytes, int byteCount);
extern int PangeaNet_PollMessage(void* outBytes, int maxByteCount);
extern void PangeaNet_ReportDesync(uint32_t frame, uint32_t localHash, uint32_t remoteHash);
extern void PangeaNet_ReportMatchEnded(int reason);
extern void PangeaNet_ReportMatchResult(const char* resultJson);
#endif

#define PANGEA_NET_LIFECYCLE_MAGIC 0x504E4D53u
#define PANGEA_NET_LIFECYCLE_VERSION 1u
#define PANGEA_NET_LIFECYCLE_MATCH_END 1u
#define PANGEA_NET_LIFECYCLE_MATCH_RESULT 2u
#define PANGEA_NET_LIFECYCLE_BUFFER_SIZE 4096
#define PANGEA_NET_LIFECYCLE_BACKLOG_CAPACITY 64
#define PANGEA_NET_LIFECYCLE_RESULT_PLAYERS 2

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
	PangeaNetResultPlayer players[PANGEA_NET_LIFECYCLE_RESULT_PLAYERS];
	int32_t fromHost;
} PangeaNetMatchResultPacket;

typedef struct
{
	int byteCount;
	uint8_t bytes[PANGEA_NET_LIFECYCLE_BUFFER_SIZE];
} PangeaNetQueuedPacket;

static PangeaNetQueuedPacket gPangeaNetBacklog[PANGEA_NET_LIFECYCLE_BACKLOG_CAPACITY];
static int gPangeaNetBacklogHead = 0;
static int gPangeaNetBacklogTail = 0;
static uint32_t gPangeaNetLifecycleSequence = 0;
static int gPangeaNetLastSentLifecycleReason = PANGEA_NET_MATCH_STATE_NONE;
static int gPangeaNetRemoteLifecycleReason = PANGEA_NET_MATCH_STATE_NONE;
static uint32_t gPangeaNetRemoteLifecycleSequence = 0;
static int gPangeaNetLastMatchEndReason = PANGEA_NET_MATCH_STATE_NONE;
static int gPangeaNetHasMatchResult = 0;
static int gPangeaNetResultPublished = 0;

static const char* PangeaNetBridge_GetLobbyIdString(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_GetLobbyIdString();
#else
	return "00000000-0000-0000-0000-000000000000";
#endif
}

static const char* PangeaNetBridge_GetMatchIdString(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_GetMatchIdString();
#else
	return "00000000-0000-0000-0000-000000000000";
#endif
}

static bool PangeaNetBacklogIsFull(void)
{
	return ((gPangeaNetBacklogTail + 1) % PANGEA_NET_LIFECYCLE_BACKLOG_CAPACITY) == gPangeaNetBacklogHead;
}

static bool PangeaNetBacklogIsEmpty(void)
{
	return gPangeaNetBacklogHead == gPangeaNetBacklogTail;
}

static void PangeaNetBacklogPush(const void* bytes, int byteCount)
{
	if (!bytes || byteCount <= 0 || byteCount > PANGEA_NET_LIFECYCLE_BUFFER_SIZE)
	{
		return;
	}

	if (PangeaNetBacklogIsFull())
	{
		gPangeaNetBacklogHead = (gPangeaNetBacklogHead + 1) % PANGEA_NET_LIFECYCLE_BACKLOG_CAPACITY;
	}

	PangeaNetQueuedPacket* slot = &gPangeaNetBacklog[gPangeaNetBacklogTail];
	slot->byteCount = byteCount;
	SDL_memcpy(slot->bytes, bytes, (size_t) byteCount);
	gPangeaNetBacklogTail = (gPangeaNetBacklogTail + 1) % PANGEA_NET_LIFECYCLE_BACKLOG_CAPACITY;
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

	SDL_memcpy(outBytes, slot->bytes, (size_t) slot->byteCount);
	gPangeaNetBacklogHead = (gPangeaNetBacklogHead + 1) % PANGEA_NET_LIFECYCLE_BACKLOG_CAPACITY;
	return slot->byteCount;
}

static int PangeaNetBuildMatchResultPacket(int reason, PangeaNetMatchResultPacket* outPacket)
{
	if (!outPacket)
	{
		return 0;
	}

	const int playerCount = SDL_clamp(PangeaNetBridge_GetPlayerCount(), 1, PANGEA_NET_LIFECYCLE_RESULT_PLAYERS);
	int winnerPlayerIndex = -1;
	int bestPlacement = 9999;
	for (int i = 0; i < playerCount; i++)
	{
		const int placement = (int) gPlayerInfo[i].place;
		const int normalizedPlacement = placement > 0 ? placement : i + 1;
		if (normalizedPlacement < bestPlacement)
		{
			bestPlacement = normalizedPlacement;
			winnerPlayerIndex = i;
		}
	}

	outPacket->magic = PANGEA_NET_LIFECYCLE_MAGIC;
	outPacket->version = PANGEA_NET_LIFECYCLE_VERSION;
	outPacket->messageType = PANGEA_NET_LIFECYCLE_MATCH_RESULT;
	outPacket->sequence = ++gPangeaNetLifecycleSequence;
	outPacket->matchIdLow = PangeaNetBridge_GetMatchIdLow();
	outPacket->matchIdHigh = PangeaNetBridge_GetMatchIdHigh();
	outPacket->seed = PangeaNetBridge_GetMatchSeed();
	outPacket->endReason = reason;
	outPacket->winnerPlayerIndex = winnerPlayerIndex;
	outPacket->winningTeam = -1;
	outPacket->mode = gGameMode;
	outPacket->trackOrLevel = gTrackNum;
	outPacket->playerCount = playerCount;
	outPacket->fromHost = 1;
	SDL_memset(outPacket->players, 0, sizeof(outPacket->players));

	for (int i = 0; i < playerCount; i++)
	{
		PangeaNetResultPlayer* resultPlayer = &outPacket->players[i];
		const int placement = (int) gPlayerInfo[i].place;
		resultPlayer->playerIndex = (uint8_t) i;
		resultPlayer->placement = (uint8_t) (placement > 0 ? placement : i + 1);
		resultPlayer->finished = gPlayerInfo[i].raceComplete ? 1u : 0u;
		resultPlayer->eliminated = gPlayerInfo[i].isEliminated ? 1u : 0u;
		resultPlayer->score = (int16_t) gCapturedFlagCount[gPlayerInfo[i].team & 1];
		resultPlayer->lapsCompleted = (uint16_t) SDL_max(0, gPlayerInfo[i].lapNum);
		resultPlayer->checkpoint = (uint16_t) SDL_max(0, gPlayerInfo[i].checkpointNum);
	}

	return 1;
}

static const char* PangeaNetEndReasonName(int reason)
{
	return reason == PANGEA_NET_MATCH_STATE_GAME_OVER ? "game-over" : "level-completed";
}

static void PangeaNetEmitMatchResultJson(const PangeaNetMatchResultPacket* packet)
{
	if (!packet || packet->playerCount <= 0)
	{
		return;
	}

	const char* modeName = "multiplayerRace";
	switch (packet->mode)
	{
		case GAME_MODE_TAG1:
			modeName = "multiplayerTag1";
			break;
		case GAME_MODE_TAG2:
			modeName = "multiplayerTag2";
			break;
		case GAME_MODE_SURVIVAL:
			modeName = "multiplayerSurvival";
			break;
		case GAME_MODE_CAPTUREFLAG:
			modeName = "multiplayerQuestForFire";
			break;
		case GAME_MODE_MULTIPLAYERRACE:
		default:
			modeName = "multiplayerRace";
			break;
	}

	char json[1024];
	char playersJson[768];
	char placementsJson[128];
	playersJson[0] = '\0';
	placementsJson[0] = '\0';
	for (int i = 0; i < packet->playerCount && i < PANGEA_NET_LIFECYCLE_RESULT_PLAYERS; i++)
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
			(int) (gPlayerInfo[i].team & 1),
			(int) player->placement,
			player->finished ? "true" : "false",
			player->eliminated ? "true" : "false",
			(int) player->score,
			(int) player->lapsCompleted,
			(int) player->checkpoint);
		SDL_strlcat(playersJson, entry, sizeof(playersJson));
		SDL_snprintf(
			placementEntry,
			(int) sizeof(placementEntry),
			"%s%d",
			i > 0 ? "," : "",
			(int) player->placement);
		SDL_strlcat(placementsJson, placementEntry, sizeof(placementsJson));
	}

	const char* lobbyId = PangeaNetBridge_GetLobbyIdString();
	const char* matchId = PangeaNetBridge_GetMatchIdString();
	if (!lobbyId || lobbyId[0] == '\0')
	{
		lobbyId = "00000000-0000-0000-0000-000000000000";
	}
	if (!matchId || matchId[0] == '\0')
	{
		matchId = "00000000-0000-0000-0000-000000000000";
	}
	char trackOrLevel[64];
	SDL_snprintf(trackOrLevel, (int) sizeof(trackOrLevel), "track-%d", packet->trackOrLevel);

	SDL_snprintf(
		json,
		(int) sizeof(json),
		"{\"lobbyId\":\"%s\",\"matchId\":\"%s\",\"gameId\":\"cromagrally\",\"mode\":\"%s\",\"trackOrLevel\":\"%s\",\"seed\":%u,\"endedAt\":\"1970-01-01T00:00:00Z\",\"endReason\":\"%s\",\"winnerPlayerIndex\":%d,\"winningTeam\":\"none\",\"placements\":[%s],\"players\":[%s]}",
		lobbyId,
		matchId,
		modeName,
		trackOrLevel,
		(unsigned) packet->seed,
		PangeaNetEndReasonName(packet->endReason),
		packet->winnerPlayerIndex,
		placementsJson,
		playersJson);

	gPangeaNetHasMatchResult = 1;
	gPangeaNetLastMatchEndReason = packet->endReason;
	PangeaNetBridge_ReportMatchResult(json);
}

void PangeaNetBridge_ResetMatchLifecycle(void)
{
	gPangeaNetLifecycleSequence = 0;
	gPangeaNetLastSentLifecycleReason = PANGEA_NET_MATCH_STATE_NONE;
	gPangeaNetRemoteLifecycleReason = PANGEA_NET_MATCH_STATE_NONE;
	gPangeaNetRemoteLifecycleSequence = 0;
	gPangeaNetLastMatchEndReason = PANGEA_NET_MATCH_STATE_NONE;
	gPangeaNetHasMatchResult = 0;
	gPangeaNetResultPublished = 0;
	gPangeaNetBacklogHead = 0;
	gPangeaNetBacklogTail = 0;
}

int PangeaNetBridge_IsEnabled(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_IsEnabled();
#else
	return 0;
#endif
}

int PangeaNetBridge_IsHost(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_IsHost();
#else
	return 1;
#endif
}

int PangeaNetBridge_GetLocalPlayerIndex(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_GetLocalPlayerIndex();
#else
	return 0;
#endif
}

int PangeaNetBridge_GetPlayerCount(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_GetPlayerCount();
#else
	return 1;
#endif
}

uint32_t PangeaNetBridge_GetMatchSeed(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_GetMatchSeed();
#else
	return 1;
#endif
}

uint32_t PangeaNetBridge_GetMatchIdLow(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_GetMatchIdLow();
#else
	return 1;
#endif
}

uint32_t PangeaNetBridge_GetMatchIdHigh(void)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_GetMatchIdHigh();
#else
	return 0;
#endif
}

int PangeaNetBridge_SendReliable(const void* bytes, int byteCount)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_SendReliable(bytes, byteCount);
#else
	(void) bytes;
	(void) byteCount;
	return 0;
#endif
}

int PangeaNetBridge_SendUnreliable(const void* bytes, int byteCount)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_SendUnreliable(bytes, byteCount);
#else
	(void) bytes;
	(void) byteCount;
	return 0;
#endif
}

int PangeaNetBridge_PollMessage(void* outBytes, int maxByteCount)
{
	const int queued = PangeaNetBacklogPop(outBytes, maxByteCount);
	if (queued > 0)
	{
		return queued;
	}
#ifdef __EMSCRIPTEN__
	return PangeaNet_PollMessage(outBytes, maxByteCount);
#else
	(void) outBytes;
	(void) maxByteCount;
	return 0;
#endif
}

void PangeaNetBridge_ReportDesync(uint32_t frame, uint32_t localHash, uint32_t remoteHash)
{
#ifdef __EMSCRIPTEN__
	PangeaNet_ReportDesync(frame, localHash, remoteHash);
#else
	(void) frame;
	(void) localHash;
	(void) remoteHash;
#endif
}

void PangeaNetBridge_ReportMatchEnded(int reason)
{
#ifdef __EMSCRIPTEN__
	PangeaNet_ReportMatchEnded(reason);
#else
	(void) reason;
#endif
}

void PangeaNetBridge_ReportMatchResult(const char* resultJson)
{
#ifdef __EMSCRIPTEN__
	if (resultJson)
	{
		PangeaNet_ReportMatchResult(resultJson);
	}
#else
	(void) resultJson;
#endif
}

int PangeaNetBridge_GetRemoteLifecycleReason(void)
{
	return gPangeaNetRemoteLifecycleReason;
}

int PangeaNetBridge_GetLastMatchEndReason(void)
{
	return gPangeaNetLastMatchEndReason;
}

int PangeaNetBridge_HasMatchResult(void)
{
	return gPangeaNetHasMatchResult;
}

void PangeaNetBridge_UpdateMatchLifecycle(void)
{
	if (!PangeaNetBridge_IsEnabled())
	{
		return;
	}

	uint8_t payload[PANGEA_NET_LIFECYCLE_BUFFER_SIZE];
	for (;;)
	{
		const int byteCount = PangeaNetBridge_PollMessage(payload, (int) sizeof(payload));
		if (byteCount <= 0)
		{
			break;
		}

		if (byteCount == (int) sizeof(PangeaNetLifecyclePacket))
		{
			const PangeaNetLifecyclePacket* packet = (const PangeaNetLifecyclePacket*) payload;
			if (packet->magic == PANGEA_NET_LIFECYCLE_MAGIC
				&& packet->version == PANGEA_NET_LIFECYCLE_VERSION
				&& packet->messageType == PANGEA_NET_LIFECYCLE_MATCH_END)
			{
				if (!PangeaNetBridge_IsHost()
					&& packet->fromHost != 0
					&& packet->sequence > gPangeaNetRemoteLifecycleSequence)
				{
					gPangeaNetRemoteLifecycleSequence = packet->sequence;
					gPangeaNetRemoteLifecycleReason = packet->reason;
					PangeaNetBridge_ReportMatchEnded(packet->reason);
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
				if (!PangeaNetBridge_IsHost()
					&& packet->fromHost != 0
					&& packet->sequence > gPangeaNetRemoteLifecycleSequence)
				{
					gPangeaNetRemoteLifecycleSequence = packet->sequence;
					PangeaNetEmitMatchResultJson(packet);
				}
				continue;
			}
		}

		PangeaNetBacklogPush(payload, byteCount);
		break;
	}
}

void PangeaNetBridge_PublishLocalMatchLifecycle(void)
{
	if (!PangeaNetBridge_IsEnabled() || !PangeaNetBridge_IsHost())
	{
		return;
	}

	int reason = PANGEA_NET_MATCH_STATE_NONE;
	if (gGameOver)
	{
		reason = PANGEA_NET_MATCH_STATE_GAME_OVER;
	}
	else if (gTrackCompleted)
	{
		reason = PANGEA_NET_MATCH_STATE_TRACK_COMPLETED;
	}

	if (reason == PANGEA_NET_MATCH_STATE_NONE || reason == gPangeaNetLastSentLifecycleReason)
	{
		return;
	}

	PangeaNetLifecyclePacket packet =
	{
		.magic = PANGEA_NET_LIFECYCLE_MAGIC,
		.version = PANGEA_NET_LIFECYCLE_VERSION,
		.messageType = PANGEA_NET_LIFECYCLE_MATCH_END,
		.sequence = ++gPangeaNetLifecycleSequence,
		.reason = reason,
		.fromHost = 1,
	};

	if (PangeaNetBridge_SendReliable(&packet, (int) sizeof(packet)) != 0)
	{
		gPangeaNetLastSentLifecycleReason = reason;
		gPangeaNetLastMatchEndReason = reason;
		PangeaNetBridge_ReportMatchEnded(reason);
		if (!gPangeaNetResultPublished)
		{
			PangeaNetMatchResultPacket resultPacket;
			if (PangeaNetBuildMatchResultPacket(reason, &resultPacket))
			{
				if (PangeaNetBridge_SendReliable(&resultPacket, (int) sizeof(resultPacket)) != 0)
				{
					gPangeaNetResultPublished = 1;
					PangeaNetEmitMatchResultJson(&resultPacket);
				}
			}
		}
	}
}

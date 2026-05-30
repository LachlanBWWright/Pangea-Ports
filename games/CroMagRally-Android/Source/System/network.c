/****************************/
/*   	  NETWORK.C	   	    */
/* (c)2000 Pangea Software  */
/* By Brian Greenstone      */
/****************************/

typedef void* NSpMessageHeader;
typedef void* NSpPlayerLeftMessage;


/***************/
/* EXTERNALS   */
/***************/

#include "game.h"
#include "network.h"
#include "window.h"
#include "pangea_net.h"
#include <math.h>

/**********************/
/*     PROTOTYPES     */
/**********************/

#if 0
static void InitPlayerNamesListBox(Rect *r, WindowPtr myDialog);
//static pascal Boolean GatherGameDialogCallback (DialogRef dp,EventRecord *event, short *item);
static void ShowNamesOfJoinedPlayers(void);
static OSErr Client_WaitForGameConfigInfo(void);
//static pascal Boolean Client_WaitForGameConfigInfoDialogCallback (DialogRef dp,EventRecord *event, short *item);
static OSErr HostSendGameConfigInfo(void);
static void HandleGameConfigMessage(NetConfigMessageType *inMessage);
//static Boolean HandleOtherNetMessage(NSpMessageHeader	*message);
//static void PlayerUnexpectedlyLeavesGame(NSpPlayerLeftMessage *mess);
static OSErr  Host_DoGatherPlayersDialog(void);
static Boolean PlayerReceiveVehicleTypeFromOthers(short *playerNum, short *charType, short *sex);
#endif

/****************************/
/*    CONSTANTS             */
/****************************/

#define	DATA_TIMEOUT	2						// # seconds for data to timeout

#define	kNBPType		"CMR5"

/**********************/
/*     VARIABLES      */
/**********************/

static int	gNumGatheredPlayers = 0;			// this is only used during gathering, gNumRealPlayers should be used during game!

Boolean		gNetSprocketInitialized = false;

Boolean		gIsNetworkHost = false;
Boolean		gIsNetworkClient = false;
Boolean		gNetGameInProgress = false;

void* /*NSpGameReference*/	gNetGame = nil;

#if 0
static Str31				gameName;
static Str31				gNetPlayerName;
static Str31				password;
static Str31				kJoinDialogLabel = "Choose a Game:";
#endif

//ListHandle		gTheList;
//short			gNumRowsInList;
Str32			gPlayerNameStrings[MAX_PLAYERS];

uint32_t			gClientSendCounter[MAX_PLAYERS];
uint32_t			gHostSendCounter;
int				gTimeoutCounter;

//NetHostControlInfoMessageType	gHostOutMess;
//NetClientControlInfoMessageType	gClientOutMess;

Boolean		gHostNetworkGame = false;
Boolean		gJoinNetworkGame = false;

#ifdef __EMSCRIPTEN__
#define PANGEA_NET_MAGIC 0x54454E50u /* 'PNET' little-endian */
	#define PANGEA_NET_VERSION 7u
#define PANGEA_NET_PLAYER_NA 0xFFFFu
#define PANGEA_NET_MAX_PACKET_SIZE 4096
#define PANGEA_NET_LOCAL_RECONCILE_SNAP_DISTANCE 6.0f
#define PANGEA_NET_LOCAL_RECONCILE_BLEND 0.35f
#define PANGEA_NET_REMOTE_INTERPOLATION_BLEND 0.3f
#define PANGEA_NET_REMOTE_TELEPORT_DISTANCE 180.0f
#define PANGEA_NET_INPUT_TIMEOUT_MS 750.0
#define PANGEA_NET_INPUT_DISCONNECT_MS 2500.0
#define PANGEA_NET_INPUT_HISTORY_CAP 64
#define PANGEA_NET_REMOTE_BUFFER_CAP 8
#define PANGEA_NET_REMOTE_INTERP_DELAY 2
#define PANGEA_NET_KEYFRAME_INTERVAL_FRAMES 20u
#define PANGEA_NET_KEYFRAME_TIMEOUT_MS 1200.0

enum
{
	kPangeaPacketMatchConfig = 1,
	kPangeaPacketClientInput = 2,
	kPangeaPacketHostSnapshot = 3,
	kPangeaPacketReliableEvent = 4,
	kPangeaPacketClientAck = 5,
	kPangeaPacketPause = 6,
	kPangeaPacketResume = 7,
	kPangeaPacketDisconnect = 8,
	kPangeaPacketProtocolError = 9,
	kPangeaPacketVehicleType = 10,
	kPangeaPacketKeyframeResendRequest = 11,
	kPangeaPacketTagHandoffRequest = 12
};

enum
{
	kPangeaReliableEventRaceComplete = 1,
	kPangeaReliableEventEliminated = 2,
	kPangeaReliableEventTagHandoff = 3
};

#define PANGEA_NET_TAG_SPAZ_TIMER 3.0f

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
} PangeaNetPacketHeader;

typedef struct
{
	uint8_t* bytes;
	int byteCount;
	int cursor;
	int ok;
} PangeaNetWriter;

typedef struct
{
	const uint8_t* bytes;
	int byteCount;
	int cursor;
	int ok;
} PangeaNetReader;

typedef struct
{
	uint32_t lastReceivedInputTick;
	uint32_t lastAckedSnapshotTick;
	uint32_t lastSentSequence;
	uint32_t lastReceivedSequence;
	float rttMs;
	float jitterMs;
	uint32_t inputBufferLength;
	uint8_t timeoutState;
	double lastInputTimeMs;
} PangeaNetPlayerConnState;

typedef PangeaNetPlayerCarState PangeaNetSnapshotPlayerState;

typedef struct
{
	PangeaNetSnapshotPlayerState fromState;
	PangeaNetSnapshotPlayerState toState;
	Boolean hasFrom;
	Boolean hasTo;
} PangeaNetRemoteInterpState;

typedef struct
{
	uint32_t sequence;
	uint32_t controlBits;
	uint32_t controlBitsNew;
	OGLVector2D analogSteering;
} PangeaNetInputHistoryEntry;

static short gPendingVehicleType[MAX_PLAYERS];
static short gPendingVehicleSex[MAX_PLAYERS];
static Boolean gHavePendingVehicleType[MAX_PLAYERS];
static uint32_t gExpectedMatchSeed = 1;
static PangeaNetHostSnapshotPacket gPendingSnapshot;
static Boolean gHavePendingSnapshot = false;
static uint32_t gPangeaMatchId = 1;
static uint32_t gPangeaMatchIdHigh = 0;
static uint32_t gPangeaLocalTick = 0;
static uint32_t gPangeaSendSequence = 1;
static uint32_t gPangeaLastHostSnapshotSeq = 0;
static float gPangeaCorrectionDistance = 0.0f;
static uint32_t gPangeaDroppedPacketCount = 0;
static uint32_t gPangeaReorderedPacketCount = 0;
static uint32_t gPangeaLastHostKeyframeSeq = 0;
static uint32_t gPangeaLastHostDeltaSeq = 0;
static uint32_t gPangeaLastSnapshotStateHash = 0;
static uint8_t gPangeaForceKeyframe = 0;
static double gPangeaLastReceivedKeyframeAtMs = 0.0;
static double gPangeaLastResendRequestAtMs = 0.0;
static PangeaNetPlayerConnState gPangeaConnState[MAX_PLAYERS];
static PangeaNetRemoteInterpState gPangeaRemoteInterp[MAX_PLAYERS];
static PangeaNetInputHistoryEntry gPangeaLocalInputHistory[PANGEA_NET_INPUT_HISTORY_CAP];
static uint32_t gPangeaLocalInputHistoryHead = 0;
static uint32_t gPangeaLocalInputHistoryCount = 0;
static PangeaNetSnapshotPlayerState gPangeaRemoteSnapshotBuffer[MAX_PLAYERS][PANGEA_NET_REMOTE_BUFFER_CAP];
static uint32_t gPangeaRemoteSnapshotHead[MAX_PLAYERS] = {0};
static uint32_t gPangeaRemoteSnapshotCount[MAX_PLAYERS] = {0};
static uint8_t gPangeaReliableRaceCompleteSent[MAX_PLAYERS] = {0};
static uint8_t gPangeaReliableEliminatedSent[MAX_PLAYERS] = {0};
static uint32_t gPangeaLastAckedKeyframeSeq[MAX_PLAYERS] = {0};
static uint32_t gPangeaLastAckedDeltaSeq[MAX_PLAYERS] = {0};

extern short gNumTorches;
extern ObjNode *gTorchObjs[];

#define PANGEA_NET_MAX_TORCHES 12

static uint8_t gPendingTorchCount = 0;
static uint8_t gPendingTorchMode[PANGEA_NET_MAX_TORCHES];
static uint8_t gPendingTorchCarrier[PANGEA_NET_MAX_TORCHES];
static float gPendingTorchCoordX[PANGEA_NET_MAX_TORCHES];
static float gPendingTorchCoordY[PANGEA_NET_MAX_TORCHES];
static float gPendingTorchCoordZ[PANGEA_NET_MAX_TORCHES];
static Boolean gHavePendingTorchState = false;
static short gPangeaPrevWhoIsIt = -1;

void PangeaNetBridge_SetRuntimeMatchIdentity(uint32_t matchIdLow, uint32_t matchIdHigh)
{
	gPangeaMatchId = matchIdLow;
	gPangeaMatchIdHigh = matchIdHigh;
	PangeaNetBridge_ResetMatchLifecycle();
}

static Boolean PangeaNet_ShouldLogSequence(uint32_t sequence)
{
	return sequence <= 5 || (sequence % 60u) == 0u;
}

static uint32_t PangeaNet_HashMix(uint32_t hash, uint32_t value)
{
	hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
	return hash;
}

static uint32_t PangeaNet_HashF32(float value)
{
	uint32_t bits = 0;
	SDL_memcpy(&bits, &value, sizeof(bits));
	return bits;
}

static uint32_t PangeaNet_ComputeAuthoritativeStateHash(short playerCount)
{
	uint32_t hash = 0x811C9DC5u;
	const short clampedPlayerCount = playerCount > MAX_PLAYERS ? MAX_PLAYERS : playerCount;
	hash = PangeaNet_HashMix(hash, (uint32_t)clampedPlayerCount);
	hash = PangeaNet_HashMix(hash, (uint32_t)gGameMode);
	hash = PangeaNet_HashMix(hash, (uint32_t)gTrackCompleted);
	hash = PangeaNet_HashMix(hash, (uint32_t)gWhoIsIt);
	hash = PangeaNet_HashMix(hash, (uint32_t)gWhoWasIt);
	hash = PangeaNet_HashMix(hash, (uint32_t)gCapturedFlagCount[0]);
	hash = PangeaNet_HashMix(hash, (uint32_t)gCapturedFlagCount[1]);
	hash = PangeaNet_HashMix(hash, (uint32_t)gNumPlayersEliminated);
	hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gReTagTimer));
	for (short i = 0; i < clampedPlayerCount; i++)
	{
		hash = PangeaNet_HashMix(hash, (uint32_t)i);
		hash = PangeaNet_HashMix(hash, (uint32_t)gPlayerInfo[i].lapNum);
		hash = PangeaNet_HashMix(hash, (uint32_t)gPlayerInfo[i].checkpointNum);
		hash = PangeaNet_HashMix(hash, (uint32_t)gPlayerInfo[i].place);
		hash = PangeaNet_HashMix(hash, (uint32_t)(gPlayerInfo[i].raceComplete ? 1 : 0));
		hash = PangeaNet_HashMix(hash, (uint32_t)(gPlayerInfo[i].wrongWay ? 1 : 0));
		hash = PangeaNet_HashMix(hash, (uint32_t)(gPlayerInfo[i].isEliminated ? 1 : 0));
		hash = PangeaNet_HashMix(hash, (uint32_t)(gPlayerInfo[i].isIt ? 1 : 0));
		hash = PangeaNet_HashMix(hash, (uint32_t)(gPlayerInfo[i].movingBackwards ? 1 : 0));
		hash = PangeaNet_HashMix(hash, (uint32_t)(gPlayerInfo[i].accelBackwards ? 1 : 0));
		hash = PangeaNet_HashMix(hash, (uint32_t)(gPlayerInfo[i].braking ? 1 : 0));
		hash = PangeaNet_HashMix(hash, (uint32_t)(gPlayerInfo[i].onWater ? 1 : 0));
		hash = PangeaNet_HashMix(hash, (uint32_t)gPlayerInfo[i].powType);
		hash = PangeaNet_HashMix(hash, (uint32_t)gPlayerInfo[i].powQuantity);
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].objNode ? gPlayerInfo[i].objNode->Rot.x : 0.0f));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].objNode ? gPlayerInfo[i].objNode->Rot.z : 0.0f));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].objNode ? gPlayerInfo[i].objNode->DeltaRot.x : 0.0f));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].objNode ? gPlayerInfo[i].objNode->DeltaRot.y : 0.0f));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].objNode ? gPlayerInfo[i].objNode->DeltaRot.z : 0.0f));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].currentRPM));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].skidDot));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].health));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].tagTimer));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].frozenTimer));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].greasedTiresTimer));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].nitroTimer));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].stickyTiresTimer));
		hash = PangeaNet_HashMix(hash, PangeaNet_HashF32(gPlayerInfo[i].invisibilityTimer));
	}
	for (int t = 0; t < gNumTorches && t < PANGEA_NET_MAX_TORCHES; t++)
	{
		const ObjNode* torch = gTorchObjs[t];
		if (torch)
		{
			hash = PangeaNet_HashMix(hash, (uint32_t)torch->Mode);
			hash = PangeaNet_HashMix(hash, (uint32_t)(torch->Mode == 1 ? (uint8_t)torch->PlayerNum : 0xFFu));
		}
	}
	return hash;
}

static void PangeaNet_RecordLocalInput(uint32_t sequence, short playerNum)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
	{
		return;
	}
	uint32_t slot = (gPangeaLocalInputHistoryHead + gPangeaLocalInputHistoryCount) % PANGEA_NET_INPUT_HISTORY_CAP;
	if (gPangeaLocalInputHistoryCount >= PANGEA_NET_INPUT_HISTORY_CAP)
	{
		gPangeaLocalInputHistoryHead = (gPangeaLocalInputHistoryHead + 1) % PANGEA_NET_INPUT_HISTORY_CAP;
		slot = (gPangeaLocalInputHistoryHead + gPangeaLocalInputHistoryCount - 1) % PANGEA_NET_INPUT_HISTORY_CAP;
	}
	else
	{
		gPangeaLocalInputHistoryCount++;
	}
	gPangeaLocalInputHistory[slot].sequence = sequence;
	gPangeaLocalInputHistory[slot].controlBits = gPlayerInfo[playerNum].controlBits;
	gPangeaLocalInputHistory[slot].controlBitsNew = gPlayerInfo[playerNum].controlBits_New;
	gPangeaLocalInputHistory[slot].analogSteering = gPlayerInfo[playerNum].analogSteering;
}

static void PangeaNet_ReapplyUnackedLocalInput(uint32_t lastProcessedInputSequence, short playerNum)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
	{
		return;
	}
	for (uint32_t i = 0; i < gPangeaLocalInputHistoryCount; i++)
	{
		const uint32_t slot = (gPangeaLocalInputHistoryHead + i) % PANGEA_NET_INPUT_HISTORY_CAP;
		const PangeaNetInputHistoryEntry* entry = &gPangeaLocalInputHistory[slot];
		if (entry->sequence <= lastProcessedInputSequence)
		{
			continue;
		}
		gPlayerInfo[playerNum].controlBits = entry->controlBits;
		gPlayerInfo[playerNum].controlBits_New = entry->controlBitsNew;
		gPlayerInfo[playerNum].analogSteering = entry->analogSteering;
	}
}

static void PangeaNet_PushRemoteSnapshot(short playerNum, const PangeaNetPlayerCarState* source)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS || !source)
	{
		return;
	}
	uint32_t slot = (gPangeaRemoteSnapshotHead[playerNum] + gPangeaRemoteSnapshotCount[playerNum]) % PANGEA_NET_REMOTE_BUFFER_CAP;
	if (gPangeaRemoteSnapshotCount[playerNum] >= PANGEA_NET_REMOTE_BUFFER_CAP)
	{
		gPangeaRemoteSnapshotHead[playerNum] = (gPangeaRemoteSnapshotHead[playerNum] + 1) % PANGEA_NET_REMOTE_BUFFER_CAP;
		slot = (gPangeaRemoteSnapshotHead[playerNum] + gPangeaRemoteSnapshotCount[playerNum] - 1) % PANGEA_NET_REMOTE_BUFFER_CAP;
	}
	else
	{
		gPangeaRemoteSnapshotCount[playerNum]++;
	}
	PangeaNetSnapshotPlayerState* target = &gPangeaRemoteSnapshotBuffer[playerNum][slot];
	SDL_memset(target, 0, sizeof(*target));
	target->coord = source->coord;
	target->rotX = source->rotX;
	target->rotY = source->rotY;
	target->rotZ = source->rotZ;
	target->delta = source->delta;
	target->deltaRot = source->deltaRot;
	target->steering = source->steering;
	target->currentThrust = source->currentThrust;
	target->currentRPM = source->currentRPM;
	target->skidDot = source->skidDot;
	target->controlBits = source->controlBits;
	target->controlBitsNew = source->controlBitsNew;
	target->analogSteering = source->analogSteering;
	target->lapNum = source->lapNum;
	target->checkpointNum = source->checkpointNum;
	target->place = source->place;
	target->raceComplete = source->raceComplete;
	target->wrongWay = source->wrongWay;
	target->isEliminated = source->isEliminated;
	target->powType = source->powType;
	target->powQuantity = source->powQuantity;
	target->health = source->health;
	target->tagTimer = source->tagTimer;
	target->frozenTimer = source->frozenTimer;
	target->greasedTiresTimer = source->greasedTiresTimer;
	target->nitroTimer = source->nitroTimer;
	target->stickyTiresTimer = source->stickyTiresTimer;
	target->invisibilityTimer = source->invisibilityTimer;
	target->isIt = source->isIt;
	target->movingBackwards = source->movingBackwards;
	target->accelBackwards = source->accelBackwards;
	target->braking = source->braking;
	target->onWater = source->onWater;
}

static int PangeaNet_GetDelayedRemoteSnapshot(short playerNum, PangeaNetSnapshotPlayerState* outSnapshot)
{
	if (playerNum < 0 || playerNum >= MAX_PLAYERS || !outSnapshot)
	{
		return 0;
	}
	const uint32_t count = gPangeaRemoteSnapshotCount[playerNum];
	if (count == 0)
	{
		return 0;
	}
	const uint32_t delayedOffset = count > PANGEA_NET_REMOTE_INTERP_DELAY
		? (count - 1 - PANGEA_NET_REMOTE_INTERP_DELAY)
		: (count - 1);
	const uint32_t slot = (gPangeaRemoteSnapshotHead[playerNum] + delayedOffset) % PANGEA_NET_REMOTE_BUFFER_CAP;
	*outSnapshot = gPangeaRemoteSnapshotBuffer[playerNum][slot];
	return 1;
}

static void PangeaNetWriter_Init(PangeaNetWriter* writer, void* bytes, int byteCount)
{
	writer->bytes = (uint8_t*)bytes;
	writer->byteCount = byteCount;
	writer->cursor = 0;
	writer->ok = 1;
}

static void PangeaNetReader_Init(PangeaNetReader* reader, const void* bytes, int byteCount)
{
	reader->bytes = (const uint8_t*)bytes;
	reader->byteCount = byteCount;
	reader->cursor = 0;
	reader->ok = 1;
}

static void PangeaNetWriter_WriteU8(PangeaNetWriter* writer, uint8_t value)
{
	if (!writer->ok || writer->cursor + 1 > writer->byteCount)
	{
		writer->ok = 0;
		return;
	}
	writer->bytes[writer->cursor++] = value;
}

static void PangeaNetWriter_WriteU16(PangeaNetWriter* writer, uint16_t value)
{
	if (!writer->ok || writer->cursor + 2 > writer->byteCount)
	{
		writer->ok = 0;
		return;
	}
	writer->bytes[writer->cursor++] = (uint8_t)(value & 0xFFu);
	writer->bytes[writer->cursor++] = (uint8_t)((value >> 8) & 0xFFu);
}

static void PangeaNetWriter_WriteU32(PangeaNetWriter* writer, uint32_t value)
{
	if (!writer->ok || writer->cursor + 4 > writer->byteCount)
	{
		writer->ok = 0;
		return;
	}
	writer->bytes[writer->cursor++] = (uint8_t)(value & 0xFFu);
	writer->bytes[writer->cursor++] = (uint8_t)((value >> 8) & 0xFFu);
	writer->bytes[writer->cursor++] = (uint8_t)((value >> 16) & 0xFFu);
	writer->bytes[writer->cursor++] = (uint8_t)((value >> 24) & 0xFFu);
}

static void PangeaNetWriter_WriteF32(PangeaNetWriter* writer, float value)
{
	uint32_t bits = 0;
	SDL_memcpy(&bits, &value, sizeof(bits));
	PangeaNetWriter_WriteU32(writer, bits);
}

static uint8_t PangeaNetReader_ReadU8(PangeaNetReader* reader)
{
	if (!reader->ok || reader->cursor + 1 > reader->byteCount)
	{
		reader->ok = 0;
		return 0;
	}
	return reader->bytes[reader->cursor++];
}

static uint16_t PangeaNetReader_ReadU16(PangeaNetReader* reader)
{
	const uint16_t lo = PangeaNetReader_ReadU8(reader);
	const uint16_t hi = PangeaNetReader_ReadU8(reader);
	return (uint16_t)(lo | (hi << 8));
}

static uint32_t PangeaNetReader_ReadU32(PangeaNetReader* reader)
{
	const uint32_t b0 = PangeaNetReader_ReadU8(reader);
	const uint32_t b1 = PangeaNetReader_ReadU8(reader);
	const uint32_t b2 = PangeaNetReader_ReadU8(reader);
	const uint32_t b3 = PangeaNetReader_ReadU8(reader);
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

static float PangeaNetReader_ReadF32(PangeaNetReader* reader)
{
	const uint32_t bits = PangeaNetReader_ReadU32(reader);
	float value = 0.0f;
	SDL_memcpy(&value, &bits, sizeof(value));
	return value;
}

static int PangeaNet_WriteHeader(PangeaNetWriter* writer, const PangeaNetPacketHeader* header)
{
	PangeaNetWriter_WriteU32(writer, header->magic);
	PangeaNetWriter_WriteU16(writer, header->version);
	PangeaNetWriter_WriteU16(writer, header->packetType);
	PangeaNetWriter_WriteU32(writer, header->matchIdLow);
	PangeaNetWriter_WriteU32(writer, header->matchIdHigh);
	PangeaNetWriter_WriteU32(writer, header->tick);
	PangeaNetWriter_WriteU32(writer, header->sequence);
	PangeaNetWriter_WriteU16(writer, header->playerIndex);
	PangeaNetWriter_WriteU16(writer, header->reserved);
	return writer->ok;
}

static int PangeaNet_ReadHeader(PangeaNetReader* reader, PangeaNetPacketHeader* outHeader)
{
	outHeader->magic = PangeaNetReader_ReadU32(reader);
	outHeader->version = PangeaNetReader_ReadU16(reader);
	outHeader->packetType = PangeaNetReader_ReadU16(reader);
	outHeader->matchIdLow = PangeaNetReader_ReadU32(reader);
	outHeader->matchIdHigh = PangeaNetReader_ReadU32(reader);
	outHeader->tick = PangeaNetReader_ReadU32(reader);
	outHeader->sequence = PangeaNetReader_ReadU32(reader);
	outHeader->playerIndex = PangeaNetReader_ReadU16(reader);
	outHeader->reserved = PangeaNetReader_ReadU16(reader);
	return reader->ok;
}

static void PangeaNet_SendReliableEvent(uint8_t eventType, short playerNum)
{
	uint8_t bytes[128];
	PangeaNetWriter writer;
	PangeaNetPacketHeader header;
	header.magic = PANGEA_NET_MAGIC;
	header.version = PANGEA_NET_VERSION;
	header.packetType = kPangeaPacketReliableEvent;
	header.matchIdLow = gPangeaMatchId;
	header.matchIdHigh = gPangeaMatchIdHigh;
	header.tick = gPangeaLocalTick;
	header.sequence = gPangeaSendSequence++;
	header.playerIndex = PANGEA_NET_PLAYER_NA;
	header.reserved = 0;
	PangeaNetWriter_Init(&writer, bytes, (int)sizeof(bytes));
	PangeaNet_WriteHeader(&writer, &header);
	PangeaNetWriter_WriteU8(&writer, eventType);
	PangeaNetWriter_WriteU8(&writer, (uint8_t)playerNum);
	PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[playerNum].lapNum);
	PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[playerNum].place);
	PangeaNetWriter_WriteU8(&writer, gPlayerInfo[playerNum].raceComplete ? 1 : 0);
	PangeaNetWriter_WriteU8(&writer, gPlayerInfo[playerNum].isEliminated ? 1 : 0);
	if (!writer.ok)
	{
		return;
	}
	PangeaNetBridge_SendReliable(bytes, writer.cursor);
}

static void PangeaNet_SendTagHandoffEvent(void)
{
	uint8_t bytes[128];
	PangeaNetWriter writer;
	PangeaNetPacketHeader header;
	header.magic = PANGEA_NET_MAGIC;
	header.version = PANGEA_NET_VERSION;
	header.packetType = kPangeaPacketReliableEvent;
	header.matchIdLow = gPangeaMatchId;
	header.matchIdHigh = gPangeaMatchIdHigh;
	header.tick = gPangeaLocalTick;
	header.sequence = gPangeaSendSequence++;
	header.playerIndex = PANGEA_NET_PLAYER_NA;
	header.reserved = 0;
	PangeaNetWriter_Init(&writer, bytes, (int)sizeof(bytes));
	PangeaNet_WriteHeader(&writer, &header);
	PangeaNetWriter_WriteU8(&writer, kPangeaReliableEventTagHandoff);
	PangeaNetWriter_WriteU8(&writer, (uint8_t)gWhoIsIt);
	PangeaNetWriter_WriteU8(&writer, (uint8_t)gWhoWasIt);
	PangeaNetWriter_WriteU8(&writer, 0);
	PangeaNetWriter_WriteF32(&writer, gReTagTimer);
	if (!writer.ok)
	{
		return;
	}
	PangeaNetBridge_SendReliable(bytes, writer.cursor);
}

static void PangeaNet_SetTaggedPlayer(short playerNum)
{
	for (short i = 0; i < MAX_PLAYERS; i++)
	{
		gPlayerInfo[i].isIt = i == playerNum;
	}

	gWhoIsIt = playerNum;
}

static Boolean PangeaNet_ApplyTagHandoff(short fromPlayer, short toPlayer)
{
	if (fromPlayer < 0 || fromPlayer >= MAX_PLAYERS || toPlayer < 0 || toPlayer >= MAX_PLAYERS)
	{
		return false;
	}
	if (gGameMode != GAME_MODE_TAG1 && gGameMode != GAME_MODE_TAG2)
	{
		return false;
	}
	if (gTrackCompleted || gWhoIsIt != fromPlayer)
	{
		return false;
	}
	if ((toPlayer == gWhoWasIt) && (gReTagTimer > 0.0f))
	{
		return false;
	}

	gWhoWasIt = fromPlayer;
	PangeaNet_SetTaggedPlayer(toPlayer);
	gReTagTimer = PANGEA_NET_TAG_SPAZ_TIMER;
	PangeaNet_ForceKeyframe();
	return true;
}

static void PangeaNet_SetRemoteInterpTarget(short playerNum, const PangeaNetPlayerCarState* source)
{
	PangeaNetRemoteInterpState* interp = &gPangeaRemoteInterp[playerNum];
	if (interp->hasTo)
	{
		interp->fromState = interp->toState;
		interp->hasFrom = true;
	}
	else
	{
		SDL_memset(&interp->fromState, 0, sizeof(interp->fromState));
		interp->fromState.coord = source->coord;
		interp->fromState.rotX = source->rotX;
		interp->fromState.rotY = source->rotY;
		interp->fromState.rotZ = source->rotZ;
		interp->fromState.delta = source->delta;
		interp->fromState.deltaRot = source->deltaRot;
		interp->fromState.steering = source->steering;
		interp->fromState.currentThrust = source->currentThrust;
		interp->fromState.currentRPM = source->currentRPM;
		interp->fromState.skidDot = source->skidDot;
		interp->fromState.controlBits = source->controlBits;
		interp->fromState.controlBitsNew = source->controlBitsNew;
		interp->fromState.analogSteering = source->analogSteering;
		interp->fromState.lapNum = source->lapNum;
		interp->fromState.checkpointNum = source->checkpointNum;
		interp->fromState.place = source->place;
		interp->fromState.raceComplete = source->raceComplete;
		interp->fromState.wrongWay = source->wrongWay;
		interp->fromState.powType = source->powType;
		interp->fromState.powQuantity = source->powQuantity;
		interp->fromState.health = source->health;
		interp->fromState.tagTimer = source->tagTimer;
		interp->fromState.isIt = source->isIt;
		interp->fromState.isEliminated = 0;
		interp->fromState.frozenTimer = 0.0f;
		interp->fromState.greasedTiresTimer = 0.0f;
		interp->fromState.nitroTimer = 0.0f;
		interp->fromState.stickyTiresTimer = 0.0f;
		interp->fromState.invisibilityTimer = 0.0f;
		interp->fromState.movingBackwards = source->movingBackwards;
		interp->fromState.accelBackwards = source->accelBackwards;
		interp->fromState.braking = source->braking;
		interp->fromState.onWater = source->onWater;
		interp->hasFrom = true;
	}

	SDL_memset(&interp->toState, 0, sizeof(interp->toState));
	interp->toState.coord = source->coord;
	interp->toState.rotX = source->rotX;
	interp->toState.rotY = source->rotY;
	interp->toState.rotZ = source->rotZ;
	interp->toState.delta = source->delta;
	interp->toState.deltaRot = source->deltaRot;
	interp->toState.steering = source->steering;
	interp->toState.currentThrust = source->currentThrust;
	interp->toState.currentRPM = source->currentRPM;
	interp->toState.skidDot = source->skidDot;
	interp->toState.controlBits = source->controlBits;
	interp->toState.controlBitsNew = source->controlBitsNew;
	interp->toState.analogSteering = source->analogSteering;
	interp->toState.lapNum = source->lapNum;
	interp->toState.checkpointNum = source->checkpointNum;
	interp->toState.place = source->place;
	interp->toState.raceComplete = source->raceComplete;
	interp->toState.wrongWay = source->wrongWay;
	interp->toState.powType = source->powType;
	interp->toState.powQuantity = source->powQuantity;
	interp->toState.health = source->health;
	interp->toState.tagTimer = source->tagTimer;
	interp->toState.isIt = source->isIt;
	interp->toState.isEliminated = 0;
	interp->toState.frozenTimer = 0.0f;
	interp->toState.greasedTiresTimer = 0.0f;
	interp->toState.nitroTimer = 0.0f;
	interp->toState.stickyTiresTimer = 0.0f;
	interp->toState.invisibilityTimer = 0.0f;
	interp->toState.movingBackwards = source->movingBackwards;
	interp->toState.accelBackwards = source->accelBackwards;
	interp->toState.braking = source->braking;
	interp->toState.onWater = source->onWater;
	interp->hasTo = true;
}

static void HandlePangeaNetMessage(const void* bytes, int byteCount)
{
	PangeaNetReader reader;
	PangeaNetPacketHeader header;
	PangeaNetReader_Init(&reader, bytes, byteCount);

	if (!PangeaNet_ReadHeader(&reader, &header))
	{
		SDL_Log("CroMag net: malformed packet (header)");
		return;
	}

	if (header.magic != PANGEA_NET_MAGIC || header.version != PANGEA_NET_VERSION)
	{
		gPangeaDroppedPacketCount++;
		SDL_Log("CroMag net: drop packet bad header magic=%08x version=%u bytes=%d drops=%u",
			(unsigned)header.magic,
			(unsigned)header.version,
			byteCount,
			(unsigned)gPangeaDroppedPacketCount);
		return;
	}

	if (header.matchIdLow != gPangeaMatchId || header.matchIdHigh != gPangeaMatchIdHigh)
	{
		gPangeaDroppedPacketCount++;
		SDL_Log("CroMag net: drop packet match mismatch type=%u got=%u:%u expected=%u:%u drops=%u",
			(unsigned)header.packetType,
			(unsigned)header.matchIdHigh,
			(unsigned)header.matchIdLow,
			(unsigned)gPangeaMatchIdHigh,
			(unsigned)gPangeaMatchId,
			(unsigned)gPangeaDroppedPacketCount);
		return;
	}

	if (header.sequence <= gPangeaConnState[gMyNetworkPlayerNum].lastReceivedSequence && header.sequence != 0)
	{
		gPangeaReorderedPacketCount++;
	}
	gPangeaConnState[gMyNetworkPlayerNum].lastReceivedSequence = header.sequence;

	if (header.packetType == kPangeaPacketClientInput)
	{
		const short playerNum = (short)header.playerIndex;
		if (!gIsNetworkHost || playerNum < 0 || playerNum >= MAX_PLAYERS)
		{
			return;
		}

		gPlayerInfo[playerNum].controlBits = PangeaNetReader_ReadU32(&reader);
		gPlayerInfo[playerNum].controlBits_New = PangeaNetReader_ReadU32(&reader);
		gPlayerInfo[playerNum].analogSteering.x = PangeaNetReader_ReadF32(&reader);
		gPlayerInfo[playerNum].analogSteering.y = PangeaNetReader_ReadF32(&reader);
		if (!reader.ok)
		{
			SDL_Log("CroMag net: malformed client input payload");
			return;
		}

		gPangeaConnState[playerNum].lastReceivedInputTick = header.tick;
		gPangeaConnState[playerNum].lastReceivedSequence = header.sequence;
		gPangeaConnState[playerNum].inputBufferLength = 1;
		gPangeaConnState[playerNum].timeoutState = 0;
		gPangeaConnState[playerNum].lastInputTimeMs = (double)SDL_GetTicks();
		if (PangeaNet_ShouldLogSequence(header.sequence))
		{
			SDL_Log("CroMag net: host recv input seq=%u tick=%u player=%d bits=%08x new=%08x steer=(%.2f,%.2f) pos=(%.1f,%.1f,%.1f)",
				(unsigned)header.sequence,
				(unsigned)header.tick,
				playerNum,
				(unsigned)gPlayerInfo[playerNum].controlBits,
				(unsigned)gPlayerInfo[playerNum].controlBits_New,
				gPlayerInfo[playerNum].analogSteering.x,
				gPlayerInfo[playerNum].analogSteering.y,
				gPlayerInfo[playerNum].coord.x,
				gPlayerInfo[playerNum].coord.y,
				gPlayerInfo[playerNum].coord.z);
		}
		return;
	}

	if (header.packetType == kPangeaPacketVehicleType)
	{
		const short playerNum = (short)header.playerIndex;
		if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		{
			return;
		}
		gPendingVehicleType[playerNum] = (short)PangeaNetReader_ReadU16(&reader);
		gPendingVehicleSex[playerNum] = (short)PangeaNetReader_ReadU16(&reader);
		if (!reader.ok)
		{
			SDL_Log("CroMag net: malformed vehicle payload");
			return;
		}
		gHavePendingVehicleType[playerNum] = true;
		return;
	}

	if (header.packetType == kPangeaPacketTagHandoffRequest)
	{
		const short observerPlayer = (short)header.playerIndex;
		const short fromPlayer = (short)PangeaNetReader_ReadU8(&reader);
		const short toPlayer = (short)PangeaNetReader_ReadU8(&reader);
		if (!gIsNetworkHost || !reader.ok)
		{
			return;
		}
		if (observerPlayer != fromPlayer && observerPlayer != toPlayer)
		{
			return;
		}
		if (PangeaNet_ApplyTagHandoff(fromPlayer, toPlayer) && gDebugMode)
		{
			SDL_Log("CroMag net: host accepted tag handoff request observer=%d from=%d to=%d", observerPlayer, fromPlayer, toPlayer);
		}
		return;
	}

	if (header.packetType == kPangeaPacketReliableEvent)
	{
		if (!gIsNetworkClient)
		{
			return;
		}
		const uint8_t eventType = PangeaNetReader_ReadU8(&reader);
		if (!reader.ok)
		{
			return;
		}
			if (eventType == kPangeaReliableEventTagHandoff)
			{
				const uint8_t whoIsIt = PangeaNetReader_ReadU8(&reader);
				const uint8_t whoWasIt = PangeaNetReader_ReadU8(&reader);
				(void) PangeaNetReader_ReadU8(&reader);
				const float reTagTimer = PangeaNetReader_ReadF32(&reader);
				if (reader.ok)
				{
					gWhoWasIt = (short)whoWasIt;
					gReTagTimer = reTagTimer;
					PangeaNet_SetTaggedPlayer((short)whoIsIt);
				}
				return;
			}
		const short playerNum = (short)PangeaNetReader_ReadU8(&reader);
		const short lap = (short)PangeaNetReader_ReadU16(&reader);
		const short place = (short)PangeaNetReader_ReadU16(&reader);
		const uint8_t raceComplete = PangeaNetReader_ReadU8(&reader);
		const uint8_t eliminated = PangeaNetReader_ReadU8(&reader);
		if (!reader.ok || playerNum < 0 || playerNum >= MAX_PLAYERS)
		{
			return;
		}
		if (eventType == kPangeaReliableEventRaceComplete)
		{
			gPlayerInfo[playerNum].raceComplete = raceComplete != 0;
			gPlayerInfo[playerNum].lapNum = lap;
			gPlayerInfo[playerNum].place = place;
		}
		else if (eventType == kPangeaReliableEventEliminated)
		{
			gPlayerInfo[playerNum].isEliminated = eliminated != 0;
		}
		return;
	}

	if (header.packetType == kPangeaPacketHostSnapshot)
	{
		if (!gIsNetworkClient || header.sequence <= gPangeaLastHostSnapshotSeq)
		{
			gPangeaDroppedPacketCount++;
			SDL_Log("CroMag net: drop snapshot seq=%u last=%u isClient=%d drops=%u",
				(unsigned)header.sequence,
				(unsigned)gPangeaLastHostSnapshotSeq,
				gIsNetworkClient,
				(unsigned)gPangeaDroppedPacketCount);
			return;
		}

		gPendingSnapshot.protocolVersion = PangeaNetReader_ReadU16(&reader);
		gPendingSnapshot.snapshotKind = PangeaNetReader_ReadU8(&reader);
		gPendingSnapshot.reserved0 = PangeaNetReader_ReadU8(&reader);
		gPendingSnapshot.packetType = kPangeaPacketHostSnapshot;
		gPendingSnapshot.snapshotSeq = header.sequence;
		gPendingSnapshot.frameCounter = header.tick;
		gPendingSnapshot.stateHash = PangeaNetReader_ReadU32(&reader);
		gPendingSnapshot.lastKeyframeSeq = PangeaNetReader_ReadU32(&reader);
		gPendingSnapshot.lastDeltaSeq = PangeaNetReader_ReadU32(&reader);
		gPendingSnapshot.playerCount = (uint8_t)PangeaNetReader_ReadU8(&reader);
		gPendingSnapshot.pad[0] = gPendingSnapshot.pad[1] = gPendingSnapshot.pad[2] = 0;
		gPendingSnapshot.whoIsIt = PangeaNetReader_ReadU16(&reader);
		gPendingSnapshot.whoWasIt = PangeaNetReader_ReadU16(&reader);
		gPendingSnapshot.capturedFlagCount[0] = PangeaNetReader_ReadU16(&reader);
		gPendingSnapshot.capturedFlagCount[1] = PangeaNetReader_ReadU16(&reader);
		gPendingSnapshot.numPlayersEliminated = PangeaNetReader_ReadU16(&reader);
		gPendingSnapshot.reserved1 = PangeaNetReader_ReadU16(&reader);
		gPendingSnapshot.reTagTimer = PangeaNetReader_ReadF32(&reader);
		if (!reader.ok || gPendingSnapshot.protocolVersion != PANGEA_NET_VERSION)
		{
			SDL_Log("CroMag net: rejected snapshot protocolVersion=%u expected=%u",
				(unsigned)gPendingSnapshot.protocolVersion,
				(unsigned)PANGEA_NET_VERSION);
			gPangeaDroppedPacketCount++;
			return;
		}

		for (short i = 0; i < MAX_PLAYERS; i++)
		{
			PangeaNetPlayerCarState* s = &gPendingSnapshot.players[i];
			s->coord.x = PangeaNetReader_ReadF32(&reader);
			s->coord.y = PangeaNetReader_ReadF32(&reader);
			s->coord.z = PangeaNetReader_ReadF32(&reader);
			s->rotX = PangeaNetReader_ReadF32(&reader);
			s->rotY = PangeaNetReader_ReadF32(&reader);
			s->rotZ = PangeaNetReader_ReadF32(&reader);
			s->delta.x = PangeaNetReader_ReadF32(&reader);
			s->delta.y = PangeaNetReader_ReadF32(&reader);
			s->delta.z = PangeaNetReader_ReadF32(&reader);
			s->deltaRot.x = PangeaNetReader_ReadF32(&reader);
			s->deltaRot.y = PangeaNetReader_ReadF32(&reader);
			s->deltaRot.z = PangeaNetReader_ReadF32(&reader);
			s->steering = PangeaNetReader_ReadF32(&reader);
			s->currentThrust = PangeaNetReader_ReadF32(&reader);
			s->currentRPM = PangeaNetReader_ReadF32(&reader);
			s->skidDot = PangeaNetReader_ReadF32(&reader);
			s->controlBits = PangeaNetReader_ReadU32(&reader);
			s->controlBitsNew = PangeaNetReader_ReadU32(&reader);
			s->analogSteering.x = PangeaNetReader_ReadF32(&reader);
			s->analogSteering.y = PangeaNetReader_ReadF32(&reader);
			s->lapNum = (short)PangeaNetReader_ReadU16(&reader);
			s->checkpointNum = (short)PangeaNetReader_ReadU16(&reader);
			s->place = (short)PangeaNetReader_ReadU16(&reader);
			s->raceComplete = PangeaNetReader_ReadU8(&reader);
			s->wrongWay = PangeaNetReader_ReadU8(&reader);
			s->isEliminated = PangeaNetReader_ReadU8(&reader);
			s->powType = (short)PangeaNetReader_ReadU16(&reader);
			s->powQuantity = (short)PangeaNetReader_ReadU16(&reader);
			s->health = PangeaNetReader_ReadF32(&reader);
			s->tagTimer = PangeaNetReader_ReadF32(&reader);
			s->frozenTimer = PangeaNetReader_ReadF32(&reader);
			s->greasedTiresTimer = PangeaNetReader_ReadF32(&reader);
			s->nitroTimer = PangeaNetReader_ReadF32(&reader);
			s->stickyTiresTimer = PangeaNetReader_ReadF32(&reader);
			s->invisibilityTimer = PangeaNetReader_ReadF32(&reader);
			s->isIt = PangeaNetReader_ReadU8(&reader);
			s->movingBackwards = PangeaNetReader_ReadU8(&reader);
			s->accelBackwards = PangeaNetReader_ReadU8(&reader);
			s->braking = PangeaNetReader_ReadU8(&reader);
			s->onWater = PangeaNetReader_ReadU8(&reader);
			(void) PangeaNetReader_ReadU8(&reader);
			(void) PangeaNetReader_ReadU8(&reader);
			(void) PangeaNetReader_ReadU8(&reader);
			s->lastProcessedInputSequence = PangeaNetReader_ReadU32(&reader);

			if (i < MAX_PLAYERS)
			{
				gPlayerInfo[i].isIt = s->isIt != 0;
				gPlayerInfo[i].isEliminated = s->isEliminated != 0;
				gPlayerInfo[i].wrongWay = s->wrongWay != 0;
				gPlayerInfo[i].movingBackwards = s->movingBackwards != 0;
				gPlayerInfo[i].accelBackwards = s->accelBackwards != 0;
				gPlayerInfo[i].braking = s->braking != 0;
				gPlayerInfo[i].onWater = s->onWater != 0;
				gPlayerInfo[i].currentRPM = s->currentRPM;
				gPlayerInfo[i].skidDot = s->skidDot;
				gPlayerInfo[i].tagTimer = s->tagTimer;
				gPlayerInfo[i].frozenTimer = s->frozenTimer;
				gPlayerInfo[i].greasedTiresTimer = s->greasedTiresTimer;
				gPlayerInfo[i].nitroTimer = s->nitroTimer;
				gPlayerInfo[i].stickyTiresTimer = s->stickyTiresTimer;
				gPlayerInfo[i].invisibilityTimer = s->invisibilityTimer;
				if (i != gMyNetworkPlayerNum)
				{
					PangeaNet_PushRemoteSnapshot(i, s);
				}
			}
		}

		/* Read torch objective state */
		{
			const uint8_t pendingTorchCount = PangeaNetReader_ReadU8(&reader);
			gPendingTorchCount = pendingTorchCount < PANGEA_NET_MAX_TORCHES ? pendingTorchCount : PANGEA_NET_MAX_TORCHES;
			for (uint8_t t = 0; t < gPendingTorchCount; t++)
			{
				gPendingTorchMode[t] = PangeaNetReader_ReadU8(&reader);
				gPendingTorchCarrier[t] = PangeaNetReader_ReadU8(&reader);
				gPendingTorchCoordX[t] = PangeaNetReader_ReadF32(&reader);
				gPendingTorchCoordY[t] = PangeaNetReader_ReadF32(&reader);
				gPendingTorchCoordZ[t] = PangeaNetReader_ReadF32(&reader);
			}
			gHavePendingTorchState = reader.ok;
		}

		if (!reader.ok)
		{
			SDL_Log("CroMag net: malformed snapshot payload");
			return;
		}

		gPangeaLastHostSnapshotSeq = header.sequence;
		gPangeaConnState[gMyNetworkPlayerNum].lastAckedSnapshotTick = header.tick;
		gPangeaLastSnapshotStateHash = gPendingSnapshot.stateHash;
		if (gPendingSnapshot.snapshotKind == kPangeaSnapshotKeyframe)
		{
			gPangeaLastHostKeyframeSeq = gPendingSnapshot.snapshotSeq;
			gPangeaLastReceivedKeyframeAtMs = (double)SDL_GetTicks();
		}
		else if (gPendingSnapshot.snapshotKind == kPangeaSnapshotDelta)
		{
			gPangeaLastHostDeltaSeq = gPendingSnapshot.snapshotSeq;
		}
		gHavePendingSnapshot = true;
		if (PangeaNet_ShouldLogSequence(header.sequence))
		{
			const short remotePlayer = gMyNetworkPlayerNum == 0 ? 1 : 0;
			const PangeaNetPlayerCarState* local = &gPendingSnapshot.players[gMyNetworkPlayerNum];
			const PangeaNetPlayerCarState* remote = &gPendingSnapshot.players[remotePlayer];
			SDL_Log("CroMag net: client recv snapshot seq=%u kind=%u tick=%u players=%u local%d=(%.1f,%.1f,%.1f) remote%d=(%.1f,%.1f,%.1f) remoteBits=%08x",
				(unsigned)header.sequence,
				(unsigned)gPendingSnapshot.snapshotKind,
				(unsigned)header.tick,
				(unsigned)gPendingSnapshot.playerCount,
				gMyNetworkPlayerNum,
				local->coord.x,
				local->coord.y,
				local->coord.z,
				remotePlayer,
				remote->coord.x,
				remote->coord.y,
				remote->coord.z,
				(unsigned)remote->controlBits);
		}
		return;
	}

	if (header.packetType == kPangeaPacketClientAck)
	{
		if (!gIsNetworkHost)
		{
			return;
		}
		const short playerNum = (short)header.playerIndex;
		if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		{
			return;
		}
		gPangeaConnState[playerNum].lastAckedSnapshotTick = header.tick;
		const uint32_t ackedSnapshotSeq = PangeaNetReader_ReadU32(&reader);
		gPangeaLastAckedKeyframeSeq[playerNum] = PangeaNetReader_ReadU32(&reader);
		gPangeaLastAckedDeltaSeq[playerNum] = PangeaNetReader_ReadU32(&reader);
		if (!reader.ok)
		{
			gPangeaLastAckedDeltaSeq[playerNum] = 0;
			gPangeaLastAckedKeyframeSeq[playerNum] = 0;
		}
		else if (ackedSnapshotSeq > gPangeaConnState[playerNum].lastSentSequence)
		{
			gPangeaConnState[playerNum].lastSentSequence = ackedSnapshotSeq;
		}
		return;
	}

	if (header.packetType == kPangeaPacketKeyframeResendRequest)
	{
		if (!gIsNetworkHost)
		{
			return;
		}
		const short playerNum = (short)header.playerIndex;
		if (playerNum < 0 || playerNum >= MAX_PLAYERS)
		{
			return;
		}
		gPangeaForceKeyframe = 1;
		return;
	}

	SDL_Log("CroMag net: protocol error packetType=%u", (unsigned)header.packetType);
}

static void PumpPangeaNetMessages(void)
{
	uint8_t buffer[PANGEA_NET_MAX_PACKET_SIZE];
	int byteCount;

	for (;;)
	{
		byteCount = PangeaNetBridge_PollMessage(buffer, (int)sizeof(buffer));
		if (byteCount <= 0)
		{
			break;
		}
		HandlePangeaNetMessage(buffer, byteCount);
	}
}
#endif


/******************* INIT NETWORK MANAGER *********************/
//
// Called once at boot
//

void InitNetworkManager(void)
{
#ifdef __EMSCRIPTEN__
	gNetSprocketInitialized = true;
#else
	IMPLEMENT_ME_SOFT();
#endif
#if 0
OSStatus    iErr;

	if ((!gOSX) || OSX_PACKAGE)
	{
	            /*********************/
	            /* INIT NET SPROCKET */
	            /*********************/

		iErr = NSpInitialize(sizeof(NetHostControlInfoMessageType), kBufferSize, kQElements, kGameID, kTimeout);
	    if (iErr)
	        DoFatalAlert("InitNetworkManager: NSpInitialize failed!");

		gNetSprocketInitialized = true;
	}
#endif
}


/********************** END NETWORK GAME ******************************/
//
// Called from CleanupLevel() or when a player bails from game unexpectedly.
//

void EndNetworkGame(void)
{
	IMPLEMENT_ME_SOFT();
#if 0
OSErr	iErr;

	if ((!gNetGameInProgress) || (!gNetGame))								// only if we're running a net game
		return;

		/* THE HOST MUST TERMINATE IT ENTIRELY */

	if (gIsNetworkHost)
	{
		Wait(40);						// do this pause to let clients sync up so they don't get the terminate message prematurely
		iErr = NSpGame_Dispose(gNetGame, kNSpGameFlag_ForceTerminateGame);	// do not renegotiate a new host
		if (iErr)
			DoFatalAlert("EndNetworkGame: NSpGame_Dispose failed!");
	}

			/* CLIENTS CAN JUST BAIL NORMALLY */
	else
	{
		iErr = NSpGame_Dispose(gNetGame, 0);
		if (iErr)
			DoFatalAlert("EndNetworkGame: NSpGame_Dispose failed!");
	}
#endif


	gNetGameInProgress 	= false;
	gIsNetworkHost	= false;
	gIsNetworkClient	= false;
	gNetGame			= nil;
	gNumGatheredPlayers	= 0;
}


#pragma mark -



/****************** SETUP NETWORK HOSTING *********************/
//
// Called when this computer's user has selected to be a host for a net game.
//
// OUTPUT:  true == cancelled.
//

Boolean SetupNetworkHosting(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled())
	{
		short i;
		gNetGameInProgress = true;
		gIsNetworkHost = PangeaNetBridge_IsHost() != 0;
		gIsNetworkClient = !gIsNetworkHost;
		gMyNetworkPlayerNum = (short)PangeaNetBridge_GetLocalPlayerIndex();
		gNumRealPlayers = (short)PangeaNetBridge_GetPlayerCount();
		gExpectedMatchSeed = PangeaNetBridge_GetMatchSeed();
		SetMyRandomSeed((unsigned long)gExpectedMatchSeed);
		gPangeaMatchId = PangeaNetBridge_GetMatchIdLow();
		gPangeaMatchIdHigh = PangeaNetBridge_GetMatchIdHigh();
		gPangeaLocalTick = 0;
		gPangeaSendSequence = 1;
		gPangeaLastHostSnapshotSeq = 0;
		gPangeaLastHostKeyframeSeq = 0;
		gPangeaLastHostDeltaSeq = 0;
		gPangeaLastSnapshotStateHash = 0;
		gPangeaForceKeyframe = 0;
		gPangeaLastReceivedKeyframeAtMs = (double)SDL_GetTicks();
		gPangeaLastResendRequestAtMs = 0.0;
		gPangeaCorrectionDistance = 0.0f;
		gPangeaDroppedPacketCount = 0;
		gPangeaReorderedPacketCount = 0;
		gPangeaLocalInputHistoryHead = 0;
		gPangeaLocalInputHistoryCount = 0;
		gHostSendCounter = 0;
		gTimeoutCounter = 0;
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			gClientSendCounter[i] = 0;
			gHavePendingVehicleType[i] = false;
			gPangeaConnState[i].lastReceivedInputTick = 0;
			gPangeaConnState[i].lastAckedSnapshotTick = 0;
			gPangeaConnState[i].lastSentSequence = 0;
			gPangeaConnState[i].lastReceivedSequence = 0;
			gPangeaConnState[i].rttMs = 0.0f;
			gPangeaConnState[i].jitterMs = 0.0f;
			gPangeaConnState[i].inputBufferLength = 0;
			gPangeaConnState[i].timeoutState = 0;
			gPangeaConnState[i].lastInputTimeMs = 0.0;
			gPangeaRemoteInterp[i].hasFrom = false;
			gPangeaRemoteInterp[i].hasTo = false;
			gPangeaRemoteSnapshotHead[i] = 0;
			gPangeaRemoteSnapshotCount[i] = 0;
			gPangeaReliableRaceCompleteSent[i] = 0;
			gPangeaReliableEliminatedSent[i] = 0;
			gPangeaLastAckedKeyframeSeq[i] = 0;
			gPangeaLastAckedDeltaSeq[i] = 0;
		}
		return false;
	}
#endif

	IMPLEMENT_ME_SOFT();
	return true;
#if 0
OSStatus 					status;
Boolean 					okHit;
NSpProtocolListReference	theList = NULL;
NSpProtocolReference 		atRef;

	GammaOn();
	Enter2D(true);

	MyFlushEvents();

    gNetGame = nil;

	gHostSendCounter = 0;
	gTimeoutCounter = 0;

			/* GET SOME NAMES */

	CopyPString(gPlayerSaveData.playerName, gNetPlayerName);
	// TODO: this is probably STR_RACE + gGameMode - GAME_MODE_MULTIPLAYERRACE
	GetIndStringC(gameName, 1000 + gGamePrefs.language, 16 + (gGameMode - GAME_MODE_MULTIPLAYERRACE));	// name of game is game mode string

	password[0] = 0;


			/* CREATE A PROTOCOL LIST */

	status = NSpProtocolList_New(NULL, &theList);
	if (status)
		DoFatalAlert("SetupNetworkHosting: NSpProtocolList_New failed!");

	if (!gOSX)
	{
		atRef = NSpProtocol_CreateAppleTalk(gameName,kNBPType, 0,0);		// create appletalk protocol ref
		NSpProtocolList_Append(theList, atRef);								// append protocol refs
	}

			/* DO HOSTING UI */
			//
			//	Note!  Do NOT pass in string constants, as the user can change these values
			//


	TurnOffISp();
	InitCursor();
	okHit = NSpDoModalHostDialog(theList, gameName, gNetPlayerName, password, nil);
	TurnOnISp();
	if (!okHit)
		goto failure;


			/* NEW HOST GAME */

	status = NSpGame_Host(&gNetGame, theList, MAX_PLAYERS, gameName,
				password, gNetPlayerName, 0, kNSpClientServer, 0);
	if (status || (gNetGame==nil))
	{
		DoAlert("SetupNetworkHosting: NSpGame_Host failed!");
		ShowSystemErr_NonFatal(status);	//----------
		goto failure;
	}


			/* LET USERS JOIN IN */

	if (Host_DoGatherPlayersDialog())
		goto failure;



		/*************************************/
		/* TELL ALL CLIENT PLAYERS SOME INFO */
		/*************************************/

	if (HostSendGameConfigInfo())
	    goto failure;

	HideCursor();
	return(false);


			/* SOMETHING WENT WRONG, SO BE GRACEFUL */

failure:

    if (gNetGame)
    {
        NSpGame_Dispose(gNetGame, 0);
        gNetGame = nil;
    }

	if (theList != nil)
		NSpProtocolList_Dispose(theList);

	HideCursor();

	Exit2D();
	return(true);
#endif
}


/*************** SETUP NETWORK JOIN ************************/
//
// OUTPUT:	false == let's go!
//			true = cancel
//

Boolean SetupNetworkJoin(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled())
	{
		return SetupNetworkHosting();
	}
#endif

	IMPLEMENT_ME_SOFT();
	return true;
#if 0
NSpAddressReference	theAddress;
OSStatus			status;
int					i;


//	GameScreenToBlack();
	GammaOn();
	Enter2D(true);

	MyFlushEvents();
	InitCursor();

	gNetGame = nil;

	for (i = 0; i < MAX_PLAYERS; i++)
		gClientSendCounter[i] = 0;
	gTimeoutCounter = 0;

	CopyPString(gPlayerSaveData.playerName, gNetPlayerName);		// use loaded player's name
	password[0] = 0;

			/* DO UI FOR JOINING GAME */
			//
			//	passing an empty string (not nil) for the type causes NetSprocket to use the game id passed in to initialize
			//

	TurnOffISp();
	theAddress = NSpDoModalJoinDialog(kNBPType, kJoinDialogLabel, gNetPlayerName, password, NULL);
	TurnOnISp();

	if (theAddress == NULL)		// The user cancelled
	{
		HideCursor();
		return(true);
	}


				/* JOIN IN */

	status = NSpGame_Join(&gNetGame, theAddress, gNetPlayerName, password, 0, NULL, 0, 0);
	if (status)
	{
		HideCursor();
		return(true);												// an error will occur if user selects "blank" line in dialog above (sounds like an NSp bug to me!)
	}

	NSpReleaseAddressReference(theAddress);							// always dispose of this after _Join


			/* WAIT WHILE OTHERS JOIN ALSO */

	status = Client_WaitForGameConfigInfo();
	if (status)
	{
        if (gNetGame)
        {
            NSpGame_Dispose(gNetGame, 0);
            gNetGame = nil;
        }
	}

	HideCursor();
	Exit2D();
	return status;
#endif
}


#pragma mark -

/********************* DO MY CUSTOM GATHER GAME DIALOG **********************/
//
// Displays dialog which shows all currently gathered players.
//
// OUTPUT: OSErr = noErr if all's well.
//

#if 0
static OSErr  Host_DoGatherPlayersDialog(void)
{
 	IMPLEMENT_ME_SOFT();
	return unimpErr;
DialogRef 		myDialog;
DialogItemType			itemType,itemHit;
ControlHandle	itemHandle;
Rect			itemRect;
Boolean			dialogDone,cancelled = false;
ModalFilterUPP	myProc;

	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);
	FlushEventQueue(GetMainEventQueue());

	gNumGatheredPlayers = 0;												// noone gathered yet

	myDialog = GetNewDialog(130,nil,MOVE_TO_FRONT);


			/* SET OUTLINE FOR USERITEM */

	GetDialogItem(myDialog,1,&itemType,(Handle *)&itemHandle,&itemRect);					// default button
	SetDialogItem(myDialog, 2, userItem, (Handle)NewUserItemUPP(DoBold), &itemRect);


			/* INIT LIST BOX */

	GetDialogItem(myDialog,4,&itemType,(Handle *)&itemHandle,&itemRect);					// player's box
	SetDialogItem(myDialog,4, userItem,(Handle)NewUserItemUPP(DoOutline), &itemRect);
	InitPlayerNamesListBox(&itemRect,GetDialogWindow(myDialog));													// create list manager list


				/*************/
				/* DO DIALOG */
				/*************/

	dialogDone = false;
	myProc = NewModalFilterUPP(GatherGameDialogCallback);
	while(dialogDone == false)
	{
		ModalDialog(myProc, &itemHit);
		switch (itemHit)
		{
			case 	3:
					cancelled = true;
					dialogDone = true;
					break;

			case	1:									// see if PLAY
					dialogDone = true;
					break;
		}
	}

		/* STOP ADVERTISING THIS GAME SINCE WE'RE ALL SET TO GO */

	NSpGame_EnableAdvertising(gNetGame, nil, false);


			/* CLEANUP */

	DisposeModalFilterUPP(myProc);
	DisposeDialog(myDialog);

	GameScreenToBlack();
	return(cancelled);
}


/******************** INIT GATHER LIST BOX *************************/
//
// Creates the List Manager list box which will contain a list of all the joiners in this game.
//

static void InitPlayerNamesListBox(Rect *r, WindowPtr myDialog)
{
Rect	dataBounds;
Point	cSize;

	r->right -= 15;														// make room for scroll bars & outline
	r->top += 1;
	r->bottom -= 1;
	r->left += 1;

	gNumRowsInList = 0;
	SetRect(&dataBounds,0,0,1,gNumRowsInList);							// no entries yet
	cSize.h = cSize.v = 0;
	gTheList = LNew(r, &dataBounds, cSize, 0, myDialog, true, false, false, false);

	ShowNamesOfJoinedPlayers();
}



/**************** GATHER GAME DIALOG CALLBACK *************************/

static Boolean GatherGameDialogCallback (DialogRef dp,EventRecord *event, short *item)
{
	IMPLEMENT_ME_SOFT();
	return true;
#if 0
char 			c;
Point			eventPoint;
static	long	tick = 0;
NSpMessageHeader	*message;

	dp; item;

				/* HANDLE DIALOG EVENTS */

	SetPort(GetDialogPort(dp));										// make sure we're drawing to this dialog

	switch(event->what)
	{
		case	keyDown:								// we have a key press
				c = event->message & 0x00FF;			// what character is it?
				break;

		case	mouseDown:								// mouse was clicked
				eventPoint = event->where;				// get the location of click
				GlobalToLocal (&eventPoint);			// got to make it local
				break;
	}

			/*******************************************/
			/* CHECK FOR OTHER PLAYERS WANTING TO JOIN */
			/*******************************************/

	while ((message = NSpMessage_Get(gNetGame)) != NULL)	// read Net message
	{
		switch(message->what)
		{
			case	kNSpPlayerLeft:						// see if someone decided to un-join
					ShowNamesOfJoinedPlayers();
					break;

			case 	kNSpPlayerJoined:					// see if we've got a new player joining
			case	kNSpJoinRequest:
					ShowNamesOfJoinedPlayers();
					break;

			default:
					HandleOtherNetMessage(message);
		}
		NSpMessage_Release(gNetGame, message);
	}

			/* KEEP MUSIC PLAYING */

	if (gSongPlayingFlag)
		MoviesTask(gSongMovie, 0);

	return(false);
#endif
}


/***************** SHOW NAMES OF JOINED PLAYERS ************************/
//
// For the Gather Game and Wait for Config dialogs, it displays list of joined players by updating
// the List Manager list for this dialog.
//

static void ShowNamesOfJoinedPlayers(void)
{
	IMPLEMENT_ME_SOFT();
#if 0
short	i;
Cell	theCell;
NSpPlayerEnumerationPtr	players;
OSStatus	status;


	status = NSpPlayer_GetEnumeration(gNetGame, &players);
	if (status != noErr)
		return;

		/* COPY NBP NAMES INTO LIST BUFFER */

	gNumGatheredPlayers =  players->count;
	for (i=0; i < gNumGatheredPlayers; i++)
	{
		NSpPlayerInfoPtr	thePlayer;

		NSpPlayer_GetInfo(gNetGame, players->playerInfo[i]->id, &thePlayer);

		CopyPStr(thePlayer->name, gPlayerNameStrings[i]);
	}

	NSpPlayer_ReleaseEnumeration(gNetGame, players);


		/* DELETE ALL EXISTING ROWS IN LIST */

	LDelRow(0, 0, gTheList);
	gNumRowsInList = 0;


			/* ADD NAMES TO LIST */

	if (gNumGatheredPlayers > 0)
		LAddRow(gNumGatheredPlayers, 0, gTheList);		// create rows


	for (i=0; i < gNumGatheredPlayers; i++)
	{
		if (i == (gNumGatheredPlayers-1))				// reactivate draw on last cell
			LSetDrawingMode(true,gTheList);							// turn on updating
		theCell.h = 0;
		theCell.v = i;
		LSetCell(&gPlayerNameStrings[i][1], gPlayerNameStrings[i][0], theCell, gTheList);
		gNumRowsInList++;
	}
#endif
}




#pragma mark -

/*********************** SEND GAME CONFIGURATION INFO *******************************/
//
// Once everyone is in and we (the host) start things, then send this to all players to tell them we're on!
//

static OSErr HostSendGameConfigInfo(void)
{
	IMPLEMENT_ME_SOFT();
	return unimpErr;
#if 0
OSStatus				status;
NetConfigMessageType			message;
NSpPlayerEnumerationPtr	playerList;
NSpPlayerID				hostID,clientID;
short					i,p;
NSpPlayerInfoPtr		playerInfoPtr;

			/* GET PLAYER INFO */

	hostID = NSpPlayer_GetMyID(gNetGame);							// get my/host ID
	status = NSpPlayer_GetEnumeration(gNetGame, &playerList);
	gNumRealPlayers = playerList->count;							// get # players (host + clients)

	gMyNetworkPlayerNum = 0;										// the host is always player #0


			/***********************************************/
			/* SEND GAME CONFIGURATION INFO TO ALL PLAYERS */
			/***********************************************/
			//
			// Send one message at a time to each individual client player with
			// specific info for each client.
			//

	p = 1;															// start assigning player nums at 1 since Host is always #0
	for (i = 0; i < gNumRealPlayers; i++)
	{
		playerInfoPtr =  playerList->playerInfo[i];					// point to NSp's player info list

		gPlayerInfo[i].nspPlayerID = clientID = playerInfoPtr->id;	// get NSp's playerID (for use when player leaves game)

		if (clientID != hostID)										// don't send start info to myself/host
		{
					/* MAKE NEW MESSAGE */

			NSpClearMessageHeader(&message.h);
			message.h.to 			= clientID;						// send to this client
			message.h.what 			= kNetConfigureMessage;			// set message type
			message.h.messageLen 	= sizeof(message);				// set size of message

			message.gameMode 		= gGameMode;					// set game Mode
			message.age		 		= gTheAge;						// set Age
			message.trackNum		= gTrackNum;					// set track #
			message.numPlayers 		= gNumRealPlayers;				// set # players
			message.playerNum 		= p++;							// set player #
			message.numAgesCompleted = gPlayerSaveData.numAgesCompleted;
			message.difficulty		= gGamePrefs.difficulty;		// set difficulty
			message.tagDuration		= gGamePrefs.tagDuration;		// set tag duration

			status = NSpMessage_Send(gNetGame, &message.h, kNSpSendFlag_Registered);	// send message
			if (status)
			{
				DoAlert("HostSendGameConfigInfo: NSpMessage_Send failed!");
				break;
			}
		}
	}

			/************/
			/* CLEAN UP */
			/************/

	NSpPlayer_ReleaseEnumeration(gNetGame,playerList);					// dispose of player list

	return(status);
#endif
}




/******************** WAIT FOR GAME CONFIGURATION INFO *****************************/
//
// Waits for others to join and then Host to tell me which player # I am et.al.
//
// OUTPUT:	OSErr == noErr if all went well, otherwise aborted.
//

static OSErr Client_WaitForGameConfigInfo(void)
{
	IMPLEMENT_ME_SOFT();
	return unimpErr;
#if 0
DialogRef	myDialog;
Boolean		dialogDone,cancelled;
DialogItemType			itemType,itemHit,i;
ControlHandle	itemHandle;
Rect			itemRect;
ModalFilterUPP	myProc;
NSpPlayerEnumerationPtr	playerList;
NSpPlayerInfoPtr		playerInfoPtr;

	FlushEvents (everyEvent, REMOVE_ALL_EVENTS);
	FlushEventQueue(GetMainEventQueue());
	gNumGatheredPlayers = 0;												// noone gathered yet


			/* FIRST GET GAME PLAYER ID'S */

	NSpPlayer_GetEnumeration(gNetGame, &playerList);
	gNumRealPlayers = playerList->count;									// get # players (host + clients)
	for (i = 0; i < gNumRealPlayers; i++)
	{
		playerInfoPtr =  playerList->playerInfo[i];					// point to NSp's player info list
		gPlayerInfo[i].nspPlayerID = playerInfoPtr->id;					// get NSp's playerID (for use when player leaves game)
	}
	NSpPlayer_ReleaseEnumeration(gNetGame,playerList);					// dispose of player list


			/********************************************/
			/* MAKE "WAITING FOR OTHERS TO JOIN" DIALOG */
			/********************************************/

	myDialog = GetNewDialog(131,nil,MOVE_TO_FRONT);


			/* SET OUTLINE FOR USERITEM */

	GetDialogItem(myDialog,1,&itemType,(Handle *)&itemHandle,&itemRect);					// default button
	SetDialogItem(myDialog, 3, userItem, (Handle)NewUserItemUPP(DoBold), &itemRect);


			/* INIT LIST BOX */

	GetDialogItem(myDialog,4,&itemType,(Handle *)&itemHandle,&itemRect);					// player's box
	SetDialogItem(myDialog,4, userItem,(Handle)NewUserItemUPP(DoOutline), &itemRect);
	InitPlayerNamesListBox(&itemRect,GetDialogWindow(myDialog));													// create list manager list


	/* LET'S WAIT FOR HOST TO TELL US SOMETHING, OR WE CAN ALWAYS CANCEL */

	dialogDone = cancelled = false;
	myProc = NewModalFilterUPP(Client_WaitForGameConfigInfoDialogCallback);
	while(dialogDone == false)
	{
		ModalDialog(myProc, &itemHit);
		switch (itemHit)
		{
				/* PLAYER CANCELLED */

			case 	1:
					cancelled = true;
					dialogDone = true;
//					NSpGame_Dispose(gNetGame, 0);											// tell host that I'm gone
					EndNetworkGame();
					break;

			case	100:
					dialogDone = true;
					break;
			default:
				dialogDone = false;
			break;
		}
	}

	DisposeModalFilterUPP(myProc);
	DisposeDialog(myDialog);
	return(cancelled);
#endif
}


/********************* WAIT FOR GAME CONFIG INFO: DIALOG CALLBACK ***************************/
//
// Returns TRUE if game start info was received.  Upon return, "item" will be set to 100.
//

#if 0
static Boolean Client_WaitForGameConfigInfoDialogCallback (DialogRef dp,EventRecord *event, short *item)
{
	IMPLEMENT_ME_SOFT();
	return false;
#if 0
NSpMessageHeader *message;
Boolean handled = false;

	SetPort(GetDialogPort(dp));										// make sure we're drawing to this dialog


			/* HANDLE NET SPROCKET EVENTS */

	while ((message = NSpMessage_Get(gNetGame)) != nil)							// get message from Net
	{
		switch(message->what)													// handle message
		{
			case	kNetConfigureMessage:										// GOT GAME START INFO
					HandleGameConfigMessage((NetConfigMessageType *)message);
					*item = 100;
					handled = true;
					goto got_config;
					break;

			case 	kNSpGameTerminated:											// Host terminated the game :(
					*item = 1;
					handled = true;
					break;

			case	kNSpJoinApproved:
					break;

			case	kNSpPlayerLeft:												// see if someone decided to un-join
					ShowNamesOfJoinedPlayers();
					break;

			case	kNSpPlayerJoined:
					ShowNamesOfJoinedPlayers();
					break;

			case	kNSpError:
					DoFatalAlert("Client_WaitForGameConfigInfoDialogCallback: message == kNSpError");
					break;

			default:
					HandleOtherNetMessage(message);

		}
		NSpMessage_Release(gNetGame, message);										// dispose of message
	}

got_config:

			/* HANDLE DIALOG EVENTS */

	switch (event->what)
	{
		case keyDown:
			switch (event->message & charCodeMask)
			{
				case 	0x03:  					// Enter
				case 	0x0D: 					// Return
						*item = 1;
						handled = true;
						break;

				case 	0x1B:  					// Escape
						*item = 1;
						handled = true;
						break;

				case 	'.':  					// Command-period
						if (event->modifiers & cmdKey)
						{
							*item = 1;
							handled = true;
						}
						break;
			}
	}

			/* KEEP MUSIC PLAYING */

	if (gSongPlayingFlag)
		MoviesTask(gSongMovie, 0);


	return(handled);
#endif
}
#endif




/************************* HANDLE GAME CONFIGURATION MESSAGE *****************************/
//
// Called while polling in Client_WaitForGameConfigInfoDialogCallback.
//

static void HandleGameConfigMessage(NetConfigMessageType *inMessage)
{
	IMPLEMENT_ME_SOFT();
#if 0
	gGameMode 			= inMessage->gameMode;
	gTheAge 			= inMessage->age;
	gTrackNum 			= inMessage->trackNum;
	gNumRealPlayers 	= inMessage->numPlayers;
	gMyNetworkPlayerNum = inMessage->playerNum;
	gGamePrefs.difficulty = inMessage->difficulty;
	gGamePrefs.tagDuration = inMessage->tagDuration;

	if ((inMessage->numAgesCompleted & AGE_MASK_AGE) > GetNumAgesCompleted())	// if better than our current game, then pseudo-logout that saved game
		gSavedPlayerIsLoaded = false;
	gPlayerSaveData.numAgesCompleted = inMessage->numAgesCompleted;
#endif
}

#endif

#pragma mark -


/********************* HOST WAIT FOR PLAYERS TO PREPARE LEVEL *******************************/
//
// Called right beofre PlayArea().  This waits for the sync message from the other client players
// indicating that they are ready to start playing.
//

void HostWaitForPlayersToPrepareLevel(void)
{
	// Level sync is coordinated by the TypeScript layer before the C game starts.
	(void)0;
#if 0
OSStatus				status;
NetSyncMessageType		outMess;
NSpMessageHeader 		*inMess;
Boolean 				sync = false;
short					n = 1;					// start @ 1 because the host (us) is already ready

int						startTick = TickCount();

		/********************************/
		/* WAIT FOR ALL CLIENTS TO SYNC */
		/********************************/

	while(!sync)
	{
		inMess = NSpMessage_Get(gNetGame);					// get message
		if (inMess)
		{
			switch(inMess->what)
			{
				case	kNetSyncMessage:
						n++;								// we got another player
						if (n == gNumRealPlayers)				// see if that's all of them
							sync = true;
						break;

				case	kNSpError:
						DoFatalAlert("HostWaitForPlayersToPrepareLevel: message == kNSpError");
						break;

				default:
						HandleOtherNetMessage(inMess);
			}
			NSpMessage_Release(gNetGame, inMess);			// dispose of message
		}

		if (gSongPlayingFlag)												// keep music playing
			MoviesTask(gSongMovie, 0);

		if ((TickCount() - startTick) > (60 * 60 * 2))			// if no response for 2 minutes, then time out
		{
			DoFatalAlert("No Response from other player(s), something has gone wrong.");
		}
	}



		/*******************************/
		/* TELL ALL CIENTS WE'RE READY */
		/*******************************/

	NSpClearMessageHeader(&outMess.h);
	outMess.h.to 			= kNSpAllPlayers;						// send to all clients
	outMess.h.what 			= kNetSyncMessage;						// set message type
	outMess.h.messageLen 	= sizeof(outMess);						// set size of message
	outMess.playerNum 		= 0;									// (not used this time)
	status = NSpMessage_Send(gNetGame, &outMess.h, kNSpSendFlag_Registered);	// send message
	if (status)
		DoFatalAlert("HostWaitForPlayersToPrepareLevel: NSpMessage_Send failed!");
#endif
}



/********************* CLIENT TELL HOST LEVEL IS PREPARED *******************************/
//
// Called right beofre PlayArea().  This waits for the sync message from the other client players
// indicating that they are ready to start playing.
//

void ClientTellHostLevelIsPrepared(void)
{
	// Level sync is coordinated by the TypeScript layer before the C game starts.
	(void)0;
#if 0
OSStatus				status;
NetSyncMessageType		outMess;
Boolean 				sync = false;
NSpMessageHeader 		*inMess;

		/***********************************/
		/* TELL THE HOST THAT WE ARE READY */
		/***********************************/

	NSpClearMessageHeader(&outMess.h);
	outMess.h.to 			= kNSpHostOnly;										// send to this host
	outMess.h.what 			= kNetSyncMessage;									// set message type
	outMess.h.messageLen 	= sizeof(outMess);									// set size of message
	outMess.playerNum 		= gMyNetworkPlayerNum;										// set player num
	status = NSpMessage_Send(gNetGame, &outMess.h, kNSpSendFlag_Registered);	// send message
	if (status)
		DoFatalAlert("ClientTellHostLevelIsPrepared: NSpMessage_Send failed!");


		/**************************/
		/* WAIT FOR HOST TO REPLY */
		/**************************/

	while(!sync)
	{
		inMess = NSpMessage_Get(gNetGame);											// get message
		if (inMess)
		{
			switch(inMess->what)
			{
				case	kNetSyncMessage:
						sync = true;
						break;

				case	kNSpError:
						DoFatalAlert("HostWaitForPlayersToPrepareLevel: message == kNSpError");
						break;

				default:
						HandleOtherNetMessage(inMess);
			}
			NSpMessage_Release(gNetGame, inMess);									// dispose of message
		}

		if (gSongPlayingFlag)												// keep music playing
			MoviesTask(gSongMovie, 0);

	}
#endif
}


#pragma mark -


/************** SEND HOST CONTROL INFO TO CLIENTS *********************/
//
// The host sends this at the beginning of each frame to all of the network clients.
// This data contains the gFramesPerSecond/Frac info plus the key controls state bitfields for each player.
//

void HostSend_ControlInfoToClients(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled() && gIsNetworkHost)
	{
		// Host-authoritative web flow does not broadcast lockstep control packets.
		return;
	}
#endif

	IMPLEMENT_ME_SOFT();
#if 0

OSStatus						status;
short							i;


				/* BUILD MESSAGE */

	NSpClearMessageHeader(&gHostOutMess.h);

	gHostOutMess.h.to 			= kNSpAllPlayers;						// send to all clients
	gHostOutMess.h.what 		= kNetHostControlInfoMessage;			// set message type
	gHostOutMess.h.messageLen 	= sizeof(gHostOutMess);						// set size of message

	gHostOutMess.frameCounter	= gHostSendCounter++;					// send the frame counter & inc
	gHostOutMess.fps 			= gFramesPerSecond;						// fps
	gHostOutMess.fpsFrac		= gFramesPerSecondFrac;					// fps frac
	gHostOutMess.randomSeed		= MyRandomLong();						// send the host's current random value for sync verification

	for (i = 0; i < MAX_PLAYERS; i++)								// control bits
	{
		gHostOutMess.controlBits[i] = gPlayerInfo[i].controlBits;
		gHostOutMess.controlBitsNew[i] = gPlayerInfo[i].controlBits_New;
		gHostOutMess.analogSteering[i] = gPlayerInfo[i].analogSteering;
	}

			/* SEND IT */

	status = NSpMessage_Send(gNetGame, &gHostOutMess.h, kNSpSendFlag_Registered);
	if (status)
		DoFatalAlert("HostSend_ControlInfoToClients: NSpMessage_Send failed!");
#endif
}


/************** GET NETWORK CONTROL INFO FROM HOST *********************/
//
// The client reads this from the host at the beginning of each frame.
// This data will contain the fps and control bitfield info for each player.
//

void ClientReceive_ControlInfoFromHost(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled() && gIsNetworkClient)
	{
		PumpPangeaNetMessages();
		return;
	}
#endif

	IMPLEMENT_ME_SOFT();
#if 0

NetHostControlInfoMessageType		*mess;
NSpMessageHeader 					*inMess;
uint32_t								tick,i;
Boolean								gotIt = false;


	tick = TickCount();														// init tick for timeout

	do
	{
		inMess = NSpMessage_Get(gNetGame);									// get message
		if (inMess)
		{
			tick = TickCount();												// reset tick for timeout
			switch(inMess->what)
			{
				case	kNetHostControlInfoMessage:
						mess = (NetHostControlInfoMessageType *)inMess;

						if (mess->frameCounter < gHostSendCounter)			// see if this is an old packet, possibly a duplicate.  If so, skip it
							break;
						if (mess->frameCounter > gHostSendCounter)			// see if we skipped a packet; one must have gotten lost
							DoFatalAlert("ClientReceive_ControlInfoFromHost: It seems Net Sprocket has lost a packet");
						gHostSendCounter++;									// inc host counter since the next packet we get will be +1

						gFramesPerSecond 		= mess->fps;
						gFramesPerSecondFrac 	= mess->fpsFrac;

						if (MyRandomLong() != mess->randomSeed)				// verify that host's random # is in sync with ours!
						{
							DoFatalAlert("ClientReceive_ControlInfoFromHost: Not in sync!  Net Sprocket must have lost some data.");
						}

						for (i = 0; i < MAX_PLAYERS; i++)					// control bits
						{
							gPlayerInfo[i].controlBits 		= mess->controlBits[i];
							gPlayerInfo[i].controlBits_New 	= mess->controlBitsNew[i];
							gPlayerInfo[i].analogSteering	= mess->analogSteering[i];
						}

						gotIt = true;
						break;

				case	kNSpError:
						DoFatalAlert("ClientReceive_ControlInfoFromHost: message == kNSpError");
						break;

				default:
						if (HandleOtherNetMessage(inMess))
							return;
			}
			NSpMessage_Release(gNetGame, inMess);			// dispose of message
		}

				/* SEE IF WE ARE NOT GETTING THE PACKET */
				//
				// If this happens, then it is possible that Net Sprocket lost a packet.  There is no way to know who's packet got lost
				// so go ahead and send our most recent packet again in case it was us.  The Host will throw out any dupes that it gets.
				//

		if ((TickCount() - tick) > (DATA_TIMEOUT*60))		// see if we've been waiting longer than n seconds
		{
			gTimeoutCounter++;								// keep track of how often this happens
			if (gTimeoutCounter > 3)
				DoFatalAlert("ClientReceive_ControlInfoFromHost: the network is losing too much data, must abort.");

			NSpMessage_Send(gNetGame, &gClientOutMess.h, kNSpSendFlag_Registered);	// resend the last message
			tick = TickCount();														// reset tick
		}

	}while(!gotIt);

	gTimeoutCounter = 0;
#endif
}



/************** CLIENT SEND CONTROL INFO TO HOST *********************/
//
// At the end of each frame, the client sends the new control state info to the host for
// the next frame.
//

void ClientSend_ControlInfoToHost(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled() && gIsNetworkClient)
	{
		uint8_t packetBytes[128];
		PangeaNetWriter writer;
		const short playerNum = gMyNetworkPlayerNum;
		PangeaNetPacketHeader header;

		gPangeaLocalTick++;
		header.magic = PANGEA_NET_MAGIC;
		header.version = PANGEA_NET_VERSION;
		header.packetType = kPangeaPacketClientInput;
		header.matchIdLow = gPangeaMatchId;
		header.matchIdHigh = gPangeaMatchIdHigh;
		header.tick = gPangeaLocalTick;
		header.sequence = gPangeaSendSequence++;
		header.playerIndex = (uint16_t)playerNum;
		header.reserved = 0;

		PangeaNetWriter_Init(&writer, packetBytes, (int)sizeof(packetBytes));
		PangeaNet_WriteHeader(&writer, &header);
		PangeaNetWriter_WriteU32(&writer, gPlayerInfo[playerNum].controlBits);
		PangeaNetWriter_WriteU32(&writer, gPlayerInfo[playerNum].controlBits_New);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[playerNum].analogSteering.x);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[playerNum].analogSteering.y);

		if (!writer.ok)
		{
			SDL_Log("CroMag net: failed to encode client input");
			return;
		}

		PangeaNet_RecordLocalInput(header.sequence, playerNum);
		gPangeaConnState[playerNum].lastSentSequence = header.sequence;
		const int sent = PangeaNetBridge_SendReliable(packetBytes, writer.cursor);
		if (PangeaNet_ShouldLogSequence(header.sequence) || !sent)
		{
			SDL_Log("CroMag net: client send input seq=%u tick=%u player=%d bits=%08x new=%08x steer=(%.2f,%.2f) bytes=%d sent=%d",
				(unsigned)header.sequence,
				(unsigned)header.tick,
				playerNum,
				(unsigned)gPlayerInfo[playerNum].controlBits,
				(unsigned)gPlayerInfo[playerNum].controlBits_New,
				gPlayerInfo[playerNum].analogSteering.x,
				gPlayerInfo[playerNum].analogSteering.y,
				writer.cursor,
				sent);
		}
		return;
	}
#endif

	IMPLEMENT_ME_SOFT();
#if 0
OSStatus						status;


				/* BUILD MESSAGE */

	NSpClearMessageHeader(&gClientOutMess.h);

	gClientOutMess.h.to 			= kNSpHostOnly;							// send to Host
	gClientOutMess.h.what 			= kNetClientControlInfoMessage;			// set message type
	gClientOutMess.h.messageLen 	= sizeof(gClientOutMess);				// set size of message

	gClientOutMess.frameCounter		= gClientSendCounter[gMyNetworkPlayerNum]++;	// send client frame counter & inc
	gClientOutMess.playerNum		= gMyNetworkPlayerNum;
	gClientOutMess.controlBits 		= gPlayerInfo[gMyNetworkPlayerNum].controlBits;
	gClientOutMess.controlBitsNew  	= gPlayerInfo[gMyNetworkPlayerNum].controlBits_New;
	gClientOutMess.analogSteering 	= gPlayerInfo[gMyNetworkPlayerNum].analogSteering;


			/* SEND IT */

	status = NSpMessage_Send(gNetGame, &gClientOutMess.h, kNSpSendFlag_Registered);
//	if (status)
//		DoFatalAlert("ClientSend_ControlInfoToHost: NSpMessage_Send failed!");
#endif
}


/*************** HOST GET CONTROL INFO FROM CLIENTS ***********************/

void HostReceive_ControlInfoFromClients(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled() && gIsNetworkHost)
	{
		PumpPangeaNetMessages();
		const double nowMs = (double)SDL_GetTicks();
		for (short i = 0; i < gNumRealPlayers; i++)
		{
			if (i == gMyNetworkPlayerNum)
			{
				continue;
			}

			const double ageMs = nowMs - gPangeaConnState[i].lastInputTimeMs;
			if (gPangeaConnState[i].lastInputTimeMs <= 0.0)
			{
				gPangeaConnState[i].timeoutState = 1;
				continue;
			}

			if (ageMs >= PANGEA_NET_INPUT_DISCONNECT_MS)
			{
				gPangeaConnState[i].timeoutState = 3;
				gPlayerInfo[i].controlBits = 0;
				gPlayerInfo[i].controlBits_New = 0;
				gPlayerInfo[i].analogSteering.x = 0.0f;
				gPlayerInfo[i].analogSteering.y = 0.0f;
				gPlayerInfo[i].isEliminated = true;
				continue;
			}

			if (ageMs >= PANGEA_NET_INPUT_TIMEOUT_MS)
			{
				// Missing-input window: keep applying the last known input briefly.
				gPangeaConnState[i].timeoutState = 2;
			}
			else
			{
				gPangeaConnState[i].timeoutState = 0;
			}
		}
		return;
	}
#endif

	IMPLEMENT_ME_SOFT();
#if 0
NetClientControlInfoMessageType		*mess;
NSpMessageHeader 					*inMess;
uint32_t								tick;
Boolean								gotIt = false;
short								n,i;


	n = 1;                                                  // start @ 1 because the host already has his own info
	tick = TickCount();										// start tick for timeout

	while(n < gNumRealPlayers)								// loop until I've got the message from all players
	{
		inMess = NSpMessage_Get(gNetGame);					// get message
		if (inMess)
		{
			tick = TickCount();								// reset tick for timeout
			switch(inMess->what)
			{
				case	kNetClientControlInfoMessage:
						mess = (NetClientControlInfoMessageType *)inMess;

						i = mess->playerNum;									// get player #

						if (mess->frameCounter < gClientSendCounter[i])			// see if this is an old packet, possibly a duplicate.  If so, skip it
							break;
						if (mess->frameCounter > gClientSendCounter[i])			// see if we skipped a packet; one must have gotten lost
							DoFatalAlert("HostReceive_ControlInfoFromClients: It seems Net Sprocket has lost a packet");
						gClientSendCounter[i]++;								// inc counter since the next packet we get will be +1


						gPlayerInfo[i].controlBits	= mess->controlBits;
						gPlayerInfo[i].controlBits_New = mess->controlBitsNew;
						gPlayerInfo[i].analogSteering = mess->analogSteering;

						n++;
						break;

				case	kNSpError:
						DoFatalAlert("HostReceive_ControlInfoFromClients: message == kNSpError");
						break;

				default:
						if (HandleOtherNetMessage(inMess))
							return;

			}
			NSpMessage_Release(gNetGame, inMess);			// dispose of message
		}

		if ((TickCount() - tick) > (DATA_TIMEOUT*60))		// see if we've been waiting longer than n seconds
		{
			gTimeoutCounter++;								// keep track of how often this happens
			if (gTimeoutCounter > 3)
				DoFatalAlert("HostReceive_ControlInfoFromClients: the network is losing too much data, must abort.");

			NSpMessage_Send(gNetGame, &gHostOutMess.h, kNSpSendFlag_Registered);
			tick = TickCount();														// reset tick
		}

	}
#endif
}


#pragma mark -


/********************* PLAYER BROADCAST VEHICLE TYPE *******************************/
//
// Tell all of the other net players what character type we want to be.
//

void PlayerBroadcastVehicleType(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled())
	{
		uint8_t packetBytes[96];
		PangeaNetWriter writer;
		PangeaNetPacketHeader header;
		const short playerNum = gMyNetworkPlayerNum;

		header.magic = PANGEA_NET_MAGIC;
		header.version = PANGEA_NET_VERSION;
		header.packetType = kPangeaPacketVehicleType;
		header.matchIdLow = gPangeaMatchId;
		header.matchIdHigh = gPangeaMatchIdHigh;
		header.tick = gPangeaLocalTick;
		header.sequence = gPangeaSendSequence++;
		header.playerIndex = (uint16_t)playerNum;
		header.reserved = 0;

		PangeaNetWriter_Init(&writer, packetBytes, (int)sizeof(packetBytes));
		PangeaNet_WriteHeader(&writer, &header);
		PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[playerNum].vehicleType);
		PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[playerNum].sex);

		if (!writer.ok)
		{
			SDL_Log("CroMag net: failed to encode vehicle type");
			return;
		}

		PangeaNetBridge_SendReliable(packetBytes, writer.cursor);

		gPendingVehicleType[playerNum] = gPlayerInfo[playerNum].vehicleType;
		gPendingVehicleSex[playerNum] = gPlayerInfo[playerNum].sex;
		gHavePendingVehicleType[gMyNetworkPlayerNum] = true;
		return;
	}
#endif

	IMPLEMENT_ME_SOFT();
#if 0
OSStatus					status;
NetPlayerCharTypeMessage	outMess;


				/* BUILD MESSAGE */

	NSpClearMessageHeader(&outMess.h);

	outMess.h.to 			= kNSpAllPlayers;						// send to all clients
	outMess.h.what 			= kNetPlayerCharTypeMessage;			// set message type
	outMess.h.messageLen 	= sizeof(outMess);						// set size of message

	outMess.playerNum		= gMyNetworkPlayerNum;					// player #
	outMess.vehicleType		= gPlayerInfo[gMyNetworkPlayerNum].vehicleType;
	outMess.sex				= gPlayerInfo[gMyNetworkPlayerNum].sex;

			/* SEND IT */

	status = NSpMessage_Send(gNetGame, &outMess.h, kNSpSendFlag_Registered);
	if (status)
		DoFatalAlert("PlayerBroadcastVehicleType: NSpMessage_Send failed!");
#endif
}

/***************** GET VEHICLE SELECTION FROM NET PLAYERS ***********************/

void GetVehicleSelectionFromNetPlayers(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled())
	{
		uint32_t timeoutTick = TickCount() + (DATA_TIMEOUT * 60 * 6);
		short i;

		for (;;)
		{
			short count = 0;
			PumpPangeaNetMessages();

			for (i = 0; i < gNumRealPlayers; i++)
			{
				if (gHavePendingVehicleType[i])
				{
					gPlayerInfo[i].vehicleType = gPendingVehicleType[i];
					gPlayerInfo[i].sex = gPendingVehicleSex[i];
					count++;
				}
			}

			if (count >= gNumRealPlayers)
			{
				break;
			}

			if (TickCount() > timeoutTick)
			{
				break;
			}
		}
		return;
	}
#endif

	IMPLEMENT_ME_SOFT();
#if 0
short	playerNum, charType, count, sex;

	ShowLoadingPicture();													// show something while we wait

	count = 1;																// start count @ 1 since we have our own local info already

	do
	{
		if (PlayerReceiveVehicleTypeFromOthers(&playerNum, &charType, &sex))		// check for network message
		{
			gPlayerInfo[playerNum].vehicleType = charType;					// save this player's type
			gPlayerInfo[playerNum].sex = sex;								// save this player's sex
			count++;														// inc count of received info
		}

		if (gSongPlayingFlag)												// keep music playing
			MoviesTask(gSongMovie, 0);

	}while(count < gNumRealPlayers);
#endif
}


#if 0
/*************** PLAYER RECEIVE CHARACTER TYPE FROM OTHERS ***********************/
//
// Receive above message from other players.
//
// OUTPUT: true if got a char type, playerNum/charType
//

static Boolean PlayerReceiveVehicleTypeFromOthers(short *playerNum, short *charType, short *sex)
{
	IMPLEMENT_ME_SOFT(); return false;

NetPlayerCharTypeMessage		*mess;
NSpMessageHeader 				*inMess;
Boolean							gotType = false;

	inMess = NSpMessage_Get(gNetGame);					// get message
	if (inMess)
	{
		switch(inMess->what)
		{
			case	kNetPlayerCharTypeMessage:
					mess = (NetPlayerCharTypeMessage *)inMess;

					*playerNum	= mess->playerNum;					// get player #
					*charType	= mess->vehicleType;				// get character type
					*sex		= mess->sex;						// get character sex
					gotType 	= true;
					break;

			case	kNSpError:
					DoFatalAlert("PlayerReceiveVehicleTypeFromOthers: message == kNSpError");
					break;

			default:
					HandleOtherNetMessage(inMess);

		}
		NSpMessage_Release(gNetGame, inMess);			// dispose of message
	}

	return(gotType);
}


/******************* HANDLE OTHER NET MESSAGE ***********************/
//
// Called when other message handler's get a message that they don't expect to get.
//
// OUTPUT: returns TRUE if game terminated
//

static Boolean HandleOtherNetMessage(NSpMessageHeader	*message)
{
	IMPLEMENT_ME_SOFT(); return true;
#if 0

	switch(message->what)
	{

					/* AN ERROR MESSAGE */

		case	kNSpError:
				DoFatalAlert("HandleOtherNetMessage: kNSpError");


					/* A PLAYER UNEXPECTEDLY HAS LEFT THE GAME */

		case	kNSpPlayerLeft:
				PlayerUnexpectedlyLeavesGame((NSpPlayerLeftMessage *)message);
				break;

					/* THE HOST HAS UNEXPECTEDLY LEFT THE GAME */

		case	kNSpGameTerminated:
				DoAlert("Game Terminated: The Host has unexpectedly quit the game.");
				EndNetworkGame();
				gGameOver = true;
				break;

					/* NULL PACKET */

		case	kNetNullPacket:
				break;

		case	kNSpJoinRequest:
				DoFatalAlert("HandleOtherNetMessage: kNSpJoinRequest");

		case	kNSpJoinApproved:
				DoFatalAlert("HandleOtherNetMessage: kNSpJoinApproved");

		case	kNSpJoinDenied:
				DoFatalAlert("HandleOtherNetMessage: kNSpJoinDenied");

		case	kNSpPlayerJoined:
				DoFatalAlert("HandleOtherNetMessage: kNSpPlayerJoined");

		case	kNSpHostChanged:
				DoFatalAlert("HandleOtherNetMessage: kNSpHostChanged");

		case	kNSpGroupCreated:
				DoFatalAlert("HandleOtherNetMessage: kNSpGroupCreated");

		case	kNSpGroupDeleted:
				DoFatalAlert("HandleOtherNetMessage: kNSpGroupDeleted");

		case	kNSpPlayerAddedToGroup:
				DoFatalAlert("HandleOtherNetMessage: kNSpPlayerAddedToGroup");

		case	kNSpPlayerRemovedFromGroup:
				DoFatalAlert("HandleOtherNetMessage: kNSpPlayerAddedToGroup");

		case	kNSpPlayerTypeChanged:
				DoFatalAlert("HandleOtherNetMessage: kNSpPlayerAddedToGroup");

		default:
				DoFatalAlert("HandleOtherNetMessage: unknown");
	}

	return(gGameOver);
#endif
}


/***************** PLAYER UNEXPECTEDLY LEAVES GAME ***********************/
//
// Called when HandleOtherNetMessage() gets a kNSpPlayerLeft message from one of the other players.
//
// INPUT: playerID = ID# of player that sent message
//

static void PlayerUnexpectedlyLeavesGame(NSpPlayerLeftMessage *mess)
{
	IMPLEMENT_ME_SOFT();
#if 0
int			i;
NSpPlayerID	playerID = mess->playerID;

		/* FIND PLAYER NUM THAT MATCHES THE ID */

	for (i = 0; i < gNumTotalPlayers; i++)
	{
		if (!gPlayerInfo[i].isComputer)							// skip computer players
		{
			if (gPlayerInfo[i].nspPlayerID == playerID)			// see if ID matches
				 goto matched_id;
		}
	}
	DoFatalAlert("PlayerUnexpectedlyLeavesGame: cannot find matching player id#");


matched_id:
	gPlayerInfo[i].isComputer = true;							// turn it into a computer player.
	gPlayerInfo[i].isEliminated = true;							// also eliminate from battles
	gNumGatheredPlayers--;										// one less net player in the game
	gNumRealPlayers--;

	if (gNumRealPlayers <= 1)									// see if nobody to play with
		gGameOver = true;

			/* HANDLE SPECIFICS */

	switch(gGameMode)
	{
		case	GAME_MODE_TAG1:
		case	GAME_MODE_TAG2:
				if (gPlayerInfo[i].isIt)
					ChooseTaggedPlayer();
				break;
	}
#endif
}
#endif

#pragma mark -

/********************* PLAYER BROADCAST NULL PACKET *******************************/
//
// Send a dummy packet to all other players to let them know we're still active, but we're
// probably paused for some reason.  The recipients will then just wait and not time out.
//

void PlayerBroadcastNullPacket(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled())
	{
		PumpPangeaNetMessages();

		if (gIsNetworkClient)
		{
			ClientSend_ControlInfoToHost();
		}
		else if (gIsNetworkHost)
		{
			HostReceive_ControlInfoFromClients();
			HostSend_SnapshotToClients();
		}
		return;
	}
#endif

	(void)0;
#if 0
OSStatus					status;
NSpMessageHeader			outMess;


				/* BUILD MESSAGE */

	NSpClearMessageHeader(&outMess);

	outMess.to 			= kNSpAllPlayers;						// send to all clients
	outMess.what 		= kNetNullPacket;						// set message type
	outMess.messageLen 	= sizeof(outMess);						// set size of message


			/* SEND IT */

	status = NSpMessage_Send(gNetGame, &outMess, kNSpSendFlag_Registered);
	if (status)
		DoFatalAlert("PlayerBroadcastNullPacket: NSpMessage_Send failed!");
#endif
}


#pragma mark -


/********************* HOST SEND SNAPSHOT TO CLIENTS *******************************/
//
// Called by the host after MoveEverything() to broadcast authoritative car state to clients.
//

void HostSend_SnapshotToClients(void)
{
#ifdef __EMSCRIPTEN__
	if (!PangeaNetBridge_IsEnabled() || !gIsNetworkHost)
	{
		return;
	}

	uint8_t packetBytes[PANGEA_NET_MAX_PACKET_SIZE];
	PangeaNetWriter writer;
	PangeaNetPacketHeader header;
	uint8_t snapshotKind = kPangeaSnapshotDelta;
	uint32_t stateHash = 0;
	uint32_t advertisedKeyframeSeq = gPangeaLastHostKeyframeSeq;
	uint8_t clientNeedsKeyframe = 0;
	short i;

	gPangeaLocalTick++;
	for (i = 0; i < gNumRealPlayers; i++)
	{
		if (i == gMyNetworkPlayerNum)
		{
			continue;
		}
		if (gPangeaLastHostKeyframeSeq != 0 && gPangeaLastAckedKeyframeSeq[i] < gPangeaLastHostKeyframeSeq)
		{
			clientNeedsKeyframe = 1;
			break;
		}
	}
	if (gPangeaForceKeyframe || gPangeaLastHostKeyframeSeq == 0 || (gPangeaLocalTick % PANGEA_NET_KEYFRAME_INTERVAL_FRAMES) == 0)
	{
		snapshotKind = kPangeaSnapshotKeyframe;
	}
	if (clientNeedsKeyframe)
	{
		snapshotKind = kPangeaSnapshotKeyframe;
	}
	if (gTrackCompleted)
	{
		snapshotKind = kPangeaSnapshotMatchEnd;
	}
	header.magic = PANGEA_NET_MAGIC;
	header.version = PANGEA_NET_VERSION;
	header.packetType = kPangeaPacketHostSnapshot;
	header.matchIdLow = gPangeaMatchId;
	header.matchIdHigh = gPangeaMatchIdHigh;
	header.tick = gPangeaLocalTick;
	header.sequence = gPangeaSendSequence++;
	header.playerIndex = PANGEA_NET_PLAYER_NA;
	header.reserved = 0;
	if (snapshotKind == kPangeaSnapshotKeyframe)
	{
		advertisedKeyframeSeq = header.sequence;
	}

	PangeaNetWriter_Init(&writer, packetBytes, (int)sizeof(packetBytes));
	PangeaNet_WriteHeader(&writer, &header);
	stateHash = PangeaNet_ComputeAuthoritativeStateHash(gNumRealPlayers);
	PangeaNetWriter_WriteU16(&writer, (uint16_t)PANGEA_NET_VERSION);
	PangeaNetWriter_WriteU8(&writer, snapshotKind);
	PangeaNetWriter_WriteU8(&writer, 0);
	PangeaNetWriter_WriteU32(&writer, stateHash);
	PangeaNetWriter_WriteU32(&writer, advertisedKeyframeSeq);
	PangeaNetWriter_WriteU32(&writer, gPangeaLastHostDeltaSeq);
	PangeaNetWriter_WriteU8(&writer, (uint8_t)gNumRealPlayers);
	PangeaNetWriter_WriteU16(&writer, (uint16_t)gWhoIsIt);
	PangeaNetWriter_WriteU16(&writer, (uint16_t)gWhoWasIt);
	PangeaNetWriter_WriteU16(&writer, (uint16_t)gCapturedFlagCount[0]);
	PangeaNetWriter_WriteU16(&writer, (uint16_t)gCapturedFlagCount[1]);
	PangeaNetWriter_WriteU16(&writer, (uint16_t)gNumPlayersEliminated);
	PangeaNetWriter_WriteU16(&writer, 0);
	PangeaNetWriter_WriteF32(&writer, gReTagTimer);

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		const ObjNode* obj = gPlayerInfo[i].objNode;
		const OGLVector3D zeroDelta = {0.0f, 0.0f, 0.0f};
		const OGLVector3D delta = obj ? obj->Delta : zeroDelta;
		const OGLVector3D deltaRot = obj ? obj->DeltaRot : zeroDelta;

		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].coord.x);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].coord.y);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].coord.z);
		PangeaNetWriter_WriteF32(&writer, obj ? obj->Rot.x : 0.0f);
		PangeaNetWriter_WriteF32(&writer, obj ? obj->Rot.y : 0.0f);
		PangeaNetWriter_WriteF32(&writer, obj ? obj->Rot.z : 0.0f);
		PangeaNetWriter_WriteF32(&writer, delta.x);
		PangeaNetWriter_WriteF32(&writer, delta.y);
		PangeaNetWriter_WriteF32(&writer, delta.z);
		PangeaNetWriter_WriteF32(&writer, deltaRot.x);
		PangeaNetWriter_WriteF32(&writer, deltaRot.y);
		PangeaNetWriter_WriteF32(&writer, deltaRot.z);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].steering);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].currentThrust);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].currentRPM);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].skidDot);
		PangeaNetWriter_WriteU32(&writer, gPlayerInfo[i].controlBits);
		PangeaNetWriter_WriteU32(&writer, gPlayerInfo[i].controlBits_New);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].analogSteering.x);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].analogSteering.y);
		PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[i].lapNum);
		PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[i].checkpointNum);
		PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[i].place);
		PangeaNetWriter_WriteU8(&writer, gPlayerInfo[i].raceComplete ? 1 : 0);
		PangeaNetWriter_WriteU8(&writer, gPlayerInfo[i].wrongWay ? 1 : 0);
		PangeaNetWriter_WriteU8(&writer, gPlayerInfo[i].isEliminated ? 1 : 0);
		PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[i].powType);
		PangeaNetWriter_WriteU16(&writer, (uint16_t)gPlayerInfo[i].powQuantity);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].health);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].tagTimer);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].frozenTimer);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].greasedTiresTimer);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].nitroTimer);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].stickyTiresTimer);
		PangeaNetWriter_WriteF32(&writer, gPlayerInfo[i].invisibilityTimer);
		PangeaNetWriter_WriteU8(&writer, gPlayerInfo[i].isIt ? 1 : 0);
		PangeaNetWriter_WriteU8(&writer, gPlayerInfo[i].movingBackwards ? 1 : 0);
		PangeaNetWriter_WriteU8(&writer, gPlayerInfo[i].accelBackwards ? 1 : 0);
		PangeaNetWriter_WriteU8(&writer, gPlayerInfo[i].braking ? 1 : 0);
		PangeaNetWriter_WriteU8(&writer, gPlayerInfo[i].onWater ? 1 : 0);
		PangeaNetWriter_WriteU8(&writer, 0);
		PangeaNetWriter_WriteU8(&writer, 0);
		PangeaNetWriter_WriteU8(&writer, 0);
		PangeaNetWriter_WriteU32(&writer, gPangeaConnState[i].lastReceivedSequence);
	}

	/* Write torch objective state */
	PangeaNetWriter_WriteU8(&writer, (uint8_t)gNumTorches);
	for (int t = 0; t < gNumTorches && t < PANGEA_NET_MAX_TORCHES; t++)
	{
		const ObjNode* torch = gTorchObjs[t];
		if (!torch)
		{
			PangeaNetWriter_WriteU8(&writer, 0xFF);
			PangeaNetWriter_WriteU8(&writer, 0xFF);
			PangeaNetWriter_WriteF32(&writer, 0.0f);
			PangeaNetWriter_WriteF32(&writer, 0.0f);
			PangeaNetWriter_WriteF32(&writer, 0.0f);
			continue;
		}
		PangeaNetWriter_WriteU8(&writer, (uint8_t)torch->Mode);
		PangeaNetWriter_WriteU8(&writer, (uint8_t)(torch->Mode == 1 ? torch->PlayerNum : 0xFF));
		PangeaNetWriter_WriteF32(&writer, torch->Coord.x);
		PangeaNetWriter_WriteF32(&writer, torch->Coord.y);
		PangeaNetWriter_WriteF32(&writer, torch->Coord.z);
	}

	if (!writer.ok)
	{
		SDL_Log("CroMag net: failed to encode host snapshot");
		return;
	}

	const int sent = PangeaNetBridge_SendReliable(packetBytes, writer.cursor);
	if (snapshotKind == kPangeaSnapshotKeyframe)
	{
		gPangeaLastHostKeyframeSeq = header.sequence;
	}
	else
	{
		gPangeaLastHostDeltaSeq = header.sequence;
	}
	gPangeaForceKeyframe = 0;
	if (PangeaNet_ShouldLogSequence(header.sequence) || !sent)
	{
		const ObjNode* player0Obj = gPlayerInfo[0].objNode;
		const ObjNode* player1Obj = gPlayerInfo[1].objNode;
		SDL_Log("CroMag net: host send snapshot seq=%u kind=%u tick=%u bytes=%d sent=%d p0=(%.1f,%.1f,%.1f) p1=(%.1f,%.1f,%.1f) p1Bits=%08x p1Obj=%d",
			(unsigned)header.sequence,
			(unsigned)snapshotKind,
			(unsigned)header.tick,
			writer.cursor,
			sent,
			gPlayerInfo[0].coord.x,
			gPlayerInfo[0].coord.y,
			gPlayerInfo[0].coord.z,
			gPlayerInfo[1].coord.x,
			gPlayerInfo[1].coord.y,
			gPlayerInfo[1].coord.z,
			(unsigned)gPlayerInfo[1].controlBits,
			player0Obj && player1Obj ? 1 : 0);
	}
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if (gPlayerInfo[i].raceComplete && !gPangeaReliableRaceCompleteSent[i])
		{
			PangeaNet_SendReliableEvent(kPangeaReliableEventRaceComplete, i);
			gPangeaReliableRaceCompleteSent[i] = 1;
		}
		if (gPlayerInfo[i].isEliminated && !gPangeaReliableEliminatedSent[i])
		{
			PangeaNet_SendReliableEvent(kPangeaReliableEventEliminated, i);
			gPangeaReliableEliminatedSent[i] = 1;
		}
	}
	if (gPangeaPrevWhoIsIt >= 0 && gWhoIsIt != gPangeaPrevWhoIsIt)
	{
		PangeaNet_SendTagHandoffEvent();
	}
	gPangeaPrevWhoIsIt = gWhoIsIt;
#endif
}


/********************* CLIENT APPLY PENDING SNAPSHOT *******************************/
//
// Called by clients before MoveEverything() to apply the latest host snapshot to
// remote cars (own car is not overridden).
//

void ClientApplyPendingSnapshot(void)
{
#ifdef __EMSCRIPTEN__
	if (!PangeaNetBridge_IsEnabled() || !gIsNetworkClient)
	{
		return;
	}

	PumpPangeaNetMessages();

	if (!gHavePendingSnapshot)
	{
		const double nowMs = (double)SDL_GetTicks();
		if (gPangeaLastReceivedKeyframeAtMs > 0.0
			&& (nowMs - gPangeaLastReceivedKeyframeAtMs) > PANGEA_NET_KEYFRAME_TIMEOUT_MS
			&& (nowMs - gPangeaLastResendRequestAtMs) > 350.0)
		{
			uint8_t resendBytes[64];
			PangeaNetWriter resendWriter;
			PangeaNetPacketHeader resendHeader;
			resendHeader.magic = PANGEA_NET_MAGIC;
			resendHeader.version = PANGEA_NET_VERSION;
			resendHeader.packetType = kPangeaPacketKeyframeResendRequest;
			resendHeader.matchIdLow = gPangeaMatchId;
			resendHeader.matchIdHigh = gPangeaMatchIdHigh;
			resendHeader.tick = gPangeaLocalTick;
			resendHeader.sequence = gPangeaSendSequence++;
			resendHeader.playerIndex = (uint16_t)gMyNetworkPlayerNum;
			resendHeader.reserved = 0;
			PangeaNetWriter_Init(&resendWriter, resendBytes, (int)sizeof(resendBytes));
			PangeaNet_WriteHeader(&resendWriter, &resendHeader);
			PangeaNetWriter_WriteU32(&resendWriter, gPangeaLastHostKeyframeSeq);
			if (resendWriter.ok)
			{
				PangeaNetBridge_SendReliable(resendBytes, resendWriter.cursor);
			}
			gPangeaLastResendRequestAtMs = nowMs;
			PangeaNetBridge_ReportDesync(gPangeaLocalTick, 0, gPangeaLastSnapshotStateHash);
		}
		return;
	}

	short i;
	gWhoIsIt = (short) gPendingSnapshot.whoIsIt;
	gWhoWasIt = (short) gPendingSnapshot.whoWasIt;
	gCapturedFlagCount[0] = (short) gPendingSnapshot.capturedFlagCount[0];
	gCapturedFlagCount[1] = (short) gPendingSnapshot.capturedFlagCount[1];
	gNumPlayersEliminated = (short) gPendingSnapshot.numPlayersEliminated;
	gReTagTimer = gPendingSnapshot.reTagTimer;
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		const PangeaNetPlayerCarState* s = &gPendingSnapshot.players[i];
		if (i == gMyNetworkPlayerNum)
		{
			const float dx = gPlayerInfo[i].coord.x - s->coord.x;
			const float dy = gPlayerInfo[i].coord.y - s->coord.y;
			const float dz = gPlayerInfo[i].coord.z - s->coord.z;
			const float correctionDistance = sqrtf(dx * dx + dy * dy + dz * dz);
			gPangeaCorrectionDistance = correctionDistance;

			gPlayerInfo[i].lapNum = s->lapNum;
			gPlayerInfo[i].checkpointNum = s->checkpointNum;
			gPlayerInfo[i].place = s->place;
			gPlayerInfo[i].raceComplete = s->raceComplete != 0;
			gPlayerInfo[i].wrongWay = s->wrongWay != 0;
			gPlayerInfo[i].powType = s->powType;
			gPlayerInfo[i].powQuantity = s->powQuantity;
			gPlayerInfo[i].health = s->health;
			gPlayerInfo[i].currentRPM = s->currentRPM;
			gPlayerInfo[i].skidDot = s->skidDot;
			gPlayerInfo[i].tagTimer = s->tagTimer;
			gPlayerInfo[i].isIt = s->isIt != 0;
			gPlayerInfo[i].movingBackwards = s->movingBackwards != 0;
			gPlayerInfo[i].accelBackwards = s->accelBackwards != 0;
			gPlayerInfo[i].braking = s->braking != 0;
			gPlayerInfo[i].onWater = s->onWater != 0;
			gPlayerInfo[i].isEliminated = gPlayerInfo[i].isEliminated || (s->health <= 0.0f);

			if (gPlayerInfo[i].objNode)
			{
				gPlayerInfo[i].coord = s->coord;
				gPlayerInfo[i].objNode->Coord = s->coord;
				gPlayerInfo[i].objNode->Delta = s->delta;
				gPlayerInfo[i].objNode->DeltaRot = s->deltaRot;
				gPlayerInfo[i].objNode->Rot.x = s->rotX;
				gPlayerInfo[i].objNode->Rot.y = s->rotY;
				gPlayerInfo[i].objNode->Rot.z = s->rotZ;
			}
			PangeaNet_ReapplyUnackedLocalInput(s->lastProcessedInputSequence, i);
			continue;
		}

		if (!gPlayerInfo[i].objNode)
		{
			continue;
		}

		PangeaNetSnapshotPlayerState delayedRemote = *s;
		if (PangeaNet_GetDelayedRemoteSnapshot(i, &delayedRemote))
		{
			s = &delayedRemote;
		}
		PangeaNet_SetRemoteInterpTarget(i, s);
		PangeaNetRemoteInterpState* interp = &gPangeaRemoteInterp[i];

		if (!interp->hasTo)
		{
			continue;
		}

		const OGLPoint3D targetCoord = interp->toState.coord;
		gPlayerInfo[i].coord = targetCoord;
		gPlayerInfo[i].objNode->Coord = targetCoord;
		gPlayerInfo[i].objNode->Rot.y = interp->toState.rotY;
		gPlayerInfo[i].objNode->Delta = interp->toState.delta;
		gPlayerInfo[i].steering = interp->toState.steering;
		gPlayerInfo[i].currentThrust = interp->toState.currentThrust;
		gPlayerInfo[i].controlBits = interp->toState.controlBits;
		gPlayerInfo[i].controlBits_New = interp->toState.controlBitsNew;
		gPlayerInfo[i].analogSteering = interp->toState.analogSteering;
		gPlayerInfo[i].lapNum = interp->toState.lapNum;
		gPlayerInfo[i].checkpointNum = interp->toState.checkpointNum;
		gPlayerInfo[i].place = interp->toState.place;
		gPlayerInfo[i].raceComplete = interp->toState.raceComplete != 0;
		gPlayerInfo[i].wrongWay = interp->toState.wrongWay != 0;
		gPlayerInfo[i].powType = interp->toState.powType;
		gPlayerInfo[i].powQuantity = interp->toState.powQuantity;
		gPlayerInfo[i].health = interp->toState.health;
		gPlayerInfo[i].currentRPM = interp->toState.currentRPM;
		gPlayerInfo[i].skidDot = interp->toState.skidDot;
		gPlayerInfo[i].tagTimer = interp->toState.tagTimer;
		gPlayerInfo[i].isIt = interp->toState.isIt != 0;
		gPlayerInfo[i].movingBackwards = interp->toState.movingBackwards != 0;
		gPlayerInfo[i].accelBackwards = interp->toState.accelBackwards != 0;
		gPlayerInfo[i].braking = interp->toState.braking != 0;
		gPlayerInfo[i].onWater = interp->toState.onWater != 0;
		gPlayerInfo[i].objNode->Rot.x = interp->toState.rotX;
		if (PangeaNet_ShouldLogSequence(gPendingSnapshot.snapshotSeq))
		{
			SDL_Log("CroMag net: client apply remote snapshot seq=%u player=%d objCoord=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f) status=%08x hidden=%d shadow=%d",
				(unsigned)gPendingSnapshot.snapshotSeq,
				i,
				gPlayerInfo[i].objNode->Coord.x,
				gPlayerInfo[i].objNode->Coord.y,
				gPlayerInfo[i].objNode->Coord.z,
				targetCoord.x,
				targetCoord.y,
				targetCoord.z,
				(unsigned)gPlayerInfo[i].objNode->StatusBits,
				(gPlayerInfo[i].objNode->StatusBits & STATUS_BIT_HIDDEN) != 0,
				gPlayerInfo[i].objNode->ShadowNode != nil);
		}
		gPlayerInfo[i].objNode->Rot.y = interp->toState.rotY;
		gPlayerInfo[i].objNode->Rot.z = interp->toState.rotZ;
		gPlayerInfo[i].objNode->DeltaRot = interp->toState.deltaRot;
	}

	/* Apply torch objective state from snapshot */
	if (gHavePendingTorchState)
	{
		for (uint8_t t = 0; t < gPendingTorchCount && t < gNumTorches; t++)
		{
			ObjNode* torch = gTorchObjs[t];
			if (!torch) continue;
			const uint8_t mode = gPendingTorchMode[t];
			const uint8_t carrier = gPendingTorchCarrier[t];
			if (mode != 1 && torch->Mode == 1 && torch->CapturedFlag)
			{
				/* Torch was being carried — release from car */
				ObjNode* car = (ObjNode*)torch->CapturedFlag;
				car->CapturedFlag = nil;
				torch->CapturedFlag = nil;
			}
			torch->Mode = mode;
			if (mode == 1 && carrier < MAX_PLAYERS && gPlayerInfo[carrier].objNode)
			{
				ObjNode* car = gPlayerInfo[carrier].objNode;
				car->CapturedFlag = (Ptr)torch;
				torch->CapturedFlag = (Ptr)car;
			}
			torch->Coord.x = gPendingTorchCoordX[t];
			torch->Coord.y = gPendingTorchCoordY[t];
			torch->Coord.z = gPendingTorchCoordZ[t];
			UpdateObjectTransforms(torch);
		}
		gHavePendingTorchState = false;
	}

	{
		const uint32_t localHash = PangeaNet_ComputeAuthoritativeStateHash((short)gPendingSnapshot.playerCount);
		if (localHash != gPendingSnapshot.stateHash)
		{
			PangeaNetBridge_ReportDesync(gPendingSnapshot.frameCounter, localHash, gPendingSnapshot.stateHash);
		}
		uint8_t ackBytes[96];
		PangeaNetWriter writer;
		PangeaNetPacketHeader ackHeader;
		ackHeader.magic = PANGEA_NET_MAGIC;
		ackHeader.version = PANGEA_NET_VERSION;
		ackHeader.packetType = kPangeaPacketClientAck;
		ackHeader.matchIdLow = gPangeaMatchId;
		ackHeader.matchIdHigh = gPangeaMatchIdHigh;
		ackHeader.tick = gPendingSnapshot.frameCounter;
		ackHeader.sequence = gPangeaSendSequence++;
		ackHeader.playerIndex = (uint16_t)gMyNetworkPlayerNum;
		ackHeader.reserved = 0;
		PangeaNetWriter_Init(&writer, ackBytes, (int)sizeof(ackBytes));
		PangeaNet_WriteHeader(&writer, &ackHeader);
		PangeaNetWriter_WriteU32(&writer, gPendingSnapshot.snapshotSeq);
		PangeaNetWriter_WriteU32(&writer, gPendingSnapshot.lastKeyframeSeq);
		PangeaNetWriter_WriteU32(&writer, gPendingSnapshot.lastDeltaSeq);
		if (writer.ok)
		{
			PangeaNetBridge_SendReliable(ackBytes, writer.cursor);
		}
	}
	gHavePendingSnapshot = false;
#endif
}

Boolean PangeaNet_IsHostAuthoritativeRemotePlayer(short playerNum)
{
#ifdef __EMSCRIPTEN__
	if (!PangeaNetBridge_IsEnabled() || !gIsNetworkClient)
	{
		return false;
	}
	if (playerNum < 0 || playerNum >= MAX_PLAYERS)
	{
		return false;
	}
	return playerNum != gMyNetworkPlayerNum;
#else
	(void)playerNum;
	return false;
#endif
}

void PangeaNet_ForceKeyframe(void)
{
#ifdef __EMSCRIPTEN__
	if (PangeaNetBridge_IsEnabled() && gIsNetworkHost)
	{
		gPangeaForceKeyframe = 1;
	}
#endif
}

void PangeaNet_RequestTagHandoff(short fromPlayer, short toPlayer)
{
#ifdef __EMSCRIPTEN__
	if (!PangeaNetBridge_IsEnabled())
	{
		return;
	}
	if (gIsNetworkHost)
	{
		PangeaNet_ApplyTagHandoff(fromPlayer, toPlayer);
		return;
	}
	if (!gIsNetworkClient)
	{
		return;
	}
	if (fromPlayer < 0 || fromPlayer >= MAX_PLAYERS || toPlayer < 0 || toPlayer >= MAX_PLAYERS)
	{
		return;
	}
	if (fromPlayer != gMyNetworkPlayerNum && toPlayer != gMyNetworkPlayerNum)
	{
		return;
	}

	uint8_t bytes[64];
	PangeaNetWriter writer;
	PangeaNetPacketHeader header;
	header.magic = PANGEA_NET_MAGIC;
	header.version = PANGEA_NET_VERSION;
	header.packetType = kPangeaPacketTagHandoffRequest;
	header.matchIdLow = gPangeaMatchId;
	header.matchIdHigh = gPangeaMatchIdHigh;
	header.tick = gPangeaLocalTick;
	header.sequence = gPangeaSendSequence++;
	header.playerIndex = (uint16_t)gMyNetworkPlayerNum;
	header.reserved = 0;
	PangeaNetWriter_Init(&writer, bytes, (int)sizeof(bytes));
	PangeaNet_WriteHeader(&writer, &header);
	PangeaNetWriter_WriteU8(&writer, (uint8_t)fromPlayer);
	PangeaNetWriter_WriteU8(&writer, (uint8_t)toPlayer);
	if (writer.ok)
	{
		PangeaNetBridge_SendReliable(bytes, writer.cursor);
	}
#else
	(void)fromPlayer;
	(void)toPlayer;
#endif
}

Boolean PangeaNet_IsHostAuthoritativeCpuSimulation(short playerNum)
{
	if (!PangeaNet_IsHostAuthoritativeRemotePlayer(playerNum))
	{
		return false;
	}
	return gPlayerInfo[playerNum].isComputer;
}

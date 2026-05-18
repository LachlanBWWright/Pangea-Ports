//
// network.h
//

#pragma once

//#include <NetSprocket.h>
#include "main.h"

enum
{
	kStandardMessageSize	= 256,	//0,
	kBufferSize				= 200000, 	//0,
	kQElements				= 200,
	kTimeout				= 0
};


enum
{
	kNetConfigureMessage = 1,
	kNetSyncMessage,
	kNetHostControlInfoMessage,
	kNetClientControlInfoMessage,
	kNetPlayerCharTypeMessage,
	kNetNullPacket
};


		/***************************/
		/* MESSAGE DATA STRUCTURES */
		/***************************/

		/* GAME CONFIGURATION MESSAGE */

typedef struct
{
//	NSpMessageHeader	h;
	int					gameMode;							// game mode (tag, race, etc.)
	int					age;								// which age to play for race mode
	int					trackNum;							// which track to play for battle modes
	long				playerNum;							// this player's index
	long				numPlayers;							// # players in net game
	short				numAgesCompleted;					// pass saved game value to clients so we're all the same here
	short				difficulty;							// pass host's difficulty setting so we're in sync
	short				tagDuration;						// # minutes in tag game
}NetConfigMessageType;

		/* SYNC MESSAGE */

typedef struct
{
//	NSpMessageHeader	h;
	long				playerNum;							// this player's index
}NetSyncMessageType;


		/* HOST CONTROL INFO MESSAGE */

typedef struct
{
//	NSpMessageHeader	h;
	float				fps, fpsFrac;
	uint32_t				randomSeed;					// simply used for error checking (all machines should have same seed!)
	uint32_t				controlBits[MAX_PLAYERS];
	uint32_t				controlBitsNew[MAX_PLAYERS];
	OGLVector2D			analogSteering[MAX_PLAYERS];
	uint32_t				frameCounter;
}NetHostControlInfoMessageType;


		/* CLIENT CONTROL INFO MESSAGE */

typedef struct
{
//	NSpMessageHeader	h;
	short				playerNum;
	uint32_t				controlBits;
	uint32_t				controlBitsNew;
	uint32_t				frameCounter;
	OGLVector2D			analogSteering;
}NetClientControlInfoMessageType;


		/* PLAYER CHAR TYPE MESSAGE */

typedef struct
{
//	NSpMessageHeader	h;
	short				playerNum;
	short				vehicleType;
	short				sex;				// 0 = male, 1 = female
}NetPlayerCharTypeMessage;


		/* PER-PLAYER CAR STATE (for host snapshots) */

typedef struct
{
	OGLPoint3D			coord;
	float				rotY;
	OGLVector3D			delta;
	float				steering;
	float				currentThrust;
	uint32_t			controlBits;
	uint32_t			controlBitsNew;
	OGLVector2D			analogSteering;
	short				lapNum;
	short				checkpointNum;
	short				place;
	uint8_t				raceComplete;
	uint8_t				pad;
	short				powType;
	short				powQuantity;
	float				health;
	float				frozenTimer;
	float				greasedTiresTimer;
	float				nitroTimer;
	float				stickyTiresTimer;
	float				invisibilityTimer;
	uint8_t				isEliminated;
	uint8_t				pad2[3];
	uint32_t			lastProcessedInputSequence;
}PangeaNetPlayerCarState;

enum
{
	kPangeaSnapshotDelta = 0,
	kPangeaSnapshotKeyframe = 1,
	kPangeaSnapshotCorrection = 2,
	kPangeaSnapshotMatchEnd = 3
};


		/* HOST SNAPSHOT PACKET */

typedef struct
{
	uint16_t					protocolVersion;
	uint8_t						snapshotKind;
	uint8_t						reserved0;
	uint32_t					packetType;
	uint32_t					snapshotSeq;
	uint32_t					frameCounter;
	uint32_t					stateHash;
	uint32_t					lastKeyframeSeq;
	uint32_t					lastDeltaSeq;
	uint8_t						playerCount;
	uint8_t						pad[3];
	PangeaNetPlayerCarState		players[MAX_PLAYERS];
}PangeaNetHostSnapshotPacket;


//===============================================================================


void InitNetworkManager(void);
Boolean SetupNetworkHosting(void);
Boolean SetupNetworkJoin(void);
void ClientTellHostLevelIsPrepared(void);
void HostWaitForPlayersToPrepareLevel(void);

void HostSend_ControlInfoToClients(void);
void ClientSend_ControlInfoToHost(void);
void ClientReceive_ControlInfoFromHost(void);
void HostReceive_ControlInfoFromClients(void);

void HostSend_SnapshotToClients(void);
void ClientApplyPendingSnapshot(void);
Boolean PangeaNet_IsHostAuthoritativeRemotePlayer(short playerNum);
Boolean PangeaNet_IsHostAuthoritativeCpuSimulation(short playerNum);

void PlayerBroadcastVehicleType(void);
void GetVehicleSelectionFromNetPlayers(void);


void EndNetworkGame(void);
void PlayerBroadcastNullPacket(void);

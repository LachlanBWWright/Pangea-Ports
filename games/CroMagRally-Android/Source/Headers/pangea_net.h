#pragma once

#include <stdint.h>
#include "main.h"

int PangeaNetBridge_IsEnabled(void);
int PangeaNetBridge_IsHost(void);
int PangeaNetBridge_GetLocalPlayerIndex(void);
int PangeaNetBridge_GetPlayerCount(void);
uint32_t PangeaNetBridge_GetMatchSeed(void);
uint32_t PangeaNetBridge_GetMatchIdLow(void);
uint32_t PangeaNetBridge_GetMatchIdHigh(void);
void PangeaNetBridge_SetRuntimeMatchIdentity(uint32_t matchIdLow, uint32_t matchIdHigh);
int PangeaNetBridge_SendReliable(const void* bytes, int byteCount);
int PangeaNetBridge_SendUnreliable(const void* bytes, int byteCount);
int PangeaNetBridge_PollMessage(void* outBytes, int maxByteCount);
void PangeaNetBridge_ReportDesync(uint32_t frame, uint32_t localHash, uint32_t remoteHash);
void PangeaNetBridge_ReportMatchEnded(int reason);
void PangeaNetBridge_ReportMatchResult(const char* resultJson);
void PangeaNetBridge_ResetMatchLifecycle(void);
void PangeaNetBridge_UpdateMatchLifecycle(void);
void PangeaNetBridge_PublishLocalMatchLifecycle(void);
int PangeaNetBridge_GetRemoteLifecycleReason(void);
int PangeaNetBridge_GetLastMatchEndReason(void);
int PangeaNetBridge_HasMatchResult(void);

enum
{
	PANGEA_NET_MATCH_STATE_NONE = 0,
	PANGEA_NET_MATCH_STATE_GAME_OVER = 1,
	PANGEA_NET_MATCH_STATE_TRACK_COMPLETED = 2,
};

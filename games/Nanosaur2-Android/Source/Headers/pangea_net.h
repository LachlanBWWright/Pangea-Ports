#pragma once

#ifdef __EMSCRIPTEN__
#include <stdint.h>

int PangeaNet_IsEnabled(void);
int PangeaNet_IsHost(void);
int PangeaNet_GetLocalPlayerIndex(void);
int PangeaNet_GetPlayerCount(void);
int PangeaNet_IsOnlineMatch(void);
int PangeaNet_IsLocalPlayer(int playerNum);
int PangeaNet_IsRemotePlayer(int playerNum);
int PangeaNet_ShouldSimulateGameplayForPlayer(int playerNum);
int PangeaNet_ShouldRenderReplicatedPlayer(int playerNum);
uint32_t PangeaNet_GetMatchSeed(void);
uint32_t PangeaNet_GetMatchIdLow(void);
uint32_t PangeaNet_GetMatchIdHigh(void);
int PangeaNet_SendReliable(const void* bytes, int byteCount);
int PangeaNet_SendUnreliable(const void* bytes, int byteCount);
int PangeaNet_PollMessage(void* outBytes, int maxByteCount);
double PangeaNet_NowMilliseconds(void);
void PangeaNet_ReportDesync(uint32_t frame, uint32_t localHash, uint32_t remoteHash);
void PangeaNet_ReportMatchEnded(int reason);
int PangeaNet_GetRemoteLifecycleReason(void);
void PangeaNet_ResetNetworkSequenceTracking(void);
void PangeaNet_UpdateMatchLifecycle(void);
void PangeaNet_PublishLocalMatchLifecycle(void);
int PangeaNet_HostIsRemoteNeedActive(int playerNum, int needID);
int PangeaNet_HostIsRemoteNeedDown(int playerNum, int needID);
float PangeaNet_HostGetRemoteAnalogX(int playerNum);
float PangeaNet_HostGetRemoteAnalogZ(int playerNum);
void PangeaNet_ClientSendInput(void);
void PangeaNet_HostReceiveInputs(void);
void PangeaNet_HostSendSnapshot(void);
void PangeaNet_ClientApplySnapshot(void);
#endif

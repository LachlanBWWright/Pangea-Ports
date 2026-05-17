#pragma once

#include <stdint.h>
#include "main.h"

int PangeaNetBridge_IsEnabled(void);
int PangeaNetBridge_IsHost(void);
int PangeaNetBridge_GetLocalPlayerIndex(void);
int PangeaNetBridge_GetPlayerCount(void);
uint32_t PangeaNetBridge_GetMatchSeed(void);
int PangeaNetBridge_SendReliable(const void* bytes, int byteCount);
int PangeaNetBridge_PollMessage(void* outBytes, int maxByteCount);

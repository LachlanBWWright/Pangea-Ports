#include "pangea_net.h"

#ifdef __EMSCRIPTEN__
extern int PangeaNet_IsEnabled(void);
extern int PangeaNet_IsHost(void);
extern int PangeaNet_GetLocalPlayerIndex(void);
extern int PangeaNet_GetPlayerCount(void);
extern uint32_t PangeaNet_GetMatchSeed(void);
extern int PangeaNet_SendReliable(const void* bytes, int byteCount);
extern int PangeaNet_PollMessage(void* outBytes, int maxByteCount);
#endif

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

int PangeaNetBridge_PollMessage(void* outBytes, int maxByteCount)
{
#ifdef __EMSCRIPTEN__
	return PangeaNet_PollMessage(outBytes, maxByteCount);
#else
	(void) outBytes;
	(void) maxByteCount;
	return 0;
#endif
}

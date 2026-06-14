#include "../Headers/profiling.h"
#include <SDL3/SDL.h> // For SDL_GetPerformanceCounter and SDL_GetPerformanceFrequency

ProfilePhase gProfilePhases[NUM_PROFILE_PHASES];
static uint64_t gPerformanceFrequency;
static int gCurrentPhase = -1;

int gDrawCallsThisFrame = 0;
int gCacheLookupsThisFrame = 0;
int gCacheHitsThisFrame = 0;
int gCacheMissesThisFrame = 0;
int gCacheEvictionsThisFrame = 0;
int gCacheInvalidationsThisFrame = 0;
int gIndexScansThisFrame = 0;
int gIndicesScannedThisFrame = 0;
int gVerticesUploadedThisFrame = 0;
int gBytesUploadedThisFrame = 0;
int gImmediateDrawsThisFrame = 0;
int gImmediateBytesUploadedThisFrame = 0;
int gImmediateSourceDrawsThisFrame[NUM_PROFILE_IMMEDIATE_SOURCES] = {0};
int gImmediateSourceBytesThisFrame[NUM_PROFILE_IMMEDIATE_SOURCES] = {0};

int gDrawCallsLastFrame = 0;
int gCacheLookupsLastFrame = 0;
int gCacheHitsLastFrame = 0;
int gCacheMissesLastFrame = 0;
int gCacheEvictionsLastFrame = 0;
int gCacheInvalidationsLastFrame = 0;
int gIndexScansLastFrame = 0;
int gIndicesScannedLastFrame = 0;
int gVerticesUploadedLastFrame = 0;
int gBytesUploadedLastFrame = 0;
int gImmediateDrawsLastFrame = 0;
int gImmediateBytesUploadedLastFrame = 0;
int gImmediateSourceDrawsLastFrame[NUM_PROFILE_IMMEDIATE_SOURCES] = {0};
int gImmediateSourceBytesLastFrame[NUM_PROFILE_IMMEDIATE_SOURCES] = {0};
float gRenderSectionMsLastFrame[NUM_PROFILE_RENDER_SECTIONS] = {0};
float gRenderSubphaseMsLastFrame[NUM_PROFILE_RENDER_SUBPHASES] = {0};
float gSwapSubphaseMsLastFrame[NUM_PROFILE_SWAP_SUBPHASES] = {0};

static ProfileImmediateSource gPendingImmediateSource = PROFILE_IMMEDIATE_OTHER;
static uint64_t gRenderSectionStartTick[NUM_PROFILE_RENDER_SECTIONS] = {0};
static uint64_t gRenderSectionTicksThisFrame[NUM_PROFILE_RENDER_SECTIONS] = {0};
static uint64_t gRenderSubphaseStartTick[NUM_PROFILE_RENDER_SUBPHASES] = {0};
static uint64_t gRenderSubphaseTicksThisFrame[NUM_PROFILE_RENDER_SUBPHASES] = {0};
static int gRenderSubphaseDepth[NUM_PROFILE_RENDER_SUBPHASES] = {0};
static uint64_t gSwapSubphaseStartTick[NUM_PROFILE_SWAP_SUBPHASES] = {0};
static uint64_t gSwapSubphaseTicksThisFrame[NUM_PROFILE_SWAP_SUBPHASES] = {0};

void InitProfiling(void) {
    gPerformanceFrequency = SDL_GetPerformanceFrequency();
    for (int i = 0; i < NUM_PROFILE_PHASES; ++i) {
        gProfilePhases[i].start_tick = 0;
        gProfilePhases[i].total_ticks = 0;
        gProfilePhases[i].samples = 0;
        gProfilePhases[i].last_frame_ms = 0.0f;
    }
    gProfilePhases[PROFILE_PHASE_INPUT].name = "Input";
    gProfilePhases[PROFILE_PHASE_GAME_LOGIC].name = "Game Logic";
    gProfilePhases[PROFILE_PHASE_RENDERING].name = "Rendering";
    gProfilePhases[PROFILE_PHASE_UI].name = "UI";
    gProfilePhases[PROFILE_PHASE_SWAP_BUFFERS].name = "Swap Buffers";
    
    gCurrentPhase = -1;
}

void StartProfilePhase(ProfilePhaseType phase_type) {
    // Auto-end the current phase if one is active
    if (gCurrentPhase != -1) {
        EndProfilePhase((ProfilePhaseType)gCurrentPhase);
    }

    if (phase_type >= 0 && phase_type < NUM_PROFILE_PHASES) {
        gProfilePhases[phase_type].start_tick = SDL_GetPerformanceCounter();
        gCurrentPhase = phase_type;
    }
}

void EndProfilePhase(ProfilePhaseType phase_type) {
    if (phase_type >= 0 && phase_type < NUM_PROFILE_PHASES) {
        uint64_t end_tick = SDL_GetPerformanceCounter();
        if (gProfilePhases[phase_type].start_tick != 0) { // Ensure phase was started
            gProfilePhases[phase_type].total_ticks += (end_tick - gProfilePhases[phase_type].start_tick);
            gProfilePhases[phase_type].samples++;
            gProfilePhases[phase_type].start_tick = 0; // Reset for next frame
        }
        
        // If we just ended the current tracking phase, mark it as none
        if (gCurrentPhase == (int)phase_type) {
            gCurrentPhase = -1;
        }
    }
}

float GetProfilePhaseMs(ProfilePhaseType phase_type) {
    if (phase_type >= 0 && phase_type < NUM_PROFILE_PHASES) {
        if (gProfilePhases[phase_type].samples > 0) {
            return (float)(((double)gProfilePhases[phase_type].total_ticks * 1000.0) / (double)gPerformanceFrequency);
        }
        return gProfilePhases[phase_type].last_frame_ms;
    }
    return 0.0f;
}

void SetImmediateDrawSource(ProfileImmediateSource source) {
    if (source >= 0 && source < NUM_PROFILE_IMMEDIATE_SOURCES) {
        gPendingImmediateSource = source;
    }
}

ProfileImmediateSource ConsumeImmediateDrawSource(void) {
    ProfileImmediateSource source = gPendingImmediateSource;
    gPendingImmediateSource = PROFILE_IMMEDIATE_OTHER;
    return source;
}

void BeginRenderSection(ProfileRenderSection section) {
    if (section >= 0 && section < NUM_PROFILE_RENDER_SECTIONS) {
        gRenderSectionStartTick[section] = SDL_GetPerformanceCounter();
    }
}

void EndRenderSection(ProfileRenderSection section) {
    if (section >= 0 && section < NUM_PROFILE_RENDER_SECTIONS) {
        uint64_t startTick = gRenderSectionStartTick[section];
        if (startTick != 0) {
            gRenderSectionTicksThisFrame[section] += SDL_GetPerformanceCounter() - startTick;
            gRenderSectionStartTick[section] = 0;
        }
    }
}

float GetRenderSectionMs(ProfileRenderSection section) {
    if (section >= 0 && section < NUM_PROFILE_RENDER_SECTIONS) {
        return gRenderSectionMsLastFrame[section];
    }
    return 0.0f;
}

void BeginRenderSubphase(ProfileRenderSubphase subphase) {
    if (subphase >= 0 && subphase < NUM_PROFILE_RENDER_SUBPHASES) {
        if (gRenderSubphaseDepth[subphase] == 0) {
            gRenderSubphaseStartTick[subphase] = SDL_GetPerformanceCounter();
        }
        gRenderSubphaseDepth[subphase]++;
    }
}

void EndRenderSubphase(ProfileRenderSubphase subphase) {
    if (subphase >= 0 && subphase < NUM_PROFILE_RENDER_SUBPHASES) {
        if (gRenderSubphaseDepth[subphase] <= 0) {
            return;
        }

        gRenderSubphaseDepth[subphase]--;
        if (gRenderSubphaseDepth[subphase] == 0) {
            uint64_t startTick = gRenderSubphaseStartTick[subphase];
            if (startTick != 0) {
                gRenderSubphaseTicksThisFrame[subphase] += SDL_GetPerformanceCounter() - startTick;
                gRenderSubphaseStartTick[subphase] = 0;
            }
        }
    }
}

float GetRenderSubphaseMs(ProfileRenderSubphase subphase) {
    if (subphase >= 0 && subphase < NUM_PROFILE_RENDER_SUBPHASES) {
        return gRenderSubphaseMsLastFrame[subphase];
    }
    return 0.0f;
}

void BeginSwapSubphase(ProfileSwapSubphase subphase) {
    if (subphase >= 0 && subphase < NUM_PROFILE_SWAP_SUBPHASES) {
        gSwapSubphaseStartTick[subphase] = SDL_GetPerformanceCounter();
    }
}

void EndSwapSubphase(ProfileSwapSubphase subphase) {
    if (subphase >= 0 && subphase < NUM_PROFILE_SWAP_SUBPHASES) {
        uint64_t startTick = gSwapSubphaseStartTick[subphase];
        if (startTick != 0) {
            gSwapSubphaseTicksThisFrame[subphase] += SDL_GetPerformanceCounter() - startTick;
            gSwapSubphaseStartTick[subphase] = 0;
        }
    }
}

float GetSwapSubphaseMs(ProfileSwapSubphase subphase) {
    if (subphase >= 0 && subphase < NUM_PROFILE_SWAP_SUBPHASES) {
        return gSwapSubphaseMsLastFrame[subphase];
    }
    return 0.0f;
}

void ResetProfilingForFrame(void) {
    for (int i = 0; i < NUM_PROFILE_PHASES; ++i) {
        gProfilePhases[i].last_frame_ms = gProfilePhases[i].samples > 0
            ? (float)(((double)gProfilePhases[i].total_ticks * 1000.0) / (double)gPerformanceFrequency)
            : 0.0f;
        gProfilePhases[i].total_ticks = 0;
        gProfilePhases[i].samples = 0;
        gProfilePhases[i].start_tick = 0;
    }

    gDrawCallsLastFrame = gDrawCallsThisFrame;
    gCacheLookupsLastFrame = gCacheLookupsThisFrame;
    gCacheHitsLastFrame = gCacheHitsThisFrame;
    gCacheMissesLastFrame = gCacheMissesThisFrame;
    gCacheEvictionsLastFrame = gCacheEvictionsThisFrame;
    gCacheInvalidationsLastFrame = gCacheInvalidationsThisFrame;
    gIndexScansLastFrame = gIndexScansThisFrame;
    gIndicesScannedLastFrame = gIndicesScannedThisFrame;
    gVerticesUploadedLastFrame = gVerticesUploadedThisFrame;
    gBytesUploadedLastFrame = gBytesUploadedThisFrame;
    gImmediateDrawsLastFrame = gImmediateDrawsThisFrame;
    gImmediateBytesUploadedLastFrame = gImmediateBytesUploadedThisFrame;
    for (int i = 0; i < NUM_PROFILE_IMMEDIATE_SOURCES; i++) {
        gImmediateSourceDrawsLastFrame[i] = gImmediateSourceDrawsThisFrame[i];
        gImmediateSourceBytesLastFrame[i] = gImmediateSourceBytesThisFrame[i];
    }
    for (int i = 0; i < NUM_PROFILE_RENDER_SECTIONS; i++) {
        gRenderSectionMsLastFrame[i] = (float)(((double)gRenderSectionTicksThisFrame[i] * 1000.0) / (double)gPerformanceFrequency);
    }
    for (int i = 0; i < NUM_PROFILE_RENDER_SUBPHASES; i++) {
        gRenderSubphaseMsLastFrame[i] = (float)(((double)gRenderSubphaseTicksThisFrame[i] * 1000.0) / (double)gPerformanceFrequency);
    }
    for (int i = 0; i < NUM_PROFILE_SWAP_SUBPHASES; i++) {
        gSwapSubphaseMsLastFrame[i] = (float)(((double)gSwapSubphaseTicksThisFrame[i] * 1000.0) / (double)gPerformanceFrequency);
    }

    gDrawCallsThisFrame = 0;
    gCacheLookupsThisFrame = 0;
    gCacheHitsThisFrame = 0;
    gCacheMissesThisFrame = 0;
    gCacheEvictionsThisFrame = 0;
    gCacheInvalidationsThisFrame = 0;
    gIndexScansThisFrame = 0;
    gIndicesScannedThisFrame = 0;
    gVerticesUploadedThisFrame = 0;
    gBytesUploadedThisFrame = 0;
    gImmediateDrawsThisFrame = 0;
    gImmediateBytesUploadedThisFrame = 0;
    for (int i = 0; i < NUM_PROFILE_IMMEDIATE_SOURCES; i++) {
        gImmediateSourceDrawsThisFrame[i] = 0;
        gImmediateSourceBytesThisFrame[i] = 0;
    }
    for (int i = 0; i < NUM_PROFILE_RENDER_SECTIONS; i++) {
        gRenderSectionStartTick[i] = 0;
        gRenderSectionTicksThisFrame[i] = 0;
    }
    for (int i = 0; i < NUM_PROFILE_RENDER_SUBPHASES; i++) {
        gRenderSubphaseStartTick[i] = 0;
        gRenderSubphaseTicksThisFrame[i] = 0;
        gRenderSubphaseDepth[i] = 0;
    }
    for (int i = 0; i < NUM_PROFILE_SWAP_SUBPHASES; i++) {
        gSwapSubphaseStartTick[i] = 0;
        gSwapSubphaseTicksThisFrame[i] = 0;
    }
    gPendingImmediateSource = PROFILE_IMMEDIATE_OTHER;
}

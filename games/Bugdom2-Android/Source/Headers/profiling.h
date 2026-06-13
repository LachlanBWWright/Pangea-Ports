#ifndef PROFILING_H
#define PROFILING_H

#include <stdint.h> // For uint64_t, uint32_t

// Enum for profiling phases
typedef enum {
    PROFILE_PHASE_INPUT = 0,
    PROFILE_PHASE_GAME_LOGIC,
    PROFILE_PHASE_RENDERING,
    PROFILE_PHASE_UI,
    PROFILE_PHASE_SWAP_BUFFERS,
    NUM_PROFILE_PHASES
} ProfilePhaseType;

typedef enum {
    PROFILE_IMMEDIATE_OTHER = 0,
    PROFILE_IMMEDIATE_TEXT,
    PROFILE_IMMEDIATE_INFOBAR,
    PROFILE_IMMEDIATE_SPARKLE,
    PROFILE_IMMEDIATE_WATER,
    PROFILE_IMMEDIATE_SHADOW,
    PROFILE_IMMEDIATE_LENS_FLARE,
    PROFILE_IMMEDIATE_SHARDS,
    PROFILE_IMMEDIATE_LINES,
    NUM_PROFILE_IMMEDIATE_SOURCES
} ProfileImmediateSource;

// Struct to hold profiling data for a single phase
typedef struct {
    uint64_t start_tick;      // Start time of the current measurement
    uint64_t total_ticks;     // Accumulated ticks for this phase
    uint32_t samples;         // Number of samples taken
    float last_frame_ms;      // Total milliseconds measured in the previous frame
    const char* name;         // Name of the phase
} ProfilePhase;

// Global array of profiling phases
extern ProfilePhase gProfilePhases[NUM_PROFILE_PHASES];

extern int gDrawCallsThisFrame;
extern int gCacheLookupsThisFrame;
extern int gCacheHitsThisFrame;
extern int gCacheMissesThisFrame;
extern int gCacheEvictionsThisFrame;
extern int gCacheInvalidationsThisFrame;
extern int gIndexScansThisFrame;
extern int gIndicesScannedThisFrame;
extern int gVerticesUploadedThisFrame;
extern int gBytesUploadedThisFrame;
extern int gImmediateDrawsThisFrame;
extern int gImmediateBytesUploadedThisFrame;
extern int gImmediateSourceDrawsThisFrame[NUM_PROFILE_IMMEDIATE_SOURCES];
extern int gImmediateSourceBytesThisFrame[NUM_PROFILE_IMMEDIATE_SOURCES];

extern int gDrawCallsLastFrame;
extern int gCacheLookupsLastFrame;
extern int gCacheHitsLastFrame;
extern int gCacheMissesLastFrame;
extern int gCacheEvictionsLastFrame;
extern int gCacheInvalidationsLastFrame;
extern int gIndexScansLastFrame;
extern int gIndicesScannedLastFrame;
extern int gVerticesUploadedLastFrame;
extern int gBytesUploadedLastFrame;
extern int gImmediateDrawsLastFrame;
extern int gImmediateBytesUploadedLastFrame;
extern int gImmediateSourceDrawsLastFrame[NUM_PROFILE_IMMEDIATE_SOURCES];
extern int gImmediateSourceBytesLastFrame[NUM_PROFILE_IMMEDIATE_SOURCES];

// Initialize all profiling phases
void InitProfiling(void);

// Start timing a specific phase
void StartProfilePhase(ProfilePhaseType phase_type);

// End timing a specific phase
void EndProfilePhase(ProfilePhaseType phase_type);

// Get the measured millisecond cost of a phase for this frame (or the previous frame if not measured yet)
float GetProfilePhaseMs(ProfilePhaseType phase_type);

// Tag the next immediate-mode draw for debug attribution.
void SetImmediateDrawSource(ProfileImmediateSource source);

// Returns and clears the pending immediate-mode draw attribution.
ProfileImmediateSource ConsumeImmediateDrawSource(void);

// Call this at the end of each frame to snapshot totals for debug display and reset accumulators
void ResetProfilingForFrame(void);

#endif // PROFILING_H

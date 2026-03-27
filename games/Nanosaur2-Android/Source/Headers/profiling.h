#ifndef PROFILING_H
#define PROFILING_H

#include <stdint.h> // For uint64_t, uint32_t

// Enum for profiling phases
typedef enum {
    PROFILE_PHASE_INPUT = 0,
    PROFILE_PHASE_GAME_LOGIC,
    PROFILE_PHASE_RENDERING,
    PROFILE_PHASE_CULLING,
    PROFILE_PHASE_TERRAIN,
    PROFILE_PHASE_OBJECTS,
    PROFILE_PHASE_SKELETONS,
    PROFILE_PHASE_UI,
    PROFILE_PHASE_SWAP_BUFFERS,
    PROFILE_PHASE_ASYNC_YIELD,
    NUM_PROFILE_PHASES
} ProfilePhaseType;

// Struct to hold profiling data for a single phase
typedef struct {
    uint64_t start_tick;      // Start time of the current measurement
    uint64_t total_ticks;     // Accumulated ticks for this phase
    uint32_t samples;         // Number of samples taken
    const char* name;         // Name of the phase
} ProfilePhase;

// Global array of profiling phases
extern ProfilePhase gProfilePhases[NUM_PROFILE_PHASES];

// Initialize all profiling phases
void InitProfiling(void);

// Start timing a specific phase
void StartProfilePhase(ProfilePhaseType phase_type);

// End timing a specific phase
void EndProfilePhase(ProfilePhaseType phase_type);

// Get the average millisecond cost of a phase
float GetProfilePhaseAvgMs(ProfilePhaseType phase_type);

// Call this at the end of each frame to reset accumulated totals for average calculation
void ResetProfilingForFrame(void);

#endif // PROFILING_H

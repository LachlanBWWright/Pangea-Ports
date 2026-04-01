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

// Struct to hold profiling data for a single phase
typedef struct {
    uint64_t start_tick;          // Start time of the current measurement
    uint64_t total_ticks;         // Accumulated ticks for this phase (current frame)
    uint32_t samples;             // Number of samples taken (current frame)
    float    last_frame_ms;       // Snapshot of the previous completed frame's average ms
    const char* name;             // Name of the phase
} ProfilePhase;

// Global array of profiling phases
extern ProfilePhase gProfilePhases[NUM_PROFILE_PHASES];

// Initialize all profiling phases
void InitProfiling(void);

// Start timing a specific phase
void StartProfilePhase(ProfilePhaseType phase_type);

// End timing a specific phase
void EndProfilePhase(ProfilePhaseType phase_type);

// Get the average millisecond cost of a phase over the CURRENT partial frame.
// NOTE: phases not yet completed in this frame return the previous frame's value.
float GetProfilePhaseAvgMs(ProfilePhaseType phase_type);

// Call this at the end of each frame to snapshot and reset accumulated totals.
// After this call, GetProfilePhaseAvgMs() returns the PREVIOUS frame's values
// for phases not yet measured in the new frame.
void ResetProfilingForFrame(void);

#endif // PROFILING_H

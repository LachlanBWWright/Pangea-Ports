#include "../Headers/profiling.h"
#include <SDL3/SDL.h>

ProfilePhase gProfilePhases[NUM_PROFILE_PHASES];
static uint64_t gPerformanceFrequency;
static int gCurrentPhase = -1;

void InitProfiling(void) {
    gPerformanceFrequency = SDL_GetPerformanceFrequency();
    for (int i = 0; i < NUM_PROFILE_PHASES; ++i) {
        gProfilePhases[i].start_tick    = 0;
        gProfilePhases[i].total_ticks   = 0;
        gProfilePhases[i].samples       = 0;
        gProfilePhases[i].last_frame_ms = 0.0f;
    }
    gProfilePhases[PROFILE_PHASE_INPUT].name        = "Input";
    gProfilePhases[PROFILE_PHASE_GAME_LOGIC].name   = "Game Logic";
    gProfilePhases[PROFILE_PHASE_RENDERING].name    = "Rendering";
    gProfilePhases[PROFILE_PHASE_UI].name           = "UI";
    gProfilePhases[PROFILE_PHASE_SWAP_BUFFERS].name = "Swap Buffers";

    gCurrentPhase = -1;
}

void StartProfilePhase(ProfilePhaseType phase_type) {
    if (gCurrentPhase >= 0 && gCurrentPhase < NUM_PROFILE_PHASES) {
        EndProfilePhase((ProfilePhaseType)gCurrentPhase);
    }

    if (phase_type >= 0 && phase_type < NUM_PROFILE_PHASES) {
        gProfilePhases[phase_type].start_tick = SDL_GetPerformanceCounter();
        gCurrentPhase = (int)phase_type;
    }
}

void EndProfilePhase(ProfilePhaseType phase_type) {
    if (phase_type >= 0 && phase_type < NUM_PROFILE_PHASES) {
        uint64_t end_tick = SDL_GetPerformanceCounter();
        if (gProfilePhases[phase_type].start_tick != 0) {
            gProfilePhases[phase_type].total_ticks +=
                (end_tick - gProfilePhases[phase_type].start_tick);
            gProfilePhases[phase_type].samples++;
            gProfilePhases[phase_type].start_tick = 0;
        }

        if (gCurrentPhase == (int)phase_type) {
            gCurrentPhase = -1;
        }
    }
}

// Returns the current frame's average ms for a phase if it has samples,
// or the previous frame's snapshot if the phase hasn't run yet this frame.
float GetProfilePhaseAvgMs(ProfilePhaseType phase_type) {
    if (phase_type < 0 || phase_type >= NUM_PROFILE_PHASES) return 0.0f;

    ProfilePhase* p = &gProfilePhases[phase_type];

    if (p->samples > 0 && gPerformanceFrequency > 0) {
        double total_ms = ((double)p->total_ticks * 1000.0) / (double)gPerformanceFrequency;
        return (float)(total_ms / (double)p->samples);
    }

    // Phase not yet measured this frame — return previous frame's snapshot
    return p->last_frame_ms;
}

void ResetProfilingForFrame(void) {
    // Auto-end any phase still open at end of frame
    if (gCurrentPhase >= 0 && gCurrentPhase < NUM_PROFILE_PHASES) {
        EndProfilePhase((ProfilePhaseType)gCurrentPhase);
    }

    for (int i = 0; i < NUM_PROFILE_PHASES; ++i) {
        // Snapshot the completed frame's value before resetting
        if (gProfilePhases[i].samples > 0 && gPerformanceFrequency > 0) {
            double total_ms = ((double)gProfilePhases[i].total_ticks * 1000.0)
                              / (double)gPerformanceFrequency;
            gProfilePhases[i].last_frame_ms =
                (float)(total_ms / (double)gProfilePhases[i].samples);
        }
        gProfilePhases[i].total_ticks = 0;
        gProfilePhases[i].samples     = 0;
        gProfilePhases[i].start_tick  = 0;
    }
}

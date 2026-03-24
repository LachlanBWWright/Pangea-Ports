#include "../Headers/profiling.h"
#include <SDL3/SDL.h> // For SDL_GetPerformanceCounter and SDL_GetPerformanceFrequency

ProfilePhase gProfilePhases[NUM_PROFILE_PHASES];
static uint64_t gPerformanceFrequency;

void InitProfiling(void) {
    gPerformanceFrequency = SDL_GetPerformanceFrequency();
    for (int i = 0; i < NUM_PROFILE_PHASES; ++i) {
        gProfilePhases[i].start_tick = 0;
        gProfilePhases[i].total_ticks = 0;
        gProfilePhases[i].samples = 0;
    }
    gProfilePhases[PROFILE_PHASE_INPUT].name = "Input";
    gProfilePhases[PROFILE_PHASE_GAME_LOGIC].name = "Game Logic";
    gProfilePhases[PROFILE_PHASE_RENDERING].name = "Rendering";
    gProfilePhases[PROFILE_PHASE_SWAP_BUFFERS].name = "Swap Buffers";
}

void StartProfilePhase(ProfilePhaseType phase_type) {
    if (phase_type >= 0 && phase_type < NUM_PROFILE_PHASES) {
        gProfilePhases[phase_type].start_tick = SDL_GetPerformanceCounter();
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
    }
}

float GetProfilePhaseAvgMs(ProfilePhaseType phase_type) {
    if (phase_type >= 0 && phase_type < NUM_PROFILE_PHASES && gProfilePhases[phase_type].samples > 0) {
        double avg_ticks = (double)gProfilePhases[phase_type].total_ticks / gProfilePhases[phase_type].samples;
        return (float)((avg_ticks / gPerformanceFrequency) * 1000.0);
    }
    return 0.0f;
}

void ResetProfilingForFrame(void) {
    for (int i = 0; i < NUM_PROFILE_PHASES; ++i) {
        gProfilePhases[i].total_ticks = 0;
        gProfilePhases[i].samples = 0;
    }
}

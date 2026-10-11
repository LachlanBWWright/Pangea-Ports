#ifndef PANGEA_TOUCH_CONTROLS_DRAW_H
#define PANGEA_TOUCH_CONTROLS_DRAW_H
#include <SDL3/SDL.h>
#ifdef __ANDROID__
void TouchControls_DrawGL(SDL_Window* window);
void TouchControls_DrawSDL(SDL_Renderer* renderer);
#else
static inline void TouchControls_DrawGL(SDL_Window* window) { (void)window; }
static inline void TouchControls_DrawSDL(SDL_Renderer* renderer) { (void)renderer; }
#endif
#endif

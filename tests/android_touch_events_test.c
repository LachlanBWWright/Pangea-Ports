#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define PANGEA_TOUCH_IMPLEMENTATION
#include "TouchControls.h"

/* Stub only SDL window access: pointer capture and input logic are the real implementation. */
SDL_WindowID SDL_GetWindowID(SDL_Window* window) { (void)window; return 1; }
static SDL_Rect safeArea = {20, 10, 760, 450};
static SDL_MouseButtonFlags mouseButtons = SDL_BUTTON_LMASK;
static int relativeDrains;
bool SDL_GetWindowSize(SDL_Window* window, int* w, int* h)
{ (void)window; *w = 800; *h = 480; return true; }
bool SDL_GetWindowSafeArea(SDL_Window* window, SDL_Rect* rect)
{ (void)window; *rect = safeArea; return true; }
#undef SDL_memset
void* SDL_memset(void* dst, int value, size_t size) { return memset(dst, value, size); }
int SDL_memcmp(const void* a, const void* b, size_t size) { return memcmp(a, b, size); }
SDL_MouseButtonFlags SDL_GetMouseState(float* x, float* y)
{ if (x) *x = 400; if (y) *y = 240; return mouseButtons; }
SDL_MouseButtonFlags SDL_GetRelativeMouseState(float* x, float* y)
{ if (x) *x = 0; if (y) *y = 0; relativeDrains++; return 0; }

static bool finger(Uint32 type, SDL_FingerID id, float x, float y)
{
    SDL_Event event = {0};
    event.type = type;
    event.tfinger.windowID = 1;
    event.tfinger.touchID = 1;
    event.tfinger.fingerID = id;
    event.tfinger.x = x / 800;
    event.tfinger.y = y / 480;
    return TouchControls_HandleEvent(&event);
}

int main(void)
{
    TouchControlButton buttons[] = {{"Jump", 10}, {"Attack", 11}};
    TouchControlProfile profile = {0, 1, 2, 3, 4, 5, 6, buttons, 2, 20, 21, 22, 23};
    TouchControls_BeginFrame((SDL_Window*)1, &profile);
    const TouchLayout* layout = TouchControls_GetLayout();
    float bx = layout->actions[0].x, by = layout->actions[0].y;
    assert(finger(SDL_EVENT_FINGER_DOWN, 1, bx, by));
    assert(finger(SDL_EVENT_FINGER_DOWN, 2, bx, by));
    assert(TouchControls_NeedPressed(10));
    assert(finger(SDL_EVENT_FINGER_UP, 1, bx, by));
    assert(TouchControls_NeedPressed(10));
    assert(finger(SDL_EVENT_FINGER_MOTION, 2, 400, 240));
    assert(!TouchControls_NeedPressed(10));
    assert(finger(SDL_EVENT_FINGER_MOTION, 2, bx, by));
    assert(TouchControls_NeedPressed(10));
    assert(finger(SDL_EVENT_FINGER_CANCELED, 2, bx, by));
    assert(!TouchControls_NeedPressed(10));

    float sx = layout->stick.x, sy = layout->stick.y;
    assert(finger(SDL_EVENT_FINGER_DOWN, 3, sx, sy));
    assert(TouchControls_StickActive());
    assert(finger(SDL_EVENT_FINGER_MOTION, 3, sx + 0.001f, sy));
    assert(TouchControls_GetStickX() > 0);
    assert(TouchControls_NeedAnalog(3) == TouchControls_GetStickX());
    assert(TouchControls_NeedAnalog(23) == TouchControls_GetStickX());
    assert(finger(SDL_EVENT_FINGER_MOTION, 3, 2000, 2000));
    assert(fabsf(hypotf(TouchControls_GetStickX(), TouchControls_GetStickY()) - 1) < 0.00001f);
    float travel = layout->stick.radius * 0.62f;
    assert(fabsf((TouchControls_GetKnobX() - sx) / travel - TouchControls_GetStickX()) < 0.00001f);
    assert(fabsf((TouchControls_GetKnobY() - sy) / travel - TouchControls_GetStickY()) < 0.00001f);
    assert(finger(SDL_EVENT_FINGER_UP, 3, 2000, 2000));
    assert(!TouchControls_StickActive() && TouchControls_GetStickX() == 0);

    TouchControls_Reset();
    assert(!finger(SDL_EVENT_FINGER_DOWN, 4, 400, 240));
    SDL_Event mouse = {0};
    mouse.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    mouse.button.which = SDL_TOUCH_MOUSEID;
    mouse.button.button = SDL_BUTTON_LEFT;
    assert(!TouchControls_HandleEvent(&mouse));
    float mx, my;
    assert(TouchControls_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK);
    assert(mx == 400 && my == 240);
    assert(!finger(SDL_EVENT_FINGER_UP, 4, 400, 240));
    assert(finger(SDL_EVENT_FINGER_DOWN, 5, bx, by));
    assert(TouchControls_HandleEvent(&mouse));
    assert(!(TouchControls_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK));
    mouse.button.which = 0;
    TouchControls_HandleEvent(&mouse);
    assert(TouchControls_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK);
    mouse.type = SDL_EVENT_MOUSE_BUTTON_UP;
    TouchControls_HandleEvent(&mouse);
    assert(!(TouchControls_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK));
    SDL_Event lost = {0}; lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    TouchControls_HandleEvent(&lost);
    assert(!TouchControls_NeedPressed(10) && !TouchControls_StickActive());
    assert(!(TouchControls_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK));
    assert(finger(SDL_EVENT_FINGER_DOWN, 6, bx, by));
    safeArea.x += 10; safeArea.w -= 10;
    TouchControls_BeginFrame((SDL_Window*)1, &profile);
    assert(!TouchControls_NeedPressed(10));
    assert(!(TouchControls_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK));
    /* Racing profiles map GAS to forward; steering must preserve that held action. */
    TouchControlButton racingButtons[] = {{"GAS", 0}};
    TouchControlProfile racing = profile;
    racing.buttons = racingButtons; racing.buttonCount = 1;
    TouchControls_BeginFrame((SDL_Window*)1, &racing);
    layout = TouchControls_GetLayout();
    assert(finger(SDL_EVENT_FINGER_DOWN, 7, layout->actions[0].x, layout->actions[0].y));
    assert(finger(SDL_EVENT_FINGER_DOWN, 8, layout->stick.x + 10, layout->stick.y));
    assert(TouchControls_GetStickX() > 0 && TouchControls_GetStickY() == 0);
    assert(TouchControls_NeedAnalog(0) == 1);
    assert(TouchControls_NeedPressed(0));
    int oldDrains = relativeDrains;
    TouchControls_BeginFrame((SDL_Window*)1, &racing);
    assert(relativeDrains == oldDrains + 1);
    /* A secondary ordinary finger still aims while the primary holds GAS. */
    assert(!finger(SDL_EVENT_FINGER_DOWN, 9, 400, 240));
    assert(!finger(SDL_EVENT_FINGER_MOTION, 9, 420, 220));
    assert(!(TouchControls_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK));
    assert(fabsf(mx - 420) < .001f && fabsf(my - 220) < .001f);
    assert(TouchControls_NeedPressed(0) && TouchControls_StickActive());
    assert(!finger(SDL_EVENT_FINGER_UP, 9, 420, 220));
    puts("Android touch event capture and lifecycle passed");
}

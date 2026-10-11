#ifndef PANGEA_TOUCH_CONTROLS_H
#define PANGEA_TOUCH_CONTROLS_H
#include <SDL3/SDL.h>
#include "TouchLayout.h"
#include "TouchStick.h"

typedef struct TouchControlButton { const char* label; int need; } TouchControlButton;
typedef struct TouchControlProfile
{
    int upNeed, downNeed, leftNeed, rightNeed;
    int confirmNeed, backNeed, pauseNeed;
    const TouchControlButton* buttons;
    int buttonCount;
    int uiUpNeed, uiDownNeed, uiLeftNeed, uiRightNeed;
} TouchControlProfile;

void TouchControls_BeginFrame(SDL_Window*, const TouchControlProfile*);
bool TouchControls_HandleEvent(const SDL_Event*);
bool TouchControls_NeedPressed(int);
float TouchControls_NeedAnalog(int);
bool TouchControls_StickActive(void);
float TouchControls_GetStickX(void);
float TouchControls_GetStickY(void);
float TouchControls_GetKnobX(void);
float TouchControls_GetKnobY(void);
void TouchControls_Reset(void);
const TouchLayout* TouchControls_GetLayout(void);
const TouchControlProfile* TouchControls_GetProfile(void);
bool TouchControls_ButtonHeld(int);
SDL_MouseButtonFlags TouchControls_GetMouseState(float*, float*);

#ifdef PANGEA_TOUCH_IMPLEMENTATION
typedef struct TouchPointer
{
    SDL_TouchID touch;
    SDL_FingerID finger;
    bool active;
    bool captured;
    int control; /* -1: ordinary screen touch; -2: captured stick */
} TouchPointer;
static TouchPointer tcPointers[32];
static TouchLayout tcLayout;
static PangeaTouchStick tcStick;
static const TouchControlProfile* tcProfile;
static SDL_WindowID tcWindow;
static bool tcEnabled, tcSuppressMouse;
static int tcPrimary = -1;
static SDL_MouseButtonFlags tcPhysicalButtons;
static float tcMouseX, tcMouseY;

void TouchControls_Reset(void)
{
    SDL_memset(tcPointers, 0, sizeof tcPointers);
    tcPrimary = -1;
    /* A canceled control finger may still be present in SDL's polled mouse
       state. Keep its synthetic press suppressed until mouse-up or a new
       primary touch establishes fresh ownership. */
    tcStick = PangeaTouchStick_Reset(tcLayout.stick.x, tcLayout.stick.y);
}
void TouchControls_BeginFrame(SDL_Window* window, const TouchControlProfile* profile)
{
#ifdef __ANDROID__
    tcEnabled = window && profile;
#else
    tcEnabled = false;
#endif
    if (profile != tcProfile || (window && SDL_GetWindowID(window) != tcWindow))
        TouchControls_Reset();
    tcProfile = profile;
    tcWindow = window ? SDL_GetWindowID(window) : 0;
    if (!tcEnabled) return;
    /* Control dragging must not accumulate camera deltas for release. */
    if (tcSuppressMouse) SDL_GetRelativeMouseState(NULL, NULL);
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    SDL_Rect safe = {0, 0, width, height};
    SDL_GetWindowSafeArea(window, &safe);
    TouchLayout nextLayout;
    TouchLayout_ComputeWithInsets(&nextLayout, width, height, profile->buttonCount,
        safe.x, safe.y, width-safe.x-safe.w, height-safe.y-safe.h);
    if (SDL_memcmp(&tcLayout, &nextLayout, sizeof tcLayout) != 0)
        TouchControls_Reset();
    tcLayout = nextLayout;
    if (!TouchControls_StickActive())
        tcStick = PangeaTouchStick_Reset(tcLayout.stick.x, tcLayout.stick.y);
}
static int tcHit(float x, float y)
{
    for (int i = 0; i < tcLayout.actionCount; i++)
        if (TouchLayout_Contains(&tcLayout.actions[i], x, y)) return i;
    const int needs[3] = {tcProfile->confirmNeed, tcProfile->backNeed, tcProfile->pauseNeed};
    for (int i = 0; i < 3; i++)
        if (needs[i] >= 0 && TouchLayout_Contains(&tcLayout.utility[i], x, y)) return 8+i;
    return -1;
}
bool TouchControls_StickActive(void)
{
    for (int i = 0; i < 32; i++)
        if (tcPointers[i].active && tcPointers[i].control == -2) return true;
    return false;
}
bool TouchControls_ButtonHeld(int button)
{
    if (!tcEnabled) return false;
    for (int i = 0; i < 32; i++)
        if (tcPointers[i].active && tcPointers[i].control == button) return true;
    return false;
}
bool TouchControls_HandleEvent(const SDL_Event* event)
{
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        if (event->button.which != SDL_TOUCH_MOUSEID)
        {
            SDL_MouseButtonFlags mask = SDL_BUTTON_MASK(event->button.button);
            if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) tcPhysicalButtons |= mask;
            else tcPhysicalButtons &= ~mask;
            tcMouseX = event->button.x; tcMouseY = event->button.y;
        }
        else if (!tcSuppressMouse)
        { tcMouseX = event->button.x; tcMouseY = event->button.y; }
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION
        && (event->motion.which != SDL_TOUCH_MOUSEID || !tcSuppressMouse))
    { tcMouseX = event->motion.x; tcMouseY = event->motion.y; }
    if (!tcEnabled) return false;
    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST || event->type == SDL_EVENT_WINDOW_HIDDEN
        || event->type == SDL_EVENT_WILL_ENTER_BACKGROUND || event->type == SDL_EVENT_DID_ENTER_BACKGROUND)
    { TouchControls_Reset(); tcPhysicalButtons = 0; return false; }
    if ((event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP)
        && event->button.which == SDL_TOUCH_MOUSEID)
    {
        bool consumed = tcSuppressMouse;
        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) tcSuppressMouse = false;
        return consumed;
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION && event->motion.which == SDL_TOUCH_MOUSEID)
        return tcSuppressMouse;
    if (event->type != SDL_EVENT_FINGER_DOWN && event->type != SDL_EVENT_FINGER_MOTION
        && event->type != SDL_EVENT_FINGER_UP && event->type != SDL_EVENT_FINGER_CANCELED) return false;
    const SDL_TouchFingerEvent* finger = &event->tfinger;
    if (finger->windowID && finger->windowID != tcWindow) return false;
    int slot = -1;
    for (int i = 0; i < 32; i++)
        if (tcPointers[i].active && tcPointers[i].touch == finger->touchID
            && tcPointers[i].finger == finger->fingerID) { slot = i; break; }
    const float x = finger->x * tcLayout.width, y = finger->y * tcLayout.height;
    if (event->type == SDL_EVENT_FINGER_DOWN)
    {
        if (slot >= 0) return tcPointers[slot].captured;
        for (int i = 0; i < 32; i++) if (!tcPointers[i].active) { slot = i; break; }
        if (slot < 0) return false;
        int control = tcHit(x, y);
        if (!TouchControls_StickActive() && TouchLayout_Contains(&tcLayout.stick, x, y)) control = -2;
        tcPointers[slot] = (TouchPointer){finger->touchID, finger->fingerID, true, control != -1, control};
        if (tcPrimary < 0) { tcPrimary = slot; tcSuppressMouse = control != -1; }
    }
    if (slot < 0) return false;
    TouchPointer* pointer = &tcPointers[slot];
    bool consumed = pointer->captured;
    /* SDL synthesizes mouse events from one finger only. Keep aiming from
       ordinary secondary fingers available while a control owns that mouse. */
    if (!consumed && event->type != SDL_EVENT_FINGER_UP
        && event->type != SDL_EVENT_FINGER_CANCELED)
    { tcMouseX = x; tcMouseY = y; }
    if (event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED)
    {
        if (pointer->control == -2) tcStick = PangeaTouchStick_Reset(tcLayout.stick.x, tcLayout.stick.y);
        pointer->active = false;
        if (tcPrimary == slot) tcPrimary = -1;
        return consumed;
    }
    if (pointer->control == -2)
        tcStick = PangeaTouchStick_Update(tcLayout.stick.x, tcLayout.stick.y,
            tcLayout.stick.radius, tcLayout.stick.radius * 0.38f, x, y);
    else if (consumed)
        pointer->control = tcHit(x, y);
    return consumed;
}
float TouchControls_NeedAnalog(int need)
{
    if (!tcEnabled || !tcProfile || need < 0) return 0;
    float value = 0;
    if (need == tcProfile->leftNeed || need == tcProfile->uiLeftNeed) value = fmaxf(value, -tcStick.x);
    if (need == tcProfile->rightNeed || need == tcProfile->uiRightNeed) value = fmaxf(value, tcStick.x);
    if (need == tcProfile->upNeed || need == tcProfile->uiUpNeed) value = fmaxf(value, -tcStick.y);
    if (need == tcProfile->downNeed || need == tcProfile->uiDownNeed) value = fmaxf(value, tcStick.y);
    /* Racing pedals and other action buttons can share a movement need.
       Steering capture must not hide an independently held pedal. */
    for (int i = 0; i < tcLayout.actionCount; i++)
        if (tcProfile->buttons[i].need == need && TouchControls_ButtonHeld(i)) return 1.0f;
    const int needs[3] = {tcProfile->confirmNeed, tcProfile->backNeed, tcProfile->pauseNeed};
    for (int i = 0; i < 3; i++)
        if (needs[i] == need && TouchControls_ButtonHeld(8+i)) return 1.0f;
    return value;
}
bool TouchControls_NeedPressed(int need)
{
    if (!tcEnabled || !tcProfile || need < 0) return false;
    for (int i = 0; i < tcLayout.actionCount; i++)
        if (tcProfile->buttons[i].need == need && TouchControls_ButtonHeld(i)) return true;
    const int needs[3] = {tcProfile->confirmNeed, tcProfile->backNeed, tcProfile->pauseNeed};
    for (int i = 0; i < 3; i++) if (needs[i] == need && TouchControls_ButtonHeld(8+i)) return true;
    return TouchControls_NeedAnalog(need) > 0.0f;
}
float TouchControls_GetStickX(void) { return tcEnabled ? tcStick.x : 0; }
float TouchControls_GetStickY(void) { return tcEnabled ? tcStick.y : 0; }
float TouchControls_GetKnobX(void) { return tcStick.knobX; }
float TouchControls_GetKnobY(void) { return tcStick.knobY; }
const TouchLayout* TouchControls_GetLayout(void) { return tcEnabled ? &tcLayout : NULL; }
const TouchControlProfile* TouchControls_GetProfile(void) { return tcEnabled ? tcProfile : NULL; }
SDL_MouseButtonFlags TouchControls_GetMouseState(float* x, float* y)
{
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(x, y);
    if (tcEnabled && tcSuppressMouse)
    {
        if (x) *x = tcMouseX;
        if (y) *y = tcMouseY;
        buttons = (buttons & ~SDL_BUTTON_LMASK) | (tcPhysicalButtons & SDL_BUTTON_LMASK);
    }
    else
    {
        if (x) tcMouseX = *x;
        if (y) tcMouseY = *y;
    }
    return tcEnabled ? buttons | tcPhysicalButtons : buttons;
}
#endif
#endif

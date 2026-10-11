#ifndef PANGEA_TOUCH_LAYOUT_H
#define PANGEA_TOUCH_LAYOUT_H

/* Rendering and hit testing share these circles in viewport pixel coordinates. */
typedef struct TouchControlCircle
{
    float x, y, radius;
} TouchControlCircle;

typedef struct TouchLayout
{
    float width, height;
    TouchControlCircle stick;
    TouchControlCircle actions[8];
    TouchControlCircle utility[3];
    int actionCount;
} TouchLayout;

static inline int TouchLayout_Contains(const TouchControlCircle* circle, float x, float y)
{
    float dx = x - circle->x;
    float dy = y - circle->y;
    return circle->radius > 0.0f && dx * dx + dy * dy <= circle->radius * circle->radius;
}

static inline void TouchLayout_ComputeWithInsets(TouchLayout* layout,
    float width, float height, int actionCount,
    float left, float top, float right, float bottom)
{
    *layout = (TouchLayout){0};
    layout->width = width;
    layout->height = height;
    if (width <= 0.0f || height <= 0.0f)
        return;
    if (left < 0.0f) left = 0.0f;
    if (top < 0.0f) top = 0.0f;
    if (right < 0.0f) right = 0.0f;
    if (bottom < 0.0f) bottom = 0.0f;
    float safeWidth = width - left - right;
    float safeHeight = height - top - bottom;
    if (safeWidth <= 0.0f || safeHeight <= 0.0f)
        return;
    /* Portrait uses a taller two-column action bank so targets remain thumb sized.
       Landscape uses four columns to leave the centre of the game unobstructed. */
    int columns = safeWidth < safeHeight ? 2 : 4;
    float widthUnits = columns == 2 ? 10.5f : 14.0f;
    float heightUnits = columns == 2 ? 14.0f : 10.5f;
    float unit = safeWidth / widthUnits;
    if (safeHeight / heightUnits < unit)
        unit = safeHeight / heightUnits;
    float safeRight = width - right;
    float safeBottom = height - bottom;
    layout->actionCount = actionCount < 0 ? 0 : (actionCount > 8 ? 8 : actionCount);
    layout->stick = (TouchControlCircle){left + 3.0f * unit, safeBottom - 3.0f * unit, 2.0f * unit};
    /* Reserve a distinct footprint even when a game has fewer actions.
       This keeps the primary action stable when context-sensitive actions appear. */
    for (int i = 0; i < layout->actionCount; i++)
    {
        layout->actions[i] = (TouchControlCircle){
            safeRight - (1.8f + 2.0f * (i % columns)) * unit,
            safeBottom - (1.8f + 2.0f * (i / columns)) * unit,
            0.8f * unit};
    }
    for (int i = 0; i < 3; i++)
    {
        layout->utility[i] = (TouchControlCircle){
            left + safeWidth * 0.5f + (i - 1) * 2.0f * unit,
            top + 1.8f * unit,
            0.8f * unit};
    }
}

static inline void TouchLayout_Compute(TouchLayout* layout, float width, float height, int actionCount)
{
    TouchLayout_ComputeWithInsets(layout, width, height, actionCount, 0.0f, 0.0f, 0.0f, 0.0f);
}

#endif

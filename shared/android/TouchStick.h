#ifndef PANGEA_TOUCH_STICK_H
#define PANGEA_TOUCH_STICK_H

#include <math.h>

/* Coordinates and radii must use the same units as the rendered controls. */
typedef struct PangeaTouchStick
{
	float x;
	float y;
	float knobX;
	float knobY;
} PangeaTouchStick;

static inline PangeaTouchStick PangeaTouchStick_Reset(float centerX, float centerY)
{
	PangeaTouchStick stick = {0.0f, 0.0f, centerX, centerY};
	return stick;
}

static inline PangeaTouchStick PangeaTouchStick_Update(
	float centerX, float centerY,
	float baseRadius, float knobRadius,
	float pointerX, float pointerY)
{
	PangeaTouchStick stick = PangeaTouchStick_Reset(centerX, centerY);
	const float travel = baseRadius - knobRadius;
	if (!isfinite(travel) || travel <= 0.0f || knobRadius < 0.0f
		|| !isfinite(pointerX) || !isfinite(pointerY)
		|| !isfinite(centerX) || !isfinite(centerY))
	{
		return stick;
	}

	float dx = pointerX - centerX;
	float dy = pointerY - centerY;
	const float distance = hypotf(dx, dy);
	if (distance > travel)
	{
		const float scale = travel / distance;
		dx *= scale;
		dy *= scale;
	}

	/* No deadzone: every displacement moves both the knob and the input. */
	stick.x = dx / travel;
	stick.y = dy / travel;
	stick.knobX = centerX + dx;
	stick.knobY = centerY + dy;
	return stick;
}

#endif

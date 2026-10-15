#include "ScriptRuntimeState.h"

#include <math.h>

int BillyScript_LogicalPlayerCount(bool areaActive, bool hasBody, bool targetPractice)
{
	return hasBody || (areaActive && targetPractice) ? 1 : 0;
}

float BillyScript_AdvanceClock(float elapsed, float delta)
{
	if (!isfinite(delta) || delta <= 0.0f) return elapsed;
	float next = elapsed + delta;
	return isfinite(next) ? next : elapsed;
}

PangeaScriptInputAction BillyScript_SampleAction(const char* name, bool previous, bool held, bool pressed)
{
	return (PangeaScriptInputAction){name, held, pressed, previous && !held};
}

bool BillyScript_ValidBounds(const PangeaScriptBounds* bounds)
{
	return bounds && isfinite(bounds->min.x) && isfinite(bounds->min.y) && isfinite(bounds->min.z)
		&& isfinite(bounds->max.x) && isfinite(bounds->max.y) && isfinite(bounds->max.z)
		&& bounds->min.x <= bounds->max.x && bounds->min.y <= bounds->max.y && bounds->min.z <= bounds->max.z;
}

bool BillyScript_TranslateBounds(PangeaScriptBounds* bounds, const PangeaScriptVector3* delta)
{
	if (!BillyScript_ValidBounds(bounds) || !delta) return false;
	PangeaScriptBounds moved = {
		{bounds->min.x + delta->x, bounds->min.y + delta->y, bounds->min.z + delta->z},
		{bounds->max.x + delta->x, bounds->max.y + delta->y, bounds->max.z + delta->z},
	};
	if (!BillyScript_ValidBounds(&moved)) return false;
	*bounds = moved;
	return true;
}

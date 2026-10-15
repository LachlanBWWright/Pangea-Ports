#include "ScriptRuntimeState.h"

#include <assert.h>
#include <float.h>
#include <math.h>

int main(void)
{
	assert(BillyScript_LogicalPlayerCount(true, false, true) == 1);
	assert(BillyScript_LogicalPlayerCount(false, false, true) == 0);
	assert(BillyScript_LogicalPlayerCount(true, false, false) == 0);
	assert(BillyScript_LogicalPlayerCount(true, true, false) == 1);
	float elapsed = 0.0f;
	for (int i = 0; i < 120; i++) elapsed = BillyScript_AdvanceClock(elapsed, 1.0f / 60.0f);
	assert(fabsf(elapsed - 2.0f) < 0.0001f);
	assert(BillyScript_AdvanceClock(elapsed, 0.0f) == elapsed);
	assert(BillyScript_AdvanceClock(elapsed, -1.0f) == elapsed);
	assert(BillyScript_AdvanceClock(elapsed, NAN) == elapsed);
	assert(BillyScript_AdvanceClock(elapsed, INFINITY) == elapsed);
	assert(BillyScript_AdvanceClock(FLT_MAX, FLT_MAX) == FLT_MAX);

	PangeaScriptInputAction edge = BillyScript_SampleAction("fire", false, true, true);
	assert(edge.held && edge.pressed && !edge.released);
	edge = BillyScript_SampleAction("fire", edge.held, true, false);
	assert(edge.held && !edge.pressed && !edge.released);
	edge = BillyScript_SampleAction("fire", edge.held, false, false);
	assert(!edge.held && !edge.pressed && edge.released);
	edge = BillyScript_SampleAction("fire", edge.held, false, false);
	assert(!edge.held && !edge.pressed && !edge.released);
	PangeaScriptBounds bounds = {{-1, -2, -3}, {1, 2, 3}};
	PangeaScriptVector3 delta = {10, 20, -30};
	assert(BillyScript_TranslateBounds(&bounds, &delta));
	assert(bounds.min.x == 9 && bounds.max.x == 11);
	assert(bounds.min.y == 18 && bounds.max.y == 22);
	assert(bounds.min.z == -33 && bounds.max.z == -27);
	delta.x = INFINITY;
	assert(!BillyScript_TranslateBounds(&bounds, &delta));
	assert(bounds.min.x == 9 && bounds.max.x == 11);
	bounds.min.x = bounds.max.x + 1;
	assert(!BillyScript_ValidBounds(&bounds));
	return 0;
}

#include "pangea_script.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

static bool finite_vector(const PangeaScriptVector3* vector)
{
	return vector && isfinite(vector->x) && isfinite(vector->y) && isfinite(vector->z);
}

static bool valid_bounds(const PangeaScriptBounds* bounds)
{
	return bounds && finite_vector(&bounds->min) && finite_vector(&bounds->max) &&
		bounds->min.x <= bounds->max.x && bounds->min.y <= bounds->max.y && bounds->min.z <= bounds->max.z;
}

static bool matching_handle(PangeaScriptObjectHandle a, PangeaScriptObjectHandle b)
{
	return a.id == b.id && a.generation == b.generation;
}

static bool intersect_bounds(const PangeaScriptVector3* origin, const PangeaScriptVector3* delta,
	const PangeaScriptBounds* bounds, bool sweep, float* fraction, PangeaScriptVector3* normal)
{
	const float start[] = {origin->x, origin->y, origin->z};
	const float movement[] = {delta->x, delta->y, delta->z};
	const float minimum[] = {bounds->min.x, bounds->min.y, bounds->min.z};
	const float maximum[] = {bounds->max.x, bounds->max.y, bounds->max.z};
	double entry = -INFINITY, exit = INFINITY;
	int entryAxis = -1, dominantAxis = -1;
	float entrySign = 0, dominantMovement = 0;
	for (int axis = 0; axis < 3; axis++)
	{
		if (fabsf(movement[axis]) > dominantMovement)
		{
			dominantAxis = axis; dominantMovement = fabsf(movement[axis]);
		}
		if (movement[axis] == 0)
		{
			if (start[axis] < minimum[axis] || start[axis] > maximum[axis]) return false;
			// Tangential box movement must slide; a zero-depth axis still permits 2D collision queries.
			if (sweep && maximum[axis] > minimum[axis] && (start[axis] == minimum[axis] || start[axis] == maximum[axis])) return false;
			continue;
		}
		double first = ((double)minimum[axis]-start[axis])/movement[axis];
		double last = ((double)maximum[axis]-start[axis])/movement[axis];
		float sign = -1;
		if (first > last) { double swapped = first; first = last; last = swapped; sign = 1; }
		if (first > entry) { entry = first; entryAxis = axis; entrySign = sign; }
		if (last < exit) exit = last;
		if (entry > exit) return false;
	}
	if (dominantAxis < 0 || entry > 1 || exit < 0 || (sweep && exit <= 0 && entry < 0)) return false;
	*normal = (PangeaScriptVector3){0};
	float components[3] = {0};
	if (entry < 0)
	{
		*fraction = 0;
		components[dominantAxis] = movement[dominantAxis] > 0 ? -1 : 1;
	}
	else
	{
		*fraction = (float)entry;
		components[entryAxis] = entrySign;
	}
	normal->x = components[0]; normal->y = components[1]; normal->z = components[2];
	return true;
}

static bool query_geometry(const PangeaScriptBounds* broadphase, const PangeaScriptVector3* origin,
	const PangeaScriptVector3* delta, const PangeaScriptVector3* halfSize, const char* category,
	PangeaScriptObjectHandle ignore, PangeaScriptCollisionHit* output)
{
	PangeaScriptObjectHandle objects[64];
	int count = 0;
	if (!valid_bounds(broadphase) || !PangeaScript_OverlapBox(broadphase, category, objects, 64, &count) || count >= 64) return false;
	*output = (PangeaScriptCollisionHit){0};
	float nearest = FLT_MAX;
	for (int i = 0; i < count; i++)
	{
		if (matching_handle(objects[i], ignore)) continue;
		PangeaScriptBounds bounds;
		if (!PangeaScript_GetObjectBounds(objects[i], &bounds) || !valid_bounds(&bounds)) return false;
		if (halfSize)
		{
			bounds.min.x -= halfSize->x; bounds.max.x += halfSize->x;
			bounds.min.y -= halfSize->y; bounds.max.y += halfSize->y;
			bounds.min.z -= halfSize->z; bounds.max.z += halfSize->z;
			if (!valid_bounds(&bounds)) return false;
		}
		float fraction;
		PangeaScriptVector3 normal;
		if (!intersect_bounds(origin,delta,&bounds,halfSize != NULL,&fraction,&normal) || fraction >= nearest) continue;
		nearest = fraction;
		*output = (PangeaScriptCollisionHit){.object = objects[i], .fraction = fraction, .normal = normal,
			.position = {origin->x+delta->x*fraction, origin->y+delta->y*fraction, origin->z+delta->z*fraction}};
		if (!finite_vector(&output->position)) return false;
	}
	return true;
}

bool PangeaScript_Raycast(const PangeaScriptVector3* from, const PangeaScriptVector3* to,
	const char* category, PangeaScriptObjectHandle ignore, PangeaScriptCollisionHit* output)
{
	if (!finite_vector(from) || !finite_vector(to) || !category || !output) return false;
	*output = (PangeaScriptCollisionHit){0};
	PangeaScriptVector3 delta = {to->x-from->x, to->y-from->y, to->z-from->z};
	if (!finite_vector(&delta)) return false;
	PangeaScriptBounds bounds = {.min = {fminf(from->x,to->x),fminf(from->y,to->y),fminf(from->z,to->z)},
		.max = {fmaxf(from->x,to->x),fmaxf(from->y,to->y),fmaxf(from->z,to->z)}};
	return query_geometry(&bounds,from,&delta,NULL,category,ignore,output);
}

bool PangeaScript_SweepBox(const PangeaScriptBounds* bounds, const PangeaScriptVector3* delta,
	const char* category, PangeaScriptObjectHandle ignore, PangeaScriptCollisionHit* output)
{
	if (!valid_bounds(bounds) || !finite_vector(delta) || !category || !output) return false;
	*output = (PangeaScriptCollisionHit){0};
	PangeaScriptVector3 center = {bounds->min.x*.5f+bounds->max.x*.5f,bounds->min.y*.5f+bounds->max.y*.5f,bounds->min.z*.5f+bounds->max.z*.5f};
	PangeaScriptVector3 half = {bounds->max.x*.5f-bounds->min.x*.5f,bounds->max.y*.5f-bounds->min.y*.5f,bounds->max.z*.5f-bounds->min.z*.5f};
	PangeaScriptBounds broadphase = {.min = {bounds->min.x+fminf(0,delta->x),bounds->min.y+fminf(0,delta->y),bounds->min.z+fminf(0,delta->z)},
		.max = {bounds->max.x+fmaxf(0,delta->x),bounds->max.y+fmaxf(0,delta->y),bounds->max.z+fmaxf(0,delta->z)}};
	return query_geometry(&broadphase,&center,delta,&half,category,ignore,output);
}

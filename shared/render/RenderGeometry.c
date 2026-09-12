#include "RenderGeometry.h"

static void InvalidateIfPresent(const void *pointer, RenderGeometryInvalidateFunction invalidate)
{
	if (pointer)
		invalidate(pointer);
}

void RenderGeometry_InvalidateDynamicStreams(
	const void *points,
	const void *normals,
	const void *colorsFloat,
	const void *colorsByte,
	const void *triangles,
	RenderGeometryInvalidateFunction invalidate)
{
	InvalidateIfPresent(points, invalidate);
	InvalidateIfPresent(normals, invalidate);
	InvalidateIfPresent(colorsFloat, invalidate);
	InvalidateIfPresent(colorsByte, invalidate);
	InvalidateIfPresent(triangles, invalidate);
}

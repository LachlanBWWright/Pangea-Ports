#ifndef PANGEA_RENDER_GEOMETRY_H
#define PANGEA_RENDER_GEOMETRY_H

#include <stddef.h>

#define RENDER_MAX_TEXTURE_LAYERS 4

typedef void (*RenderGeometryInvalidateFunction)(const void *pointer);

void RenderGeometry_InvalidateDynamicStreams(
	const void *points,
	const void *normals,
	const void *colorsFloat,
	const void *colorsByte,
	const void *triangles,
	RenderGeometryInvalidateFunction invalidate);

#endif

#ifndef PANGEA_RENDER_POLICY_H
#define PANGEA_RENDER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum RenderMaterialAlphaMode
{
	RENDER_ALPHA_OPAQUE = 0,
	RENDER_ALPHA_CUTOUT,
	RENDER_ALPHA_BLEND,
} RenderMaterialAlphaMode;

RenderMaterialAlphaMode RenderPolicy_ResolveAlphaMode(
	bool textureHasAlpha,
	float diffuseAlpha,
	uint32_t materialFlags,
	uint32_t clipAlphaFlag,
	uint32_t alwaysBlendFlag);

bool RenderPolicy_ShouldBlend(RenderMaterialAlphaMode alphaMode);

bool RenderPolicy_IsMeshTransparent(
	bool textureIsBlended,
	float meshAlpha,
	float modifierAlpha,
	float autoFadeFactor,
	bool glow);

#endif

#include "RenderPolicy.h"

RenderMaterialAlphaMode RenderPolicy_ResolveAlphaMode(
	bool textureHasAlpha,
	float diffuseAlpha,
	uint32_t materialFlags,
	uint32_t clipAlphaFlag,
	uint32_t alwaysBlendFlag)
{
	if (materialFlags & clipAlphaFlag)
		return RENDER_ALPHA_CUTOUT;

	if (textureHasAlpha || diffuseAlpha != 1.0f || (materialFlags & alwaysBlendFlag))
		return RENDER_ALPHA_BLEND;

	return RENDER_ALPHA_OPAQUE;
}

bool RenderPolicy_ShouldBlend(RenderMaterialAlphaMode alphaMode)
{
	return alphaMode == RENDER_ALPHA_BLEND;
}

bool RenderPolicy_IsMeshTransparent(
	bool textureIsBlended,
	float meshAlpha,
	float modifierAlpha,
	float autoFadeFactor,
	bool glow)
{
	return textureIsBlended
		|| meshAlpha < 0.999f
		|| modifierAlpha < 0.999f
		|| autoFadeFactor < 0.999f
		|| glow;
}

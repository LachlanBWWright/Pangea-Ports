#pragma once

#include "equates.h"
#include "structures.h"
#include "pangea_script.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static inline bool MikeScript_AnimationValid(const void* animationList, int animation)
{
	int16_t count;
	if (!animationList || animation < 0) return false;
	memcpy(&count, (const char*)animationList - sizeof(count), sizeof(count));
	return animation < count;
}

static inline bool MikeScript_ToFixed(float value, int32_t* output)
{
	double scaled = round((double)value * 65536.0);
	if (!output || !isfinite(scaled) || scaled < INT32_MIN || scaled > INT32_MAX)
		return false;
	*output = (int32_t)scaled;
	return true;
}

static inline bool MikeScript_CoordinateValid(float value)
{
	return isfinite(value) && value >= INT16_MIN && value <= INT16_MAX;
}

static inline bool MikeScript_GetObjectBounds(void* nativeObject, PangeaScriptBounds* output)
{
	const ObjNode* object = nativeObject;
	if (!object || !output || object->CType == INVALID_NODE_FLAG || !object->CType
		|| object->LeftSide >= object->RightSide || object->TopSide >= object->BottomSide)
		return false;
	*output = (PangeaScriptBounds){
		.min = {(float)object->LeftSide, (float)object->TopSide, 0},
		.max = {(float)object->RightSide, (float)object->BottomSide, 0},
	};
	return true;
}

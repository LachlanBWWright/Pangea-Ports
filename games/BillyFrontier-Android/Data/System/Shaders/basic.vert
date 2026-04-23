precision highp float;
precision mediump int;

attribute vec3 aPosition;
attribute vec3 aNormal;
attribute vec4 aColor;
attribute vec2 aTexCoord0;
attribute vec2 aTexCoord1;

uniform mat4 uMVPMatrix;
uniform mat4 uModelViewMatrix;
uniform mediump mat3 uNormalMatrix;

uniform vec3 uAmbientLight;
uniform vec3 uLightDirection[4];
uniform vec3 uLightColor[4];
uniform int uNumLights;

uniform int uFogEnabled;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFogDensity;
uniform int uFogMode;

uniform vec4 uMaterialColor;
uniform int uUseLighting;
uniform int uUseVertexColor;

uniform int uUseTexture0;
uniform int uUseTexture1;
uniform mat4 uTextureMatrix;

varying vec4 vColor;
varying vec2 vTexCoord0;
varying vec2 vTexCoord1;
varying float vFogFactor;
varying vec3 vNormal;

void main()
{
	gl_Position = uMVPMatrix * vec4(aPosition, 1.0);

	vec4 viewPos = uModelViewMatrix * vec4(aPosition, 1.0);
	float fogCoord = length(viewPos.xyz);

	if (uFogEnabled != 0)
	{
		if (uFogMode == 0)
		{
			float fogRange = uFogEnd - uFogStart;
			if (abs(fogRange) > 0.001)
				vFogFactor = (uFogEnd - fogCoord) / fogRange;
			else
				vFogFactor = 1.0;
		}
		else if (uFogMode == 1)
		{
			vFogFactor = exp(-uFogDensity * fogCoord);
		}
		else
		{
			vFogFactor = exp(-pow(uFogDensity * fogCoord, 2.0));
		}
		vFogFactor = clamp(vFogFactor, 0.0, 1.0);
	}
	else
	{
		vFogFactor = 1.0;
	}

	vec3 normal = normalize(uNormalMatrix * aNormal);
	vNormal = normal;

	vec4 finalColor = uMaterialColor;
	if (uUseLighting != 0)
	{
		vec3 ambient = uAmbientLight;
		vec3 diffuse = vec3(0.0);

		if (uNumLights > 0) diffuse += uLightColor[0] * max(dot(normal, uLightDirection[0]), 0.0);
		if (uNumLights > 1) diffuse += uLightColor[1] * max(dot(normal, uLightDirection[1]), 0.0);
		if (uNumLights > 2) diffuse += uLightColor[2] * max(dot(normal, uLightDirection[2]), 0.0);
		if (uNumLights > 3) diffuse += uLightColor[3] * max(dot(normal, uLightDirection[3]), 0.0);
		finalColor.rgb *= (ambient + diffuse);
	}

	if (uUseVertexColor != 0)
		finalColor *= aColor;

	vColor = finalColor;

	if (uUseTexture0 != 0)
	{
		vec4 texCoord = uTextureMatrix * vec4(aTexCoord0, 0.0, 1.0);
		vTexCoord0 = texCoord.xy;
	}
	else
	{
		vTexCoord0 = aTexCoord0;
	}

	vTexCoord1 = aTexCoord1;
}

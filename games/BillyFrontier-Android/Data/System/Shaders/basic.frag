precision mediump float;
precision mediump int;

uniform sampler2D uTexture0;
uniform sampler2D uTexture1;
uniform int uUseTexture0;
uniform int uUseTexture1;
uniform int uMultiTextureMode;
uniform int uMultiTextureCombine;
uniform int uUseSphereMap;
uniform mediump mat3 uNormalMatrix;
uniform int uFogEnabled;
uniform vec3 uFogColor;
uniform int uAlphaTestEnabled;
uniform int uAlphaFunc;
uniform float uAlphaRef;
uniform float uGlobalTransparency;
uniform vec3 uGlobalColorFilter;

varying vec4 vColor;
varying vec2 vTexCoord0;
varying vec2 vTexCoord1;
varying float vFogFactor;
varying vec3 vNormal;

void main()
{
	vec4 color = vColor;

	if (uUseTexture0 != 0)
	{
		vec4 texColor = texture2D(uTexture0, vTexCoord0);
		color *= texColor;
	}

	if (uUseTexture1 != 0)
	{
		vec2 texCoord1 = vTexCoord1;
		if (uUseSphereMap != 0)
		{
			vec3 normal = normalize(vNormal);
			vec3 viewNormal = normalize(uNormalMatrix * normal);
			texCoord1 = viewNormal.xy * 0.5 + 0.5;
		}

		vec4 texColor1 = texture2D(uTexture1, texCoord1);
		if (uMultiTextureCombine == 0)
			color *= texColor1;
		else if (uMultiTextureCombine == 1)
			color.rgb += texColor1.rgb;
	}

	color.rgb *= uGlobalColorFilter;
	color.a *= uGlobalTransparency;

	if (uAlphaTestEnabled != 0)
	{
		bool discard_frag = false;
		if (uAlphaFunc == 0) discard_frag = true;
		else if (uAlphaFunc == 1) discard_frag = color.a >= uAlphaRef;
		else if (uAlphaFunc == 2) discard_frag = abs(color.a - uAlphaRef) > 0.001;
		else if (uAlphaFunc == 3) discard_frag = color.a > uAlphaRef;
		else if (uAlphaFunc == 4) discard_frag = color.a <= uAlphaRef;
		else if (uAlphaFunc == 5) discard_frag = abs(color.a - uAlphaRef) < 0.001;
		else if (uAlphaFunc == 6) discard_frag = color.a < uAlphaRef;
		if (discard_frag) discard;
	}

	if (uFogEnabled != 0)
	{
		color.rgb = mix(uFogColor, color.rgb, vFogFactor);
	}

	gl_FragColor = color;
}

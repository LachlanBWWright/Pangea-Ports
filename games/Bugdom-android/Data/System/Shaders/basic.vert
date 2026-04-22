precision mediump float;
precision mediump int;
attribute vec3 a_Position;
attribute vec3 a_Normal;
attribute vec2 a_TexCoord;
attribute vec4 a_Color;
uniform mat4 u_Projection;
uniform mat4 u_ModelView;
uniform mat3 u_NormalMatrix;
uniform int  u_LightingEnabled;
uniform vec4 u_AmbientColor;
uniform int  u_NumLights;
uniform vec3 u_LightDir0;
uniform vec4 u_LightColor0;
uniform vec3 u_LightDir1;
uniform vec4 u_LightColor1;
uniform int  u_UseVertexColors;
uniform vec4 u_DiffuseColor;
uniform int  u_FogEnabled;
uniform float u_FogStart;
uniform float u_FogEnd;
varying vec2 v_TexCoord;
varying vec4 v_Color;
varying float v_FogFactor;
void main() {
    vec4 eyePos = u_ModelView * vec4(a_Position, 1.0);
    gl_Position = u_Projection * eyePos;
    v_TexCoord = a_TexCoord;
    vec4 baseColor = (u_UseVertexColors != 0) ? a_Color : u_DiffuseColor;
    if (u_LightingEnabled != 0) {
        vec3 eyeNormal = normalize(u_NormalMatrix * a_Normal);
        vec3 lit = u_AmbientColor.rgb;
        if (u_NumLights > 0) { lit += u_LightColor0.rgb * max(dot(eyeNormal, u_LightDir0), 0.0); }
        if (u_NumLights > 1) { lit += u_LightColor1.rgb * max(dot(eyeNormal, u_LightDir1), 0.0); }
        v_Color = vec4(baseColor.rgb * clamp(lit, 0.0, 1.0), baseColor.a);
    } else {
        v_Color = baseColor;
    }
    if (u_FogEnabled != 0) {
        float d = length(eyePos.xyz);
        v_FogFactor = clamp((u_FogEnd - d) / (u_FogEnd - u_FogStart), 0.0, 1.0);
    } else {
        v_FogFactor = 1.0;
    }
}

precision mediump float;
precision mediump int;
uniform sampler2D u_Texture0;
uniform int u_TextureEnabled;
uniform int u_AlphaTestEnabled;
uniform float u_AlphaThreshold;
uniform vec4 u_FogColor;
uniform int u_FogEnabled;
varying vec2 v_TexCoord;
varying vec4 v_Color;
varying float v_FogFactor;
void main() {
    vec4 color;
    if (u_TextureEnabled != 0) {
        color = v_Color * texture2D(u_Texture0, v_TexCoord);
    } else {
        color = v_Color;
    }
    if (u_AlphaTestEnabled != 0 && color.a < u_AlphaThreshold) discard;
    if (u_FogEnabled != 0) { color.rgb = mix(u_FogColor.rgb, color.rgb, v_FogFactor); }
    gl_FragColor = color;
}

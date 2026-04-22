precision mediump float;
precision mediump int;
varying vec4  v_color;
varying vec2  v_tc0;
varying vec2  v_tc1;
varying float v_fog_depth;
// Use int instead of bool for the same GLSL ES 1.0 compatibility reason
uniform int       u_texture0;
uniform int       u_texture1;
uniform sampler2D u_sampler0;
uniform sampler2D u_sampler1;
uniform int       u_texenv0;   // 0=MODULATE 1=ADD 2=REPLACE 3=COMBINE_ADD
uniform int       u_texenv1;
uniform int       u_fog;
uniform int       u_fog_mode;  // 0=LINEAR 1=EXP 2=EXP2
uniform float     u_fog_start;
uniform float     u_fog_end;
uniform float     u_fog_density;
uniform vec4      u_fog_color;
uniform int       u_alpha_test;
uniform int       u_alpha_func; // 0=NEVER 1=LESS 2=EQUAL 3=LEQUAL 4=GREATER 5=NOTEQUAL 6=GEQUAL 7=ALWAYS
uniform float     u_alpha_ref;
void main() {
  vec4 color = v_color;
  if (u_texture0 != 0) {
    vec4 tex = texture2D(u_sampler0, v_tc0);
    if      (u_texenv0 == 0) color *= tex;
    else if (u_texenv0 == 1) { color.rgb = min(color.rgb+tex.rgb,1.0); color.a *= tex.a; }
    else if (u_texenv0 == 2) color = tex;
    else if (u_texenv0 == 3) { color.rgb = min(color.rgb+tex.rgb,1.0); }
  }
  if (u_texture1 != 0) {
    vec4 tex = texture2D(u_sampler1, v_tc1);
    if      (u_texenv1 == 0) color *= tex;
    else if (u_texenv1 == 1) { color.rgb = min(color.rgb+tex.rgb,1.0); color.a *= tex.a; }
    else if (u_texenv1 == 2) color = tex;
    else if (u_texenv1 == 3) { color.rgb = min(color.rgb+tex.rgb,1.0); }
  }
  if (u_alpha_test != 0) {
    float a = color.a;
    if      (u_alpha_func == 0) discard;
    else if (u_alpha_func == 1 && a >= u_alpha_ref) discard;
    else if (u_alpha_func == 2 && a != u_alpha_ref) discard;
    else if (u_alpha_func == 3 && a >  u_alpha_ref) discard;
    else if (u_alpha_func == 4 && a <= u_alpha_ref) discard;
    else if (u_alpha_func == 5 && a == u_alpha_ref) discard;
    else if (u_alpha_func == 6 && a <  u_alpha_ref) discard;
  }
  if (u_fog != 0) {
    float ff;
    if      (u_fog_mode == 0) ff = (u_fog_end - v_fog_depth) / (u_fog_end - u_fog_start);
    else if (u_fog_mode == 1) ff = exp(-u_fog_density * v_fog_depth);
    else { float d = u_fog_density * v_fog_depth; ff = exp(-d*d); }
    ff = clamp(ff, 0.0, 1.0);
    color.rgb = mix(u_fog_color.rgb, color.rgb, ff);
  }
  gl_FragColor = color;
}

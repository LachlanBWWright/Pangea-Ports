precision mediump float;
precision mediump int;
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec4 a_color;
attribute vec2 a_texcoord0;
attribute vec2 a_texcoord1;
uniform mat4 u_mv;
uniform mat4 u_proj;
uniform mat3 u_normal_mat;
uniform mat4 u_tex_matrix;
uniform mat4 u_tex_matrix2;
uniform vec4 u_current_color;
// Use int instead of bool: bool uniforms can be unreliable in GLSL ES 1.0
uniform int  u_use_color_array;
uniform int  u_lighting;
uniform vec4 u_ambient;
uniform int  u_num_lights;
uniform vec4 u_light_pos[4];
uniform vec4 u_light_diff[4];
uniform vec4 u_light_amb[4];
uniform int  u_fog;
uniform int  u_texgen;
varying vec4 v_color;
varying vec2 v_tc0;
varying vec2 v_tc1;
varying float v_fog_depth;
// Helper: compute contribution from one light
vec3 light_contrib(int li, vec3 n, vec4 ep) {
  vec3 ld = (u_light_pos[li].w == 0.0)
           ? normalize(vec3(u_light_pos[li]))
           : normalize(vec3(u_light_pos[li]) - vec3(ep));
  float d = max(dot(n, ld), 0.0);
  return u_light_amb[li].rgb + d * u_light_diff[li].rgb;
}
void main() {
  vec4 eye_pos = u_mv * vec4(a_position, 1.0);
  gl_Position  = u_proj * eye_pos;
  vec4 vc = (u_use_color_array != 0) ? a_color : u_current_color;
  if (u_lighting != 0) {
    vec3 n = normalize(u_normal_mat * a_normal);
    vec4 color = u_ambient;
    // Unrolled 4-light loop — avoids break+non-constant-bound (invalid GLSL ES 1.0)
    if (u_num_lights > 0) color.rgb += light_contrib(0, n, eye_pos);
    if (u_num_lights > 1) color.rgb += light_contrib(1, n, eye_pos);
    if (u_num_lights > 2) color.rgb += light_contrib(2, n, eye_pos);
    if (u_num_lights > 3) color.rgb += light_contrib(3, n, eye_pos);
    v_color = clamp(color, 0.0, 1.0) * vc;
  } else {
    v_color = vc;
  }
  // Sphere-map texcoords from eye-space normal
  if (u_texgen != 0) {
    vec3 r = reflect(normalize(vec3(eye_pos)), normalize(u_normal_mat * a_normal));
    float m = 2.0 * sqrt(r.x*r.x + r.y*r.y + (r.z+1.0)*(r.z+1.0));
    v_tc1 = vec2(r.x/m + 0.5, r.y/m + 0.5);
    v_tc0 = (u_tex_matrix * vec4(a_texcoord0, 0.0, 1.0)).xy;
  } else {
    v_tc0 = (u_tex_matrix  * vec4(a_texcoord0, 0.0, 1.0)).xy;
    v_tc1 = (u_tex_matrix2 * vec4(a_texcoord1, 0.0, 1.0)).xy;
  }
  v_fog_depth = (u_fog != 0) ? abs(eye_pos.z) : 0.0;
}

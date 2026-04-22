#version 300 es
uniform mat4 u_proj;
uniform mat4 u_mv;
uniform mat4 u_texMatrix;
uniform bool u_lighting;
uniform vec4 u_ambient;
uniform int  u_nLights;
uniform vec3 u_lightDir[4];
uniform vec4 u_lightDiff[4];
uniform bool u_fog;
uniform int  u_fogMode;
uniform float u_fogDensity;
uniform float u_fogStart;
uniform float u_fogEnd;
layout(location=0) in vec3 a_pos;
layout(location=1) in vec3 a_norm;
layout(location=2) in vec4 a_color;
layout(location=3) in vec2 a_uv;
out vec4 v_color;
out vec2 v_uv;
out float v_fog;
void main(){
  vec4 ep = u_mv * vec4(a_pos,1.0);
  gl_Position = u_proj * ep;
  vec4 base = a_color;
  if(u_lighting){
    vec3 n = normalize(mat3(u_mv) * a_norm);
    vec4 lit = u_ambient * base;
    for(int i=0;i<4;i++){
      if(i<u_nLights){
        float d=max(dot(n,normalize(u_lightDir[i])),0.0);
        lit+=vec4(d*u_lightDiff[i].rgb*base.rgb,0.0);
      }
    }
    v_color=vec4(clamp(lit.rgb,0.0,1.0),base.a);
  } else {
    v_color=base;
  }
  v_uv=(u_texMatrix*vec4(a_uv,0.0,1.0)).xy;
  if(u_fog){
    float dist=abs(ep.z);
    if(u_fogMode==1){
      v_fog=clamp((u_fogEnd-dist)/max(u_fogEnd-u_fogStart,0.0001),0.0,1.0);
    } else if(u_fogMode==2){
      v_fog=exp(-u_fogDensity*dist);
    } else {
      float fd=u_fogDensity*dist;
      v_fog=exp(-fd*fd);
    }
  } else {
    v_fog=1.0;
  }
}

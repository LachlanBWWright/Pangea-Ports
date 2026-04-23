#version 300 es
precision mediump float;
uniform sampler2D u_tex;
uniform bool u_useTex;
uniform vec4 u_fogColor;
uniform bool u_alphaTest;
uniform int u_alphaFunc;
uniform float u_alphaRef;
in vec4 v_color;
in vec2 v_uv;
in float v_fog;
out vec4 fragColor;
void main(){
  vec4 c;
  if(u_useTex){
    c=v_color*texture(u_tex,v_uv);
  } else {
    c=v_color;
  }
  if(u_alphaTest){
    float a = c.a;
    if      (u_alphaFunc == 0) discard; // NEVER
    else if (u_alphaFunc == 1 && a >= u_alphaRef) discard; // LESS
    else if (u_alphaFunc == 2 && a != u_alphaRef) discard; // EQUAL
    else if (u_alphaFunc == 3 && a >  u_alphaRef) discard; // LEQUAL
    else if (u_alphaFunc == 4 && a <= u_alphaRef) discard; // GREATER
    else if (u_alphaFunc == 5 && a == u_alphaRef) discard; // NOTEQUAL
    else if (u_alphaFunc == 6 && a <  u_alphaRef) discard; // GEQUAL
    else if (u_alphaFunc == 7) {} // ALWAYS
  } else {
    if(c.a==0.0) discard;
  }
  c.rgb=mix(u_fogColor.rgb,c.rgb,v_fog);
  fragColor=c;
}

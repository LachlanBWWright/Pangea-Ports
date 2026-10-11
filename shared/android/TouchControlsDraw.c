#include "TouchControlsDraw.h"
#include "TouchControls.h"
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <math.h>
#include <string.h>

typedef struct Vertex { float x,y,r,g,b,a; } Vertex;
static Vertex vertices[24000];
static int count;
static void triangle(float x1,float y1,float x2,float y2,float x3,float y3,float r,float g,float b,float a)
{
    if (count+3 > (int)(sizeof vertices/sizeof vertices[0])) return;
    vertices[count++]=(Vertex){x1,y1,r,g,b,a};
    vertices[count++]=(Vertex){x2,y2,r,g,b,a};
    vertices[count++]=(Vertex){x3,y3,r,g,b,a};
}
static void rect(float x,float y,float w,float h)
{
    triangle(x,y,x+w,y,x,y+h,1,1,1,.95f);
    triangle(x+w,y,x+w,y+h,x,y+h,1,1,1,.95f);
}
static void circle(float x,float y,float radius,bool held)
{
    const float fill=held ? .25f : .05f;
    const float alpha=held ? .8f : .52f;
    for(int i=0;i<48;i++)
    {
        float a=i*6.2831853f/48,b=(i+1)*6.2831853f/48;
        float x1=x+cosf(a)*radius,y1=y+sinf(a)*radius;
        float x2=x+cosf(b)*radius,y2=y+sinf(b)*radius;
        triangle(x,y,x1,y1,x2,y2,fill,held?.48f:.08f,held?.7f:.12f,alpha);
        float inner=radius-fmaxf(1.0f,radius*.045f);
        float ix1=x+cosf(a)*inner,iy1=y+sinf(a)*inner;
        float ix2=x+cosf(b)*inner,iy2=y+sinf(b)*inner;
        triangle(x1,y1,x2,y2,ix1,iy1,.85f,.92f,1,.85f);
        triangle(x2,y2,ix2,iy2,ix1,iy1,.85f,.92f,1,.85f);
    }
}
/* Compact 5x7 glyphs keep action labels readable without any game font resource. */
static const unsigned char glyphs[26][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
static void label(const TouchControlCircle* c,const char* text)
{
    if(!text || !*text) return;
    int n=(int)strlen(text);
    float s=fminf(c->radius*.18f,c->radius*1.55f/(n*6.0f));
    float x=c->x-(n*6-1)*s*.5f,y=c->y-7*s*.5f;
    for(int i=0;i<n;i++)
    {
        unsigned char ch=(unsigned char)text[i]; if(ch>='a'&&ch<='z') ch-=32;
        if(ch>='A'&&ch<='Z')
            for(int row=0;row<7;row++) for(int col=0;col<5;col++)
                if(glyphs[ch-'A'][row]&(1<<(4-col))) rect(x+(i*6+col)*s,y+row*s,s,s);
    }
}
static const TouchLayout* geometry(void)
{
    const TouchLayout* l=TouchControls_GetLayout();
    if(!l || l->width<=0 || l->height<=0) return NULL;
    count=0;
    circle(l->stick.x,l->stick.y,l->stick.radius,false);
    circle(TouchControls_GetKnobX(),TouchControls_GetKnobY(),l->stick.radius*.38f,TouchControls_StickActive());
    const TouchControlProfile* p=TouchControls_GetProfile();
    for(int i=0;i<l->actionCount;i++)
    {circle(l->actions[i].x,l->actions[i].y,l->actions[i].radius,TouchControls_ButtonHeld(i));label(&l->actions[i],p->buttons[i].label);}
    static const char* utility[]={"OK","BACK","PAUSE"};
    const int needs[]={p->confirmNeed,p->backNeed,p->pauseNeed};
    for(int i=0;i<3;i++)
    {if(needs[i]<0)continue;circle(l->utility[i].x,l->utility[i].y,l->utility[i].radius,TouchControls_ButtonHeld(8+i));label(&l->utility[i],utility[i]);}
    return l;
}
void TouchControls_DrawGL(SDL_Window* window)
{
    (void)window;
    const TouchLayout* l=geometry(); if(!l) return;
    /* Resources belong to the current GL context; recreate after context replacement. */
    static SDL_GLContext context;
    static GLuint program,vao,vbo;
    SDL_GLContext current=SDL_GL_GetCurrentContext();
    if(!current) return;
    if(context!=current){context=current;program=vao=vbo=0;}
    GLint prevProgram,prevVAO,prevBuffer,viewport[4],srcRGB,dstRGB,srcA,dstA,eqRGB,eqA;
    GLboolean blend=glIsEnabled(GL_BLEND),depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST),stencil=glIsEnabled(GL_STENCIL_TEST),mask[4];
    glGetIntegerv(GL_CURRENT_PROGRAM,&prevProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&prevVAO);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&prevBuffer);glGetIntegerv(GL_VIEWPORT,viewport);
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB);glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcA);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstA);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRGB);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqA);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    if(!program)
    {
        const char* vs="#version 300 es\nlayout(location=0) in vec2 pos;layout(location=1) in vec4 col;uniform vec2 size;out vec4 color;void main(){gl_Position=vec4(pos.x/size.x*2.-1.,1.-pos.y/size.y*2.,0.,1.);color=col;}";
        const char* fs="#version 300 es\nprecision mediump float;in vec4 color;out vec4 outColor;void main(){outColor=color;}";
        GLuint v=glCreateShader(GL_VERTEX_SHADER),f=glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(v,1,&vs,NULL);glCompileShader(v);glShaderSource(f,1,&fs,NULL);glCompileShader(f);
        program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);
        GLint linked=GL_FALSE;
        glGetProgramiv(program,GL_LINK_STATUS,&linked);
        if(!linked)
        {
            char log[512];glGetProgramInfoLog(program,sizeof log,NULL,log);
            SDL_Log("Touch overlay shader failed: %s",log);
            glDeleteProgram(program);program=0;
            return; /* No render state has been changed before successful link. */
        }
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
    }
    int width,height;SDL_GetWindowSizeInPixels(window,&width,&height);glViewport(0,0,width,height);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glEnable(GL_BLEND);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glUseProgram(program);glUniform2f(glGetUniformLocation(program,"size"),l->width,l->height);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,count*sizeof(Vertex),vertices,GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(2*sizeof(float)));glDrawArrays(GL_TRIANGLES,0,count);
    glBindVertexArray(prevVAO);glBindBuffer(GL_ARRAY_BUFFER,prevBuffer);glUseProgram(prevProgram);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glBlendFuncSeparate(srcRGB,dstRGB,srcA,dstA);glBlendEquationSeparate(eqRGB,eqA);glColorMask(mask[0],mask[1],mask[2],mask[3]);
    if(!blend)glDisable(GL_BLEND);
    if(depth)glEnable(GL_DEPTH_TEST);
    if(cull)glEnable(GL_CULL_FACE);
    if(scissor)glEnable(GL_SCISSOR_TEST);
    if(stencil)glEnable(GL_STENCIL_TEST);
}
void TouchControls_DrawSDL(SDL_Renderer* renderer)
{
    const TouchLayout* l=geometry();if(!l)return;
    int logicalW,logicalH;SDL_RendererLogicalPresentation mode;
    SDL_GetRenderLogicalPresentation(renderer,&logicalW,&logicalH,&mode);
    SDL_Rect viewport,clip;float scaleX,scaleY;bool clipped=SDL_RenderClipEnabled(renderer);SDL_BlendMode blend;
    SDL_GetRenderViewport(renderer,&viewport);SDL_GetRenderClipRect(renderer,&clip);SDL_GetRenderScale(renderer,&scaleX,&scaleY);SDL_GetRenderDrawBlendMode(renderer,&blend);
    SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED);SDL_SetRenderViewport(renderer,NULL);SDL_SetRenderClipRect(renderer,NULL);SDL_SetRenderScale(renderer,1,1);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    int w,h;SDL_GetRenderOutputSize(renderer,&w,&h);
    static SDL_Vertex converted[24000];
    for(int i=0;i<count;i++)converted[i]=(SDL_Vertex){{vertices[i].x*w/l->width,vertices[i].y*h/l->height},{vertices[i].r,vertices[i].g,vertices[i].b,vertices[i].a},{0,0}};
    SDL_RenderGeometry(renderer,NULL,converted,count,NULL,0);
    SDL_SetRenderLogicalPresentation(renderer,logicalW,logicalH,mode);SDL_SetRenderViewport(renderer,&viewport);SDL_SetRenderClipRect(renderer,clipped?&clip:NULL);SDL_SetRenderScale(renderer,scaleX,scaleY);SDL_SetRenderDrawBlendMode(renderer,blend);
}
#endif

#include <assert.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include <GLES3/gl3.h>
#include "TouchControls.h"
#include "TouchControlsDraw.h"
static TouchLayout layout;
static TouchControlButton buttons[]={{"JUMP",1},{"FIRE",2}};
static TouchControlProfile profile={.buttons=buttons,.buttonCount=2,.confirmNeed=3,.backNeed=4,.pauseNeed=5};
const TouchLayout* TouchControls_GetLayout(void){return &layout;}
const TouchControlProfile* TouchControls_GetProfile(void){return &profile;}
bool TouchControls_ButtonHeld(int i){return i==0;}
bool TouchControls_StickActive(void){return true;}
float TouchControls_GetKnobX(void){return layout.stick.x+layout.stick.radius*.4f;}
float TouchControls_GetKnobY(void){return layout.stick.y;}
int main(int argc,char**argv)
{
 (void)argv;assert(SDL_Init(SDL_INIT_VIDEO));TouchLayout_Compute(&layout,960,540,2);
 if(argc>1)
 {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
  SDL_Window*w=SDL_CreateWindow("test",960,540,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!w){fprintf(stderr,"%s\n",SDL_GetError());return 2;}
  SDL_GLContext c=SDL_GL_CreateContext(w);if(!c){fprintf(stderr,"%s\n",SDL_GetError());return 2;}
  glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glViewport(3,4,100,200);glEnable(GL_DEPTH_TEST);glEnable(GL_SCISSOR_TEST);glColorMask(1,0,1,0);glEnable(GL_CULL_FACE);glEnable(GL_STENCIL_TEST);glDisable(GL_BLEND);glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_ZERO,GL_ONE);glBlendEquationSeparate(GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT);GLuint originalVAO,originalBuffer;glGenVertexArrays(1,&originalVAO);glGenBuffers(1,&originalBuffer);glBindVertexArray(originalVAO);glBindBuffer(GL_ARRAY_BUFFER,originalBuffer);
  TouchControls_DrawGL(w);GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);assert(viewport[0]==3&&viewport[1]==4&&viewport[2]==100&&viewport[3]==200);assert(glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_SCISSOR_TEST));GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);assert(mask[0]&&!mask[1]&&mask[2]&&!mask[3]);
  assert(glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_STENCIL_TEST)&&!glIsEnabled(GL_BLEND));GLint actualState;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&actualState);assert((GLuint)actualState==originalVAO);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&actualState);assert((GLuint)actualState==originalBuffer);glGetIntegerv(GL_BLEND_SRC_RGB,&actualState);assert(actualState==GL_ONE);glGetIntegerv(GL_BLEND_DST_RGB,&actualState);assert(actualState==GL_ZERO);glGetIntegerv(GL_BLEND_SRC_ALPHA,&actualState);assert(actualState==GL_ZERO);glGetIntegerv(GL_BLEND_DST_ALPHA,&actualState);assert(actualState==GL_ONE);glGetIntegerv(GL_BLEND_EQUATION_RGB,&actualState);assert(actualState==GL_FUNC_SUBTRACT);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&actualState);assert(actualState==GL_FUNC_REVERSE_SUBTRACT);
  unsigned char pixel[4];glReadPixels((int)layout.actions[0].x,(int)(540-layout.actions[0].y+layout.actions[0].radius*.5f),1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[2]>20);assert(glGetError()==GL_NO_ERROR);
  glDeleteBuffers(1,&originalBuffer);glDeleteVertexArrays(1,&originalVAO);SDL_GL_DestroyContext(c);SDL_DestroyWindow(w);puts("GLES3 draw and state restore passed");
 }
 else
 {
  SDL_Surface*s=SDL_CreateSurface(960,540,SDL_PIXELFORMAT_RGBA32);assert(s);SDL_Renderer*r=SDL_CreateSoftwareRenderer(s);assert(r);
  SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);SDL_SetRenderLogicalPresentation(r,640,480,SDL_LOGICAL_PRESENTATION_LETTERBOX);SDL_Rect vp={3,4,100,200},clip={2,2,20,20};SDL_SetRenderViewport(r,&vp);SDL_SetRenderClipRect(r,&clip);SDL_SetRenderScale(r,2,3);SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_ADD);
  TouchControls_DrawSDL(r);SDL_FlushRenderer(r);
  int w,h;SDL_RendererLogicalPresentation mode;SDL_GetRenderLogicalPresentation(r,&w,&h,&mode);assert(w==640&&h==480&&mode==SDL_LOGICAL_PRESENTATION_LETTERBOX);float sx,sy;SDL_GetRenderScale(r,&sx,&sy);assert(sx==2&&sy==3);SDL_Rect actual;SDL_GetRenderViewport(r,&actual);assert(actual.x==3&&actual.y==4&&actual.w==100&&actual.h==200);assert(SDL_RenderClipEnabled(r));SDL_GetRenderClipRect(r,&actual);assert(actual.x==2&&actual.y==2&&actual.w==20&&actual.h==20);SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(r,&blend);assert(blend==SDL_BLENDMODE_ADD);
  unsigned char* pixel=(unsigned char*)s->pixels+(int)(layout.actions[0].y+layout.actions[0].radius*.5f)*s->pitch+(int)layout.actions[0].x*4;assert(pixel[2]>20);
  SDL_DestroyRenderer(r);SDL_DestroySurface(s);puts("SDL software draw and state restore passed");
 }
 SDL_Quit();return 0;
}

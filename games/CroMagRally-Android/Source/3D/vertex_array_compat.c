//
// vertex_array_compat.c
// Client-side vertex array compatibility implementation for WebGL
//
// Performance-critical path: the game issues ~200+ draw calls per frame.
// Each call must upload vertex data from CPU arrays to GPU buffers.
//
// Optimization strategy (non-interleaved direct upload):
//   Instead of copying client arrays into intermediate buffers and then
//   interleaving them into a single VBO (6 data passes per draw call),
//   we upload each attribute array directly to its own VBO (1 pass each).
//   For float arrays with stride==0, this is a single glBufferData with
//   the client pointer — zero CPU-side copying.  For GL_UNSIGNED_BYTE
//   colors, WebGL's normalized attribute support (GL_TRUE) lets the GPU
//   do the 0–255 → 0.0–1.0 conversion, also eliminating CPU work.
//

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)

#include "game.h"
#include <string.h>
#include <stdlib.h>
#include <GLES2/gl2.h>

// Render statistics counters (defined in render_stats_stub.c for this game)
extern int gDrawCallsThisFrame;
extern int gVerticesThisFrame;
extern int gBufferUploadsThisFrame;

// #undef the macros so we can call the real GL functions for drawing.
// Our CompatGL_* implementations set up VBOs and call the real functions
// directly, bypassing the recursive macro redirect.
#undef glDrawElements
#undef glDrawArrays

VertexArrayState gVertexArrayState;
static int gCurrentClientTexture = 0; // 0 or 1 for GL_TEXTURE0 or GL_TEXTURE1

// ── Persistent GPU buffers (DrawArrays / immediate path) ────────────────
// One VBO per vertex attribute (non-interleaved layout).
// Created on first use, never deleted — avoids per-draw-call
// glGenBuffers/glDeleteBuffers which stall the GPU pipeline.
static GLuint sAttrVBO[5] = {0};  // pos, norm, color, tc0, tc1
static GLuint sIndexIBO = 0;

// Bitmask tracking which vertex attribute arrays are currently enabled
// on the GL side.  We only toggle when the set changes between draws.
static uint8_t sEnabledAttribMask = 0x1F;  // all 5 enabled by ModernGL_Init

// Persistent scratch buffer for ushort→uint index conversion
static GLuint* sIdxConvertBuf = NULL;
static int sIdxConvertBufCap = 0;

// ── Static-geometry draw cache ───────────────────────────────────────────
// BG3D model data lives at stable C pointers across frames. A pointer-keyed
// LRU cache lets CompatGL_DrawElements skip all glBufferData calls when the
// same geometry is redrawn (cache hit). Each entry owns its own per-attribute
// VBOs and IBO so it never aliases with other entries.
#define DRAW_CACHE_SIZE 128

typedef struct {
    int       valid;
    uint32_t  last_used;
    const void* vptr;
    const void* nptr;
    const void* cptr;
    const void* tc0ptr;
    const void* tc1ptr;
    const void* idxptr;
    int       vertex_count;
    GLsizei   index_count;
    GLenum    color_type;
    uint8_t   attrib_mask;
    GLuint    attr_vbo[5];
    GLuint    ibo;
} DrawCacheEntry;

static DrawCacheEntry sDC[DRAW_CACHE_SIZE];
static uint32_t sDC_clock = 1;

static DrawCacheEntry* DC_Find(const void* vptr, const void* nptr,
    const void* cptr, const void* tc0ptr, const void* tc1ptr,
    const void* idxptr, int vertexCount, GLsizei indexCount,
    uint8_t attribMask)
{
    for (int i = 0; i < DRAW_CACHE_SIZE; i++) {
        DrawCacheEntry* e = &sDC[i];
        if (!e->valid) continue;
        if (e->vptr   != vptr   || e->nptr    != nptr   || e->cptr  != cptr  ||
            e->tc0ptr != tc0ptr || e->tc1ptr  != tc1ptr || e->idxptr != idxptr ||
            e->vertex_count != vertexCount || e->index_count != indexCount ||
            e->attrib_mask != attribMask)
            continue;
        e->last_used = sDC_clock++;
        return e;
    }
    return NULL;
}

static DrawCacheEntry* DC_Alloc(void)
{
    DrawCacheEntry* e = NULL;
    uint32_t oldest = 0xFFFFFFFFu;
    int oldest_i = 0;
    for (int i = 0; i < DRAW_CACHE_SIZE; i++) {
        if (!sDC[i].valid) { e = &sDC[i]; break; }
        if (sDC[i].last_used < oldest) { oldest = sDC[i].last_used; oldest_i = i; }
    }
    if (!e) e = &sDC[oldest_i];
    if (!e->ibo) {
        glGenBuffers(5, e->attr_vbo);
        glGenBuffers(1, &e->ibo);
    }
    e->valid = 1;
    return e;
}

// ── Attribute enable/disable helpers ────────────────────────────────────

// Ensure exactly the attributes in 'needed' (bitmask) are enabled.
// Disabled attributes get a per-vertex constant via glVertexAttrib*.
static void SyncAttribEnables(uint8_t needed)
{
    uint8_t diff = sEnabledAttribMask ^ needed;
    if (!diff) return;  // fast-path: nothing changed

    for (int i = 0; i < 5; i++)
    {
        if (!(diff & (1u << i))) continue;

        if (needed & (1u << i))
        {
            glEnableVertexAttribArray(i);
        }
        else
        {
            glDisableVertexAttribArray(i);
            // Set per-vertex constant for the disabled attribute
            switch (i)
            {
                case ATTRIB_LOCATION_NORMAL:   glVertexAttrib3f(i, 0.0f, 1.0f, 0.0f); break;
                case ATTRIB_LOCATION_COLOR:    glVertexAttrib4f(i, 1.0f, 1.0f, 1.0f, 1.0f); break;
                case ATTRIB_LOCATION_TEXCOORD0:
                case ATTRIB_LOCATION_TEXCOORD1: glVertexAttrib2f(i, 0.0f, 0.0f); break;
                default: break;
            }
        }
    }
    sEnabledAttribMask = needed;
}

// Restore all 5 attributes to enabled state.  Called after non-interleaved
// draws so that the interleaved path (ModernGL_DrawGeometry, used for
// immediate-mode emulation) still works without its own enable tracking.
static void RestoreAllAttribs(void)
{
    SyncAttribEnables(0x1F);
}

void CompatGL_EnableClientState(GLenum array)
{
    switch (array)
    {
        case GL_VERTEX_ARRAY:
            gVertexArrayState.vertexArrayEnabled = true;
            break;
        case GL_NORMAL_ARRAY:
            gVertexArrayState.normalArrayEnabled = true;
            break;
        case GL_COLOR_ARRAY:
            gVertexArrayState.colorArrayEnabled = true;
            break;
        case GL_TEXTURE_COORD_ARRAY:
            if (gCurrentClientTexture >= 0 && gCurrentClientTexture < 2)
                gVertexArrayState.texCoordArrayEnabled[gCurrentClientTexture] = true;
            break;
    }
    gVertexArrayState.isDirty = true;
}

void CompatGL_DisableClientState(GLenum array)
{
    switch (array)
    {
        case GL_VERTEX_ARRAY:
            gVertexArrayState.vertexArrayEnabled = false;
            break;
        case GL_NORMAL_ARRAY:
            gVertexArrayState.normalArrayEnabled = false;
            break;
        case GL_COLOR_ARRAY:
            gVertexArrayState.colorArrayEnabled = false;
            break;
        case GL_TEXTURE_COORD_ARRAY:
            if (gCurrentClientTexture >= 0 && gCurrentClientTexture < 2)
                gVertexArrayState.texCoordArrayEnabled[gCurrentClientTexture] = false;
            break;
    }
    gVertexArrayState.isDirty = true;
}

void CompatGL_VertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    gVertexArrayState.vertexPointer = pointer;
    gVertexArrayState.vertexSize = size;
    gVertexArrayState.vertexType = type;
    gVertexArrayState.vertexStride = stride;
    gVertexArrayState.isDirty = true;
}

void CompatGL_NormalPointer(GLenum type, GLsizei stride, const void* pointer)
{
    gVertexArrayState.normalPointer = pointer;
    gVertexArrayState.isDirty = true;
}

void CompatGL_ColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    gVertexArrayState.colorPointer = pointer;
    gVertexArrayState.colorSize = size;
    gVertexArrayState.colorType = type;
    gVertexArrayState.colorStride = stride;
    gVertexArrayState.isDirty = true;
}

void CompatGL_TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
    if (gCurrentClientTexture >= 0 && gCurrentClientTexture < 2)
    {
        gVertexArrayState.texCoordPointers[gCurrentClientTexture] = pointer;
        gVertexArrayState.texCoordSize[gCurrentClientTexture] = size;
        gVertexArrayState.texCoordStride[gCurrentClientTexture] = stride;
        gVertexArrayState.isDirty = true;
    }
}

void CompatGL_ClientActiveTexture(GLenum texture)
{
    // Convert GL_TEXTURE0_ARB/GL_TEXTURE1_ARB to index
    if (texture == GL_TEXTURE0_ARB || texture == GL_TEXTURE0)
        gCurrentClientTexture = 0;
    else if (texture == GL_TEXTURE1_ARB || texture == GL_TEXTURE1)
        gCurrentClientTexture = 1;
}

void CompatGL_DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    // Sync vertex color state to shader (only mark dirty on change)
    extern ModernGLState gModernGLState;
    if (gModernGLState.useVertexColor != gVertexArrayState.colorArrayEnabled)
    {
        gModernGLState.useVertexColor = gVertexArrayState.colorArrayEnabled;
        gModernGLState.dirtyFlags |= MODERNGL_DIRTY_MATERIAL;
    }

    // Update shader state before drawing
    extern void CompatGL_UpdateShaderState(void);
    CompatGL_UpdateShaderState();

    if (type != GL_UNSIGNED_INT && type != GL_UNSIGNED_SHORT)
        return; // Unsupported index type

    // ── Find maximum index to determine vertex count ──────────────────
    GLuint maxIdx = 0;
    if (type == GL_UNSIGNED_INT)
    {
        const GLuint* idx = (const GLuint*)indices;
        for (GLsizei i = 0; i < count; i++)
            if (idx[i] > maxIdx) maxIdx = idx[i];
    }
    else
    {
        const GLushort* idx = (const GLushort*)indices;
        for (GLsizei i = 0; i < count; i++)
            if ((GLuint)idx[i] > maxIdx) maxIdx = idx[i];
    }

    int vertexCount = (int)maxIdx + 1;

    Boolean hasPos   = gVertexArrayState.vertexArrayEnabled && gVertexArrayState.vertexPointer;
    Boolean hasNorm  = gVertexArrayState.normalArrayEnabled && gVertexArrayState.normalPointer;
    Boolean hasColor = gVertexArrayState.colorArrayEnabled  && gVertexArrayState.colorPointer;
    Boolean hasTC0   = gVertexArrayState.texCoordArrayEnabled[0] && gVertexArrayState.texCoordPointers[0];
    Boolean hasTC1   = gVertexArrayState.texCoordArrayEnabled[1] && gVertexArrayState.texCoordPointers[1];

    uint8_t attribMask = (1u << ATTRIB_LOCATION_POSITION);
    if (hasNorm)  attribMask |= (1u << ATTRIB_LOCATION_NORMAL);
    if (hasColor) attribMask |= (1u << ATTRIB_LOCATION_COLOR);
    if (hasTC0)   attribMask |= (1u << ATTRIB_LOCATION_TEXCOORD0);
    if (hasTC1)   attribMask |= (1u << ATTRIB_LOCATION_TEXCOORD1);

    SyncAttribEnables(attribMask);

    // ── Draw cache lookup ─────────────────────────────────────────────
    DrawCacheEntry* entry = DC_Find(
        gVertexArrayState.vertexPointer, gVertexArrayState.normalPointer,
        gVertexArrayState.colorPointer, gVertexArrayState.texCoordPointers[0],
        gVertexArrayState.texCoordPointers[1], indices,
        vertexCount, count, attribMask);

    int uploads = 0;

    if (!entry)
    {
        // Cache miss — upload data with GL_STATIC_DRAW and store in cache.
        entry = DC_Alloc();
        entry->vptr   = gVertexArrayState.vertexPointer;
        entry->nptr   = gVertexArrayState.normalPointer;
        entry->cptr   = gVertexArrayState.colorPointer;
        entry->tc0ptr = gVertexArrayState.texCoordPointers[0];
        entry->tc1ptr = gVertexArrayState.texCoordPointers[1];
        entry->idxptr = indices;
        entry->vertex_count = vertexCount;
        entry->index_count  = count;
        entry->color_type   = gVertexArrayState.colorType;
        entry->attrib_mask  = attribMask;
        entry->last_used    = sDC_clock++;

        if (hasPos)
        {
            glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_POSITION]);
            glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * (GLsizeiptr)sizeof(GLfloat),
                         gVertexArrayState.vertexPointer, GL_STATIC_DRAW);
            uploads++;
        }
        if (hasNorm)
        {
            glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_NORMAL]);
            glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * (GLsizeiptr)sizeof(GLfloat),
                         gVertexArrayState.normalPointer, GL_STATIC_DRAW);
            uploads++;
        }
        if (hasColor)
        {
            glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_COLOR]);
            if (gVertexArrayState.colorType == GL_FLOAT)
                glBufferData(GL_ARRAY_BUFFER, vertexCount * 4 * (GLsizeiptr)sizeof(GLfloat),
                             gVertexArrayState.colorPointer, GL_STATIC_DRAW);
            else
                glBufferData(GL_ARRAY_BUFFER, vertexCount * 4 * (GLsizeiptr)sizeof(GLubyte),
                             gVertexArrayState.colorPointer, GL_STATIC_DRAW);
            uploads++;
        }
        if (hasTC0)
        {
            glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_TEXCOORD0]);
            glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * (GLsizeiptr)sizeof(GLfloat),
                         gVertexArrayState.texCoordPointers[0], GL_STATIC_DRAW);
            uploads++;
        }
        if (hasTC1)
        {
            glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_TEXCOORD1]);
            glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * (GLsizeiptr)sizeof(GLfloat),
                         gVertexArrayState.texCoordPointers[1], GL_STATIC_DRAW);
            uploads++;
        }

        // Index buffer: always use GL_UNSIGNED_INT for simplicity
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry->ibo);
        if (type == GL_UNSIGNED_INT)
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * (GLsizeiptr)sizeof(GLuint),
                         indices, GL_STATIC_DRAW);
        }
        else
        {
            if (count > sIdxConvertBufCap)
            {
                int newCap = count > sIdxConvertBufCap * 2 ? count : sIdxConvertBufCap * 2;
                if (newCap < 256) newCap = 256;
                free(sIdxConvertBuf);
                sIdxConvertBuf = (GLuint*)malloc(newCap * sizeof(GLuint));
                sIdxConvertBufCap = newCap;
            }
            const GLushort* src = (const GLushort*)indices;
            for (GLsizei i = 0; i < count; i++)
                sIdxConvertBuf[i] = (GLuint)src[i];
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * (GLsizeiptr)sizeof(GLuint),
                         sIdxConvertBuf, GL_STATIC_DRAW);
        }
        uploads++;
    }

    // Bind attribute VBOs (cache hit: free; cache miss: just re-bound above)
    if (hasPos)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_POSITION]);
        glVertexAttribPointer(ATTRIB_LOCATION_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasNorm)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_NORMAL]);
        glVertexAttribPointer(ATTRIB_LOCATION_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasColor)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_COLOR]);
        if (entry->color_type == GL_FLOAT)
            glVertexAttribPointer(ATTRIB_LOCATION_COLOR, 4, GL_FLOAT, GL_FALSE, 0, 0);
        else
            glVertexAttribPointer(ATTRIB_LOCATION_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);
    }
    if (hasTC0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_TEXCOORD0]);
        glVertexAttribPointer(ATTRIB_LOCATION_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasTC1)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attr_vbo[ATTRIB_LOCATION_TEXCOORD1]);
        glVertexAttribPointer(ATTRIB_LOCATION_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry->ibo);
    glDrawElements(mode, count, GL_UNSIGNED_INT, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Restore all attribs for the interleaved path (immediate mode)
    RestoreAllAttribs();

    // ── Profiling counters ───────────────────────────────────────────
    gDrawCallsThisFrame++;
    gVerticesThisFrame += vertexCount;
    gBufferUploadsThisFrame += uploads;
}

void CompatGL_DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    // Sync vertex color state to shader (only mark dirty on change)
    extern ModernGLState gModernGLState;
    if (gModernGLState.useVertexColor != gVertexArrayState.colorArrayEnabled)
    {
        gModernGLState.useVertexColor = gVertexArrayState.colorArrayEnabled;
        gModernGLState.dirtyFlags |= MODERNGL_DIRTY_MATERIAL;
    }

    // Update shader state before drawing
    extern void CompatGL_UpdateShaderState(void);
    CompatGL_UpdateShaderState();

    // ── Create persistent VBOs on first use ──────────────────────────
    if (!sAttrVBO[0])
    {
        glGenBuffers(5, sAttrVBO);
        glGenBuffers(1, &sIndexIBO);
    }

    // ── Determine which attributes are provided ──────────────────────
    Boolean hasPos   = gVertexArrayState.vertexArrayEnabled && gVertexArrayState.vertexPointer;
    Boolean hasNorm  = gVertexArrayState.normalArrayEnabled && gVertexArrayState.normalPointer;
    Boolean hasColor = gVertexArrayState.colorArrayEnabled  && gVertexArrayState.colorPointer;
    Boolean hasTC0   = gVertexArrayState.texCoordArrayEnabled[0] && gVertexArrayState.texCoordPointers[0];
    Boolean hasTC1   = gVertexArrayState.texCoordArrayEnabled[1] && gVertexArrayState.texCoordPointers[1];

    uint8_t attribMask = (1u << ATTRIB_LOCATION_POSITION);
    if (hasNorm)  attribMask |= (1u << ATTRIB_LOCATION_NORMAL);
    if (hasColor) attribMask |= (1u << ATTRIB_LOCATION_COLOR);
    if (hasTC0)   attribMask |= (1u << ATTRIB_LOCATION_TEXCOORD0);
    if (hasTC1)   attribMask |= (1u << ATTRIB_LOCATION_TEXCOORD1);

    SyncAttribEnables(attribMask);

    int uploads = 0;

    // ── POSITIONS: direct upload with offset ─────────────────────────
    if (hasPos)
    {
        const GLfloat* src = (const GLfloat*)gVertexArrayState.vertexPointer + first * 3;
        glBindBuffer(GL_ARRAY_BUFFER, sAttrVBO[0]);
        glBufferData(GL_ARRAY_BUFFER, count * 3 * (GLsizeiptr)sizeof(GLfloat),
                     src, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(ATTRIB_LOCATION_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0);
        uploads++;
    }

    // ── NORMALS ──────────────────────────────────────────────────────
    if (hasNorm)
    {
        const GLfloat* src = (const GLfloat*)gVertexArrayState.normalPointer + first * 3;
        glBindBuffer(GL_ARRAY_BUFFER, sAttrVBO[1]);
        glBufferData(GL_ARRAY_BUFFER, count * 3 * (GLsizeiptr)sizeof(GLfloat),
                     src, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(ATTRIB_LOCATION_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0);
        uploads++;
    }

    // ── COLORS ───────────────────────────────────────────────────────
    if (hasColor)
    {
        glBindBuffer(GL_ARRAY_BUFFER, sAttrVBO[2]);
        if (gVertexArrayState.colorType == GL_FLOAT)
        {
            const GLfloat* src = (const GLfloat*)gVertexArrayState.colorPointer + first * 4;
            glBufferData(GL_ARRAY_BUFFER, count * 4 * (GLsizeiptr)sizeof(GLfloat),
                         src, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(ATTRIB_LOCATION_COLOR, 4, GL_FLOAT, GL_FALSE, 0, 0);
        }
        else if (gVertexArrayState.colorType == GL_UNSIGNED_BYTE)
        {
            const GLubyte* src = (const GLubyte*)gVertexArrayState.colorPointer + first * 4;
            glBufferData(GL_ARRAY_BUFFER, count * 4 * (GLsizeiptr)sizeof(GLubyte),
                         src, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(ATTRIB_LOCATION_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);
        }
        uploads++;
    }

    // ── TEXCOORD0 ────────────────────────────────────────────────────
    if (hasTC0)
    {
        const GLfloat* src = (const GLfloat*)gVertexArrayState.texCoordPointers[0] + first * 2;
        glBindBuffer(GL_ARRAY_BUFFER, sAttrVBO[3]);
        glBufferData(GL_ARRAY_BUFFER, count * 2 * (GLsizeiptr)sizeof(GLfloat),
                     src, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(ATTRIB_LOCATION_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        uploads++;
    }

    // ── TEXCOORD1 ────────────────────────────────────────────────────
    if (hasTC1)
    {
        const GLfloat* src = (const GLfloat*)gVertexArrayState.texCoordPointers[1] + first * 2;
        glBindBuffer(GL_ARRAY_BUFFER, sAttrVBO[4]);
        glBufferData(GL_ARRAY_BUFFER, count * 2 * (GLsizeiptr)sizeof(GLfloat),
                     src, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(ATTRIB_LOCATION_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, 0, 0);
        uploads++;
    }

    // ── DRAW (no index buffer) ───────────────────────────────────────
    glDrawArrays(mode, 0, count);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Restore all attribs for the interleaved path (immediate mode)
    RestoreAllAttribs();

    // ── Profiling counters ───────────────────────────────────────────
    gDrawCallsThisFrame++;
    gVerticesThisFrame += count;
    gBufferUploadsThisFrame += uploads;
}

#endif // __EMSCRIPTEN__ || __ANDROID__

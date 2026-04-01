//
// vertex_array_compat.c
// Client-side vertex array compatibility implementation for WebGL
//
// Performance-critical path: the game issues ~200+ draw calls per frame.
// Each call must upload vertex data from CPU arrays to GPU buffers.
//
// Optimization strategy: draw call cache with LRU eviction
//   Static geometry (terrain tiles, objects) is drawn with the same client
//   pointers every frame.  We cache the uploaded VBOs keyed by the set of
//   client-array pointers, vertex count, and index pointer.
//   On a cache hit we skip all glBufferData uploads and just bind+draw.
//   128-entry LRU cache covers the typical working set.
//

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)

#include "game.h"
#include <string.h>
#include <stdlib.h>
#include <GLES2/gl2.h>

// #undef the macros so we can call the real GL functions for drawing.
// Our CompatGL_* implementations set up VBOs and call the real functions
// directly, bypassing the recursive macro redirect.
#undef glDrawElements
#undef glDrawArrays

VertexArrayState gVertexArrayState;
static int gCurrentClientTexture = 0; // 0 or 1 for GL_TEXTURE0 or GL_TEXTURE1

// ── Draw Cache ──────────────────────────────────────────────────────────
#define DRAW_CACHE_SIZE 128

typedef struct {
    int valid;
    unsigned int last_used;
    GLuint attrVBO[5];   // pos, norm, color, tc0, tc1
    GLuint indexIBO;
    // Cache keys
    int vertexCount;
    const void* indices;
    GLsizei indexCount;
    GLenum indexType;
    const void* vertexPointer;
    const void* normalPointer;
    const void* colorPointer;
    const void* texCoordPointers[2];
    Boolean vertexArrayEnabled;
    Boolean normalArrayEnabled;
    Boolean colorArrayEnabled;
    Boolean texCoordArrayEnabled[2];
    GLenum colorType;
    GLint drawFirst;     // >=0 for DrawArrays, -1 for DrawElements
    GLsizei drawCount;
} DrawCacheEntry;

static DrawCacheEntry sDrawCache[DRAW_CACHE_SIZE];
static unsigned int sDrawCacheCounter = 0;

// Persistent scratch buffer for ushort→uint index conversion
static GLuint* sIdxConvertBuf = NULL;
static int sIdxConvertBufCap = 0;

// Bitmask tracking which vertex attribute arrays are currently enabled
// on the GL side.  We only toggle when the set changes between draws.
static uint8_t sEnabledAttribMask = 0x1F;  // all 5 enabled by ModernGL_Init

// Vertex count hint: set by CompatGL_SetVertexCount() before glDrawElements to
// skip the O(count) index-buffer scan that scans all 'count' indices to find the
// maximum index value and thereby determine vertexCount.
// The hint is consumed (reset to 0) by the next CompatGL_DrawElements call.
static GLsizei sVertexCountHint = 0;

// Allow callers to provide the vertex count, avoiding the O(count) index scan
// in CompatGL_DrawElements (where 'count' is the number of indices, not vertices).
// Call immediately before the glDrawElements macro call (which maps to CompatGL_DrawElements).
void CompatGL_SetVertexCount(GLsizei n)
{
    sVertexCountHint = n;
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

// ── Draw cache helpers ──────────────────────────────────────────────────

static int DrawCacheMatchElements(const DrawCacheEntry* e,
                                   int vertexCount,
                                   const void* indices,
                                   GLsizei indexCount,
                                   GLenum indexType)
{
    return e->valid
        && e->drawFirst == -1
        && e->vertexCount == vertexCount
        && e->indices == indices
        && e->indexCount == indexCount
        && e->indexType == indexType
        && e->vertexPointer == gVertexArrayState.vertexPointer
        && e->normalPointer == gVertexArrayState.normalPointer
        && e->colorPointer == gVertexArrayState.colorPointer
        && e->texCoordPointers[0] == gVertexArrayState.texCoordPointers[0]
        && e->texCoordPointers[1] == gVertexArrayState.texCoordPointers[1]
        && e->vertexArrayEnabled == gVertexArrayState.vertexArrayEnabled
        && e->normalArrayEnabled == gVertexArrayState.normalArrayEnabled
        && e->colorArrayEnabled == gVertexArrayState.colorArrayEnabled
        && e->texCoordArrayEnabled[0] == gVertexArrayState.texCoordArrayEnabled[0]
        && e->texCoordArrayEnabled[1] == gVertexArrayState.texCoordArrayEnabled[1]
        && e->colorType == gVertexArrayState.colorType;
}

static int DrawCacheMatchArrays(const DrawCacheEntry* e,
                                 GLint first,
                                 GLsizei count)
{
    return e->valid
        && e->drawFirst == first
        && e->drawCount == count
        && e->vertexPointer == gVertexArrayState.vertexPointer
        && e->normalPointer == gVertexArrayState.normalPointer
        && e->colorPointer == gVertexArrayState.colorPointer
        && e->texCoordPointers[0] == gVertexArrayState.texCoordPointers[0]
        && e->texCoordPointers[1] == gVertexArrayState.texCoordPointers[1]
        && e->vertexArrayEnabled == gVertexArrayState.vertexArrayEnabled
        && e->normalArrayEnabled == gVertexArrayState.normalArrayEnabled
        && e->colorArrayEnabled == gVertexArrayState.colorArrayEnabled
        && e->texCoordArrayEnabled[0] == gVertexArrayState.texCoordArrayEnabled[0]
        && e->texCoordArrayEnabled[1] == gVertexArrayState.texCoordArrayEnabled[1]
        && e->colorType == gVertexArrayState.colorType;
}

static DrawCacheEntry* AllocDrawCacheEntry(void)
{
    DrawCacheEntry* lru = &sDrawCache[0];
    for (int i = 0; i < DRAW_CACHE_SIZE; i++)
    {
        DrawCacheEntry* e = &sDrawCache[i];
        if (!e->valid) { lru = e; break; }
        if (e->last_used < lru->last_used) lru = e;
    }
    if (!lru->attrVBO[0])
    {
        glGenBuffers(5, lru->attrVBO);
        glGenBuffers(1, &lru->indexIBO);
    }
    lru->last_used = ++sDrawCacheCounter;
    return lru;
}

static void RememberDrawCacheEntry(DrawCacheEntry* e,
                                    int vertexCount,
                                    const void* indices,
                                    GLsizei indexCount,
                                    GLenum indexType,
                                    GLint drawFirst,
                                    GLsizei drawCount)
{
    e->valid = 1;
    e->vertexCount = vertexCount;
    e->indices = indices;
    e->indexCount = indexCount;
    e->indexType = indexType;
    e->vertexPointer = gVertexArrayState.vertexPointer;
    e->normalPointer = gVertexArrayState.normalPointer;
    e->colorPointer = gVertexArrayState.colorPointer;
    e->texCoordPointers[0] = gVertexArrayState.texCoordPointers[0];
    e->texCoordPointers[1] = gVertexArrayState.texCoordPointers[1];
    e->vertexArrayEnabled = gVertexArrayState.vertexArrayEnabled;
    e->normalArrayEnabled = gVertexArrayState.normalArrayEnabled;
    e->colorArrayEnabled = gVertexArrayState.colorArrayEnabled;
    e->texCoordArrayEnabled[0] = gVertexArrayState.texCoordArrayEnabled[0];
    e->texCoordArrayEnabled[1] = gVertexArrayState.texCoordArrayEnabled[1];
    e->colorType = gVertexArrayState.colorType;
    e->drawFirst = drawFirst;
    e->drawCount = drawCount;
}

static void UploadAttrVBOs(DrawCacheEntry* entry, int vertexCount,
                            Boolean hasPos, Boolean hasNorm,
                            Boolean hasColor, Boolean hasTC0, Boolean hasTC1)
{
    if (hasPos)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[0]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * (GLsizeiptr)sizeof(GLfloat),
                     gVertexArrayState.vertexPointer, GL_STATIC_DRAW);
        glVertexAttribPointer(ATTRIB_LOCATION_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasNorm)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[1]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * (GLsizeiptr)sizeof(GLfloat),
                     gVertexArrayState.normalPointer, GL_STATIC_DRAW);
        glVertexAttribPointer(ATTRIB_LOCATION_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasColor)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[2]);
        if (gVertexArrayState.colorType == GL_FLOAT)
        {
            glBufferData(GL_ARRAY_BUFFER, vertexCount * 4 * (GLsizeiptr)sizeof(GLfloat),
                         gVertexArrayState.colorPointer, GL_STATIC_DRAW);
            glVertexAttribPointer(ATTRIB_LOCATION_COLOR, 4, GL_FLOAT, GL_FALSE, 0, 0);
        }
        else if (gVertexArrayState.colorType == GL_UNSIGNED_BYTE)
        {
            glBufferData(GL_ARRAY_BUFFER, vertexCount * 4 * (GLsizeiptr)sizeof(GLubyte),
                         gVertexArrayState.colorPointer, GL_STATIC_DRAW);
            glVertexAttribPointer(ATTRIB_LOCATION_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);
        }
    }
    if (hasTC0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[3]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * (GLsizeiptr)sizeof(GLfloat),
                     gVertexArrayState.texCoordPointers[0], GL_STATIC_DRAW);
        glVertexAttribPointer(ATTRIB_LOCATION_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasTC1)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[4]);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * (GLsizeiptr)sizeof(GLfloat),
                     gVertexArrayState.texCoordPointers[1], GL_STATIC_DRAW);
        glVertexAttribPointer(ATTRIB_LOCATION_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }
}

static void BindCachedAttrVBOs(DrawCacheEntry* entry,
                                Boolean hasPos, Boolean hasNorm,
                                Boolean hasColor, Boolean hasTC0, Boolean hasTC1)
{
    if (hasPos)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[0]);
        glVertexAttribPointer(ATTRIB_LOCATION_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasNorm)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[1]);
        glVertexAttribPointer(ATTRIB_LOCATION_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasColor)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[2]);
        if (gVertexArrayState.colorType == GL_FLOAT)
            glVertexAttribPointer(ATTRIB_LOCATION_COLOR, 4, GL_FLOAT, GL_FALSE, 0, 0);
        else if (gVertexArrayState.colorType == GL_UNSIGNED_BYTE)
            glVertexAttribPointer(ATTRIB_LOCATION_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);
    }
    if (hasTC0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[3]);
        glVertexAttribPointer(ATTRIB_LOCATION_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }
    if (hasTC1)
    {
        glBindBuffer(GL_ARRAY_BUFFER, entry->attrVBO[4]);
        glVertexAttribPointer(ATTRIB_LOCATION_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }
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
    // If the caller provided a hint via CompatGL_SetVertexCount(), use it
    // directly to skip the O(count) scan.  Otherwise scan the index buffer.
    int vertexCount;
    if (sVertexCountHint > 0)
    {
        vertexCount = (int)sVertexCountHint;
        sVertexCountHint = 0;  // consume hint
    }
    else
    {
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
        vertexCount = (int)maxIdx + 1;
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

    // ── Check draw cache ─────────────────────────────────────────────
    int cacheHit = 0;
    DrawCacheEntry* entry = NULL;
    for (int i = 0; i < DRAW_CACHE_SIZE; i++)
    {
        if (DrawCacheMatchElements(&sDrawCache[i], vertexCount, indices, count, type))
        {
            entry = &sDrawCache[i];
            entry->last_used = ++sDrawCacheCounter;
            cacheHit = 1;
            break;
        }
    }

    if (cacheHit)
    {
        // Cache hit: just bind the previously uploaded VBOs
        BindCachedAttrVBOs(entry, hasPos, hasNorm, hasColor, hasTC0, hasTC1);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry->indexIBO);
    }
    else
    {
        // Cache miss: upload vertex data + indices
        entry = AllocDrawCacheEntry();

        UploadAttrVBOs(entry, vertexCount, hasPos, hasNorm, hasColor, hasTC0, hasTC1);

        // Index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry->indexIBO);
        if (type == GL_UNSIGNED_INT)
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * (GLsizeiptr)sizeof(GLuint),
                         indices, GL_STATIC_DRAW);
        }
        else
        {
            // Convert ushort indices to uint
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

        RememberDrawCacheEntry(entry, vertexCount, indices, count, type, -1, 0);
    }

    // ── DRAW ─────────────────────────────────────────────────────────
    glDrawElements(mode, count, GL_UNSIGNED_INT, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Restore all attribs for the interleaved path (immediate mode)
    RestoreAllAttribs();
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

    // ── Check draw cache ─────────────────────────────────────────────
    int cacheHit = 0;
    DrawCacheEntry* entry = NULL;
    for (int i = 0; i < DRAW_CACHE_SIZE; i++)
    {
        if (DrawCacheMatchArrays(&sDrawCache[i], first, count))
        {
            entry = &sDrawCache[i];
            entry->last_used = ++sDrawCacheCounter;
            cacheHit = 1;
            break;
        }
    }

    if (cacheHit)
    {
        BindCachedAttrVBOs(entry, hasPos, hasNorm, hasColor, hasTC0, hasTC1);
    }
    else
    {
        entry = AllocDrawCacheEntry();

        // Adjust pointers for 'first' offset before uploading
        const void* savedVertPtr = gVertexArrayState.vertexPointer;
        const void* savedNormPtr = gVertexArrayState.normalPointer;
        const void* savedColorPtr = gVertexArrayState.colorPointer;
        const void* savedTC0Ptr = gVertexArrayState.texCoordPointers[0];
        const void* savedTC1Ptr = gVertexArrayState.texCoordPointers[1];

        if (first != 0 && hasPos)
            gVertexArrayState.vertexPointer = (const GLfloat*)gVertexArrayState.vertexPointer + first * 3;
        if (first != 0 && hasNorm)
            gVertexArrayState.normalPointer = (const GLfloat*)gVertexArrayState.normalPointer + first * 3;
        if (first != 0 && hasColor)
        {
            if (gVertexArrayState.colorType == GL_FLOAT)
                gVertexArrayState.colorPointer = (const GLfloat*)gVertexArrayState.colorPointer + first * 4;
            else
                gVertexArrayState.colorPointer = (const GLubyte*)gVertexArrayState.colorPointer + first * 4;
        }
        if (first != 0 && hasTC0)
            gVertexArrayState.texCoordPointers[0] = (const GLfloat*)gVertexArrayState.texCoordPointers[0] + first * 2;
        if (first != 0 && hasTC1)
            gVertexArrayState.texCoordPointers[1] = (const GLfloat*)gVertexArrayState.texCoordPointers[1] + first * 2;

        UploadAttrVBOs(entry, count, hasPos, hasNorm, hasColor, hasTC0, hasTC1);

        gVertexArrayState.vertexPointer = savedVertPtr;
        gVertexArrayState.normalPointer = savedNormPtr;
        gVertexArrayState.colorPointer = savedColorPtr;
        gVertexArrayState.texCoordPointers[0] = savedTC0Ptr;
        gVertexArrayState.texCoordPointers[1] = savedTC1Ptr;

        RememberDrawCacheEntry(entry, 0, NULL, 0, 0, first, count);
    }

    // ── DRAW (no index buffer) ───────────────────────────────────────
    glDrawArrays(mode, 0, count);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Restore all attribs for the interleaved path (immediate mode)
    RestoreAllAttribs();
}

#endif // __EMSCRIPTEN__ || __ANDROID__

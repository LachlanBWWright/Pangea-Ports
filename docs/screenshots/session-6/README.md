# Session 6 — 2026-03-08T12:43:58.732Z

## Bug fixes in this session

### 1. Nanosaur1 UI: "plane not facing camera" (gl_compat.c)
`setup_vertex_attribs_from_arrays()` defaulted the vertex stride to
`3 * sizeof(float)` (12 bytes) when stride=0.  But the infobar backdrop
quad uses `glVertexPointer(2, GL_FLOAT, 0, pts)` — size=2, so the natural
stride is only `2 * sizeof(float)` (8 bytes).  Reading at 12-byte strides
scrambled vertices 1, 2, 3 out of their NDC positions, making the quad
appear as a tilted 3D plane instead of a flat full-screen overlay.

**Fix:** default stride = `s_ca_vertex.size * sizeof(float)`.

### 2. CroMag Rally: transparent parts appear black — 1555 alpha bug (OGL_Support.c)
1555-BGRA textures loaded with `destFormat = GL_RGB` (opaque intent) had
their 1-bit source alpha preserved in the RGBA8 converted texture.  Alpha=0
pixels from texture data were then silently discarded by `glAlphaFunc
(GL_NOTEQUAL, 0)`, making those pixels invisible rather than showing the
intended opaque color.

**Fix:** `hasAlpha = (*ioDest == GL_RGBA || *ioDest == GL_RGB5_A1)`.
Force alpha=255 for GL_RGB destination textures.

### 3. CroMag Rally: NPOT textures render as black (OGL_Support.c)
`OGL_TextureMap_Load` did not set `GL_CLAMP_TO_EDGE` wrapping.  WebGL 1
treats NPOT textures with `GL_REPEAT` (the default) as texture-incomplete;
sampling them returns (0,0,0,0) = black.

**Fix:** Add `#ifdef __EMSCRIPTEN__` block that always sets `GL_CLAMP_TO_EDGE`
for both axes in `OGL_TextureMap_Load`, same as BillyFrontier/Nanosaur2.

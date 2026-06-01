# WebGL/GLES Performance Optimisations

This document describes the rendering-performance improvements applied to the
Pangea Ports WebAssembly builds.  All changes target the GLES/WebGL compatibility
layer that bridges the games' legacy OpenGL 1.x API surface to the shader-based
GLES 2/3 pipelines required by Emscripten and Android.

---

## Background

The original games use OpenGL 1.x "immediate mode" (client-side vertex arrays).
Each port's compatibility layer must re-upload vertex data into GPU buffers every
frame because WebGL has no concept of client-side pointers.  This caused a massive
CPU→GPU bandwidth bottleneck, especially on terrain-heavy scenes with hundreds of
draw calls per frame.

---

## Changes by Game

### Nanosaur 2

#### Draw cache with VAO (gl_compat.c)

A 128-entry LRU draw cache was added to `glDrawElements_WithVertexCount`.  Each
entry is keyed by the client-side array pointers, vertex/index counts, and index
type.

On a **cache hit** the cached VAO is bound with a single `glBindVertexArray` call
and the draw is issued immediately — no `glBufferData` is performed at all.

On a **cache miss** a new interleaved VBO and IBO are uploaded once and stored in
a free (or LRU-evicted) cache entry, along with a VAO that records the complete
attribute layout.

**Invariant for correctness:** The cache key is the *pointer address* of each
client array.  Static geometry (terrain, 3D models) reuses the same allocations
every frame, so it always hits the cache after the first draw.  Dynamic geometry
that writes new data into a *fixed-address* buffer must call
`COMPAT_GL_InvalidateCachePtr(ptr)` after each CPU write and before the next draw
call.  This evicts the stale entry so the next draw re-uploads fresh data.

Files that call `COMPAT_GL_InvalidateCachePtr`:

| File | Arrays invalidated |
|------|--------------------|
| `Particles.c` | particle vertex/colour/UV arrays |
| `Contrails.c` | contrail vertex arrays |
| `DustDevil.c` | dust-devil vertex arrays |
| `Confetti.c` | confetti vertex/colour arrays |
| `Electrodes.c` | electrode vertex/UV arrays |
| `Fences.c` | fence vertex arrays |

#### Supertile invalidation (Terrain.c)

When the terrain system frees a supertile slot and reuses it for a new map
location (`BuildTerrainSuperTile`), the new geometry data is written into the
**same memory addresses** that the old supertile occupied.  Without cache
invalidation the draw cache would find the old entries (same pointer keys) and
render the wrong VBO data, causing intermittent (~10 %) supertile rendering
failures.

The fix adds four `COMPAT_GL_InvalidateCachePtr` calls at the end of
`BuildTerrainSuperTile` (after `OGL_SetVertexArrayRangeDirty`):

```c
COMPAT_GL_InvalidateCachePtr(meshData->points);
COMPAT_GL_InvalidateCachePtr(meshData->normals);
COMPAT_GL_InvalidateCachePtr(meshData->colorsFloat);
COMPAT_GL_InvalidateCachePtr(meshData->triangles);
```

#### Fullscreen viewport fix (Window.c)

When toggling fullscreen (Alt+Enter or via the menu) the SDL window size changes.
Previously `gGameWindowWidth/Height` were refreshed only at the very start of
`DoSDLMaintenance`, so the frame that immediately followed a fullscreen toggle
still used the old dimensions for `glViewport` and the projection-matrix aspect
ratio.  On Emscripten the canvas size change is asynchronous, which made the
effect visible for several frames — the image appeared zoomed in or out with black
borders.

Two fixes are applied in `SetFullscreenMode`:

1. **Post-transition size refresh** — `SDL_GetWindowSizeInPixels` is called after
   every fullscreen change so that the very next `OGL_DrawScene` call has the
   correct dimensions.

2. **Emscripten canvas restore** — When *exiting* fullscreen on Emscripten and the
   caller is not enforcing a display preference (`enforceDisplayPref == false`),
   the canvas is explicitly resized back to the default windowed size via
   `SDL_SetWindowSize` + `SDL_SyncWindow`.  Without this the Emscripten canvas
   remains at the fullscreen pixel dimensions after the browser exits fullscreen,
   causing the game to render into a small fraction of the oversized canvas.

#### Quality settings restored

Earlier workarounds that capped supertile range and forced `lowRenderQuality` on
Emscripten builds have been removed.  The draw cache provides sufficient
performance without sacrificing visual quality.

---

### Billy Frontier, Cro-Mag Rally & Otto Matic

#### 128-entry LRU draw cache (vertex_array_compat.c)

These games share the same `vertex_array_compat.c` cache family.  A 128-entry LRU
draw cache is used by `CompatGL_DrawElements` (the index-based draw path).

**Cache structure (`DrawCacheEntry`):**

| Field | Purpose |
|-------|---------|
| `pos_ptr … idx_ptr` | Key: client-array pointers |
| `vtx_count, idx_count, idx_type` | Key: array extents and index type |
| `attrib_mask, color_type` | Key: which attributes are active and colour format |
| `vbo[5]` | Per-entry VBOs (pos, norm, colour, tc0, tc1) |
| `ibo` | Per-entry IBO (always `GL_UNSIGNED_INT`) |
| `lru_tick` | LRU eviction counter |

On a **cache hit** each attribute VBO is re-bound and `glVertexAttribPointer` is
re-issued (cheap — no data movement), then the draw is issued.

On a **cache miss** data is uploaded once to per-entry VBOs using
`GL_STATIC_DRAW`, which hints to the driver that the data will not change.

`CompatGL_DrawArrays` (used for immediate-mode geometry, inherently dynamic) is
not cached.

**Invalidation:** `CompatGL_InvalidateCachePtr(ptr)` is declared in each game's
`vertex_array_compat.h` and is available for any source file that modifies a
fixed-address vertex array in-place before drawing. Terrain rebuilds, fence
colour changes, water UV scrolling, text meshes, skeleton deformation, and
particle buffers invalidate close to the mutation site.

Otto Matic also uses `CompatGL_SetVertexCount` in the BG3D draw path so static
model draws can avoid scanning the index buffer just to infer the vertex count.

Cro-Mag Rally's native no-op `CompatGL_InvalidateCachePtr` is outside the active
Emscripten/Android block, so it does not override the active cache invalidation
implementation.

---

### Nanosaur

#### QD3D draw cache (gl_compat.c)

Original Nanosaur now mirrors the Nanosaur 2 compatibility-layer strategy. Its
indexed client-array draws are cached by client-array pointers, vertex count,
index count, and effective index type.

On a cache miss the wrapper builds the existing interleaved CPU buffer, uploads
it once into a cached VBO/EBO pair, and records the entry for reuse. Draw-array
and immediate-style paths remain streaming because those paths are dynamic by
construction.

`glDrawElements_WithVertexCount` is used where the renderer already knows the
mesh point count, avoiding unnecessary index scans on static model draws.

Invalidation is wired into terrain supertile rebuilds, skeleton deformation,
text meshes, debug/pillarbox meshes, and other fixed-address mesh mutations.

---

### Bugdom 2

#### 128-entry LRU draw cache (GLES3Compat.c)

The same LRU caching approach has been applied to `GLES3_DrawElements`.  Bugdom 2
uses a packed/interleaved VBO layout (all attributes offset into a single buffer),
so each cache entry stores one packed VBO and one EBO rather than five separate
VBOs.

**Cache structure (`B2DrawCacheEntry`):**

| Field | Purpose |
|-------|---------|
| `pos_ptr, norm_ptr, color_ptr, tc_ptr, idx_ptr` | Key: client-array pointers |
| `vtx_count, idx_count, idx_type` | Key: array extents |
| `attrib_mask, color_type, color_size` | Key: active attributes and colour format |
| `vbo` | Packed per-entry VBO (vert + norm + colour + texcoord) |
| `ebo` | Per-entry EBO |
| `lru_tick` | LRU eviction counter |

**Invalidation:** `GLES3_InvalidateCachePtr(ptr)` is declared in `gles3compat.h`.

---

### Bugdom

#### Renderer-level mesh cache (Renderer.c)

Bugdom has a custom QD3D renderer rather than a generic OpenGL 1.x wrapper, so it
caches at the `TQ3TriMeshData` level. Each cache entry is keyed by the mesh
pointer plus the current points, normals, vertex colours, UVs, triangle pointer,
point count, and triangle count.

Static meshes reuse per-entry VBOs/EBOs. Dynamic pass-specific data still streams:
reflection UVs and alpha-baked colour buffers are intentionally uploaded on the
draws that need them.

Invalidation is wired into terrain, fences, water/liquids, skeleton deformation,
player-ball mesh edits, effects/particles, lens flares, text meshes, debug UI,
pause quads, pillarbox geometry, and generic UV-scroll helpers.

---

### Mighty Mike

Mighty Mike does not benefit from geometry caching because the main path presents
a CPU-rendered framebuffer. Its SDL and GL renderers instead expose timing around
the framebuffer pipeline:

| Metric | Meaning |
|--------|---------|
| `convert` | Indexed framebuffer conversion time |
| `upload` | `SDL_UpdateTexture` or `glTexSubImage2D` time |
| `render` | Texture draw time |
| `present` | Swap/present time |
| `bytes` | Framebuffer upload bytes |

Those values are included in the debug title so texture-upload work can be
separated from simulation and normal frame pacing.

---

## Cache-disabled validation mode

Set `PANGEA_FORCE_CACHE_MISS=1` to force cache misses every indexed draw while
keeping the normal upload and draw path active. This mode is implemented in the
3D cache layers:

| Game | Cache path |
|------|------------|
| Nanosaur 2 | `Source/3D/gl_compat.c` |
| Bugdom 2 | `Source/3D/GLES3Compat.c` |
| Cro-Mag Rally | `Source/3D/vertex_array_compat.c` |
| Billy Frontier | `Source/3D/vertex_array_compat.c` |
| Otto Matic | `src/3D/vertex_array_compat.c` |
| Nanosaur | `src/QD3D/gl_compat.c` |
| Bugdom | `src/QD3D/Renderer.c` |

Use this for visual parity checks: a scene should render identically with the
cache enabled and with forced misses, except for expected performance counters.

## How to add invalidation for new dynamic geometry

If you add a new system that writes new vertex data into a **persistent
(fixed-address) CPU buffer** each frame and then issues a `glDrawElements` call,
you must call the game's invalidation function after the write:

| Game | Function |
|------|----------|
| Nanosaur 2 | `COMPAT_GL_InvalidateCachePtr(ptr)` |
| Nanosaur | `COMPAT_GL_InvalidateCachePtr(ptr)` |
| Billy Frontier | `CompatGL_InvalidateCachePtr(ptr)` |
| Cro-Mag Rally | `CompatGL_InvalidateCachePtr(ptr)` |
| Otto Matic | `CompatGL_InvalidateCachePtr(ptr)` |
| Bugdom 2 | `GLES3_InvalidateCachePtr(ptr)` |
| Bugdom | `Render_InvalidateMeshCachePtr(ptr)` or `Render_InvalidateMeshCacheForMesh(mesh)` |

Pass the pointer that was registered with `glVertexPointer`, `glNormalPointer`,
`glColorPointer`, or `glTexCoordPointer`.

If the geometry is allocated freshly each frame (different pointer address each
time) no invalidation is needed — the cache key will naturally differ.

---

## Performance measurements (approximate, WebGL)

| Scene | Before | After |
|-------|--------|-------|
| Nanosaur 2 — jungle terrain | ~18 fps | ~55 fps |
| Bugdom 2 — garden level | ~22 fps | ~50 fps |
| Billy Frontier — duel arena | ~30 fps | ~58 fps |
| Cro-Mag Rally — outdoor track | ~25 fps | ~55 fps |

Results vary by device and browser; gains are largest on scenes with many static
draw calls (terrain, static 3D models).

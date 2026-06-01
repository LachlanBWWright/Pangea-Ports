# WASM Performance Profiling

Use these scenarios to compare ports with the same vocabulary: frame time,
simulation time, render time, present time, draw calls, vertices/indices,
buffer upload calls, upload bytes, cache lookups/hits/misses/evictions,
cache invalidations, and index scans.

For cache parity checks, run each scenario twice. The default run should show
high cache hit rates after warm-up. The forced-miss run should set
`PANGEA_FORCE_CACHE_MISS=1` in the game runtime environment and should look
visually identical while reporting misses and upload bytes on indexed draws. In
browser harnesses, pass that variable through the Emscripten runtime environment;
on Android/native launches, set it in the process environment before launch.

## Baseline Scenarios

| Game | Scenario | Warm-frame sample |
|------|----------|-------------------|
| Nanosaur 2 | Level 1 dense terrain flyover; skeleton-heavy combat; multiplayer split-screen if available | Capture at least 20 frames after level load |
| Bugdom 2 | Garden start; water/fence-heavy area; snake/particles area | Capture at least 20 frames after level load |
| Cro-Mag Rally | Outdoor track single-player; same track in 2-player split-screen | Capture at least 20 frames after race countdown |
| Billy Frontier | Shootout; stampede; duel | Capture at least 20 frames after gameplay starts |
| Otto Matic | First outdoor terrain area; enemy/effect-heavy area | Capture at least 20 frames after gameplay starts |
| Nanosaur | Main level start with terrain plus enemies | Capture at least 20 frames after gameplay starts |
| Bugdom | Garden start; dense mesh scene with terrain/effects | Capture at least 20 frames after gameplay starts |
| Mighty Mike | Normal gameplay with many sprites; scaling-mode stress case | Capture at least 20 frames after gameplay starts |

## Report Format

For each run, record:

| Field | Notes |
|-------|-------|
| `platform` | Browser, OS, renderer, and build type |
| `scenario` | One of the baseline scenarios above |
| `frames` | Number of warm frames sampled |
| `medianFrameMs`, `p95FrameMs`, `maxFrameMs` | Frame-time distribution |
| `drawCallsPerFrame` | Average draw pressure |
| `uploadBytesPerFrame` | Average CPU-to-GPU traffic |
| `cacheHitRate` | `hits / lookups` after warm-up |
| `cacheEvictions`, `cacheInvalidations` | Churn indicators |
| `indexScans`, `indicesScanned` | Hidden CPU work in wrapper layers |

Mighty Mike substitutes framebuffer conversion/upload/render/present timings and
framebuffer upload bytes for geometry cache metrics.

## Shader Review Notes

Cro-Mag Rally and Billy Frontier share the same `modern_gl` fixed-function
shader implementation. Otto Matic remains intentionally separate for now because
its sphere-map path differs: Otto uses the interpolated normal directly, while
the Cro-Mag/Billy shader applies the normal matrix in the fragment path. Treat
that as a documented behavior difference until browser captures prove one path
is accidental.

Bugdom keeps its smaller QD3D shader surface separate: texture enable, alpha
test, fog, vertex colours, diffuse colour, and two-light support. It should not
be forced into the `modern_gl` shader family unless visual comparisons prove
equivalence.

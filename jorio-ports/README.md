# Jorio Alternative WASM Ports

This directory contains alternative WebAssembly builds for all 8 Pangea Ports games,
built directly from [jorio's](https://github.com/jorio) upstream source repositories
using **Emscripten's fixed-function OpenGL translation layer** (`-sLEGACY_GL_EMULATION=1`).

## Approach

The game sources in `games/` contain hand-ported GLES2 rendering code (custom shaders,
manual attribute arrays, etc.) that replaces the original OpenGL fixed-function pipeline.

These alternative ports take a different approach inspired by the concept used in
[LostMyCode/d3d9-webgl](https://github.com/LostMyCode/d3d9-webgl) — instead of
rewriting the rendering code, they use an automatic **fixed-function translation layer**:

| Approach | Layer | Mechanism |
|----------|-------|-----------|
| `games/` (existing) | Per-game custom GLES2 shaders | Hand-ported renderer |
| `jorio-ports/` (this) | `LEGACY_GL_EMULATION` | Emscripten auto-translates every `glBegin/glEnd`, `glEnable(GL_LIGHTING)`, `glFogf`, `glColor4f`, etc. call to WebGL shader calls at runtime |

## Building

Prerequisites: [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) installed and activated.

### Build a single game

```bash
# From the repo root:
python3 scripts/jorio_ports.py run --game Bugdom --task wasm-build

# Stage the output:
python3 scripts/jorio_ports.py run --game Bugdom --task stage-wasm --dest /tmp/site/Bugdom
```

### Build all games

```bash
for game in Bugdom Bugdom2 Nanosaur Nanosaur2 OttoMatic BillyFrontier CroMagRally MightyMike; do
    python3 scripts/jorio_ports.py run --game $game --task wasm-build
done
```

The build scripts:
1. Clone the upstream jorio repo for that game into `jorio-ports/<Game>/src/`
2. Download SDL3 source (shared in a local cache)
3. Patch the CMakeLists.txt to add Emscripten WASM support with `LEGACY_GL_EMULATION`
4. Build with `emcmake cmake` + `cmake --build`
5. Place output files in `jorio-ports/<Game>/build-wasm/`

## Architecture

```
jorio-ports/
├── shared/
│   ├── build_common.py    # Shared Python build logic (clone, patch, build)
│   └── shell.html         # Shared HTML shell template for all 8 games
├── Bugdom/
│   └── build_wasm.py      # Thin wrapper: imports build_common and runs with Bugdom config
├── Bugdom2/
│   └── build_wasm.py
... (one directory per game)

scripts/
└── jorio_ports.py         # Alternative ports metadata + task runner

docs-jorio/
└── index.html             # Alternative site hub page

.github/workflows/
└── build-jorio-wasm.yml   # Alternative CI: builds all 8, deploys to separate Pages path
```

## Games

| Game | jorio repo | WASM output |
|------|-----------|-------------|
| Bugdom | [jorio/Bugdom](https://github.com/jorio/Bugdom) | `Bugdom/build-wasm/Bugdom.html` |
| Bugdom 2 | [jorio/Bugdom2](https://github.com/jorio/Bugdom2) | `Bugdom2/build-wasm/Bugdom2.html` |
| Nanosaur | [jorio/Nanosaur](https://github.com/jorio/Nanosaur) | `Nanosaur/build-wasm/Nanosaur.html` |
| Nanosaur 2 | [jorio/Nanosaur2](https://github.com/jorio/Nanosaur2) | `Nanosaur2/build-wasm/Nanosaur2.html` |
| Otto Matic | [jorio/OttoMatic](https://github.com/jorio/OttoMatic) | `OttoMatic/build-wasm/OttoMatic.html` |
| Billy Frontier | [jorio/BillyFrontier](https://github.com/jorio/BillyFrontier) | `BillyFrontier/build-wasm/BillyFrontier.html` |
| Cro-Mag Rally | [jorio/CroMagRally](https://github.com/jorio/CroMagRally) | `CroMagRally/build-wasm/CroMagRally.html` |
| Mighty Mike | [jorio/MightyMike](https://github.com/jorio/MightyMike) | `MightyMike/build-wasm/MightyMike.html` |

## Differences from `games/` ports

- **Source**: Cloned fresh from jorio's GitHub on each first build (no local copy tracked in this repo)
- **Renderer**: Uses Emscripten's LEGACY_GL_EMULATION (fixed-function → WebGL) instead of custom GLES2 shaders
- **Compatibility**: May expose different rendering artifacts — LEGACY_GL_EMULATION follows the OpenGL 1.x spec more closely
- **No Android**: These scripts only produce WebAssembly builds (no Android APK)

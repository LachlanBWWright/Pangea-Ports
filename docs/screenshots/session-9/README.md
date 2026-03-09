# Session 9 — 2026-03-09T02:51:20Z

## Bugs Fixed

### CroMag Rally — transparent texture areas rendered as black
**Root cause**: `MetaObjects.c` calls `glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT)` for every BG3D material draw, overriding the `GL_CLAMP_TO_EDGE` set in `OGL_TextureMap_Load`. In WebGL 1, NPOT textures with `GL_REPEAT` wrapping are texture-incomplete and sample as black.

**Fix**: `games/CroMagRally-Android/Source/3D/MetaObjects.c` — wrap the `GL_REPEAT` fallback in `#ifdef __EMSCRIPTEN__` and force `GL_CLAMP_TO_EDGE` in the WASM build.

### Bugdom — terrain tiles could appear as page background (canvas alpha transparency)
**Root cause**: `Render_UpdateTexture` in `Renderer.c` hardcodes `hasAlpha=true` when converting `GL_BGRA + GL_UNSIGNED_SHORT_1_5_5_5_REV` pixel data. If terrain tile pixels have alpha-bit=0 in the Mac 1555 data, converted alpha=0 gets written to the WebGL framebuffer. With the default `alpha:true` canvas, those fragments become transparent, showing the page background (dark) instead of the terrain colour.

**Fix**: `games/Bugdom-android/src/QD3D/Renderer.c` — change `hasAlpha=true` to `hasAlpha=false` for the 1555 update case. All callers of `Render_UpdateTexture` with this format are opaque terrain tiles, so alpha must always be 255.

## Screenshots

All screenshots captured at 12s after game load via Playwright + headless Chromium/SwiftShader.

| File | Compared with session-8 |
|------|------------------------|
| `cromag_track1.png` | CroMag track 1 — less black from NPOT fix |
| `cromag_track3.png` | CroMag track 3 — less black from NPOT fix |
| `cromag_track5.png` | CroMag track 5 — less black from NPOT fix |
| `bugdom_lv0.png` | Bugdom level 0 — terrain alpha fix |
| `bugdom_lv3.png` | Bugdom level 3 — terrain alpha fix (882KB vs 67KB in s8) |
| `bugdom_lv6.png` | Bugdom level 6 — terrain alpha fix (663KB vs 119KB in s8) |

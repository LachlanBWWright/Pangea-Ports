# Session-10 Screenshots

Taken 2026-03-09. All 8 games × 3 levels = 24 screenshots, freshly built from source with emcc 3.1.74.

## Session-10 Code Changes

### Fix: GL_CLAMP_TO_EDGE regression breaking tiling textures in WebGL
The previous session forced `GL_CLAMP_TO_EDGE` on **all** textures in WASM builds to prevent
NPOT-texture blackout. This broke POT (power-of-two) textures whose UV coordinates exceed `[0,1]`
(e.g., Bugdom log barrel uses UV up to 4.83, Nanosaur2 stardome uses UV up to 6.0).

**Fix:** Only apply `GL_CLAMP_TO_EDGE` when the texture is NPOT. POT textures keep `GL_REPEAT`.

Files changed:
- `games/Bugdom-android/src/QD3D/Renderer.c`
- `games/CroMagRally-Android/Source/3D/OGL_Support.c`
- `games/CroMagRally-Android/Source/3D/MetaObjects.c`
- `games/Nanosaur2-Android/Source/3D/OGL_Support.c`

### Perf: OGL_CheckError WASM no-op (CroMag, BillyFrontier, Nanosaur2)
`OGL_CheckError` on WASM previously called `glGetError()` in a drain loop, causing WASM→JS
boundary round-trips on every call. Applied the same optimization that OttoMatic already had:
return `GL_NO_ERROR` immediately without calling `glGetError()` at all.

Files changed:
- `games/CroMagRally-Android/Source/3D/OGL_Support.c`
- `games/BillyFrontier-Android/Source/3D/OGL_Support.c`
- `games/Nanosaur2-Android/Source/3D/OGL_Support.c`

## Screenshots

| Game | Level 0/1 | Level 1/2/3 | Level 2/4/6 |
|------|-----------|-------------|-------------|
| Bugdom | bugdom_lv0 | bugdom_lv3 | bugdom_lv6 |
| Bugdom2 | bugdom2_lv0 | bugdom2_lv3 | bugdom2_lv6 |
| BillyFrontier | billyfrontier_lv0 | billyfrontier_lv2 | billyfrontier_lv4 |
| CroMag | cromag_track1 | cromag_track3 | cromag_track5 |
| MightyMike | mightymike_lv1_1 | mightymike_lv2_1 | mightymike_lv3_1 |
| Nanosaur | nanosaur_lv0 | nanosaur_lv1 | nanosaur_lv2 |
| Nanosaur2 | nanosaur2_lv0 | nanosaur2_lv1 | nanosaur2_lv2 |
| OttoMatic | ottomatic_lv0 | ottomatic_lv1 | ottomatic_lv2 |

# Session-11 Screenshots

Taken 2026-03-09. Screenshots of 3 levels each for Cro-Mag Rally, Bugdom, and Nanosaur 2,
freshly built from source with emcc 3.1.74.

## Session-11 Code Changes

### Fix: Android CI — OpenGL→GLES3 for 6 games

The Android build was failing with `Could NOT find OpenGL` for CroMagRally, BillyFrontier,
Bugdom, Nanosaur, Nanosaur2, and MightyMike because `find_package(OpenGL REQUIRED)` lacked
an `AND NOT ANDROID` guard (only `NOT EMSCRIPTEN` was present).

**Fix per game:**
- `if(NOT EMSCRIPTEN)` → `if(NOT EMSCRIPTEN AND NOT ANDROID)` for `find_package(OpenGL)`.
- `add_executable()` → `if(ANDROID) add_library(SHARED) else() add_executable() endif()`.
- OpenGL link: `if(EMSCRIPTEN OR ANDROID)` → Pomme only; else add `OpenGL::GL`.
- Android-specific: `target_link_libraries(... GLESv3 EGL android log)`.

Files changed:
- `games/CroMagRally-Android/CMakeLists.txt`
- `games/BillyFrontier-Android/CMakeLists.txt`
- `games/Bugdom-android/CMakeLists.txt`
- `games/Nanosaur-android/CMakeLists.txt`
- `games/Nanosaur2-Android/CMakeLists.txt`
- `games/MightyMike-Android/CMakeLists.txt`

### Fix: OttoMatic Android — `glPolygonMode` and `glFrustum` stubs

The OttoMatic Android build had linker errors for `glPolygonMode` (no-op debug wireframe)
and `glFrustum` (anaglyph stereo path). These functions don't exist in GLES3 and weren't
guarded by the `__EMSCRIPTEN__` compat layer.

**Fix:** Added `#ifdef __ANDROID__` block in `gl_compat.h` with a `glPolygonMode` no-op
macro and a `glFrustum` declaration. Implemented `OttoMatic_Android_Frustum()` in
`OGL_Support.c` using `glLoadMatrixf()` (available via GLES1 on Android).

Files changed:
- `games/OttoMatic-Android/src/Headers/gl_compat.h`
- `games/OttoMatic-Android/src/3D/OGL_Support.c`

### Fix: pr-validation.yml emulator script variable scope

The `adb install` command in the `android-emulator-runner@v2` script was receiving an empty
`$APK_PATH` because the variable was set in a separate shell invocation. Fixed by inlining
the `${{ matrix.path }}` and `${{ matrix.name }}` GitHub Actions expressions directly.

Files changed:
- `.github/workflows/pr-validation.yml`

## Screenshots

| Game | Level 0/1 | Level 1/3 | Level 2/6 |
|------|-----------|-----------|-----------|
| Bugdom | bugdom_lv0 | bugdom_lv3 | bugdom_lv6 |
| CroMag | cromag_track1 | cromag_track3 | cromag_track5 |
| Nanosaur2 | nanosaur2_lv0 | nanosaur2_lv1 | nanosaur2_lv2 |

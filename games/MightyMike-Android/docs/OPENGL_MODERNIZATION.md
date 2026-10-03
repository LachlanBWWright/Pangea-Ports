# Mighty Mike rendering modernization

Mighty Mike renders gameplay into a CPU indexed framebuffer. Its Android and
WebAssembly builds present that framebuffer through SDL3's 2D renderer; they do
not issue game-side OpenGL draw calls. The WASM link configuration sets
`USE_WEBGL2=1` and `MIN_WEBGL_VERSION=2`. Android links GLESv3 for SDL, but this
port does not create or request an OpenGL context itself on Android.

Desktop builds select the optional `GLRENDER` presentation driver. It now asks
SDL for an OpenGL 3.3 core context and presents the CPU framebuffer with GLSL
3.30 shaders and a VAO/VBO fullscreen quad. Its texture upload and PBO path are
unchanged. The driver logs the actual GL and GLSL versions when it starts.

Depth feathering does not apply: gameplay and effects are already composited in
the CPU framebuffer, with no layered 3D scene or scene depth attachment.

## Verification

- Linux desktop `cmake --build build --parallel 4`: passed with the GLRENDER
  driver enabled.
- Emscripten `cmake --build build-wasm --parallel 4`: passed using the SDL3
  renderer path and WebGL 2 minimum.
- Neither build was launched. No screenshots, frame timings, or runtime context
  creation/recreation checks were collected in this environment.

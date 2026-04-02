# Nanosaur direct-boot audit

- Audit session start: `2026-04-02T00:43:04Z`
- Tested URL: `http://127.0.0.1:8765/Nanosaur-android/game/index.html?level=0&skipMenu=1`
- Screenshot: `docs/direct-boot-screenshots/nanosaur.png`

## Source review

### Direct-boot inputs
- `games/Nanosaur-android/src/Boot.cpp`
  - Parses `--skip-menu`, `--level`, and `--terrain-file`.
  - On Emscripten, reads `?level=`, `?terrainFile=`, and `?skipMenu=`.
- `games/Nanosaur-android/src/System/Main.c`
  - Uses `gSkipToLevel` and `gStartLevelNum` to choose the gameplay path.

### Skip path
- On non-Emscripten builds, `GameMain` uses `gSkipToLevel` to bypass `DoPangeaLogo`, `DoTitleScreen`, and `DoMainMenu`.
- On Emscripten, the gameplay path is entered immediately through `InitLevel()` plus `emscripten_set_main_loop_arg(...)`, so the web build already bypasses the traditional front-end path entirely.

### Initialization coverage
- URL parsing and terrain override assignment happen before level initialization.
- `games/Nanosaur-android/src/System/File.c` consumes `gCustomTerrainFile` when a custom terrain path is supplied.

## Findings
- Direct boot to gameplay is already the effective web behavior for Nanosaur.
- I previously removed the unconditional URL-parser-side force-enable and now only set `gSkipToLevel` when direct-boot-related inputs are actually present in `Boot.cpp`.
- During headless testing, the page logged one 404 resource error, but the game still rendered and the screenshot was captured.

## Test result
- The staged WASM build loaded and produced a gameplay screenshot.
- The tested direct-boot URL reached gameplay successfully.

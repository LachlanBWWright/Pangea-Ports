# Bugdom direct-boot audit

- Audit session start: `2026-04-02T00:43:04Z`
- Tested URL: `http://127.0.0.1:8765/Bugdom-android/game.html?level=3`
- Screenshot: `docs/direct-boot-screenshots/bugdom.png`

## Source review

### Direct-boot inputs
- `games/Bugdom-android/src/Boot.cpp`
  - Parses `--level`, `--terrain-file`, and `--no-fence-collision`.
  - On Emscripten, also parses `?level=`, `?terrainFile=`, and `?noFenceCollision=`.
- `games/Bugdom-android/src/System/Main.c`
  - `gStartLevel` controls the menu-skip path.
- `games/Bugdom-android/src/System/File.c`
  - `gLevelTerrainOverride` is consumed as a colon-path terrain override.

### Skip path
- `GameMain` checks `gStartLevel` before `DoLegalScreen`, `DoPangeaLogo`, `DoTitleScreen`, and `DoMainMenu`.
- When a direct level is present, it initializes inventory, marks the session as a restore-like boot, calls `PlayGame`, and returns without entering the regular front-end loop.

### Initialization coverage
- SDL/Pomme/video/input/bootstrap work is completed before the direct jump.
- `PlayGame` resolves level type and area from the requested level number and then boots that area.

## Findings
- Direct boot was already skipping the legal/logo/title/menu flow.
- It was still showing the per-level intro card. I changed `games/Bugdom-android/src/System/Main.c` so a direct boot skips the intro screen for the specifically requested start level.
- Terrain override support remains available through `terrainFile`.

## Test result
- The staged WASM build loaded and produced a gameplay screenshot for level 3.
- The page rendered successfully after the direct-boot path.

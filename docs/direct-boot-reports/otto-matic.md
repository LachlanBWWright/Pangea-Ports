# Otto Matic direct-boot audit

- Audit session start: `2026-04-02T00:43:04Z`
- Tested URL: `http://127.0.0.1:8765/OttoMatic-Android/OttoMatic.html?level=1`
- Screenshot: `docs/direct-boot-screenshots/otto-matic.png`

## Source review

### Direct-boot inputs
- `games/OttoMatic-Android/src/Boot.cpp`
  - Parses `--level` and `--terrain` into `gDirectLevelNum` and `gTerrainOverridePath`.
  - Builds a terrain override FSSpec after `Pomme::Init()`.
- `games/OttoMatic-Android/src/System/GameMain.c`
  - Uses `gDirectLevelNum` to choose the direct-boot path.
- `games/OttoMatic-Android/src/System/File.c`
  - Uses `GetTerrainOverrideSpec()` when a terrain override is active.

### Skip path
- `GameMain` checks `gDirectLevelNum` before `DoLegalScreen` and the main menu loop.
- If a level is requested, it assigns `gLevelNum`, calls `PlayGame`, and returns without visiting the legal/menu path.

### Initialization coverage
- SDL/Pomme/window/gamepad/bootstrap work is completed before `GameMain` chooses between direct boot and the normal front-end path.
- Terrain override setup happens early enough for level file selection.

## Findings
- Direct boot was already skipping the legal/menu path.
- It still showed `DoLevelIntro()` on the requested level. I changed `games/OttoMatic-Android/src/System/GameMain.c` so the direct-boot target level skips that intro screen.
- Terrain override support remains intact.

## Test result
- The staged WASM build loaded directly into gameplay and produced a screenshot.
- No page errors were captured during the Playwright run.

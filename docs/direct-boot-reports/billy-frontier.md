# Billy Frontier direct-boot audit

- Audit session start: `2026-04-02T00:43:04Z`
- Tested URL: `http://127.0.0.1:8765/BillyFrontier-Android/game/billyfrontier.html?level=1`
- Screenshot: `docs/direct-boot-screenshots/billy-frontier.png`

## Source review

### Direct-boot inputs
- `games/BillyFrontier-Android/Source/Boot.cpp`
  - `ParseCommandLineArgs` accepts `--level` and `--terrain-file` / `--terrain`.
  - `ParseEmscriptenURLParams` accepts both query-string and hash payloads and maps `level`, `terrainFile`, and `terrain` into globals.
- `games/BillyFrontier-Android/Source/System/Main.c`
  - `gDirectLaunchLevel` and `gDirectTerrainPath` drive the direct launch.

### Skip path
- `GameMain` does its warm-up/resource preload first, then checks `gDirectLaunchLevel`.
- When `gDirectLaunchLevel >= 0`, it calls one of `PlayDuel`, `PlayShootout`, `PlayStampede`, or `PlayTargetPractice` and returns before `DoLegalScreen` or the title/menu loop.

### Initialization coverage
- The direct path still runs `DoWarmUpScreen`, `InitTerrainManager`, `InitSkeletonManager`, `InitSoundTools`, and global sprite preloads before gameplay starts.
- Terrain override resolution happens in `games/BillyFrontier-Android/Source/System/LoadLevel.c`, which uses `gDirectTerrainPath` if present.

## Findings
- Direct boot is implemented in source and skips the legal/title/menu flow.
- Warm-up still runs before gameplay; this is preload/setup, not the regular title/menu path.
- I did not change Billy Frontier source during this pass.

## Test result
- The staged WASM build loaded and produced a gameplay screenshot.
- Headless Chromium logged a permissions-related promise rejection during the run, but the game still rendered and the direct level launch completed.

# Cro-Mag Rally direct-boot audit

- Audit session start: `2026-04-02T00:43:04Z`
- Tested URL: `http://127.0.0.1:8765/CroMagRally-Android/game/CroMagRally.html?track=3&car=1`
- Screenshot: `docs/direct-boot-screenshots/cro-mag-rally.png`

## Source review

### Direct-boot inputs
- `games/CroMagRally-Android/Source/Boot.cpp`
  - Parses `--track`, `--car`, `--level-override`, and `--no-fence-collision` into `gCommandLine`.
- `games/CroMagRally-Android/Source/System/Main.c`
  - Uses `gCommandLine.bootToTrack` to decide whether to skip the main flow.
- The built web shell reads `?track=`, `?car=`, `?levelOverride=`, and `?noFenceCollision=` and feeds them into the same command-line structure.

### Skip path
- `GameMain` checks `gCommandLine.bootToTrack` before `DoWarmUpScreen`, `PreloadGameArt`, and the title loop.
- If a track is specified, it forces practice mode, configures the selected car/fence setting, calls `InitArea`, `PlayArea`, and `CleanupLevel`, then continues without having shown the title/menu path first.

### Initialization coverage
- Core toolbox/input/window/render setup happens before the direct track is loaded.
- The direct path initializes player info before area setup.

## Findings
- Direct boot is source-backed and skips the regular title/menu flow.
- Cro-Mag Rally uses tracks instead of level indices, and the web parameter is 1-based.
- I did not change Cro-Mag Rally source during this pass.

## Test result
- The staged WASM build loaded directly into the requested track and produced a screenshot.
- No page errors were captured during the Playwright run.

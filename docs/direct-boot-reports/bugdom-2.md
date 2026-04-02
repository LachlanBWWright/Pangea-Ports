# Bugdom 2 direct-boot audit

- Audit session start: `2026-04-02T00:43:04Z`
- Tested URL: `http://127.0.0.1:8765/Bugdom2-Android/Bugdom2.html?level=3`
- Screenshot: `docs/direct-boot-screenshots/bugdom-2.png`

## Source review

### Direct-boot inputs
- `games/Bugdom2-Android/Source/Boot.cpp`
  - Parses `--level` and, on desktop only, `--level-override-dir`.
  - On Emscripten, reads `?level=` into `gStartLevel`.
- `games/Bugdom2-Android/Source/System/Main.c`
  - `gStartLevel` is checked before legal/title/main-menu flow.

### Skip path
- `GameMain` initializes the core systems, then checks `gStartLevel`.
- When the level is valid, it assigns `gLevelNum`, calls `PlayGame`, and returns before the title/menu path.

### Initialization coverage
- The direct path still runs the normal engine initialization: input, GL, sprite manager, BG3D manager, terrain, skeletons, sound, and object manager.
- This makes the jump into `PlayGame` relatively safe because the level pipeline is entered after the normal startup systems are live.

## Findings
- Direct boot was already skipping legal/title/menu screens.
- It still called `DoLevelIntro()` for the first requested level. I changed `games/Bugdom2-Android/Source/System/Main.c` so the specifically requested direct-boot level bypasses that intro screen.
- Bugdom 2 still relies on replacing the exact terrain file path in the virtual filesystem instead of a dedicated terrain override argument.

## Test result
- The staged WASM build loaded and produced a gameplay screenshot for level 3.
- No page errors were captured during this run.

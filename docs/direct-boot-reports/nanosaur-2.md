# Nanosaur 2 direct-boot audit

- Audit session start: `2026-04-02T00:43:04Z`
- Tested URL: `http://127.0.0.1:8765/Nanosaur2-Android/Nanosaur2.html?level=0`
- Screenshot: `docs/direct-boot-screenshots/nanosaur-2.png`

## Source review

### Direct-boot inputs
- `games/Nanosaur2-Android/Source/Boot.cpp`
  - Parses `--level` and `--terrain-override`.
  - On Emscripten, also reads `?level=` if the argv path did not already set a level.
- `games/Nanosaur2-Android/Source/System/WebCommands.c`
  - Exposes current-level and terrain-override helpers for web integration.
- `games/Nanosaur2-Android/Source/System/Main.c`
  - Uses `gCmdLevelNum` to select the direct-boot path.

### Skip path
- `GameMain` checks `gCmdLevelNum` before the main menu loop.
- If a level is requested, it sets single-player adventure/vs defaults, forces `gSkipLevelIntro = true`, initializes the player state, calls `InitLevel`, `PlayLevel`, and `CleanupLevel`, then returns.
- This skips the legal screen, intro story screen, main menu, and level intro screen.

### Initialization coverage
- The direct path explicitly performs the player-state and level initialization needed before entering gameplay.
- Terrain override conversion happens after `Pomme::Init()` so the FSSpec conversion is valid.

## Findings
- Nanosaur 2 has the cleanest direct-boot implementation of the eight games.
- It already explicitly skips both menu/title flow and level intro flow.
- I did not change Nanosaur 2 source during this pass.

## Test result
- The staged WASM build loaded directly into gameplay and produced a screenshot.
- Console output mostly consisted of runtime logging rather than actual launch failures.

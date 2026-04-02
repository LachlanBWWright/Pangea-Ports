# Mighty Mike direct-boot audit

- Audit session start: `2026-04-02T00:43:04Z`
- Tested URL: `http://127.0.0.1:8765/MightyMike-Android/index.html?level=0:1`
- Screenshot: `docs/direct-boot-screenshots/mighty-mike.png`

## Source review

### Direct-boot inputs
- `games/MightyMike-Android/src/Boot.cpp`
  - Parses `--level <scene>:<area>` and `--map-override`.
  - On Emscripten, also reads `?level=` and `?mapOverride=` into `gStartingScene`, `gStartingArea`, `gSkipMenus`, and `gCustomMapPath`.
- `games/MightyMike-Android/src/Heart/Main.c`
  - Uses `gSkipMenus` to decide whether to bypass the legal/logo/title path.

### Skip path
- When `gSkipMenus` is true, `GameMain` skips `DoLegal`, `DoPangeaLogo`, and `DoTitleScreen`.
- It initializes game state, restores the requested starting scene/area after `InitGame`, and enters `Do1PlayerGame` directly.

### Initialization coverage
- Core bootstrap and SDL/window setup happen before the direct boot.
- The direct path explicitly repairs `gStartingArea` after `InitGame()` resets it, which is important for reliable scene/area booting.
- `gCustomMapPath` is consumed by `games/MightyMike-Android/src/Playfield/Playfield.c` when a map override is active.

## Findings
- Mighty Mike already had a solid direct-boot path and menu skip logic.
- The scene/area restoration after `InitGame()` is the key initialization detail that keeps the direct boot stable.
- I did not change Mighty Mike source during this pass.

## Test result
- The staged WASM build loaded and produced a screenshot for `scene 0 / area 1`.
- No page errors were captured during the Playwright run.

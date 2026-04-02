# Level Editor Integration Plan

This repository now exposes a GitHub Pages launcher flow that is compatible with an external level editor without embedding the repo's pages in an iframe.

## Browser launcher model

- The shared launcher lives in `/docs/index.html`.
- Picking a game no longer boots it immediately.
- Each game now has launcher actions for:
  - starting normally
  - direct-launching known levels/tracks
  - optionally injecting a level file before the WASM module starts
- The launcher passes direct-boot data through `Module.arguments`, so the raw game code receives the same command-line-style inputs used by native builds.

## File injection model

- The launcher reads an uploaded file in JavaScript before the game starts.
- `Module.preRun` writes the file into Emscripten's virtual filesystem before game initialization.
- Games that support an explicit override flag also receive the matching path argument automatically.
- Games that load terrain by filename only, such as Bugdom 2, instead rely on replacing the exact bundled file path in the virtual filesystem.

## Per-game path conventions

- Some games want colon paths like `:Terrain:Custom.ter` or `:Maps:custom.map-1`.
- Others consume direct virtual filesystem paths such as `/Data/Terrain/Custom.ter`.
- The launcher keeps both concepts separate:
  - **Virtual FS target path** = where the file is written
  - **Launch override path** = what gets passed to the game if an override argument is needed

## Resize/fullscreen handling

- The shared launcher canvas uses `ResizeObserver`, viewport/fullscreen listeners, and repeated resize syncs across fullscreen transitions.
- Canvas pixel dimensions are derived from CSS size × device pixel ratio.
- `Module.setCanvasSize` is called when available so SDL/Emscripten stay aligned with the visible canvas.

## Game-specific note

- `games/Nanosaur-android/src/Boot.cpp` no longer forces every web launch to skip directly into gameplay.
- Normal menu boot is now preserved unless a level-editor launch option explicitly requests direct gameplay.

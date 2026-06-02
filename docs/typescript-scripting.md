# TypeScript Scripting

TypeScript scripting is an optional feature for Pangea Ports. Scripts are
authored in TypeScript, compiled to JavaScript before packaging or editor
injection, and loaded by the game through the shared `PangeaScript` host.

The feature is currently wired as an explicit build option:

```sh
cmake -S games/Bugdom2-Android -B games/Bugdom2-Android/build-scripted -DPANGEA_ENABLE_SCRIPTING=ON
cmake --build games/Bugdom2-Android/build-scripted
```

Bugdom 2 is the first integration target. It initializes the scripting host,
loads `Data/Scripts/config/levels.json`, selects any per-level `script` override,
attempts to load `Data/Scripts/dist/main.js` by default, exposes the standard web
status functions, and calls the initial lifecycle and terrain-item hooks.
Bugdom, Nanosaur, Otto Matic, Billy Frontier, Nanosaur 2, Mighty Mike, and
Cro-Mag Rally now use the same optional host for level, area, or race lifecycle,
frame, and item hooks.

The shared host has a no-engine fallback and an optional native Duktape backend.
Duktape is enabled only when CMake can find both `duktape.h` and the matching
library. Without an engine, compiled scripts report a runtime error while missing
scripts/configs remain non-fatal.

## Runtime Files

Bundled script files follow this layout:

```text
Data/Scripts/dist/main.js
Data/Scripts/config/levels.json
Data/Scripts/config/items.json
```

Editor injection should write compiled JavaScript into the Emscripten virtual
filesystem before game boot, then call:

```js
Module.ccall(
  "PangeaScript_SetStartupScript",
  "number",
  ["string"],
  ["Data/Scripts/dist/dev.js"],
);
Module.ccall("PangeaScript_Reload", "number", [], []);
```

## Standard Web Exports

When `PANGEA_ENABLE_SCRIPTING=ON`, WebAssembly builds export:

| Export                           | Purpose                                                        |
| -------------------------------- | -------------------------------------------------------------- |
| `_PangeaScript_IsEnabled`        | Returns whether the scripting host is compiled and initialized |
| `_PangeaScript_SetStartupScript` | Sets the compiled JavaScript startup script path               |
| `_PangeaScript_Reload`           | Reloads the startup script                                     |
| `_PangeaScript_GetLastError`     | Returns the latest scripting error string                      |
| `_PangeaScript_GetErrorCount`    | Returns accumulated non-fatal errors                           |

## Bugdom 2 Hooks

Bugdom 2 currently calls:

| Hook              | Call site                                               |
| ----------------- | ------------------------------------------------------- |
| `onLevelLoad`     | after `InitArea`                                        |
| `onLevelStart`    | immediately before `PlayArea`                           |
| `onFrame`         | once per terrain gameplay frame before `MoveEverything` |
| `onLevelComplete` | when `StartLevelCompletion` first fires                 |
| `onLevelUnload`   | before `CleanupLevel`                                   |
| `onTerrainItem`   | before native terrain item dispatch                     |

Terrain item config remapping is loaded from `levels.json` and applied before the
native terrain item table dispatch.

## Bugdom Hooks

Bugdom currently calls:

| Hook              | Call site                                         |
| ----------------- | ------------------------------------------------- |
| `onLevelLoad`     | after `InitArea`                                  |
| `onLevelStart`    | immediately before gameplay begins                |
| `onFrame`         | once per gameplay frame before object movement    |
| `onLevelComplete` | when `gAreaCompleted` is set during level cleanup |
| `onLevelUnload`   | before `CleanupLevel`                             |
| `onTerrainItem`   | before native terrain item dispatch               |
| `onSplineItem`    | before native spline item dispatch                |

Terrain item config remapping is loaded from `levels.json` before `InitArea` so
initial terrain priming sees script remaps.

## Nanosaur Hooks

Nanosaur currently calls:

| Hook              | Call site                                      |
| ----------------- | ---------------------------------------------- |
| `onLevelLoad`     | after `InitLevel` completes                    |
| `onLevelStart`    | immediately before gameplay begins             |
| `onFrame`         | once per gameplay frame before object movement |
| `onLevelComplete` | when the game ends in the win state            |
| `onLevelUnload`   | before `CleanupLevel`                          |
| `onTerrainItem`   | before native terrain item dispatch            |

Terrain item config remapping is loaded from `levels.json` before level
initialization so initial terrain priming sees script remaps.

## Otto Matic Hooks

Otto Matic currently calls:

| Hook              | Call site                                          |
| ----------------- | -------------------------------------------------- |
| `onLevelLoad`     | after `InitArea`                                   |
| `onLevelStart`    | immediately before gameplay begins                 |
| `onFrame`         | once per gameplay frame before object movement     |
| `onLevelComplete` | when `gLevelCompleted` is set before level cleanup |
| `onLevelUnload`   | before `CleanupLevel`                              |
| `onTerrainItem`   | before native terrain item dispatch                |
| `onSplineItem`    | before native spline item dispatch                 |

Terrain item config remapping is loaded from `levels.json` before `InitArea` so
initial terrain priming sees script remaps.

## Billy Frontier Hooks

Billy Frontier currently calls:

| Hook              | Call site                                           |
| ----------------- | --------------------------------------------------- |
| `onLevelLoad`     | after each mode-specific area init                  |
| `onLevelStart`    | immediately before each mode loop begins            |
| `onFrame`         | once per area frame before mode object movement     |
| `onLevelComplete` | when a mode exits completed and the player is alive |
| `onLevelUnload`   | before mode cleanup                                 |
| `onTerrainItem`   | before native terrain item dispatch                 |
| `onSplineItem`    | before native spline item dispatch                  |

Billy uses area numbers as the `levelNum` in shared script contexts. Terrain item
config remapping is loaded from `levels.json` before each mode-specific area init
so initial terrain priming sees script remaps.

## Nanosaur 2 Hooks

Nanosaur 2 currently calls these hooks for local adventure mode:

| Hook              | Call site                                      |
| ----------------- | ---------------------------------------------- |
| `onLevelLoad`     | after `InitLevel`                              |
| `onLevelStart`    | immediately before gameplay begins             |
| `onFrame`         | once per gameplay frame before object movement |
| `onLevelComplete` | when `gLevelCompleted` is set before cleanup   |
| `onLevelUnload`   | before `CleanupLevel`                          |
| `onTerrainItem`   | before native terrain item dispatch            |
| `onSplineItem`    | before native spline item dispatch             |

Versus/network modes do not initialize the script host in this first adapter.
Terrain item config remapping is loaded from `levels.json` before `InitLevel` so
initial terrain priming sees script remaps.

## Mighty Mike Hooks

Mighty Mike currently calls:

| Hook              | Call site                                                       |
| ----------------- | --------------------------------------------------------------- |
| `onLevelLoad`     | after `InitArea`                                                |
| `onLevelStart`    | immediately before each `PlayArea` loop                         |
| `onFrame`         | once per 2D simulation frame before the area update/render path |
| `onLevelComplete` | when a one-player area is completed                             |
| `onLevelUnload`   | after each `PlayArea` loop exits                                |
| `onMapItem`       | before native 2D map item dispatch                              |

Mighty Mike uses `scene * 3 + area` as the shared `levelNum` and also passes
`sceneNum`/`areaNum` on map-item contexts.

## Cro-Mag Rally Hooks

Cro-Mag Rally currently calls these hooks for local race modes only:

| Hook              | Call site                                        |
| ----------------- | ------------------------------------------------ |
| `onLevelLoad`     | before `InitArea` race setup                     |
| `onLevelStart`    | after `InitArea` finishes camera/player setup    |
| `onFrame`         | once per race frame after frame accounting       |
| `onLevelComplete` | when a local race exits through track completion |
| `onLevelUnload`   | before `CleanupLevel`                            |
| `onTerrainItem`   | before native terrain item dispatch              |

Cro-Mag uses track numbers as the shared `levelNum`. Terrain item contexts also
include `playerNum` and `networked`; the initial adapter only calls scripting
when `networked` is false.

## Sample Scripts

The shared authoring package now includes example scripts under
`shared/script-types/examples/`:

- `bugdom2-main.ts`: minimal Bugdom 2 `onObjectFrame` sample for the tagged player object.
- `ottomatic-humans.ts`: Otto Matic human hover sample using the new live-object hook.
- `mightymike-player-sway.ts`: Mighty Mike player sway sample showing the same hook on the 2D adapter.

These samples only use hooks that are implemented today. They avoid runtime
surfaces that are still stubbed in the native host, such as scripted spawning
or `player.get`, so they are safe starting points for real content.

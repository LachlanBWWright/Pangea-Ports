# Script Config

Script config files live under `Data/Scripts/config`.

`levels.json` is versioned and level keyed:

```json
{
  "version": 1,
  "levels": {
    "0": {
      "script": "Data/Scripts/main.lua",
      "itemOverrides": [
        { "from": 12, "to": 240 }
      ],
      "levelSettings": {
        "gravity": 3900,
        "tileSlipperyFactor": 0,
        "song": "slimeBoss",
        "assetDependencies": [
          { "kind": "skeleton", "id": "moth" },
          { "kind": "modelGroup", "id": "foliage" }
        ]
      }
    }
  }
}
```

The native V1 parser supports:

| Field | Status |
|-------|--------|
| `script` | Per-level Lua module selection |
| `customObjects` | Native/custom visual definitions and collision presets |
| `terrainReplacements`, `mapReplacements`, `splineReplacements` | Index/type matching against declared custom object IDs |
| `itemOverrides` | Terrain item type remapping in adapters with terrain support |
| `levelSettings` | optional per-level primitive settings exposed through typed native accessors |

`levelSettings` accepts string, number, and boolean scalar values. Native game
facades read known keys through typed accessors such as
`PangeaScript_GetLevelFloatSetting` and return their original native fallback
when a key is missing or has the wrong type.

`levelSettings.assetDependencies` is an optional array of `{ "kind", "id" }`
objects. The shared runtime stores these declarations for the active level, and
each game resolves only the IDs it can load safely. Bugdom 2 currently supports:

| Kind | IDs |
|------|-----|
| `skeleton` | `skipExplore`, `skipTunnel`, `skipTitle`, `snail`, `gnome`, `houseFly`, `evilPlant`, `chipmunk`, `snakeHead`, `buddyBug`, `checkpoint`, `flea`, `tick`, `mouseTrap`, `mouse`, `toySoldier`, `otto`, `bumbleBee`, `hoboBag`, `dragonfly`, `frog`, `moth`, `computerBug`, `roach`, `ant`, `fish` |
| `modelGroup` | `global`, `foliage` |

Bugdom 2 deliberately ignores `modelGroup` IDs for other levels for now because
the legacy `MODEL_GROUP_LEVELSPECIFIC` object IDs overlap between levels.

Invalid or missing config is non-fatal. Missing config falls back to native game
behavior.

Editor and packaging tools should validate unknown JSON with
`parseScriptConfigV1` from `@pangea-ports/script-types/config` before writing it
into `Data/Scripts/config`. The parser uses the same versioned V1 shape shown
above and returns a typed `Result` instead of throwing.

The retained TypeScript validator preserves `customObjects` and all three
replacement arrays when parsing configuration. It validates required item/type
indexes, known custom-object references, duplicate IDs, safe custom asset paths,
complete collision bounds, and native capacities (64 custom definitions, 128
replacements per kind). Arbitrary level-setting arrays are rejected: only
`assetDependencies` accepts an array. Numbers must fit native float storage.

`extraNativeItems`, `skipIntro`, and `rules` remain compatibility fields in the
historical TypeScript package; the shared native parser skips them. Put adapter
settings in `levelSettings` and loaded asset declarations in `assetDependencies`.

Bugdom 2, Otto Matic, and Mighty Mike ship a level-zero `Data/Scripts/main.lua`
starter. Its visual bobbing is opt-in: set `levelSettings.scriptBobHeight` to a
positive number; `scriptBobSpeed` defaults to 5. The starter handles only
`onObjectFrame` update events for the native player and applies a visual position
offset, preserving gameplay coordinates. Other games retain valid empty configs.

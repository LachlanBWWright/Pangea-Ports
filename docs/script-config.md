# Script Config

Script config files live under `Data/Scripts/config`.

`levels.json` is versioned and level keyed:

```json
{
  "version": 1,
  "levels": {
    "0": {
      "script": "Data/Scripts/dist/main.lua",
      "extraNativeItems": ["bugdom2.powerup"],
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

V1 config support starts with:

| Field | Status |
|-------|--------|
| `script` | Bugdom 2 per-level startup script selection is wired |
| `extraNativeItems` | native whitelist registration exists; dependency enforcement still pending |
| `itemOverrides` | Bugdom 2 terrain item type remapping is wired |
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

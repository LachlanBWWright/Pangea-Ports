# Script Config

Script config files live under `Data/Scripts/config`.

`levels.json` stays versioned and level keyed:

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

Missing or invalid config remains non-fatal. Missing scripts fall back to the
native game behavior, while runtime errors are reported through the existing
`PangeaScript` status API.

The shared host still supports `itemOverrides`, primitive `levelSettings`, and
`assetDependencies` for all eight games through the same `PangeaScript_*`
accessors.

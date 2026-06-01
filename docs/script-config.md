# Script Config

Script config files live under `Data/Scripts/config`.

`levels.json` is versioned and level keyed:

```json
{
  "version": 1,
  "levels": {
    "0": {
      "script": "Data/Scripts/dist/main.js",
      "extraNativeItems": ["bugdom2.powerup"],
      "itemOverrides": [
        { "from": 12, "to": 240 }
      ]
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

Invalid or missing config is non-fatal. Missing config falls back to native game
behavior.

Editor and packaging tools should validate unknown JSON with
`parseScriptConfigV1` from `@pangea-ports/script-types/config` before writing it
into `Data/Scripts/config`. The parser uses the same versioned V1 shape shown
above and returns a typed `Result` instead of throwing.

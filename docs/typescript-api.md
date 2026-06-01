# TypeScript API

The type package lives at `shared/script-types` and exports the authoring API
shape for game scripts.

```ts
import { pangea, defineTerrainItem } from "@pangea-ports/script-types";

export function onLevelLoad(ctx) {
  pangea.log.info(`Level ${ctx.levelNum}`);
}

export const bouncingHealth = defineTerrainItem({
  id: "custom.bouncingHealth",
  nativeType: 240,
  onSpawn(item) {
    return pangea.spawn.scripted("custom.bouncingPickup", item.position, {
      amount: item.params[0] + 10,
    })
      ? { handled: true, markInUse: true }
      : { handled: false };
  },
});
```

Scripts receive handles and plain data objects. They do not receive raw `ObjNode`,
terrain item, spline item, or map item pointers.

The package defines shared contexts for:

| Type | Purpose |
|------|---------|
| `GameContext` | game id/name |
| `LevelContext` | level number/name |
| `FrameContext` | frame number and timing |
| `TerrainItemContext` | 3D terrain item spawn context |
| `SplineItemContext` | 3D spline item spawn context |
| `MikeMapItemContext` | Mighty Mike 2D map item context |
| `ScriptedObjectDefinition` | script-defined object behavior |

It also exports game-specific hook module types from
`@pangea-ports/script-types/games`, covering the initial scripting surface for
all eight ports. These types model each game's adapter shape before every C
adapter is wired.

Config tooling can validate `Data/Scripts/config/levels.json` with:

```ts
import { parseScriptConfigV1 } from "@pangea-ports/script-types/config";

const result = parseScriptConfigV1(json);
```

The parser returns a `neverthrow` `Result` and validates unknown JSON with Zod
before exposing typed config data.

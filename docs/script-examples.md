# Script Examples

> Legacy compatibility note: these are TypeScript examples only. Production scripts use Lua;
> see `lua-scripting.md` for the supported authoring path.

## Bugdom 2 Level Log

```ts
import { pangea } from "@pangea-ports/script-types";
import type { Bugdom2LevelContext } from "@pangea-ports/script-types/games/bugdom2";

export function onLevelLoad(ctx: Bugdom2LevelContext): void {
  pangea.log.info(`Loaded level ${ctx.levelNum}`);
}
```

## Bugdom 2 Terrain Item

```ts
import { defineTerrainItem, pangea } from "@pangea-ports/script-types";

export const bouncingHealth = defineTerrainItem({
  id: "custom.bouncingHealth",
  nativeType: 240,
  onSpawn(item) {
    const handle = pangea.spawn.scripted("custom.bouncingPickup", item.position, {
      amount: item.params[0] + 10,
    });

    return handle ? { handled: true, markInUse: true } : { handled: false };
  },
});
```

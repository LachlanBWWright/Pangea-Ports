import { defineTerrainItem, pangea } from "../src/pangea";
import type { Bugdom2FrameContext, Bugdom2LevelContext } from "../src/games/bugdom2";

export function onLevelLoad(ctx: Bugdom2LevelContext): void {
  pangea.log.info(`Loading Bugdom 2 level ${ctx.levelNum}`);
}

export function onFrame(ctx: Bugdom2FrameContext): void {
  if (ctx.frameNum % 300 === 0) {
    pangea.log.info(`Level time: ${ctx.levelTimeSeconds.toFixed(2)}`);
  }
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


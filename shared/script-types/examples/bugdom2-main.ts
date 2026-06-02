import type { Bugdom2LifecycleModule } from "../src/games/bugdom2";
import { hasTag, makeVerticalBobOffset } from "./helpers";

const PLAYER_TAG = "bugdom2.player";
const PLAYER_BOB_SPEED = 5;
const PLAYER_BOB_HEIGHT = 18;

export function isBugdom2Player(tags: readonly string[]): boolean {
  return hasTag(tags, PLAYER_TAG);
}

export const onObjectFrame: NonNullable<
  Bugdom2LifecycleModule["onObjectFrame"]
> = (ctx) => {
  if (!isBugdom2Player(ctx.tags)) {
    return;
  }

  return makeVerticalBobOffset(
    ctx.levelTimeSeconds,
    PLAYER_BOB_SPEED,
    PLAYER_BOB_HEIGHT,
  );
};

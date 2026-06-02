import type { MightyMikeLifecycleModule } from "../src/games/mightymike";
import { hasTag, makeHorizontalSwayOffset } from "./helpers";

const PLAYER_TAG = "mightymike.player";
const PLAYER_SWAY_SPEED = 6;
const PLAYER_SWAY_DISTANCE = 6;

export function isMightyMikePlayer(tags: readonly string[]): boolean {
  return hasTag(tags, PLAYER_TAG);
}

export const onObjectFrame: NonNullable<
  MightyMikeLifecycleModule["onObjectFrame"]
> = (ctx) => {
  if (!isMightyMikePlayer(ctx.tags)) {
    return;
  }

  return makeHorizontalSwayOffset(
    ctx.levelTimeSeconds,
    PLAYER_SWAY_SPEED,
    PLAYER_SWAY_DISTANCE,
  );
};

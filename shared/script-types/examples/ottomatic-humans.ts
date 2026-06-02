import type { OttoMaticLifecycleModule } from "../src/games/ottomatic";
import { hasTag, makeVerticalBobOffset } from "./helpers";

const HUMAN_TAG = "ottomatic.human";
const SCIENTIST_TAG = "ottomatic.human.scientist";
const HUMAN_BOB_SPEED = 8;
const DEFAULT_HUMAN_BOB_HEIGHT = 32;
const SCIENTIST_BOB_HEIGHT = 56;

export function getOttoHumanBobHeight(tags: readonly string[]): number {
  if (hasTag(tags, SCIENTIST_TAG)) {
    return SCIENTIST_BOB_HEIGHT;
  }

  return DEFAULT_HUMAN_BOB_HEIGHT;
}

export const onObjectFrame: NonNullable<
  OttoMaticLifecycleModule["onObjectFrame"]
> = (ctx) => {
  if (!hasTag(ctx.tags, HUMAN_TAG)) {
    return;
  }

  return makeVerticalBobOffset(
    ctx.levelTimeSeconds,
    HUMAN_BOB_SPEED,
    getOttoHumanBobHeight(ctx.tags),
  );
};

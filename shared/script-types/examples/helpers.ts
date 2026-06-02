import type { ObjectFrameResult } from "../src/pangea";

export function hasTag(tags: readonly string[], tag: string): boolean {
  return tags.indexOf(tag) >= 0;
}

export function makeVerticalBobOffset(
  levelTimeSeconds: number,
  speed: number,
  amplitude: number,
): ObjectFrameResult {
  return {
    positionOffset: {
      x: 0,
      y: Math.abs(Math.sin(levelTimeSeconds * speed)) * amplitude,
      z: 0,
    },
  };
}

export function makeHorizontalSwayOffset(
  levelTimeSeconds: number,
  speed: number,
  amplitude: number,
): ObjectFrameResult {
  return {
    positionOffset: {
      x: Math.sin(levelTimeSeconds * speed) * amplitude,
      y: 0,
      z: 0,
    },
  };
}

import assert from "node:assert/strict";
import test from "node:test";

import {
  onObjectFrame as bugdom2OnObjectFrame,
  isBugdom2Player,
} from "../examples/bugdom2-main";
import {
  hasTag,
  makeHorizontalSwayOffset,
  makeVerticalBobOffset,
} from "../examples/helpers";
import {
  onObjectFrame as mikeOnObjectFrame,
  isMightyMikePlayer,
} from "../examples/mightymike-player-sway";
import {
  getOttoHumanBobHeight,
  onObjectFrame as ottoOnObjectFrame,
} from "../examples/ottomatic-humans";
import type { ObjectFrameContext } from "../src/pangea";

function makeContext(
  levelTimeSeconds: number,
  tags: readonly string[],
): ObjectFrameContext {
  return {
    gameId: "Bugdom2-Android",
    gameName: "Test Game",
    levelNum: 1,
    frameNum: 60,
    deltaSeconds: 1 / 60,
    levelTimeSeconds,
    object: { id: 1, generation: 1 },
    position: { x: 0, y: 0, z: 0 },
    tags,
  };
}

test("hasTag matches exact tags", () => {
  assert.equal(hasTag(["bugdom2.player", "bonus"], "bugdom2.player"), true);
  assert.equal(hasTag(["bonus"], "bugdom2.player"), false);
});

test("helper offsets stay on the intended axis", () => {
  const vertical = makeVerticalBobOffset(0.25, 5, 18);
  assert.equal(vertical.positionOffset?.x, 0);
  assert.equal(vertical.positionOffset?.z, 0);
  assert.ok((vertical.positionOffset?.y ?? 0) > 0);

  const horizontal = makeHorizontalSwayOffset(0.25, 6, 6);
  assert.equal(horizontal.positionOffset?.y, 0);
  assert.equal(horizontal.positionOffset?.z, 0);
  assert.notEqual(horizontal.positionOffset?.x, 0);
});

test("Bugdom 2 sample only animates the player tag", () => {
  assert.equal(isBugdom2Player(["bugdom2.player"]), true);
  assert.equal(isBugdom2Player(["bugdom2.enemy"]), false);
  assert.equal(
    bugdom2OnObjectFrame(makeContext(0.25, ["bugdom2.enemy"])),
    undefined,
  );

  const result = bugdom2OnObjectFrame(makeContext(0.25, ["bugdom2.player"]));
  assert.ok((result?.positionOffset?.y ?? 0) > 0);
});

test("Otto sample gives scientists a bigger bob", () => {
  assert.equal(getOttoHumanBobHeight(["ottomatic.human"]), 32);
  assert.equal(
    getOttoHumanBobHeight(["ottomatic.human", "ottomatic.human.scientist"]),
    56,
  );
  assert.equal(
    ottoOnObjectFrame(makeContext(0.25, ["ottomatic.player"])),
    undefined,
  );

  const result = ottoOnObjectFrame(
    makeContext(0.25, ["ottomatic.human", "ottomatic.human.scientist"]),
  );
  assert.ok((result?.positionOffset?.y ?? 0) > 0);
});

test("Mighty Mike sample only sways the player", () => {
  assert.equal(isMightyMikePlayer(["mightymike.player"]), true);
  assert.equal(isMightyMikePlayer(["mightymike.enemy"]), false);
  assert.equal(
    mikeOnObjectFrame(makeContext(0.25, ["mightymike.enemy"])),
    undefined,
  );

  const result = mikeOnObjectFrame(makeContext(0.25, ["mightymike.player"]));
  assert.notEqual(result?.positionOffset?.x, 0);
  assert.equal(result?.positionOffset?.y, 0);
});

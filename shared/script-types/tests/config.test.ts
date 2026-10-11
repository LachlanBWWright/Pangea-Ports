import assert from "node:assert/strict";
import test from "node:test";
import { parseScriptConfigV1 } from "../src/config";

function config(level: Record<string, unknown>) {
  return {version: 1, levels: {"0": level}};
}
const customObjects = [{id: "crate", visual: {kind: "customDisplayGroup", modelPath: "Data/Scripts/assets/models/crate.bg3d", modelObject: 0}}];

test("native custom objects and replacement arrays survive config parsing", () => {
  const level = {
    customObjects,
    terrainReplacements: [{customObjectId: "crate", itemIndex: 4, nativeType: 12, x: 100, z: 200}],
    mapReplacements: [{customObjectId: "crate", itemIndex: 1, nativeType: 3, x: 10.5, y: 20}],
    splineReplacements: [{customObjectId: "crate", itemIndex: 5, nativeType: 8, splineNum: 2, placement: .5}],
  };
  const result = parseScriptConfigV1(config(level));
  assert.ok(result.isOk());
  if (result.isOk()) {
    for (const field of Object.keys(level))
      assert.deepEqual(result.value.levels["0"]?.[field as keyof typeof level], level[field as keyof typeof level]);
  }
});

test("invalid references, duplicate declarations and unsafe asset paths fail", () => {
  for (const level of [
    {terrainReplacements: [{customObjectId: "missing"}]},
    {customObjects: [...customObjects, ...customObjects]},
    {customObjects: [{id: "crate", visual: {...customObjects[0]!.visual, modelPath: "Data/Scripts/assets/models/../crate.bg3d"}}]},
    {customObjects: [{id: "crate", collision: {bounds: {width: 1, height: 2}}}]},
  ]) assert.ok(parseScriptConfigV1(config(level)).isErr());
});

test("only the assetDependencies setting accepts an array", () => {
  assert.ok(parseScriptConfigV1(config({levelSettings: {gravity: 3900, assetDependencies: [{kind: "skeleton", id: "moth"}]}})).isOk());
  assert.ok(parseScriptConfigV1(config({levelSettings: {gravity: [{kind: "skeleton", id: "moth"}]}})).isErr());
  assert.ok(parseScriptConfigV1(config({levelSettings: {gravity: 1e39}})).isErr());
});

test("native capacities reject overflow instead of silently losing declarations", () => {
  assert.ok(parseScriptConfigV1(config({script: "a".repeat(260)})).isErr());
  assert.ok(parseScriptConfigV1(config({customObjects: Array.from({length: 65}, (_, i) => ({id: `object${i}`}))})).isErr());
});

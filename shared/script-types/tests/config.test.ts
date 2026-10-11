import assert from "node:assert/strict";
import test from "node:test";
import { readFileSync, readdirSync } from "node:fs";
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

test("dependency identifiers and scalar settings fit native storage at their boundaries", () => {
  const settings = Object.fromEntries(Array.from({length: 64}, (_, i) => [`setting${i}`, i]));
  const dependency = {kind: "k".repeat(31), id: "i".repeat(95)};
  assert.ok(parseScriptConfigV1(config({levelSettings: {...settings, assetDependencies: [dependency]}})).isOk());
  for (const levelSettings of [
    {...settings, overflow: true},
    {assetDependencies: [{...dependency, kind: "k".repeat(32)}]},
    {assetDependencies: [{...dependency, id: "i".repeat(96)}]},
    {assetDependencies: Array.from({length: 65}, () => dependency)},
  ]) assert.ok(parseScriptConfigV1(config({levelSettings})).isErr());
});

test("replacement array capacities accept the last native slot and reject the next", () => {
  for (const field of ["terrainReplacements", "mapReplacements", "splineReplacements"] as const) {
    const replacement = {customObjectId: "crate", itemIndex: 1, nativeType: 2, splineNum: 0};
    assert.ok(parseScriptConfigV1(config({customObjects, [field]: Array.from({length: 128}, () => replacement)})).isOk());
    assert.ok(parseScriptConfigV1(config({customObjects, [field]: Array.from({length: 129}, () => replacement)})).isErr());
  }
});

test("item remaps fit the native signed integer and array capacities", () => {
  const boundary = {from: 2147483647, to: 0};
  assert.ok(parseScriptConfigV1(config({itemOverrides: Array.from({length: 64}, () => boundary)})).isOk());
  for (const itemOverrides of [
    Array.from({length: 65}, () => boundary),
    [{from: 2147483648, to: 0}],
    [{from: 0, to: 2147483648}],
    [{from: -1, to: 0}],
    [{from: 0, to: 0.5}],
  ]) assert.ok(parseScriptConfigV1(config({itemOverrides})).isErr());
});

test("every shipped game config parses without discarding authored fields or mutating its input", () => {
  const games = new URL("../../../games/", import.meta.url);
  const directories = readdirSync(games, {withFileTypes: true}).filter(entry => entry.isDirectory());
  let checked = 0;
  for (const directory of directories) {
    const path = new URL(`${directory.name}/Data/Scripts/config/levels.json`, games);
    let source: string;
    try { source = readFileSync(path, "utf8"); }
    catch (error) {
      if ((error as NodeJS.ErrnoException).code === "ENOENT") continue;
      throw error;
    }
    const original = JSON.parse(source);
    const snapshot = structuredClone(original);
    const result = parseScriptConfigV1(original);
    assert.ok(result.isOk(), `${directory.name}: ${result.isErr() ? result.error.issues.join("; ") : ""}`);
    if (result.isOk()) {
      for (const [number, level] of Object.entries(original.levels as Record<string, Record<string, unknown>>))
        for (const [field, value] of Object.entries(level))
          assert.deepEqual(result.value.levels[number]?.[field as keyof typeof result.value.levels[string]], value, `${directory.name} level ${number}.${field}`);
    }
    assert.deepEqual(original, snapshot);
    checked++;
  }
  assert.equal(checked, 8, "all eight packaged game configurations must be checked");
});

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const declarations = readFileSync(new URL("../src/pangea.ts", import.meta.url), "utf8");
const backend = readFileSync(new URL("../../script/pangea_script_backend_lua.c", import.meta.url), "utf8");

test("declared object API matches Lua registration", () => {
  const declaredBlock = declarations.split("  readonly object: {")[1]!.split("  readonly spawn:")[0]!;
  const declared = [...declaredBlock.matchAll(/^\s+(\w+)\(/gm)].map(match => match[1]!).sort();
  const start = backend.indexOf('set_function(lua, "exists",');
  const registration = backend.slice(start, backend.indexOf('set_function(lua, "setting",', start));
  const actual = [...registration.matchAll(/set_(?:backend_)?function\(lua, "(\w+)"/g)].map(match => match[1]!).sort();
  assert.deepEqual(declared, actual);
});

test("capability declarations contain exactly the runtime table fields", () => {
  const block = declarations.split("export interface PangeaCapabilities {")[1]!.split("\n}")[0]!;
  const declared = [...block.matchAll(/readonly (\w+):/g)].map(match => match[1]!).sort();
  const runtimeBlock = backend.split("static int lua_capabilities(lua_State* lua)")[1]!.split("static int lua_diagnostics")[0]!;
  const actual = [...runtimeBlock.matchAll(/lua_setfield\(lua, -2, "(\w+)"\)/g)].map(match => match[1]!).sort();
  assert.deepEqual(declared, actual);
});

test("diagnostic trace declarations retain all runtime metadata", () => {
  const diagnostics = backend.split("static int lua_diagnostics(lua_State* lua)")[1]!.split("static int lua_spawn_native")[0]!;
  for (const [type, marker] of [
    ["PangeaCommandTraceEntry", "PangeaScriptCommandTraceEntry entry;"],
    ["PangeaLifecycleTraceEntry", "PangeaScriptLifecycleTraceEntry entry;"],
    ["PangeaFailureDiagnostic", "const LuaFailureDiagnostic* diagnostic = &backend->lastError;"],
  ]) {
    const declaredBlock = declarations.split(`export interface ${type} {`)[1]!.split("\n}")[0]!;
    const declared = [...declaredBlock.matchAll(/readonly (\w+)\??:/g)].map(match => match[1]!).sort();
    const runtimeBlock = diagnostics.split(marker!)[1]!.split(type === "PangeaFailureDiagnostic" ? 'lua_setfield(lua, -2, "lastError")' : "lua_rawseti")[0]!;
    const actual = [...runtimeBlock.matchAll(/lua_setfield\(lua, -2, "(\w+)"\)/g)].map(match => match[1]!).sort();
    assert.deepEqual(declared, actual, `${type} must expose every native diagnostic field`);
  }
});

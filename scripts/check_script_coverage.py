#!/usr/bin/env python3
"""Compare contract hooks, native call paths, Lua API registration, and TS declarations.

This is source coverage, not proof that a game scenario ran. The separate per-game
gameplay manifests report exercised native consumers and are labeled accordingly.
"""
import argparse
from collections import defaultdict, deque
from dataclasses import dataclass
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
GAMES = {
    "OTTO_MATIC": ("OttoMatic-Android", "ottomatic"),
    "BUGDOM": ("Bugdom-android", "bugdom"),
    "BUGDOM2": ("Bugdom2-Android", "bugdom2"),
    "NANOSAUR": ("Nanosaur-android", "nanosaur"),
    "NANOSAUR2": ("Nanosaur2-Android", "nanosaur2"),
    "CRO_MAG_RALLY": ("CroMagRally-Android", "cromagrally"),
    "BILLY_FRONTIER": ("BillyFrontier-Android", "billyfrontier"),
    "MIGHTY_MIKE": ("MightyMike-Android", "mightymike"),
}
DISPATCH = {
    "onObjectFrame": "PangeaScript_CallObjectFrame",
    "onTerrainItem": "PangeaScript_CallTerrainItemHook",
    "onSplineItem": "PangeaScript_CallSplineItemHook",
    "onMapItem": "PangeaScript_CallMapItemHook",
    "onTriggerEnter": "PangeaScript_CallTriggerHook",
    "onPickupCollected": "PangeaScript_CallPickupHook",
    "onWeaponHit": "PangeaScript_CallWeaponHitHook",
    "onDamage": "PangeaScript_CallDamageHook",
    "onDamageApplied": "PangeaScript_CallDamageAppliedHook",
}
ENUMS = {
    "GAME_START": "onGameStart", "GAME_SHUTDOWN": "onGameShutdown",
    "LEVEL_LOAD": "onLevelLoad", "LEVEL_START": "onLevelStart",
    "LEVEL_COMPLETE": "onLevelComplete", "LEVEL_UNLOAD": "onLevelUnload",
    "SAVE": "onSave", "LOAD": "onLoad",
}


def strip_comments(text):
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/',
                  lambda m: " " * len(m[0]) if m[0].startswith(("//", "/*")) else m[0], text)


def masked(text):
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: " " * len(m[0]), text)


def block(text, start):
    syntax = masked(text)
    depth = 1
    for index in range(start + 1, len(text)):
        depth += (syntax[index] == "{") - (syntax[index] == "}")
        if depth == 0:
            return text[start + 1:index], index + 1
    raise ValueError("Unbalanced source block")


def contract_hooks(text):
    result = {}
    for match in re.finditer(r"#define PANGEA_SCRIPT_(\w+)_HOOK_LIST\(X\)([^\n]*(?:\n[^#]*?)(?=\n#define|\Z))", text):
        result[match[1]] = set(re.findall(r'X\("(on\w+)"\)', match[2]))
    return result


@dataclass
class Function:
    name: str
    path: str
    body: str
    calls: set


def functions(sources):
    result = []
    for path, original in sources.items():
        text = strip_comments(original)
        # Expand function-like macros for call-graph edges and enum evidence.
        macros = {}
        for match in re.finditer(r"#define\s+(\w+)(?:\([^\n]*?\))?\s+([^\n]*(?:\\\n[^\n]*)*)", text):
            macros[match[1]] = match[2].replace("\\\n", " ")
        end = 0
        for match in re.finditer(r"\b(\w+)\s*\([^;{}]*\)\s*\{", masked(text)):
            if match.start() < end or match[1] in {"if", "for", "while", "switch"}:
                continue
            body, end = block(text, match.end() - 1)
            for name, expansion in macros.items():
                body = re.sub(r"\b" + re.escape(name) + r"\b", lambda _: expansion, body)
            calls = set(re.findall(r"\b(\w+)\s*\(", masked(body)))
            # C callbacks are installed by address in GameInfo and ObjNode fields.
            calls.update(re.findall(r"(?:=|&)\s*(\w+)\s*[,;}]", masked(body)))
            result.append(Function(match[1], path, body, calls))
    return result


def native_hooks(sources, flavor="level"):
    funcs = functions(sources)
    names = defaultdict(list)
    for index, function in enumerate(funcs):
        names[function.name].append(index)
    graph = {}
    for index, function in enumerate(funcs):
        edges = set()
        for call in function.calls:
            candidates = names[call]
            local = [i for i in candidates if funcs[i].path == function.path]
            edges.update(local or candidates)
        graph[index] = edges
    roots = {i for i, f in enumerate(funcs) if "/Scripting/" not in "/" + f.path
             and "Script_Probe" not in f.name}
    reached = set(roots)
    queue = deque(roots)
    while queue:
        for target in graph[queue.popleft()] - reached:
            reached.add(target)
            queue.append(target)

    def reaches_call(index, call):
        seen = {index}
        queue = deque([index])
        while queue:
            current = queue.popleft()
            if call in funcs[current].calls:
                return True
            for target in graph[current] - seen:
                seen.add(target)
                queue.append(target)
        return False

    evidence = defaultdict(list)
    for index in reached:
        f = funcs[index]
        for hook, dispatch in DISPATCH.items():
            if dispatch in f.calls or (hook == "onTriggerEnter" and f.calls & {
                    "PangeaScript_CallObjectTrigger", "PangeaScript_CallObjectTriggerWithOther",
                    "PangeaScript_CallObjectTriggerWithOtherAndPlayer"}):
                evidence[hook].append(f"{f.path}:{f.name}")
        if "PangeaScript_CallFrameHook" in f.calls:
            evidence[{"race": "onRaceFrame", "area": "onAreaFrame"}.get(flavor, "onFrame")].append(f"{f.path}:{f.name}")
        for enum, hook in ENUMS.items():
            if re.search(r"\bPANGEA_SCRIPT_HOOK_" + enum + r"\b", masked(f.body)) and reaches_call(index, "PangeaScript_CallLevelHook"):
                if flavor != "level" and hook.startswith("onLevel"):
                    hook = hook.replace("onLevel", "onRace" if flavor == "race" else "onArea")
                evidence[hook].append(f"{f.path}:{f.name}")
        if "PangeaScript_CallPlayerEvent" in f.calls:
            for hook in re.findall(r'PangeaScript_CallPlayerEvent\([^;]*?"(on\w+)"', f.body):
                evidence[hook].append(f"{f.path}:{f.name}")
        if "PangeaScript_CallNativeSaveHook" in f.calls:
            for value in re.findall(r"PangeaScript_CallNativeSaveHook\([^;]*?\b(true|false)\s*\)", f.body):
                evidence["onLoad" if value == "true" else "onSave"].append(f"{f.path}:{f.name}")
    return dict(evidence)


def ts_hooks(common, game):
    definitions = {}
    for match in re.finditer(r"export type (\w+)[^=]*=([\s\S]*?)(?=\nexport |\Z)", strip_comments(common)):
        definitions[match[1]] = match[2]
    alias = re.findall(r"export type \w+LifecycleModule[^=]*=([\s\S]*)", strip_comments(game))
    if not alias:
        raise ValueError("Missing per-game TS lifecycle declaration")
    text = alias[0]
    hooks = set(re.findall(r"\b(on\w+)\s*\(", text))
    pending = list(re.findall(r"\b\w+LifecycleModule\b", text))
    visited = set()
    while pending:
        name = pending.pop()
        if name in visited:
            continue
        visited.add(name)
        body = definitions.get(name, "")
        hooks.update(re.findall(r"\b(on\w+)\s*\(", body))
        pending.extend(re.findall(r"\b\w+LifecycleModule\b", body))
    hooks -= set(re.findall(r'"(on\w+)"', text))  # Omit keys, not declarations.
    return hooks


def api_surface(backend, types):
    backend, types = strip_comments(backend), strip_comments(types)
    api = {}
    start = types.index("export interface PangeaApi")
    body, _ = block(types, types.index("{", start))
    for match in re.finditer(r"readonly (\w+):\s*\{", body):
        members, _ = block(body, match.end() - 1)
        api[match[1]] = set(re.findall(r"^\s*(\w+)\s*(?:<[^\n]+>)?\(", members, re.M))
        api[match[1]].update(re.findall(r"^\s*readonly\s+(\w+):", members, re.M))
    installed = {}
    start = backend.index("static void install_pangea(")
    body, _ = block(backend, backend.index("{", start))
    pending = set()
    pending_fields = set()
    for line in body.splitlines():
        if line.lstrip().startswith("lua_createtable"):
            pending_fields.clear()
        pending.update(re.findall(r'set_(?:backend_)?function\(lua,\s*"(\w+)"', line))
        fields = re.findall(r'lua_setfield\(lua,\s*-2,\s*"(\w+)"\)', line)
        pending_fields.update(fields)
        if fields and (fields[-1] in api or pending):
            namespace = fields[-1]
            if namespace == "log":
                pending.update(re.findall(r'"(info|warn|error)"', body))
            if namespace in {"api", "game"}:
                pending.update(pending_fields - {namespace})
            installed.setdefault(namespace, set()).update(pending)
            pending.clear()
            pending_fields.clear()
    return installed, api


def check(root=ROOT):
    contract = strip_comments((root / "shared/script/pangea_script_contract.h").read_text())
    backend = strip_comments((root / "shared/script/pangea_script_backend_lua.c").read_text())
    types = (root / "shared/script-types/src/pangea.ts").read_text()
    common = (root / "shared/script-types/src/games/common.ts").read_text()
    advertised = contract_hooks(contract)
    hook_arrays = dict(re.findall(
        r'static const char\* (\w+)\[\]\s*=\s*\{PANGEA_SCRIPT_(\w+)_HOOK_LIST', backend))
    game_arrays = dict(re.findall(
        r'strcmp\(gameId,\s*"([^"]+)"\)\s*==\s*0\)\s*\{\s*hooks\s*=\s*(\w+)', backend))
    errors, report = [], {"games": {}, "scope": "static native dispatch paths; runtime fixtures reported separately"}
    for macro, (game, type_file) in GAMES.items():
        hooks = advertised.get(macro, set())
        if not hooks:
            errors.append(f"{game}: missing hook contract")
        if f"PANGEA_SCRIPT_{macro}_HOOK_LIST(PANGEA_SCRIPT_HOOK_NAME)" not in backend:
            errors.append(f"{game}: backend does not advertise the contract hook macro")
        if hook_arrays.get(game_arrays.get(game, "")) != macro:
            errors.append(f"{game}: backend game-to-hook-array mapping does not match contract")
        sources = {str(p.relative_to(root / "games" / game)): p.read_text()
                   for folder in ("src", "Source") for p in (root / "games" / game / folder).rglob("*")
                   if p.suffix in {".c", ".cpp", ".cc"}}
        flavor = "race" if "onRaceLoad" in hooks else "area" if "onAreaLoad" in hooks else "level"
        native = native_hooks(sources, flavor)
        declared = ts_hooks(common, (root / f"shared/script-types/src/games/{type_file}.ts").read_text())
        for hook in sorted(hooks - native.keys()):
            errors.append(f"{game}: advertised {hook} has no reachable native dispatch")
        for hook in sorted(native.keys() - hooks):
            errors.append(f"{game}: native {hook} is absent from advertised contract")
        for hook in sorted(hooks ^ declared):
            errors.append(f"{game}: TS hook mismatch {hook} ({'missing' if hook in hooks else 'phantom'})")
        manifest_path = root / "games" / game / "tests/script_gameplay_coverage.json"
        exercised = json.loads(manifest_path.read_text()) if manifest_path.exists() else {"hooks": []}
        for hook in set(exercised.get("hooks", [])) - hooks:
            errors.append(f"{game}: native gameplay fixture claims unsupported {hook}")
        if manifest_path.exists():
            test_path = root / "games" / game / "tests/native_gameplay_test.c"
            fixture = test_path.read_text() if test_path.exists() else ""
            production_functions = functions(sources)
            fixture_functions = functions({str(test_path): fixture})
            fixture_names = {function.name for function in fixture_functions}
            # Definitions, declarations and unreachable fixture helpers are not
            # evidence that a production consumer was actually invoked.
            invoked = {"main"} if "main" in fixture_names else set()
            callable_functions = fixture_functions + [function for function in production_functions
                                                       if function.name not in fixture_names]
            while True:
                expanded = invoked | {call for function in callable_functions
                                      if function.name in invoked for call in function.calls}
                if expanded == invoked:
                    break
                invoked = expanded
            production_names = {function.name for function in production_functions}
            for call in exercised.get("native_calls", []):
                if call not in production_names or call not in invoked or call in fixture_names:
                    errors.append(f"{game}: runtime fixture native call {call} lacks production definition or test invocation")
            for source in exercised.get("consumer_sources", []):
                if source not in sources:
                    errors.append(f"{game}: runtime fixture names missing consumer source {source}")
        report["games"][game] = {"advertised": sorted(hooks), "native_dispatch": native,
                                  "typescript": sorted(declared), "runtime_fixture": exercised}
    installed, declared = api_surface(backend, types)
    for namespace in sorted(installed.keys() | declared.keys()):
        for method in sorted(installed.get(namespace, set()) ^ declared.get(namespace, set())):
            errors.append(f"pangea.{namespace}.{method}: Lua/TypeScript API mismatch")
    report["errors"] = errors
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    report = check(args.root)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + "\n")
    for error in report["errors"]:
        print(error)
    print(f"8 game hook contracts and Lua/TypeScript API: {len(report['errors'])} mismatches")
    return bool(report["errors"])


if __name__ == "__main__":
    raise SystemExit(main())

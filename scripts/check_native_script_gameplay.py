#!/usr/bin/env python3
"""Execute real native gameplay consumers with Lua; report exercised hook manifests."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from check_native_script_adapters import ROOT, GAMES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=shutil.which("cc") or "cc")
    parser.add_argument("--script-build", required=True, type=Path,
                        help="Fresh CMake Lua build directory containing both static libraries")
    parser.add_argument("--sysroot-include", type=Path)
    parser.add_argument("--sdl-lib-dir", type=Path)
    parser.add_argument("--game", choices=GAMES, action="append")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    candidates = [Path("/usr/include"), Path("/usr/local/include"),
                  Path("/workspace/pangea-tools/sysroot/usr/include")]
    include = args.sysroot_include or next((p for p in candidates if (p / "SDL3/SDL.h").is_file()), None)
    if include is None:
        parser.error("SDL3 headers required; use --sysroot-include")
    libdir = args.sdl_lib_dir
    if libdir is None:
        candidates = [Path("/usr/lib/x86_64-linux-gnu"), Path("/usr/local/lib"),
                      Path("/workspace/pangea-tools/sysroot/usr/lib/x86_64-linux-gnu")]
        libdir = next((p for p in candidates if (p / "libSDL3.so").is_file()), None)
    if libdir is None:
        parser.error("SDL3 runtime library required; use --sdl-lib-dir")
    script_lib = args.script_build.resolve()
    if not (script_lib / "libPangeaScript.a").is_file():
        script_lib /= "PangeaScript"
    for name in ("libPangeaScript.a", "libPangeaLua.a"):
        if not (script_lib / name).is_file():
            parser.error(f"Missing {script_lib / name}; build Lua tests first")
    results = {}
    games = args.game or GAMES
    with tempfile.TemporaryDirectory(prefix="pangea-native-gameplay-") as directory:
        for name in games:
            game = ROOT / "games" / name
            test = game / "tests/native_gameplay_test.c"
            manifest = game / "tests/script_gameplay_coverage.json"
            if not test.is_file() or not manifest.is_file():
                print(f"FAIL {name}: missing native fixture or coverage manifest")
                results[name] = {"passed": False}
                continue
            coverage = json.loads(manifest.read_text())
            source = game / ("Source" if (game / "Source").is_dir() else "src")
            pomme = game / "extern/Pomme/src"
            work = Path(directory) / name
            work.mkdir()
            binary = work / "native-gameplay"
            command = [args.compiler, "-std=gnu11", "-Wno-multichar", "-DPANGEA_ENABLE_SCRIPTING=1",
                       "-ffunction-sections", "-fdata-sections", "-UNDEBUG"]
            for path in (include, source / "Headers", source / "Scripting", pomme, pomme / "QD3D",
                         ROOT / "shared/script", ROOT / "shared/metadata", ROOT / "shared/render"):
                command.extend(["-I", str(path)])
            consumers = [game / file for file in coverage.get("native_sources", coverage.get("consumer_sources", []))
                         if f'"../{file}"' not in test.read_text()]
            command.extend([str(test), *map(str, consumers), "-Wl,--gc-sections",
                            str(script_lib / "libPangeaScript.a"), str(script_lib / "libPangeaLua.a"),
                            "-L", str(libdir), "-lSDL3", f"-Wl,-rpath-link,{libdir}", "-lm", "-o", str(binary)])
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f"FAIL {name}: compile\n{build.stderr}")
                results[name] = {"passed": False}
                continue
            lua = game / "tests/native_gameplay.lua"
            if lua.is_file():
                destination = work / "Data/Scripts/dist"
                destination.mkdir(parents=True)
                shutil.copy(lua, destination / lua.name)
            env = dict(os.environ)
            env["LD_LIBRARY_PATH"] = str(libdir) + (":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else "")
            run = subprocess.run([str(binary)], cwd=work, env=env, capture_output=True, text=True, timeout=20)
            passed = run.returncode == 0
            results[name] = {"passed": passed, "runtime_fixture": coverage}
            print(f"{'PASS' if passed else 'FAIL'} {name}: actual native consumers -> Lua ({', '.join(coverage['hooks'])})")
            if not passed:
                print(run.stdout + run.stderr)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps({"scope": "executed native gameplay fixtures", "games": results}, indent=2) + "\n")
    count = sum(result["passed"] for result in results.values())
    print(f"{count}/{len(games)} real native gameplay/Lua tests passed")
    return count != len(games)


if __name__ == "__main__":
    raise SystemExit(main())

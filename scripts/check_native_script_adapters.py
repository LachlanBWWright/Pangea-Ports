#!/usr/bin/env python3
"""Compile and execute real game adapter regression tests without launching games."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GAMES = (
    "OttoMatic-Android", "Bugdom-android", "Bugdom2-Android", "CroMagRally-Android",
    "Nanosaur-android", "Nanosaur2-Android", "BillyFrontier-Android", "MightyMike-Android",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=shutil.which("cc") or "cc")
    parser.add_argument("--sysroot-include", type=Path)
    parser.add_argument("--game", choices=GAMES, action="append")
    args = parser.parse_args()
    includes = [Path("/usr/include"), Path("/usr/local/include"),
                Path("/workspace/pangea-tools/sysroot/usr/include")]
    sdl = args.sysroot_include or next((p for p in includes if (p / "SDL3/SDL.h").is_file()), None)
    if sdl is None:
        parser.error("SDL3 headers are required; pass --sysroot-include")
    passed = 0
    games = args.game or GAMES
    with tempfile.TemporaryDirectory(prefix="pangea-native-adapters-") as temporary:
        for game_name in games:
            game = ROOT / "games" / game_name
            test = game / "tests/script_adapter_test.c"
            if not test.is_file():
                print(f"FAIL {game_name}: missing adapter regression test")
                continue
            source = game / ("Source" if (game / "Source").is_dir() else "src")
            pomme = game / "extern/Pomme/src"
            binary = Path(temporary) / game_name
            command = [args.compiler, "-std=gnu11", "-Wno-multichar",
                       "-ffunction-sections", "-fdata-sections", "-UNDEBUG", "-DPANGEA_ENABLE_SCRIPTING=1"]
            for include in (sdl, source / "Headers", source / "Scripting", pomme, pomme / "QD3D",
                            ROOT / "shared/script", ROOT / "shared/metadata", ROOT / "shared/render"):
                command.extend(["-I", str(include)])
            command.extend([str(test), "-Wl,--gc-sections", "-lm", "-o", str(binary)])
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f"FAIL {game_name}: compile\n{build.stderr}")
                continue
            run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
            if run.returncode:
                print(f"FAIL {game_name}: regression\n{run.stdout}{run.stderr}")
                continue
            print(f"PASS {game_name}: {run.stdout.strip()}")
            passed += 1
    print(f"{passed}/{len(games)} real native scripting adapters passed")
    return passed != len(games)


if __name__ == "__main__":
    raise SystemExit(main())

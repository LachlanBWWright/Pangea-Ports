#!/usr/bin/env python3
"""Syntax-check Android input/render paths with locally installed SDL/GLES headers.

This host compiler check complements APK builds; it does not require the Android SDK.
Pass --sysroot-include when SDL3/GLES3 headers are outside the compiler search path.
"""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sysroot-include", type=Path)
    parser.add_argument("--compiler", default=shutil.which("cc") or "cc")
    args = parser.parse_args()
    failed = []
    checked = 0
    for game in sorted((ROOT / "games").iterdir()):
        source = game / ("Source" if (game / "Source").is_dir() else "src")
        pomme = game / "extern/Pomme/src"
        files = [file for pattern in (
            "System/Input.c", "Heart/Input.c", "3D/OGL_Support.c",
            "QD3D/QD3D_Support.c", "Drivers/SDLRender.c",
            "Player/Player_Control.c", "Player/Player_Terrain.c", "Player/RideBall.c", "System/SDLMaintenance.c",
        ) for file in source.glob(pattern)]
        for file in files:
            command = [args.compiler, "-std=c11", "-fsyntax-only", "-Wno-multichar", "-D__ANDROID__"]
            includes = [source / "Headers", pomme, pomme / "QD3D",
                        ROOT / "shared/metadata", ROOT / "shared/render", ROOT / "shared/android"]
            if args.sysroot_include:
                includes.insert(0, args.sysroot_include)
            for include in includes:
                command.extend(["-I", str(include)])
            result = subprocess.run(command + [str(file)], capture_output=True, text=True)
            checked += 1
            if result.returncode:
                failed.append(file)
                print(result.stderr)
                print(f"FAIL {file.relative_to(ROOT)}")
            else:
                print(f"PASS {file.relative_to(ROOT)}")
    command = [args.compiler, "-std=c11", "-fsyntax-only", "-D__ANDROID__"]
    if args.sysroot_include:
        command.extend(["-I", str(args.sysroot_include)])
    draw = ROOT / "shared/android/TouchControlsDraw.c"
    result = subprocess.run(command + [str(draw)], capture_output=True, text=True)
    checked += 1
    if result.returncode:
        failed.append(draw)
        print(result.stderr)
    print(f"{'FAIL' if result.returncode else 'PASS'} {draw.relative_to(ROOT)}")
    print(f"{checked - len(failed)}/{checked} Android input/render files passed")
    return bool(failed)


if __name__ == "__main__":
    raise SystemExit(main())

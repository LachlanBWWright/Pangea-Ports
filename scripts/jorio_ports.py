#!/usr/bin/env python3
"""
scripts/jorio_ports.py — Alternative ports metadata and task runner.

Mirrors scripts/ports.py but for the jorio-ports/ alternative WASM builds
that use Emscripten's LEGACY_GL_EMULATION fixed-function translation layer.

The key difference from scripts/ports.py:
  - Sources are cloned fresh from jorio's GitHub repos (not stored in games/)
  - WASM builds use -sLEGACY_GL_EMULATION=1 instead of -sFULL_ES2=1
  - These builds produce HTML/JS/WASM output in jorio-ports/<Game>/build-wasm/
  - No Android APK build (WASM only)

The approach is inspired by https://github.com/LostMyCode/d3d9-webgl:
  a fixed-function graphics API translation layer targeting WebGL.
  For OpenGL-based games the equivalent is Emscripten's built-in
  LEGACY_GL_EMULATION, so we use that instead.

Usage:
    python3 scripts/jorio_ports.py matrix wasm
    python3 scripts/jorio_ports.py run --game Bugdom --task wasm-build
    python3 scripts/jorio_ports.py run --game Bugdom --task stage-wasm --dest /tmp/site/Bugdom
    python3 scripts/jorio_ports.py list
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JORIO_PORTS_DIR = ROOT / "jorio-ports"

DOC_ASSET_SUFFIXES = {".css", ".png", ".webp", ".jpg", ".jpeg", ".svg", ".ico", ".gif"}

# ---------------------------------------------------------------------------
# Port metadata
# ---------------------------------------------------------------------------
# Each entry mirrors the structure in scripts/ports.py.
# 'path' is relative to repo root.
# 'wasm_build' is the command to run from the game_dir to produce outputs.
# 'wasm_outputs' are relative to the game_dir.

PORTS = [
    {
        "name": "Bugdom",
        "display_name": "Bugdom",
        "jorio_repo": "https://github.com/jorio/Bugdom.git",
        "path": "jorio-ports/Bugdom",
        "wasm_build": ["python3", "build_wasm.py"],
        "wasm_outputs": [
            "build-wasm/Bugdom.html",
            "build-wasm/Bugdom.js",
            "build-wasm/Bugdom.wasm",
            "build-wasm/Bugdom.data",
        ],
        "wasm_entrypoint": "Bugdom.html",
        "wasm_stage_subdir": "",
        "site_launch_path": "Bugdom.html",
        "site_level_example": "Bugdom.html?level=3",
    },
    {
        "name": "Bugdom2",
        "display_name": "Bugdom 2",
        "jorio_repo": "https://github.com/jorio/Bugdom2.git",
        "path": "jorio-ports/Bugdom2",
        "wasm_build": ["python3", "build_wasm.py"],
        "wasm_outputs": [
            "build-wasm/Bugdom2.html",
            "build-wasm/Bugdom2.js",
            "build-wasm/Bugdom2.wasm",
            "build-wasm/Bugdom2.data",
        ],
        "wasm_entrypoint": "Bugdom2.html",
        "wasm_stage_subdir": "",
        "site_launch_path": "Bugdom2.html",
        "site_level_example": "Bugdom2.html?level=3",
    },
    {
        "name": "Nanosaur",
        "display_name": "Nanosaur",
        "jorio_repo": "https://github.com/jorio/Nanosaur.git",
        "path": "jorio-ports/Nanosaur",
        "wasm_build": ["python3", "build_wasm.py"],
        "wasm_outputs": [
            "build-wasm/Nanosaur.html",
            "build-wasm/Nanosaur.js",
            "build-wasm/Nanosaur.wasm",
            "build-wasm/Nanosaur.data",
        ],
        "wasm_entrypoint": "Nanosaur.html",
        "wasm_stage_subdir": "",
        "site_launch_path": "Nanosaur.html",
        "site_level_example": "Nanosaur.html?level=0&skipMenu=1",
    },
    {
        "name": "Nanosaur2",
        "display_name": "Nanosaur 2",
        "jorio_repo": "https://github.com/jorio/Nanosaur2.git",
        "path": "jorio-ports/Nanosaur2",
        "wasm_build": ["python3", "build_wasm.py"],
        "wasm_outputs": [
            "build-wasm/Nanosaur2.html",
            "build-wasm/Nanosaur2.js",
            "build-wasm/Nanosaur2.wasm",
            "build-wasm/Nanosaur2.data",
        ],
        "wasm_entrypoint": "Nanosaur2.html",
        "wasm_stage_subdir": "",
        "site_launch_path": "Nanosaur2.html",
        "site_level_example": "Nanosaur2.html?level=0",
    },
    {
        "name": "OttoMatic",
        "display_name": "Otto Matic",
        "jorio_repo": "https://github.com/jorio/OttoMatic.git",
        "path": "jorio-ports/OttoMatic",
        "wasm_build": ["python3", "build_wasm.py"],
        "wasm_outputs": [
            "build-wasm/OttoMatic.html",
            "build-wasm/OttoMatic.js",
            "build-wasm/OttoMatic.wasm",
            "build-wasm/OttoMatic.data",
        ],
        "wasm_entrypoint": "OttoMatic.html",
        "wasm_stage_subdir": "",
        "site_launch_path": "OttoMatic.html",
        "site_level_example": "OttoMatic.html?level=1",
    },
    {
        "name": "BillyFrontier",
        "display_name": "Billy Frontier",
        "jorio_repo": "https://github.com/jorio/BillyFrontier.git",
        "path": "jorio-ports/BillyFrontier",
        "wasm_build": ["python3", "build_wasm.py"],
        "wasm_outputs": [
            "build-wasm/BillyFrontier.html",
            "build-wasm/BillyFrontier.js",
            "build-wasm/BillyFrontier.wasm",
            "build-wasm/BillyFrontier.data",
        ],
        "wasm_entrypoint": "BillyFrontier.html",
        "wasm_stage_subdir": "",
        "site_launch_path": "BillyFrontier.html",
        "site_level_example": "BillyFrontier.html?level=1",
    },
    {
        "name": "CroMagRally",
        "display_name": "Cro-Mag Rally",
        "jorio_repo": "https://github.com/jorio/CroMagRally.git",
        "path": "jorio-ports/CroMagRally",
        "wasm_build": ["python3", "build_wasm.py"],
        "wasm_outputs": [
            "build-wasm/CroMagRally.html",
            "build-wasm/CroMagRally.js",
            "build-wasm/CroMagRally.wasm",
            "build-wasm/CroMagRally.data",
        ],
        "wasm_entrypoint": "CroMagRally.html",
        "wasm_stage_subdir": "",
        "site_launch_path": "CroMagRally.html",
        "site_level_example": "CroMagRally.html?track=2&car=1",
    },
    {
        "name": "MightyMike",
        "display_name": "Mighty Mike",
        "jorio_repo": "https://github.com/jorio/MightyMike.git",
        "path": "jorio-ports/MightyMike",
        "wasm_build": ["python3", "build_wasm.py"],
        "wasm_outputs": [
            "build-wasm/MightyMike.html",
            "build-wasm/MightyMike.js",
            "build-wasm/MightyMike.wasm",
            "build-wasm/MightyMike.data",
        ],
        "wasm_entrypoint": "MightyMike.html",
        "wasm_stage_subdir": "",
        "site_launch_path": "MightyMike.html",
        "site_level_example": "MightyMike.html?level=1:1",
    },
]

PORTS_BY_NAME: dict[str, dict] = {p["name"]: p for p in PORTS}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _matrix_for(kind: str) -> dict:
    if kind != "wasm":
        raise SystemExit(f"Unsupported matrix kind for jorio_ports: {kind!r} (only 'wasm' is valid)")
    return {"include": [{"name": p["name"], "path": p["path"]} for p in PORTS]}


def _run_command(command: list[str], cwd: Path) -> None:
    print(f"+ ({cwd}) {' '.join(command)}")
    subprocess.run(command, cwd=cwd, check=True)


def _stage_wasm(port: dict, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    game_root = ROOT / port["path"]
    stage_subdir = port.get("wasm_stage_subdir", "")
    wasm_dest = dest / stage_subdir if stage_subdir else dest

    for relative_path in port["wasm_outputs"]:
        source = game_root / relative_path
        if source.exists():
            wasm_dest.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, wasm_dest / source.name)

    # Copy any doc assets from the jorio repo docs/ directory if present
    src_docs = game_root / "src" / "docs"
    if src_docs.exists():
        for path in src_docs.iterdir():
            if path.is_file() and path.suffix.lower() in DOC_ASSET_SUFFIXES:
                shutil.copy2(path, dest / path.name)


def _run_task(port: dict, task: str, dest: str | None) -> None:
    cwd = ROOT / port["path"]
    if task == "wasm-build":
        _run_command(port["wasm_build"], cwd)
        return
    if task == "stage-wasm":
        if not dest:
            raise SystemExit("--dest is required for stage-wasm")
        _stage_wasm(port, Path(dest))
        return
    raise SystemExit(f"Unsupported task: {task!r}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Metadata and task runner for jorio alternative WASM ports."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    matrix_parser = subparsers.add_parser("matrix", help="Emit a GitHub Actions matrix JSON document.")
    matrix_parser.add_argument("kind", choices=["wasm"])

    run_parser = subparsers.add_parser("run", help="Run a named task for a specific port.")
    run_parser.add_argument("--game", required=True, choices=sorted(PORTS_BY_NAME))
    run_parser.add_argument("--task", required=True, choices=["wasm-build", "stage-wasm"])
    run_parser.add_argument("--dest")

    subparsers.add_parser("list", help="List jorio ports.")

    args = parser.parse_args()

    if args.command == "matrix":
        print(json.dumps(_matrix_for(args.kind), separators=(",", ":")))
        return 0

    if args.command == "list":
        for port in PORTS:
            print(f"{port['name']}\t{port['path']}\t{port['jorio_repo']}")
        return 0

    port = PORTS_BY_NAME[args.game]
    _run_task(port, args.task, getattr(args, "dest", None))
    return 0


if __name__ == "__main__":
    sys.exit(main())

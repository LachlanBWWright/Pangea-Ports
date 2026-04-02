#!/usr/bin/env python3
"""
jorio-ports/BillyFrontier/build_wasm.py
Build the Billy Frontier WASM port from jorio's upstream source using
Emscripten's LEGACY_GL_EMULATION fixed-function translation layer.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "shared"))
import build_common  # noqa: E402

if __name__ == "__main__":
    build_common.build_game({
        "name": "BillyFrontier",
        "full_name": "Billy Frontier",
        "repo_url": "https://github.com/jorio/BillyFrontier.git",
        "game_dir": Path(__file__).resolve().parent,
        "exported_functions": ["main"],
    })

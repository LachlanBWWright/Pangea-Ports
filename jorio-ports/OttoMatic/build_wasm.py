#!/usr/bin/env python3
"""
jorio-ports/OttoMatic/build_wasm.py
Build the Otto Matic WASM port from jorio's upstream source using
Emscripten's LEGACY_GL_EMULATION fixed-function translation layer.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "shared"))
import build_common  # noqa: E402

if __name__ == "__main__":
    build_common.build_game({
        "name": "OttoMatic",
        "full_name": "Otto Matic",
        "repo_url": "https://github.com/jorio/OttoMatic.git",
        "game_dir": Path(__file__).resolve().parent,
        "exported_functions": ["main", "OttoMatic_SetFenceCollisions", "OttoMatic_SetTerrainPath"],
    })

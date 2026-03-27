#!/usr/bin/env python3
"""
jorio-ports/Bugdom/build_wasm.py
Build the Bugdom WASM port from jorio's upstream source using
Emscripten's LEGACY_GL_EMULATION fixed-function translation layer.

Usage:
    python3 build_wasm.py              # full build (clone + deps + configure + build)
    python3 build_wasm.py --help       # show options

See jorio-ports/shared/build_common.py for implementation details.
"""
import sys
from pathlib import Path

# Allow importing shared build_common without installing it
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "shared"))
import build_common  # noqa: E402

if __name__ == "__main__":
    build_common.build_game({
        "name": "Bugdom",
        "full_name": "Bugdom",
        "repo_url": "https://github.com/jorio/Bugdom.git",
        "game_dir": Path(__file__).resolve().parent,
        "exported_functions": ["main"],
    })

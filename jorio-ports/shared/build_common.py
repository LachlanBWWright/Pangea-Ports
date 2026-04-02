#!/usr/bin/env python3
"""
build_common.py — Shared build logic for jorio-ports WASM builds.

Each per-game build_wasm.py imports this module and calls `build_game(cfg)`.

The build process:
  1. Clone the upstream jorio repo into `src/` (skipped if already present).
  2. Download SDL3 source into a shared build cache (skipped if already present).
  3. Patch the upstream CMakeLists.txt to:
     a. Skip `find_package(OpenGL REQUIRED)` when building for Emscripten
        (OpenGL is provided by -sLEGACY_GL_EMULATION at link time, not as a
        system library).
     b. Append an `if(EMSCRIPTEN)` block that adds ASYNCIFY + LEGACY_GL_EMULATION
        link options, the HTML output suffix, and the game data preload.
  4. Configure with `emcmake cmake`.
  5. Build with `cmake --build`.
  6. Copy the HTML/JS/WASM/data output into `build-wasm/`.

The key Emscripten flag:
  -sLEGACY_GL_EMULATION=1
    Translates OpenGL 1.x fixed-function pipeline calls (glBegin/glEnd,
    glEnable(GL_LIGHTING), glFogf, glColor4f, glNormal3f, etc.) to WebGL
    shader calls at runtime.  This is conceptually the same approach as
    https://github.com/LostMyCode/d3d9-webgl — a fixed-function translation
    layer targeting WebGL — but for OpenGL rather than Direct3D 9.
"""
from __future__ import annotations

import contextlib
import hashlib
import multiprocessing
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import urllib.request
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

SDL_VER = "3.2.4"
SDL_URL = f"https://libsdl.org/release/SDL3-{SDL_VER}.tar.gz"
SDL_SHA256 = "2938328317301dfbe30176d79c251733aa5e7ec5c436c800b99ed4da7adcb0f0"

NPROC = multiprocessing.cpu_count()

# Shared cache dir for SDL source (same as existing games' build scripts)
CACHE_DIR = Path(tempfile.gettempdir()) / "pangea-games-build-cache"

# Path to the shared shell.html (relative to this file)
SHARED_DIR = Path(__file__).resolve().parent
SHELL_HTML = SHARED_DIR / "shell.html"

# ---------------------------------------------------------------------------
# CMake patches
# ---------------------------------------------------------------------------

# The jorio CMakeLists.txt calls `find_package(OpenGL REQUIRED)` and then links
# `OpenGL::GL`.  Under Emscripten, OpenGL is provided by the runtime flag
# `-sLEGACY_GL_EMULATION=1`, NOT as a host system library.  We patch both calls.

_OPENGL_FIND_ORIGINAL = "find_package(OpenGL REQUIRED)"
_OPENGL_FIND_PATCHED = textwrap.dedent("""\
    if(NOT EMSCRIPTEN)
        find_package(OpenGL REQUIRED)
    endif()
""")

_OPENGL_LINK_ORIGINAL = "target_link_libraries(${GAME_TARGET} PRIVATE Pomme OpenGL::GL)"
_OPENGL_LINK_PATCHED = textwrap.dedent("""\
    if(EMSCRIPTEN)
        target_link_libraries(${GAME_TARGET} PRIVATE Pomme)
    else()
        target_link_libraries(${GAME_TARGET} PRIVATE Pomme OpenGL::GL)
    endif()
""")

# Emscripten block appended at the end of CMakeLists.txt.
# NOTE: The shell.html path is injected by build_game() at patch time.
_EMSCRIPTEN_BLOCK_TEMPLATE = textwrap.dedent("""\

    # ---------------------------------------------------------------------------
    # Emscripten / WebAssembly support
    # (appended by jorio-ports/shared/build_common.py — do not edit manually)
    # ---------------------------------------------------------------------------
    if(EMSCRIPTEN)
        # Output a self-contained HTML page
        set_target_properties(${{GAME_TARGET}} PROPERTIES SUFFIX ".html")

        # Emscripten link flags for the LEGACY_GL_EMULATION WASM build:
        #
        #   ASYNCIFY              - Allows blocking C loops to yield to the browser
        #                          event loop without restructuring the code.
        #   ASYNCIFY_STACK_SIZE   - Enlarged stack (default 4096 is too small for
        #                          games that use deep call stacks during level load).
        #   LEGACY_GL_EMULATION   - Translates all OpenGL 1.x fixed-function calls
        #                          (glBegin/glEnd, glEnable(GL_LIGHTING), glFogf,
        #                          glColor4f, glNormal3f, matrix stack operations,
        #                          etc.) to WebGL shader calls at runtime.
        #                          This is the core "fixed-function translation
        #                          layer" approach; analogous to d3d9-webgl but for
        #                          OpenGL.
        #   GL_UNSAFE_OPTS        - Disable unsafe GL optimizations for correctness.
        #   DISABLE_EXCEPTION_CATCHING - Use JS-based C++ exceptions (compatible
        #                          with ASYNCIFY, unlike -fwasm-exceptions).
        #   ALLOW_MEMORY_GROWTH   - Let the WASM heap grow as needed.
        #   INITIAL_MEMORY        - 256 MB starting heap.
        #   preload-file          - Embed the Data/ directory into the WASM virtual
        #                          filesystem so the game can find its assets.
        #   shell-file            - Custom HTML shell (jorio-ports/shared/shell.html).

        target_link_options(${{GAME_TARGET}} PRIVATE
            "SHELL:-sASYNCIFY=1"
            "SHELL:-sASYNCIFY_STACK_SIZE=65536"
            "SHELL:-sDISABLE_EXCEPTION_CATCHING=0"
            "SHELL:-sLEGACY_GL_EMULATION=1"
            "SHELL:-sGL_UNSAFE_OPTS=0"
            "SHELL:-sGL_DEBUG=0"
            "SHELL:-sALLOW_MEMORY_GROWTH=1"
            "SHELL:-sINITIAL_MEMORY=268435456"
            "SHELL:--preload-file ${{CMAKE_SOURCE_DIR}}/Data@/Data"
            "SHELL:-sEXPORTED_FUNCTIONS=[{exported_functions}]"
            "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
            "SHELL:--shell-file {shell_html}"
        )

        # Enable C++ exceptions via ASYNCIFY-compatible JS-based mechanism
        if(NOT MSVC)
            target_compile_options(${{GAME_TARGET}} PRIVATE -fexceptions)
        endif()
    endif()
""")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _log(msg: str) -> None:
    print(msg, flush=True)


def _die(msg: str) -> None:
    print(f"\x1b[1;31mFATAL: {msg}\x1b[0m", file=sys.stderr, flush=True)
    sys.exit(1)


def _run(cmd: list[str], **kwargs: Any) -> None:
    pretty = " ".join(f'"{t}"' if " " in t else t for t in cmd)
    _log(f"+ {pretty}")
    try:
        subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError as exc:
        _die(f"Command failed (exit {exc.returncode}): {pretty}")


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        while chunk := fh.read(65536):
            h.update(chunk)
    return h.hexdigest()


# ---------------------------------------------------------------------------
# SDL3 dependency
# ---------------------------------------------------------------------------

def _ensure_sdl3(extern_dir: Path) -> Path:
    """Return path to SDL3 source directory, downloading if necessary."""
    sdl_src = extern_dir / f"SDL3-{SDL_VER}"
    if sdl_src.exists():
        _log(f"SDL3 source already present: {sdl_src}")
        return sdl_src

    tarball = CACHE_DIR / f"SDL3-{SDL_VER}.tar.gz"
    if not tarball.exists():
        _log(f"Downloading SDL3 {SDL_VER} …")
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(SDL_URL, tarball)

    actual = _sha256(tarball)
    if actual != SDL_SHA256:
        tarball.unlink(missing_ok=True)
        _die(f"SDL3 tarball checksum mismatch!\n  expected: {SDL_SHA256}\n  got:      {actual}")

    _log(f"Unpacking SDL3 into {extern_dir} …")
    extern_dir.mkdir(parents=True, exist_ok=True)
    shutil.unpack_archive(str(tarball), str(extern_dir))
    _log(f"SDL3 unpacked: {sdl_src}")
    return sdl_src


# ---------------------------------------------------------------------------
# CMake patching
# ---------------------------------------------------------------------------

def _patch_cmake(cmake_path: Path, game_cfg: dict) -> None:
    """
    Patch the upstream CMakeLists.txt so it builds cleanly under Emscripten.

    Changes made (all are idempotent — only applied when the original text is
    still present):
      1. Wrap find_package(OpenGL) in if(NOT EMSCRIPTEN).
      2. Replace the Pomme+OpenGL::GL target_link_libraries with a conditional
         version that omits OpenGL::GL under Emscripten.
      3. Append an if(EMSCRIPTEN) block with LEGACY_GL_EMULATION link options.
    """
    text = cmake_path.read_text(encoding="utf-8")
    changed = False

    # 1. Guard OpenGL find_package
    if _OPENGL_FIND_ORIGINAL in text:
        text = text.replace(_OPENGL_FIND_ORIGINAL, _OPENGL_FIND_PATCHED, 1)
        changed = True
        _log("  Patched: guarded find_package(OpenGL) for Emscripten")

    # 2. Guard OpenGL link
    if _OPENGL_LINK_ORIGINAL in text:
        text = text.replace(_OPENGL_LINK_ORIGINAL, _OPENGL_LINK_PATCHED, 1)
        changed = True
        _log("  Patched: guarded target_link_libraries(OpenGL::GL) for Emscripten")

    # 3. Append Emscripten block (only if not already present)
    if "LEGACY_GL_EMULATION" not in text:
        exported = ",".join(f"'_{f}'" for f in game_cfg.get("exported_functions", ["main"]))
        shell_path = str(SHELL_HTML).replace("\\", "/")
        emscripten_block = _EMSCRIPTEN_BLOCK_TEMPLATE.format(
            exported_functions=exported,
            shell_html=shell_path,
        )
        text += emscripten_block
        changed = True
        _log("  Patched: appended LEGACY_GL_EMULATION Emscripten block")

    if changed:
        cmake_path.write_text(text, encoding="utf-8")
        _log(f"  CMakeLists.txt saved: {cmake_path}")
    else:
        _log("  CMakeLists.txt: no patches needed (already patched)")


# ---------------------------------------------------------------------------
# Source clone / update
# ---------------------------------------------------------------------------

def _ensure_source(src_dir: Path, repo_url: str) -> None:
    """Clone jorio upstream repo if not already present; otherwise pull."""
    if (src_dir / ".git").exists():
        _log(f"Source already present at {src_dir} — skipping clone")
    else:
        _log(f"Cloning {repo_url} …")
        src_dir.parent.mkdir(parents=True, exist_ok=True)
        _run(["git", "clone", "--depth=1", "--recurse-submodules", repo_url, str(src_dir)])
        _log(f"Cloned into {src_dir}")


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def build_game(cfg: dict) -> None:
    """
    Full build pipeline for one jorio-ports game.

    cfg keys:
      name                   str  e.g. "Bugdom"
      full_name              str  e.g. "Bugdom"
      repo_url               str  e.g. "https://github.com/jorio/Bugdom.git"
      game_dir               Path directory containing this build_wasm.py
      build_subdir           str  relative path for cmake build dir (default "build-wasm")
      exported_functions     list extra C functions to export (default ["main"])
    """
    name: str = cfg["name"]
    full_name: str = cfg.get("full_name", name)
    repo_url: str = cfg["repo_url"]
    game_dir: Path = Path(cfg["game_dir"])

    build_subdir: str = cfg.get("build_subdir", "build-wasm")
    exported_fns: list[str] = cfg.get("exported_functions", ["main"])

    src_dir: Path = game_dir / "src"
    extern_dir: Path = src_dir / "extern"
    build_dir: Path = game_dir / build_subdir

    _log(f"\n{'='*60}")
    _log(f"  jorio-ports WASM build — {full_name}")
    _log(f"  Source:  {src_dir}")
    _log(f"  Build:   {build_dir}")
    _log(f"  Layer:   Emscripten LEGACY_GL_EMULATION (fixed-function → WebGL)")
    _log(f"{'='*60}\n")

    # 1. Clone source
    _ensure_source(src_dir, repo_url)

    # 2. SDL3 dependency
    sdl_src = _ensure_sdl3(extern_dir)

    # 3. Patch CMakeLists.txt
    cmake_path = src_dir / "CMakeLists.txt"
    if not cmake_path.exists():
        _die(f"CMakeLists.txt not found at {cmake_path}")
    _patch_cmake(cmake_path, {"exported_functions": exported_fns})

    # 4. Configure with emcmake cmake
    if build_dir.exists():
        _log(f"Removing old build dir: {build_dir}")
        shutil.rmtree(build_dir)

    _run([
        "emcmake", "cmake",
        "-S", str(src_dir),
        "-B", str(build_dir),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_SDL_FROM_SOURCE=ON",
        "-DSDL_STATIC=ON",
        f"-DSDL3_DIR={sdl_src}",
    ])

    # 5. Build
    _run([
        "cmake", "--build", str(build_dir),
        "-j", str(NPROC),
    ])

    # 6. Verify outputs
    expected = [name + ext for ext in (".html", ".js", ".wasm")]
    missing = [f for f in expected if not (build_dir / f).exists()]
    if missing:
        _log(f"Warning: expected output files not found: {missing}")
        _log("  The .data file may also be absent if data was inlined.")
    else:
        _log(f"\nBuild complete. Output in: {build_dir}")
        for ext in (".html", ".js", ".wasm", ".data"):
            p = build_dir / (name + ext)
            if p.exists():
                _log(f"  {p.name} ({p.stat().st_size:,} bytes)")

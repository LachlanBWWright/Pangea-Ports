#!/usr/bin/env python3
"""Execute the Android overlay with real SDL software and offscreen GLES3 renderers.

Example for a locally extracted SDK/sysroot:
  python3 scripts/check_android_touch_render.py --include-dir /path/usr/include \
      --library-dir /path/usr/lib/x86_64-linux-gnu
Requires a C compiler, SDL3, GLESv2, and an SDL offscreen video driver.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--include-dir', type=Path, required=True)
    parser.add_argument('--library-dir', type=Path, required=True)
    parser.add_argument('--cc', default=shutil.which('cc') or 'cc')
    parser.add_argument('--backend', choices=['all', 'sdl', 'gles'], default='all')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    library = args.library_dir.resolve()
    env = dict(os.environ)
    env['SDL_VIDEODRIVER'] = 'offscreen'
    env['LD_LIBRARY_PATH'] = str(library) + os.pathsep + env.get('LD_LIBRARY_PATH', '')
    with tempfile.TemporaryDirectory(prefix='pangea-touch-render-') as temp:
        exe = Path(temp) / 'render-smoke'
        env['MESA_SHADER_CACHE_DIR'] = temp
        subprocess.run([
            args.cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-D__ANDROID__',
            '-I' + str(args.include_dir.resolve()), '-I' + str(root / 'shared/android'),
            str(root / 'tests/android_touch_render_test.c'),
            str(root / 'shared/android/TouchControlsDraw.c'),
            str(library / 'libSDL3.so'), str(library / 'libGLESv2.so'),
            '-Wl,-rpath-link,' + str(library), '-lm', '-o', str(exe),
        ], check=True)
        if args.backend in ('all', 'sdl'):
            subprocess.run([str(exe)], env=env, check=True)
        if args.backend in ('all', 'gles'):
            subprocess.run([str(exe), 'gles'], env=env, check=True)


if __name__ == '__main__':
    main()

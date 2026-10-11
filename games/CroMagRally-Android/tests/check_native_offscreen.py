#!/usr/bin/env python3
"""Run the full scripting-enabled game with SDL offscreen rendering and real assets."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--executable', type=Path, required=True)
parser.add_argument('--assets', type=Path, required=True, help='Native Data directory')
parser.add_argument('--library-dir', type=Path)
parser.add_argument('--seconds', type=float, default=20)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='cromag-native-lua-') as temporary:
    root = Path(temporary)
    executable = root / 'CroMagRally'
    shutil.copy2(args.executable, executable)
    data = root / 'Data'
    data.mkdir()
    for asset in args.assets.resolve().iterdir():
        if asset.name != 'Scripts':
            (data / asset.name).symlink_to(asset, target_is_directory=asset.is_dir())
    scripts = data / 'Scripts'
    (scripts / 'dist').mkdir(parents=True)
    (scripts / 'config').mkdir()
    (scripts / 'config' / 'levels.json').write_text('{"version":1,"levels":{}}\n')
    shutil.copy2(Path(__file__).with_name('native_offscreen.lua'), scripts / 'dist' / 'main.lua')
    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', XDG_CACHE_HOME=str(root / 'cache'))
    if args.library_dir:
        env['LD_LIBRARY_PATH'] = str(args.library_dir) + ':' + env.get('LD_LIBRARY_PATH', '')
    with (root / 'run.log').open('w+') as log:
        process = subprocess.Popen([str(executable), '--track', '1', '--no-vsync'], cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            process.wait(timeout=args.seconds)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        log.seek(0)
        output = log.read()
    required = ['NATIVE_LUA_SMOKE_START', 'NATIVE_LUA_SMOKE_ADAPTER', 'NATIVE_LUA_SMOKE_FRAMES']
    missing = [marker for marker in required if marker not in output]
    errors = [line for line in output.splitlines() if '[PangeaScript ERROR]' in line or ('Script' in line and 'failed' in line)]
    if missing or errors:
        raise SystemExit(f'Native offscreen Lua smoke failed: missing={missing}, errors={errors}\n{chr(10).join(line for line in output.splitlines() if 'Script' in line or 'stack' in line or 'lua:' in line)}')
    print('Cro-Mag full native offscreen game: Lua startup, real player setters/objects, 120 race frames passed')

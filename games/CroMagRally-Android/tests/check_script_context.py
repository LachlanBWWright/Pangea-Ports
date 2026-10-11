#!/usr/bin/env python3
"""Run real Cro-Mag Lua adapter regression with an SDL3 include/library directory."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--include-dir', type=Path, required=True)
parser.add_argument('--library-dir', type=Path, required=True)
args = parser.parse_args()
game = Path(__file__).resolve().parents[1]
root = game.parents[1]
with tempfile.TemporaryDirectory(prefix='cromag-script-context-') as directory:
    executable = Path(directory) / 'context-test'
    command = [shutil.which('cc') or 'cc', '-std=c11', '-Wno-multichar',
               '-Werror=implicit-function-declaration', '-DPANGEA_ENABLE_SCRIPTING',
               '-ffunction-sections', '-fdata-sections']
    for include in [args.include_dir, game / 'Source/Headers', game / 'Source/Scripting',
                    game / 'extern/Pomme/src', root / 'shared/script']:
        command += ['-I', str(include)]
    command += [str(game / 'Source/Scripting/ScriptBindings.c'),
                str(game / 'tests/script_context_test.c'), '-Wl,--gc-sections',
                str(args.library_dir / 'libSDL3.so'),
                '-Wl,-rpath-link,' + str(args.library_dir), '-lm', '-o', str(executable)]
    subprocess.run(command, check=True)
    env = dict(os.environ)
    env['LD_LIBRARY_PATH'] = str(args.library_dir) + os.pathsep + env.get('LD_LIBRARY_PATH', '')
    subprocess.run([str(executable)], env=env, check=True)
    print('Cro-Mag native script contexts and weapon results passed')

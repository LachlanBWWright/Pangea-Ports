#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC_ASSET_SUFFIXES = {'.html', '.htm', '.css', '.js', '.png', '.webp', '.jpg', '.jpeg', '.svg', '.ico', '.gif', '.txt'}

PORTS = [
    {
        'name': 'BillyFrontier-Android',
        'path': 'games/BillyFrontier-Android',
        'native_build': ['python3', 'build.py', '--dependencies', '--configure', '--build'],
        'wasm_build': ['python3', 'build.py', '--emscripten', '--dependencies', '--configure', '--build'],
        'wasm_outputs': [
            'build/billyfrontier.html',
            'build/billyfrontier.js',
            'build/billyfrontier.wasm',
            'build/billyfrontier.data',
        ],
        'wasm_entrypoint': 'billyfrontier.html',
        'android_apk': False,
        'browser_test': None,
        'skip_to_level': {
            'status': 'unsupported',
            'web': None,
            'native': None,
            'notes': 'This port does not currently expose a documented direct level skip entry point.',
        },
    },
    {
        'name': 'Bugdom-android',
        'path': 'games/Bugdom-android',
        'native_build': ['python3', 'build.py', '--dependencies', '--configure', '--build'],
        'wasm_build': ['python3', 'build_wasm.py', '--dependencies', '--configure', '--build', '--package'],
        'wasm_outputs': [
            'dist-wasm/Bugdom.html',
            'dist-wasm/Bugdom.js',
            'dist-wasm/Bugdom.wasm',
            'dist-wasm/Bugdom.data',
        ],
        'wasm_entrypoint': 'Bugdom.html',
        'android_apk': False,
        'browser_test': 'bugdom-wasm',
        'skip_to_level': {
            'status': 'supported',
            'web': '?level=N',
            'native': None,
            'notes': 'Also supports ?terrainFile=:Terrain:Custom.ter and ?noFenceCollision=1 for editor testing.',
        },
    },
    {
        'name': 'Bugdom2-Android',
        'path': 'games/Bugdom2-Android',
        'native_build': ['python3', 'build.py', '--dependencies', '--configure', '--build'],
        'wasm_build': ['python3', 'build.py', '--emscripten', '--dependencies', '--configure', '--build', '--package'],
        'wasm_outputs': [
            'build/Bugdom2.html',
            'build/Bugdom2.js',
            'build/Bugdom2.wasm',
            'build/Bugdom2.data',
        ],
        'wasm_entrypoint': 'Bugdom2.html',
        'android_apk': True,
        'android_package': 'io.jor.bugdom2',
        'browser_test': 'bugdom2-playwright',
        'skip_to_level': {
            'status': 'supported',
            'web': '?level=N',
            'native': '--level N',
            'notes': 'Supports menu skipping and direct level boot for editor workflows.',
        },
    },
    {
        'name': 'CroMagRally-Android',
        'path': 'games/CroMagRally-Android',
        'native_build': ['python3', 'build.py', '--dependencies', '--configure', '--build'],
        'wasm_build': ['python3', 'build.py'],
        'wasm_outputs': [
            'build-wasm/CroMagRally.html',
            'build-wasm/CroMagRally.js',
            'build-wasm/CroMagRally.wasm',
            'build-wasm/CroMagRally.data',
        ],
        'wasm_entrypoint': 'CroMagRally.html',
        'android_apk': False,
        'browser_test': None,
        'skip_to_level': {
            'status': 'partial',
            'web': '?track=N&car=N',
            'native': '--track N --car N',
            'notes': 'Racing flow is track/car based rather than single-player level skipping.',
        },
    },
    {
        'name': 'MightyMike-Android',
        'path': 'games/MightyMike-Android',
        'native_build': ['python3', 'build.py', '--dependencies', '--configure', '--build'],
        'wasm_build': ['python3', 'build.py', '--wasm', '--dependencies', '--configure', '--build', '--package'],
        'wasm_outputs': [
            'build-wasm/MightyMike.js',
            'build-wasm/MightyMike.wasm',
        ],
        'wasm_entrypoint': 'index.html',
        'android_apk': False,
        'browser_test': None,
        'skip_to_level': {
            'status': 'supported',
            'web': '?level=SCENE:AREA',
            'native': '--level SCENE:AREA',
            'notes': 'Uses scene/area identifiers instead of a single integer level index.',
        },
    },
    {
        'name': 'Nanosaur-android',
        'path': 'games/Nanosaur-android',
        'native_build': ['python3', 'build.py', '--dependencies', '--configure', '--build'],
        'wasm_build': ['python3', 'build.py', '--wasm'],
        'wasm_outputs': [
            'build-wasm/Nanosaur.html',
            'build-wasm/Nanosaur.js',
            'build-wasm/Nanosaur.wasm',
            'build-wasm/Nanosaur.data',
        ],
        'wasm_entrypoint': 'Nanosaur.html',
        'android_apk': False,
        'browser_test': None,
        'skip_to_level': {
            'status': 'supported',
            'web': '?level=N&skipMenu=1',
            'native': '--level N --skip-menu',
            'notes': 'This is the clearest current reference implementation for direct boot into a playable level.',
        },
    },
    {
        'name': 'Nanosaur2-Android',
        'path': 'games/Nanosaur2-Android',
        'native_build': ['python3', 'build.py', '--dependencies', '--configure', '--build'],
        'wasm_build': ['python3', 'build.py', '--wasm'],
        'wasm_outputs': [
            'build-wasm/Nanosaur2.html',
            'build-wasm/Nanosaur2.js',
            'build-wasm/Nanosaur2.wasm',
            'build-wasm/Nanosaur2.data',
        ],
        'wasm_entrypoint': 'Nanosaur2.html',
        'android_apk': False,
        'browser_test': None,
        'skip_to_level': {
            'status': 'supported',
            'web': '?level=N',
            'native': '--level N',
            'notes': 'Also supports terrain override inputs for level-editor integration.',
        },
    },
    {
        'name': 'OttoMatic-Android',
        'path': 'games/OttoMatic-Android',
        'native_build': ['python3', 'build.py', '--dependencies', '--configure', '--build'],
        'wasm_build': ['python3', 'build.py', '--wasm'],
        'wasm_outputs': [
            'build/OttoMatic.html',
            'build/OttoMatic.js',
            'build/OttoMatic.wasm',
            'build/OttoMatic.data',
        ],
        'wasm_entrypoint': 'OttoMatic.html',
        'android_apk': False,
        'browser_test': None,
        'skip_to_level': {
            'status': 'supported',
            'web': '?level=N',
            'native': '--level N',
            'notes': 'Also supports terrain overrides in the WASM shell for editor-driven test loops.',
        },
    },
]

PORTS_BY_NAME = {port['name']: port for port in PORTS}


def _matrix_for(kind: str) -> dict:
    if kind == 'native':
        ports = PORTS
    elif kind == 'wasm':
        ports = PORTS
    elif kind == 'browser':
        ports = [port for port in PORTS if port['browser_test']]
    elif kind == 'android-apk':
        ports = [port for port in PORTS if port['android_apk']]
    else:
        raise SystemExit(f'Unsupported matrix kind: {kind}')
    return {'include': [{'name': port['name'], 'path': port['path'], 'browser_test': port['browser_test']} for port in ports]}


def _run_command(command: list[str], cwd: Path) -> None:
    print(f'+ ({cwd}) {' '.join(command)}')
    subprocess.run(command, cwd=cwd, check=True)


def _copy_if_exists(source: Path, dest_dir: Path) -> None:
    if source.exists():
        shutil.copy2(source, dest_dir / source.name)


def _stage_wasm(port: dict, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    game_root = ROOT / port['path']
    for relative_path in port['wasm_outputs']:
        source = game_root / relative_path
        _copy_if_exists(source, dest)

    docs_dir = game_root / 'docs'
    if docs_dir.exists():
        for path in docs_dir.iterdir():
            if path.is_file() and path.suffix.lower() in DOC_ASSET_SUFFIXES:
                shutil.copy2(path, dest / path.name)

    if port['name'] == 'MightyMike-Android' and not (dest / 'index.html').exists():
        docs_index = docs_dir / 'index.html'
        if docs_index.exists():
            shutil.copy2(docs_index, dest / 'index.html')


def _run_task(port: dict, task: str, dest: str | None) -> None:
    cwd = ROOT / port['path']
    if task == 'native-build':
        _run_command(port['native_build'], cwd)
        return
    if task == 'wasm-build':
        _run_command(port['wasm_build'], cwd)
        return
    if task == 'stage-wasm':
        if not dest:
            raise SystemExit('--dest is required for stage-wasm')
        _stage_wasm(port, Path(dest))
        return
    raise SystemExit(f'Unsupported task: {task}')


def main() -> int:
    parser = argparse.ArgumentParser(description='Monorepo metadata and task runner for imported Pangea ports.')
    subparsers = parser.add_subparsers(dest='command', required=True)

    matrix_parser = subparsers.add_parser('matrix', help='Emit a GitHub Actions matrix JSON document.')
    matrix_parser.add_argument('kind', choices=['native', 'wasm', 'browser', 'android-apk'])

    run_parser = subparsers.add_parser('run', help='Run a named task for a specific port.')
    run_parser.add_argument('--game', required=True, choices=sorted(PORTS_BY_NAME))
    run_parser.add_argument('--task', required=True, choices=['native-build', 'wasm-build', 'stage-wasm'])
    run_parser.add_argument('--dest')

    subparsers.add_parser('list', help='List imported games.')

    args = parser.parse_args()

    if args.command == 'matrix':
        print(json.dumps(_matrix_for(args.kind), separators=(',', ':')))
        return 0
    if args.command == 'list':
        for port in PORTS:
            print(f"{port['name']}\t{port['path']}")
        return 0

    port = PORTS_BY_NAME[args.game]
    _run_task(port, args.task, args.dest)
    return 0


if __name__ == '__main__':
    sys.exit(main())

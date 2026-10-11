#!/usr/bin/env python3
"""Verify real APK/native Lua hooks, level transitions and persisted restart state."""
import argparse
import hashlib
import json
import os
import shutil
from pathlib import Path
import struct
import subprocess
import time
import zipfile

PACKAGE = "io.jor.bugdom2.luasmoke"
ACTIVITY = "io.jor.bugdom2.GameActivity"
KEY_FILE = ".pangea-persistence-android_smoke_events.bin"
HOOKS = ("load", "start", "frame", "complete", "unload")


def decode_events(data):
    if len(data) < 8:
        raise ValueError("Truncated persistence record")
    wire, kind, version, length = struct.unpack("<BBHI", data[:8])
    if (wire, kind, version) != (1, 1, 1) or len(data) != 8 + length:
        raise ValueError("Invalid smoke persistence record")
    return data[8:].decode("utf-8")


def expected_events(runs):
    return "".join(f"run={run} level={level} hook={hook}\n"
                   for run in range(1, runs + 1) for level in (0, 1) for hook in HOOKS)


def validate_apk(path):
    with zipfile.ZipFile(path) as apk:
        config = json.loads(apk.read("assets/Scripts/config/levels.json"))
        for level in (0, 1):
            selection = config["levels"][str(level)]
            name = f"Data/Scripts/smoke/level{level}.lua"
            if selection["script"] != name or selection["levelSettings"]["smokeModule"] != level:
                raise ValueError("APK does not select the smoke modules")
            source = apk.read("assets/" + name.removeprefix("Data/")).decode()
            if "android_smoke_events" not in source or "onLevelUnload" not in source:
                raise ValueError("APK does not contain the full Lua fixture")
        if not any(name.startswith("lib/x86_64/") for name in apk.namelist()):
            raise ValueError("Smoke APK must include x86_64 emulator libraries")


def wait_for_events(read, runs, timeout, process=None):
    expected = expected_events(runs)
    deadline = time.monotonic() + timeout
    observed = ""
    while time.monotonic() < deadline:
        try:
            observed = decode_events(read())
        except (OSError, ValueError, subprocess.CalledProcessError):
            if process and process.poll() is not None:
                raise RuntimeError(f"Game exited before Lua smoke completion: {process.returncode}")
            time.sleep(.25)
            continue
        if observed == expected:
            return observed
        if process and process.poll() is not None:
            raise RuntimeError(f"Game exited before Lua smoke completion: {process.returncode}; events={observed!r}")
        if not expected.startswith(observed):
            raise RuntimeError(f"Unexpected Lua lifecycle order: {observed!r}")
        time.sleep(.25)
    raise TimeoutError(f"Lua hooks/persistence incomplete after {timeout}s: {observed!r}")


def prepare_native_directory(directory, binary):
    root = Path(__file__).resolve().parents[1]
    source = root / "games/Bugdom2-Android/Data"
    data = directory / "Data"
    data.mkdir(parents=True, exist_ok=True)
    for child in source.iterdir():
        destination = data / child.name
        if child.name != "Scripts" and not destination.exists():
            destination.symlink_to(child, target_is_directory=child.is_dir())
    scripts = data / "Scripts"
    scripts.mkdir(exist_ok=True)
    for child in (source / "Scripts").iterdir():
        destination = scripts / child.name
        if child.name not in ("config", "smoke", "persistence") and not destination.exists():
            destination.symlink_to(child, target_is_directory=child.is_dir())
    shutil.copytree(root / "tests/fixtures/android-lua-smoke/assets/Scripts", scripts, dirs_exist_ok=True)
    (scripts / "persistence").mkdir(exist_ok=True)
    destination = directory / binary.name
    if binary.resolve() != destination.resolve():
        shutil.copy2(binary, destination)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--apk", type=Path)
    mode.add_argument("--native-binary", type=Path)
    parser.add_argument("--native-directory", type=Path)
    parser.add_argument("--prepare-native-data", action="store_true", help="Create isolated native data overlay and copy smoke binary")
    parser.add_argument("--adb", default="adb")
    parser.add_argument("--serial")
    parser.add_argument("--clear-data", action="store_true", help="Reset smoke fixture app data or isolated native telemetry before the first run")
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--artifacts", type=Path, default=Path("work/android-lua-smoke-results"))
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    results = {"platform": "android" if args.apk else "native-offscreen", "runs": []}
    if args.apk:
        validate_apk(args.apk)
        results["apkSha256"] = hashlib.sha256(args.apk.read_bytes()).hexdigest()
        adb = [args.adb] + (["-s", args.serial] if args.serial else [])
        def command(*parts, check=True):
            return subprocess.run(adb + list(parts), capture_output=True, check=check).stdout
        def read():
            paths = command("shell", "run-as", PACKAGE, "find", ".", "-name", KEY_FILE).decode().splitlines()
            if len(paths) != 1:
                raise ValueError(f"Expected one app-writable telemetry file; found {paths}")
            return command("exec-out", "run-as", PACKAGE, "cat", paths[0])
        command("wait-for-device")
        command("install", "-r", str(args.apk.resolve()))
        if args.clear_data:
            if b"Success" not in command("shell", "pm", "clear", PACKAGE):
                raise RuntimeError("Failed to reset smoke app data")
        try:
            for run in (1, 2):
                command("shell", "am", "force-stop", PACKAGE)
                command("shell", "am", "start", "-W", "-n", PACKAGE + "/" + ACTIVITY)
                events = wait_for_events(read, run, args.timeout)
                temporary = command("shell", "run-as", PACKAGE, "find", ".", "-name", KEY_FILE + ".tmp")
                if temporary.strip():
                    raise RuntimeError("Atomic persistence replacement left a temporary file")
                results["runs"].append({"run": run, "events": events.splitlines()})
        finally:
            command("shell", "am", "force-stop", PACKAGE, check=False)
            (args.artifacts / "logcat.txt").write_bytes(command("logcat", "-d", check=False))
    else:
        if not args.native_directory:
            parser.error("--native-directory is required with --native-binary")
        directory = args.native_directory.resolve()
        binary = args.native_binary.resolve()
        if args.prepare_native_data:
            binary = prepare_native_directory(directory, binary)
        if args.clear_data:
            for base in (directory / "Data/Scripts/persistence", directory / "Data/Scripts"):
                for key in ("android_smoke_events", "android_smoke_runs"):
                    (base / f".pangea-persistence-{key}.bin").unlink(missing_ok=True)
        paths = [directory / "Data/Scripts/persistence" / KEY_FILE, directory / "Data/Scripts" / KEY_FILE]
        def read():
            for path in paths:
                if path.exists():
                    return path.read_bytes()
            raise FileNotFoundError(KEY_FILE)
        environment = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="dummy")
        for run in (1, 2):
            with (args.artifacts / f"native-run-{run}.log").open("wb") as log:
                process = subprocess.Popen([str(binary), "--level", "0"],
                                           cwd=directory, env=environment, stdout=log, stderr=subprocess.STDOUT)
                try:
                    events = wait_for_events(read, run, args.timeout, process)
                    results["runs"].append({"run": run, "events": events.splitlines()})
                finally:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
    (args.artifacts / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"PASS: {results['platform']} packaged Lua, both level lifecycles and persisted restart state")


if __name__ == "__main__":
    main()

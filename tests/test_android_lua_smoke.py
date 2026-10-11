import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import Mock, patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("android_lua_smoke", ROOT / "scripts/android_lua_smoke.py")
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)


class AndroidLuaSmokeHarnessTests(unittest.TestCase):
    @staticmethod
    def record(events):
        payload = events.encode()
        return struct.pack("<BBHI", 1, 1, 1, len(payload)) + payload

    def test_record_rejects_wrong_wire_kind_version_and_utf8(self):
        for header in ((2, 1, 1), (1, 2, 1), (1, 1, 2)):
            with self.subTest(header=header), self.assertRaises(ValueError):
                smoke.decode_events(struct.pack("<BBHI", *header, 0))
        with self.assertRaises(ValueError):
            smoke.decode_events(struct.pack("<BBHI", 1, 1, 1, 1) + b"\xff")

    def test_complete_telemetry_wins_over_process_exit(self):
        process = Mock(returncode=0)
        process.poll.return_value = 0
        self.assertEqual(smoke.wait_for_events(
            lambda: self.record(smoke.expected_events(2)), 2, 1, process),
            smoke.expected_events(2))

    def test_incomplete_telemetry_reports_process_exit(self):
        process = Mock(returncode=7)
        process.poll.return_value = 7
        with self.assertRaisesRegex(RuntimeError, "completion: 7"):
            smoke.wait_for_events(lambda: b"", 1, 1, process)
        with self.assertRaisesRegex(RuntimeError, "events="):
            smoke.wait_for_events(lambda: self.record(smoke.expected_events(1)[:-10]), 1, 1, process)

    def test_wait_retries_atomic_write_then_accepts_progress(self):
        expected = smoke.expected_events(1)
        read = Mock(side_effect=[FileNotFoundError(), b"partial", self.record(expected[:expected.index("\n") + 1]), self.record(expected)])
        with patch.object(smoke.time, "sleep") as sleep:
            self.assertEqual(smoke.wait_for_events(read, 1, 1), expected)
        self.assertEqual(sleep.call_count, 3)

    def test_wait_times_out_with_partial_telemetry(self):
        with patch.object(smoke.time, "monotonic", side_effect=[0, 0, 2]), patch.object(smoke.time, "sleep"):
            with self.assertRaisesRegex(TimeoutError, "incomplete"):
                smoke.wait_for_events(lambda: self.record("run=1 level=0 hook=load\n"), 1, 1)

    def test_telemetry_requires_complete_record(self):
        payload = smoke.expected_events(2).encode()
        encoded = struct.pack("<BBHI", 1, 1, 1, len(payload)) + payload
        self.assertEqual(smoke.decode_events(encoded), smoke.expected_events(2))
        for broken in (encoded[:-1], b"", encoded + b"extra"):
            with self.assertRaises(ValueError):
                smoke.decode_events(broken)

    def test_lifecycle_order_must_include_actual_frames_and_both_levels(self):
        payload = smoke.expected_events(1).replace("hook=frame", "hook=load", 1).encode()
        encoded = struct.pack("<BBHI", 1, 1, 1, len(payload)) + payload
        with self.assertRaisesRegex(RuntimeError, "Unexpected Lua lifecycle order"):
            smoke.wait_for_events(lambda: encoded, 1, 1)

    def test_shipped_fixture_assets_form_a_smoke_apk(self):
        assets = ROOT / "tests/fixtures/android-lua-smoke/assets"
        with tempfile.TemporaryDirectory() as temp:
            apk = Path(temp) / "fixture.apk"
            with zipfile.ZipFile(apk, "w") as archive:
                for path in assets.rglob("*"):
                    if path.is_file():
                        archive.write(path, "assets/" + str(path.relative_to(assets)))
                archive.writestr("lib/x86_64/libBugdom2.so", "fixture library placeholder")
            smoke.validate_apk(apk)
            missing_game = Path(temp) / "missing-game.apk"
            with zipfile.ZipFile(apk) as source, zipfile.ZipFile(missing_game, "w") as archive:
                for name in source.namelist():
                    if not name.startswith("lib/"):
                        archive.writestr(name, source.read(name))
                archive.writestr("lib/x86_64/libunrelated.so", "fixture library placeholder")
            with self.assertRaisesRegex(ValueError, "Bugdom2 library"):
                smoke.validate_apk(missing_game)
            missing_frame = Path(temp) / "missing-frame.apk"
            with zipfile.ZipFile(apk) as source, zipfile.ZipFile(missing_frame, "w") as archive:
                for name in source.namelist():
                    payload = source.read(name)
                    if name.endswith("level1.lua"):
                        payload = payload.replace(b"function module.onFrame(", b"function module.unused(")
                    archive.writestr(name, payload)
            with self.assertRaisesRegex(ValueError, "full Lua fixture"):
                smoke.validate_apk(missing_frame)
            production = Path(temp) / "production.apk"
            with zipfile.ZipFile(production, "w") as archive:
                archive.writestr("assets/Scripts/config/levels.json", json.dumps({"levels": {"0": {"script": "Data/Scripts/main.lua"}}}))
            with self.assertRaises((ValueError, KeyError)):
                smoke.validate_apk(production)


if __name__ == "__main__":
    unittest.main()

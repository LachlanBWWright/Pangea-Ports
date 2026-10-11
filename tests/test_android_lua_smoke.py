import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("android_lua_smoke", ROOT / "scripts/android_lua_smoke.py")
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)


class AndroidLuaSmokeHarnessTests(unittest.TestCase):
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
            production = Path(temp) / "production.apk"
            with zipfile.ZipFile(production, "w") as archive:
                archive.writestr("assets/Scripts/config/levels.json", json.dumps({"levels": {"0": {"script": "Data/Scripts/main.lua"}}}))
            with self.assertRaises((ValueError, KeyError)):
                smoke.validate_apk(production)


if __name__ == "__main__":
    unittest.main()

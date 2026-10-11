"""Ensure live smoke markers cannot hide crashes or script failures."""
import importlib.util
from pathlib import Path
import signal
import unittest

SPEC = importlib.util.spec_from_file_location(
    "native_smoke", Path(__file__).resolve().parents[1] / "scripts/smoke_native_script_older_ports.py")
SMOKE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SMOKE)
OUTPUT = "NATIVE_SMOKE_START\nNATIVE_SMOKE_OBJECT\nNATIVE_SMOKE_FRAME_PLAYER\n"


class NativeSmokeTests(unittest.TestCase):
    def test_successful_exit_and_deliberate_timeout(self):
        self.assertTrue(SMOKE.validate_output(OUTPUT, 0, False))
        self.assertTrue(SMOKE.validate_output(OUTPUT, -signal.SIGTERM, True))
        self.assertTrue(SMOKE.validate_output(OUTPUT, -signal.SIGKILL, True))

    def test_crash_after_markers_is_failure(self):
        for code in (1, -signal.SIGSEGV, -signal.SIGABRT, -signal.SIGTERM):
            self.assertFalse(SMOKE.validate_output(OUTPUT, code, False))
        self.assertFalse(SMOKE.validate_output(OUTPUT, -signal.SIGSEGV, True))

    def test_lua_error_after_markers_is_failure(self):
        for error in ("[PangeaScript ERROR] onFrame failed", "Assertion failure", "assertion failed", "scripting onFrame failed"):
            self.assertFalse(SMOKE.validate_output(OUTPUT + error, 0, False))

    def test_missing_frame_marker_is_failure(self):
        self.assertFalse(SMOKE.validate_output("NATIVE_SMOKE_START\nNATIVE_SMOKE_OBJECT", 0, False))


if __name__ == "__main__":
    unittest.main()

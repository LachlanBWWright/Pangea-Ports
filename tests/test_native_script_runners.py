"""A hung native fixture must fail its game without hiding later regressions."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import check_native_script_adapters as adapters
import check_native_script_gameplay as gameplay


class NativeRunnerTests(unittest.TestCase):
    def run_with_first_timeout(self, module, args):
        calls = 0
        def run(command, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise subprocess.TimeoutExpired(command, kwargs["timeout"])
            return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
        output = io.StringIO()
        with mock.patch.object(sys, "argv", [module.__file__, *args]), \
             mock.patch.object(module.subprocess, "run", side_effect=run), \
             contextlib.redirect_stdout(output):
            self.assertEqual(module.main(), 1)
        self.assertEqual(calls, 2 * len(adapters.GAMES))
        self.assertIn("timed out", output.getvalue())
        self.assertIn("7/8", output.getvalue())
        self.assertIn("PASS MightyMike-Android", output.getvalue())

    def test_adapter_timeout_keeps_running(self):
        self.run_with_first_timeout(adapters, ["--sysroot-include", "/unused"])

    def test_gameplay_timeout_is_in_report_and_keeps_running(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            for name in ("libPangeaScript.a", "libPangeaLua.a"):
                (build / name).touch()
            report = build / "report.json"
            self.run_with_first_timeout(gameplay, ["--sysroot-include", "/unused", "--sdl-lib-dir", "/unused", "--script-build", str(build), "--json", str(report)])
            games = json.loads(report.read_text())["games"]
            self.assertEqual(games[adapters.GAMES[0]]["reason"], "timeout")
            self.assertFalse(games[adapters.GAMES[0]]["passed"])
            self.assertTrue(all(games[name]["passed"] for name in adapters.GAMES[1:]))


if __name__ == "__main__":
    unittest.main()

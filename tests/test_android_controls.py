"""Run the actual shared touch geometry without an Android device or SDK."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AndroidControlsTests(unittest.TestCase):
    def test_shared_geometry(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler is required for touch geometry regression tests")
        with tempfile.TemporaryDirectory(prefix="pangea-touch-") as directory:
            binary = Path(directory) / "android-controls"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "shared/android"),
                str(ROOT / "tests/android_controls_test.c"), "-lm", "-o", str(binary),
            ], check=True, text=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("passed", result.stdout)

    def test_capture_and_lifecycle(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler is required")
        includes = [Path("/usr/include"), Path("/usr/local/include"),
                    Path("/workspace/pangea-tools/sysroot/usr/include")]
        include = next((p for p in includes if (p / "SDL3/SDL.h").is_file()), None)
        if include is None:
            self.skipTest("SDL3 headers are required for event regression tests")
        with tempfile.TemporaryDirectory(prefix="pangea-touch-events-") as directory:
            binary = Path(directory) / "android-touch-events"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-D__ANDROID__",
                "-I", str(include), "-I", str(ROOT / "shared/android"),
                str(ROOT / "tests/android_touch_events_test.c"), "-lm", "-o", str(binary),
            ], check=True, text=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("passed", result.stdout)


if __name__ == "__main__":
    unittest.main()

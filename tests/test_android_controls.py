"""Run the actual shared touch geometry without an Android device or SDK."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AndroidControlsTests(unittest.TestCase):
    def test_render_pixels_and_graphics_state(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler is required")
        roots = [Path("/usr"), Path("/usr/local"), Path("/workspace/pangea-tools/sysroot/usr")]
        root = next((p for p in roots if (p / "include/SDL3/SDL.h").is_file()
                     and (p / "include/GLES3/gl3.h").is_file()), None)
        if root is None:
            self.skipTest("SDL3 and GLES3 headers are required for renderer regression tests")
        libraries = next((p for p in (root / "lib/x86_64-linux-gnu", root / "lib64", root / "lib")
                          if (p / "libSDL3.so").exists() and (p / "libGLESv2.so").exists()), None)
        if libraries is None:
            self.skipTest("SDL3 and GLES libraries are required for renderer regression tests")
        environment = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="dummy")
        environment["LD_LIBRARY_PATH"] = str(libraries) + os.pathsep + environment.get("LD_LIBRARY_PATH", "")
        with tempfile.TemporaryDirectory(prefix="pangea-touch-render-") as directory:
            binary = Path(directory) / "android-touch-render"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-D__ANDROID__",
                "-I", str(root / "include"), "-I", str(ROOT / "shared/android"),
                str(ROOT / "tests/android_touch_render_test.c"), str(ROOT / "shared/android/TouchControlsDraw.c"),
                "-L", str(libraries), "-Wl,-rpath-link," + str(libraries),
                "-lSDL3", "-lGLESv2", "-lm", "-o", str(binary),
            ], check=True, text=True)
            for renderer, arguments in (("software", []), ("GLES3", ["gl"])):
                with self.subTest(renderer=renderer):
                    result = subprocess.run([str(binary), *arguments], env=environment, capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("state restore passed", result.stdout)

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

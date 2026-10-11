"""Exercise platform asset loading and writable Android persistence with SDL stubs."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ScriptPlatformTests(unittest.TestCase):
    def test_assets_and_persistence(self):
        compiler = shutil.which("cc")
        candidates = [Path(os.environ.get("SDL3_INCLUDE_DIR", "/usr/include")),
                      Path("/workspace/pangea-tools/sysroot/usr/include")]
        include = next((path for path in candidates if (path / "SDL3/SDL.h").exists()), None)
        if not compiler or not include:
            self.skipTest("C compiler and SDL3 headers are required")
        with tempfile.TemporaryDirectory(prefix="pangea-script-platform-") as directory:
            executable = Path(directory) / "test-platform"
            subprocess.run([compiler, "-std=c17", "-Wall", "-Wextra", "-Werror",
                            "-D__ANDROID__", "-I", str(include),
                            str(ROOT / "tests/test_pangea_script_platform.c"),
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable), directory], check=True)


if __name__ == "__main__":
    unittest.main()

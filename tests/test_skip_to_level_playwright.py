"""
Monorepo-level skip-to-level smoke tests.

Starts a simple HTTP server against the repository root, then uses
Playwright to navigate to each game's docs landing page, verifying that:
  1. The page loads without errors.
  2. Skip-to-level controls (links or URL parameter docs) are present.
  3. A screenshot can be taken (stored as artifacts in CI).

Run locally:
    python3 -m pytest tests/test_skip_to_level_playwright.py -v
    # or
    python3 tests/test_skip_to_level_playwright.py
"""
from __future__ import annotations

import os
import subprocess
import sys
import threading
import time
import unittest
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCREENSHOTS_DIR = ROOT / "docs" / "screenshots"
PORT = 19876  # unlikely to clash

# --------------------------------------------------------------------- #
#  Per-game test expectations                                            #
# --------------------------------------------------------------------- #

GAME_DOCS = [
    {
        "name": "BillyFrontier",
        "path": "games/BillyFrontier-Android/docs/index.html",
        "level_text": "?level=",
        "heading_text": "Level Editor Integration",
    },
    {
        "name": "Bugdom",
        "path": "games/Bugdom-android/docs/index.html",
        "level_text": "?level=",
        "heading_text": "Level Select",
    },
    {
        "name": "Bugdom2",
        "path": "games/Bugdom2-Android/docs/index.html",
        "level_text": "Jump to Level",
        "heading_text": "Jump to Level",
    },
    {
        "name": "CroMagRally",
        "path": "games/CroMagRally-Android/docs/index.html",
        "level_text": "?track=",
        "heading_text": "Level Editor Integration",
    },
    {
        "name": "MightyMike",
        "path": "games/MightyMike-Android/docs/index.html",
        "level_text": "?level=",
        "heading_text": "Developer",
    },
    {
        "name": "Nanosaur",
        "path": "games/Nanosaur-android/docs/index.html",
        "level_text": "?level=",
        "heading_text": "URL Parameters",
    },
    {
        "name": "Nanosaur2",
        "path": "games/Nanosaur2-Android/docs/index.html",
        "level_text": "?level=",
        "heading_text": "Skip-to-Level",
    },
    {
        "name": "OttoMatic",
        "path": "games/OttoMatic-Android/docs/index.html",
        "level_text": "?level=",
        "heading_text": "Skip-to-Level",
    },
]


# --------------------------------------------------------------------- #
#  Utility: simple HTTP server in a background thread                    #
# --------------------------------------------------------------------- #

class _SilentHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):  # suppress access logs
        pass


def _start_server(port: int, directory: str) -> HTTPServer:
    os.chdir(directory)
    server = HTTPServer(("127.0.0.1", port), _SilentHandler)
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()
    # Give the server a moment to bind.
    time.sleep(0.5)
    return server


# --------------------------------------------------------------------- #
#  Tests                                                                 #
# --------------------------------------------------------------------- #

class SkipToLevelPlaywrightTests(unittest.TestCase):
    """Playwright smoke tests for skip-to-level across all 8 game docs."""

    _server: HTTPServer | None = None
    _base_url = f"http://127.0.0.1:{PORT}"

    @classmethod
    def setUpClass(cls) -> None:
        try:
            from playwright.sync_api import sync_playwright  # noqa: F401
        except ImportError:
            raise unittest.SkipTest("playwright is not installed; run 'pip install playwright && playwright install chromium'")

        cls._server = _start_server(PORT, str(ROOT))
        SCREENSHOTS_DIR.mkdir(parents=True, exist_ok=True)

    @classmethod
    def tearDownClass(cls) -> None:
        if cls._server:
            cls._server.shutdown()

    def _take_screenshot(self, page, game_name: str) -> None:
        safe = game_name.lower().replace(" ", "_").replace("-", "_")
        dest = SCREENSHOTS_DIR / f"{safe}_skip_to_level.png"
        page.screenshot(path=str(dest), full_page=True)

    def _test_game_docs(self, game: dict) -> None:
        from playwright.sync_api import sync_playwright, expect

        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            page = browser.new_page()
            errors: list[str] = []
            page.on("pageerror", lambda e: errors.append(str(e)))

            url = f"{self._base_url}/{game['path']}"
            page.goto(url, wait_until="domcontentloaded", timeout=15_000)

            # 1. Page loaded — title must be non-empty
            title = page.title()
            self.assertTrue(title, f"{game['name']}: page title should not be empty")

            # 2. Skip-to-level text must appear somewhere on the page
            content = page.content()
            self.assertIn(
                game["level_text"],
                content,
                f"{game['name']}: '{game['level_text']}' not found on docs page",
            )

            # 3. No fatal JavaScript errors
            self.assertEqual(
                errors,
                [],
                f"{game['name']}: JS errors on load: {errors}",
            )

            # 4. Capture screenshot
            self._take_screenshot(page, game["name"])

            browser.close()

    def test_billy_frontier_skip_to_level(self):
        self._test_game_docs(GAME_DOCS[0])

    def test_bugdom_skip_to_level(self):
        self._test_game_docs(GAME_DOCS[1])

    def test_bugdom2_skip_to_level(self):
        self._test_game_docs(GAME_DOCS[2])

    def test_cromag_rally_skip_to_level(self):
        self._test_game_docs(GAME_DOCS[3])

    def test_mighty_mike_skip_to_level(self):
        self._test_game_docs(GAME_DOCS[4])

    def test_nanosaur_skip_to_level(self):
        self._test_game_docs(GAME_DOCS[5])

    def test_nanosaur2_skip_to_level(self):
        self._test_game_docs(GAME_DOCS[6])

    def test_ottomatic_skip_to_level(self):
        self._test_game_docs(GAME_DOCS[7])

    def test_screenshots_dir_is_populated(self):
        """Verify that the screenshots directory contains images for all 8 games."""
        pngs = list(SCREENSHOTS_DIR.glob("*_skip_to_level.png"))
        self.assertGreaterEqual(
            len(pngs),
            8,
            f"Expected ≥8 skip-to-level screenshots in {SCREENSHOTS_DIR}, found {len(pngs)}: {[p.name for p in pngs]}",
        )


if __name__ == "__main__":
    unittest.main()

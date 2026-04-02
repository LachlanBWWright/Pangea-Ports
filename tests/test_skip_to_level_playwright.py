"""
Monorepo-level skip-to-level smoke tests.

Starts a simple HTTP server against the repository root, then uses
Playwright to navigate to the single Pangea Ports hub page and each
game's available docs page, verifying that:
  1. The page loads without errors.
  2. Skip-to-level controls (links or URL parameters) are documented.
  3. The hub page links directly to game HTML (not per-game landing pages).

Run locally:
    python3 -m unittest tests.test_skip_to_level_playwright -v
"""
from __future__ import annotations

import os
import threading
import time
import unittest
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PORT = 19876  # unlikely to clash
PAGE_LOAD_TIMEOUT_MS = 15_000

# ---------------------------------------------------------------------------
# Games that still have per-game docs/index.html API reference pages.
# These are pages documenting the skip-to-level API, NOT the WASM game itself.
# Nanosaur2 and OttoMatic per-game landing pages were removed (single-site
# model: only the hub at docs/index.html serves as the navigation surface).
# MightyMike docs/index.html IS the game shell, not a landing page.
# ---------------------------------------------------------------------------

GAME_DOCS = [
    {
        "name": "BillyFrontier",
        "path": "games/BillyFrontier-Android/docs/index.html",
        "level_text": "?level=",
    },
    {
        "name": "Bugdom",
        "path": "games/Bugdom-android/docs/index.html",
        "level_text": "?level=",
    },
    {
        "name": "Bugdom2",
        "path": "games/Bugdom2-Android/docs/index.html",
        "level_text": "Jump to Level",
    },
    {
        "name": "CroMagRally",
        "path": "games/CroMagRally-Android/docs/index.html",
        "level_text": "?track=",
    },
    {
        "name": "MightyMike",
        "path": "games/MightyMike-Android/docs/index.html",
        "level_text": "?level=",
    },
    {
        "name": "Nanosaur",
        "path": "games/Nanosaur-android/docs/index.html",
        "level_text": "?level=",
    },
]


class _SilentHandler(SimpleHTTPRequestHandler):
    def log_message(self, *log_args):  # suppress access logs
        pass


def _start_server(port: int, directory: str) -> HTTPServer:
    os.chdir(directory)
    server = HTTPServer(("127.0.0.1", port), _SilentHandler)
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()
    time.sleep(0.5)
    return server


class SkipToLevelPlaywrightTests(unittest.TestCase):
    """Playwright smoke tests verifying skip-to-level docs pages load correctly."""

    _server: HTTPServer | None = None
    _base_url = f"http://127.0.0.1:{PORT}"

    @classmethod
    def setUpClass(cls) -> None:
        try:
            from playwright.sync_api import sync_playwright  # noqa: F401
        except ImportError:
            raise unittest.SkipTest(
                "playwright is not installed; run 'pip install playwright && playwright install chromium'"
            )
        cls._server = _start_server(PORT, str(ROOT))

    @classmethod
    def tearDownClass(cls) -> None:
        if cls._server:
            cls._server.shutdown()

    def _test_game_docs(self, game: dict) -> None:
        from playwright.sync_api import sync_playwright

        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            page = browser.new_page()
            errors: list[str] = []
            page.on("pageerror", lambda e: errors.append(str(e)))

            url = f"{self._base_url}/{game['path']}"
            page.goto(url, wait_until="domcontentloaded", timeout=PAGE_LOAD_TIMEOUT_MS)

            title = page.title()
            self.assertTrue(title, f"{game['name']}: page title should not be empty")

            content = page.content()
            self.assertIn(
                game["level_text"],
                content,
                f"{game['name']}: '{game['level_text']}' not found on docs page",
            )

            self.assertEqual(errors, [], f"{game['name']}: JS errors on load: {errors}")
            browser.close()

    def test_hub_page_loads(self):
        """The main Pangea Ports hub page should load without errors."""
        from playwright.sync_api import sync_playwright

        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            page = browser.new_page()
            errors: list[str] = []
            page.on("pageerror", lambda e: errors.append(str(e)))

            page.goto(f"{self._base_url}/docs/index.html", wait_until="domcontentloaded", timeout=PAGE_LOAD_TIMEOUT_MS)
            self.assertIn("Pangea", page.title(), "Hub page title should contain 'Pangea'")
            self.assertEqual(errors, [], f"Hub page JS errors: {errors}")

            content = page.content()
            # Hub should reference all 8 games
            # Hub should mention all 8 vendored games (some by display_name only, some with version)
            for game_name in ["Billy Frontier", "Bugdom", "Bugdom 2", "Cro-Mag Rally",
                               "Mighty Mike", "Nanosaur", "Nanosaur 2", "Otto Matic"]:
                self.assertIn(game_name, content, f"Hub should mention {game_name}")

            game_area_active = page.locator("#game-area.active").count()
            self.assertEqual(game_area_active, 0, "Hub should not auto-launch a game on first load")

            browser.close()

    def test_hub_game_selection_shows_launcher_controls(self):
        """Selecting a game on the hub should show launcher controls before booting the WASM build."""
        from playwright.sync_api import sync_playwright

        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            page = browser.new_page()
            errors: list[str] = []
            page.on("pageerror", lambda e: errors.append(str(e)))

            page.goto(f"{self._base_url}/docs/index.html", wait_until="domcontentloaded", timeout=PAGE_LOAD_TIMEOUT_MS)
            page.click('[data-game="nanosaur2"]')

            launcher_classes = page.locator("#launcher-panel").get_attribute("class") or ""
            self.assertNotIn("hidden", launcher_classes, "Launcher panel should be visible")
            self.assertEqual(page.locator("#game-area.active").count(), 0, "Selecting a game should not boot it immediately")
            self.assertIn("Nanosaur 2", page.locator("#launcher-title").text_content())
            self.assertIn("Start normally", page.locator("#launch-normal").text_content())
            self.assertEqual(page.locator("#upload-target-path").count(), 1, "Launcher should expose upload controls")
            self.assertEqual(errors, [], f"Hub game selection JS errors: {errors}")

            browser.close()

    def test_hub_page_has_no_per_game_docs_links(self):
        """The hub page should NOT contain per-game 'Docs' links (single-site model)."""
        from playwright.sync_api import sync_playwright
        import re

        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)
            page = browser.new_page()
            page.goto(f"{self._base_url}/docs/index.html", wait_until="domcontentloaded", timeout=PAGE_LOAD_TIMEOUT_MS)
            content = page.content()
            docs_links = re.findall(r'href="[^"]*index\.html"[^>]*>Docs<', content)
            self.assertEqual(docs_links, [], f"Hub should have no 'Docs' links: {docs_links}")
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

    def test_nanosaur2_has_no_per_game_landing_page(self):
        """Nanosaur 2 per-game landing page was removed; game is accessed via hub."""
        nanosaur2_index = ROOT / "games" / "Nanosaur2-Android" / "docs" / "index.html"
        self.assertFalse(
            nanosaur2_index.exists(),
            "Nanosaur2 per-game landing page should not exist; use hub for navigation",
        )

    def test_ottomatic_has_no_per_game_landing_page(self):
        """Otto Matic per-game landing page was removed; game is accessed via hub."""
        otto_index = ROOT / "games" / "OttoMatic-Android" / "docs" / "index.html"
        self.assertFalse(
            otto_index.exists(),
            "OttoMatic per-game landing page should not exist; use hub for navigation",
        )

    def test_docs_pages_hero_images_load(self):
        """Docs pages that have hero images (screenshot.webp/.png) must load them without errors."""
        from playwright.sync_api import sync_playwright

        PAGES_WITH_HEROES = [
            ("BillyFrontier", "games/BillyFrontier-Android/docs/index.html", "screenshot.webp"),
            ("Bugdom", "games/Bugdom-android/docs/index.html", "screenshot.webp"),
            ("CroMagRally", "games/CroMagRally-Android/docs/index.html", "screenshot.webp"),
            ("Nanosaur", "games/Nanosaur-android/docs/index.html", "screenshot.png"),
        ]

        with sync_playwright() as pw:
            browser = pw.chromium.launch(headless=True)

            for name, path, screenshot_ref in PAGES_WITH_HEROES:
                with self.subTest(game=name):
                    page = browser.new_page()
                    page.goto(
                        f"{self._base_url}/{path}",
                        wait_until="networkidle",
                        timeout=PAGE_LOAD_TIMEOUT_MS,
                    )
                    page.wait_for_timeout(500)

                    img_status = page.evaluate(f"""
                        (() => {{
                            const imgs = document.querySelectorAll('img[src="{screenshot_ref}"]');
                            if (imgs.length === 0) return 'no-img-found';
                            const img = imgs[0];
                            return img.complete && img.naturalWidth > 0 ? 'loaded' : 'broken';
                        }})()
                    """)

                    self.assertEqual(
                        img_status,
                        "loaded",
                        f"{name}: hero image '{screenshot_ref}' is not loading (status: {img_status}). "
                        f"The file must exist in games/{name.lower()}-*/docs/{screenshot_ref}",
                    )
                    page.close()

            browser.close()


if __name__ == "__main__":
    unittest.main()

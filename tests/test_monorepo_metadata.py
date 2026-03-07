import tempfile
import unittest
from pathlib import Path

import scripts.ports as ports

VALID_SKIP_STATUSES = ports.VALID_SKIP_STATUSES


class MonorepoMetadataTests(unittest.TestCase):
    def test_all_ports_have_consistent_hook_metadata(self):
        for port in ports.PORTS:
            with self.subTest(port=port["name"]):
                self.assertTrue(port["display_name"])
                self.assertIn("status", port["skip_to_level"])
                self.assertIn(port["skip_to_level"]["status"], VALID_SKIP_STATUSES)
                self.assertTrue(port["site_override_example"])

                if port["skip_to_level"]["status"] != "unsupported":
                    self.assertTrue(port["site_level_example"])

    def test_stage_wasm_respects_pages_layout_metadata(self):
        created_files: list[Path] = []
        try:
            with tempfile.TemporaryDirectory(prefix="pangea-stage-test-") as tmp:
                stage_root = Path(tmp)

                for port in ports.PORTS:
                    game_root = ports.ROOT / port["path"]

                    for rel_output in port["wasm_outputs"]:
                        source = game_root / rel_output
                        if not source.exists():
                            source.parent.mkdir(parents=True, exist_ok=True)
                            source.write_text(f"placeholder for {port['name']}\n", encoding="utf-8")
                            created_files.append(source)

                    dest = stage_root / port["name"]
                    ports._stage_wasm(port, dest)

                    docs_index = dest / "index.html"
                    if port.get("has_docs_landing"):
                        self.assertTrue(docs_index.exists(), f"{port['name']} should stage its docs landing page")

                    launch_path = port.get("site_launch_path")
                    if launch_path:
                        self.assertTrue((dest / launch_path).exists(), f"{port['name']} launch path should exist after staging")
                    elif port["wasm_entrypoint"] == "index.html":
                        self.assertTrue(docs_index.exists(), f"{port['name']} should have an index.html entrypoint")
        finally:
            for created in created_files:
                created.unlink(missing_ok=True)

    def test_root_pages_hub_mentions_every_game_and_hook(self):
        hub = (ports.ROOT / "docs" / "index.html").read_text(encoding="utf-8")
        self.assertIn('role="tablist"', hub)

        for port in ports.PORTS:
            with self.subTest(port=port["name"]):
                self.assertIn(port["display_name"], hub)
                if port.get("site_level_example"):
                    self.assertIn(port["site_level_example"], hub)

    def test_billy_frontier_docs_use_standard_query_params(self):
        billy_docs = (ports.ROOT / "games" / "BillyFrontier-Android" / "docs" / "index.html").read_text(encoding="utf-8")
        self.assertIn("?level=1", billy_docs)
        self.assertIn("terrainFile", billy_docs)
        self.assertIn("BF_LoadTerrainData", billy_docs)

    def test_shared_pomme_dependency_is_present_for_all_games(self):
        # Each game vendors Pomme as a per-game submodule under extern/Pomme.
        # The directory exists as an (optionally uninitialised) submodule reference.
        gitmodules = (ports.ROOT / ".gitmodules").read_text(encoding="utf-8")

        for port in ports.PORTS:
            with self.subTest(port=port["name"]):
                game_pomme = ports.ROOT / port["path"] / "extern" / "Pomme"
                self.assertTrue(
                    game_pomme.exists(),
                    f"{port['name']} should have extern/Pomme submodule directory",
                )
                self.assertIn(
                    port["path"] + "/extern/Pomme",
                    gitmodules,
                    f"{port['name']} should be registered in .gitmodules",
                )

    def test_all_ports_with_docs_landing_have_index_html(self):
        for port in ports.PORTS:
            with self.subTest(port=port["name"]):
                if port.get("has_docs_landing"):
                    docs_index = ports.ROOT / port["path"] / "docs" / "index.html"
                    self.assertTrue(
                        docs_index.exists(),
                        f"{port['name']} has has_docs_landing=True but is missing docs/index.html",
                    )

    def test_all_shells_expose_pangea_game_api(self):
        games_with_shells = [
            "Bugdom-android",
            "Nanosaur2-Android",
            "OttoMatic-Android",
        ]
        for game_name in games_with_shells:
            with self.subTest(game=game_name):
                shell = ports.ROOT / "games" / game_name / "docs" / "shell.html"
                self.assertTrue(shell.exists(), f"{game_name} should have docs/shell.html")
                shell_text = shell.read_text(encoding="utf-8")
                self.assertIn(
                    "PangeaGame",
                    shell_text,
                    f"{game_name}/docs/shell.html should expose the standard PangeaGame API",
                )
                self.assertIn(
                    "skipToLevel",
                    shell_text,
                    f"{game_name}/docs/shell.html should expose skipToLevel",
                )

    def test_nanosaur2_and_ottomatic_have_landing_pages(self):
        for game_name in ("Nanosaur2-Android", "OttoMatic-Android"):
            with self.subTest(game=game_name):
                index = ports.ROOT / "games" / game_name / "docs" / "index.html"
                self.assertTrue(index.exists(), f"{game_name} should have a docs/index.html landing page")
                content = index.read_text(encoding="utf-8")
                self.assertIn("?level=", content, f"{game_name} landing page should document skip-to-level URL param")
                self.assertIn("PangeaGame", content, f"{game_name} landing page should document PangeaGame API")


        manifest = (ports.ROOT / "games" / "Bugdom2-Android" / "android" / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")
        self.assertIn('android:icon="@mipmap/ic_launcher"', manifest)
        self.assertIn('android:roundIcon="@mipmap/ic_launcher_round"', manifest)

        res_root = ports.ROOT / "games" / "Bugdom2-Android" / "android" / "app" / "src" / "main" / "res"
        for density in ("mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi"):
            with self.subTest(density=density):
                self.assertTrue((res_root / f"mipmap-{density}" / "ic_launcher_round.png").exists())

    def test_bugdom2_android_build_bootstraps_sdl(self):
        build_gradle = (ports.ROOT / "games" / "Bugdom2-Android" / "android" / "app" / "build.gradle").read_text(encoding="utf-8")
        self.assertIn("prepareSdlSource", build_gradle)
        self.assertIn('def sdlVersion = "3.2.8"', build_gradle)
        self.assertIn('def sdlArchiveName = "SDL3-${sdlVersion}.tar.gz"', build_gradle)
        self.assertIn('"-DSDL_SHARED=OFF"', build_gradle)

        wrapper_root = ports.ROOT / "games" / "Bugdom2-Android" / "android"
        self.assertTrue((wrapper_root / "gradlew").exists())
        self.assertTrue((wrapper_root / "gradle" / "wrapper" / "gradle-wrapper.jar").exists())


if __name__ == "__main__":
    unittest.main()

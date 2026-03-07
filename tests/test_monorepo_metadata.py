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
        shared_pomme = ports.ROOT / "extern" / "Pomme" / "CMakeLists.txt"
        self.assertTrue(shared_pomme.exists())

        for port in ports.PORTS:
            with self.subTest(port=port["name"]):
                game_pomme = ports.ROOT / port["path"] / "extern" / "Pomme"
                self.assertTrue(game_pomme.exists(), f"{port['name']} should expose extern/Pomme")

    def test_bugdom2_android_manifest_wires_standard_icons(self):
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

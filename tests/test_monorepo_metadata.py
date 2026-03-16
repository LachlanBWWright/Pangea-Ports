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

                    # MightyMike uses docs/index.html as the game shell itself
                    docs_index = dest / "index.html"
                    if port.get("has_docs_landing"):
                        self.assertTrue(docs_index.exists(), f"{port['name']} should stage its game shell index page")

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
        # New single-page design uses data-game attributes on navbar buttons
        self.assertIn('data-game=', hub)
        # Every game's WASM URL must be reachable from the hub
        self.assertIn('GAME_URLS', hub)

        for port in ports.PORTS:
            with self.subTest(port=port["name"]):
                self.assertIn(port["display_name"], hub)

    def test_hub_has_no_per_game_docs_links(self):
        """The hub should NOT link to per-game docs/index.html pages; WASM is game-only."""
        hub = (ports.ROOT / "docs" / "index.html").read_text(encoding="utf-8")
        # The single-site model has no secondary "Docs" buttons pointing to per-game pages
        import re
        docs_hrefs = re.findall(r'href="[^"]*index\.html"[^>]*>Docs<', hub)
        self.assertEqual(
            docs_hrefs,
            [],
            "Hub should not contain per-game 'Docs' links: " + str(docs_hrefs),
        )

    def test_hub_launches_games_in_embed_mode(self):
        hub = (ports.ROOT / "docs" / "index.html").read_text(encoding="utf-8")
        for game_id in ["billy", "bugdom", "bugdom2", "cromag", "mighty", "nanosaur", "nanosaur2", "otto"]:
            with self.subTest(game=game_id):
                self.assertIn("?embed=1", hub)
                self.assertRegex(hub, rf"{game_id}\s*:\s*'[^']+\?embed=1'")

    def test_game_shell_templates_support_embedded_mode(self):
        shell_paths = [
            ports.ROOT / "games" / "BillyFrontier-Android" / "packaging" / "emscripten_shell.html",
            ports.ROOT / "games" / "Bugdom-android" / "docs" / "shell.html",
            ports.ROOT / "games" / "Bugdom2-Android" / "packaging" / "shell.html",
            ports.ROOT / "games" / "CroMagRally-Android" / "packaging" / "emscripten" / "shell.html",
            ports.ROOT / "games" / "MightyMike-Android" / "docs" / "index.html",
            ports.ROOT / "games" / "Nanosaur-android" / "packaging" / "emscripten" / "shell.html",
            ports.ROOT / "games" / "Nanosaur2-Android" / "packaging" / "wasm" / "shell.html",
            ports.ROOT / "games" / "OttoMatic-Android" / "docs" / "shell.html",
        ]

        for shell_path in shell_paths:
            with self.subTest(shell=str(shell_path.relative_to(ports.ROOT))):
                shell = shell_path.read_text(encoding="utf-8")
                self.assertIn("embedded-shell", shell)
                self.assertIn("embed') === '1'", shell)

    def test_bugdom2_gles3_compat_reuses_webgl_buffers(self):
        compat = (
            ports.ROOT
            / "games"
            / "Bugdom2-Android"
            / "Source"
            / "3D"
            / "GLES3Compat.c"
        ).read_text(encoding="utf-8")
        self.assertIn("gArrayVBOSize", compat)
        self.assertIn("gArrayEBOSize", compat)
        self.assertIn("gImmVBOSize", compat)
        self.assertIn("glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0", compat)
        self.assertNotIn("glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)totalSize, NULL, GL_STREAM_DRAW)", compat)
        self.assertNotIn("glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)indexSize, indices, GL_STREAM_DRAW)", compat)

    def test_cromag_alpha_masked_materials_keep_depth_writes(self):
        meta = (
            ports.ROOT
            / "games"
            / "CroMagRally-Android"
            / "Source"
            / "3D"
            / "MetaObjects.c"
        ).read_text(encoding="utf-8")
        self.assertIn("Alpha-masked textures still need depth writes", meta)
        self.assertIn("if ((diffColor2.a != 1.0f) || (matFlags & BG3D_MATERIALFLAG_ALWAYSBLEND))", meta)

    def test_nanosaur2_show_helpers_clear_stale_cull_bits(self):
        player = (
            ports.ROOT
            / "games"
            / "Nanosaur2-Android"
            / "Source"
            / "Player"
            / "Player.c"
        ).read_text(encoding="utf-8")
        objects = (
            ports.ROOT
            / "games"
            / "Nanosaur2-Android"
            / "Source"
            / "System"
            / "Objects.c"
        ).read_text(encoding="utf-8")
        objects2 = (
            ports.ROOT
            / "games"
            / "Nanosaur2-Android"
            / "Source"
            / "System"
            / "Objects2.c"
        ).read_text(encoding="utf-8")
        cull_clear = "STATUS_BIT_ISCULLED1 | STATUS_BIT_ISCULLED2 | STATUS_BIT_ISCULLED3"
        self.assertGreaterEqual(player.count(cull_clear), 2)
        self.assertIn(cull_clear, objects)
        self.assertGreaterEqual(objects2.count(cull_clear), 2)

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

    def test_mightymike_game_shell_is_the_docs_index(self):
        """MightyMike uses docs/index.html as the game shell itself (not a landing page)."""
        mighty_mike_index = ports.ROOT / "games" / "MightyMike-Android" / "docs" / "index.html"
        self.assertTrue(mighty_mike_index.exists(), "MightyMike should have docs/index.html as game shell")
        content = mighty_mike_index.read_text(encoding="utf-8")
        # It must embed a canvas and Emscripten Module config
        self.assertIn("<canvas", content, "MightyMike game shell must contain a WebGL canvas")
        self.assertIn("var Module", content, "MightyMike game shell must configure Emscripten Module")
        self.assertIn("MightyMike.js", content, "MightyMike game shell must reference MightyMike.js loader")

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

    def test_wasm_stage_does_not_include_per_game_html_from_docs(self):
        """
        The WASM staging step must NOT copy per-game landing pages from docs/
        into the staged output. Only the built game HTML/JS/WASM/data files
        and non-HTML doc assets (images) should be present.
        """
        created_files: list[Path] = []
        try:
            with tempfile.TemporaryDirectory(prefix="pangea-stage-html-test-") as tmp:
                stage_root = Path(tmp)

                for port in ports.PORTS:
                    # MightyMike-Android is the one exception: its docs/index.html IS the
                    # game shell (wasm_entrypoint = 'index.html'), not a landing page.
                    if port.get('has_docs_landing'):
                        continue

                    game_root = ports.ROOT / port["path"]
                    docs_dir = game_root / "docs"
                    if not docs_dir.exists():
                        continue

                    for rel_output in port["wasm_outputs"]:
                        source = game_root / rel_output
                        if not source.exists():
                            source.parent.mkdir(parents=True, exist_ok=True)
                            source.write_text(f"placeholder for {port['name']}\n", encoding="utf-8")
                            created_files.append(source)

                    dest = stage_root / port["name"]
                    ports._stage_wasm(port, dest)

                    # docs/index.html should NOT appear at the root of the staged output
                    with self.subTest(game=port["name"]):
                        staged_index = dest / "index.html"
                        self.assertFalse(
                            staged_index.exists(),
                            f"{port['name']}: docs/index.html should NOT be staged (WASM = game only, no per-game landing pages)",
                        )
        finally:
            for created in created_files:
                created.unlink(missing_ok=True)

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

    def test_all_games_have_android_gradle_project(self):
        """All 8 Pangea ports must have a checked-in Android Gradle project."""
        for port in ports.PORTS:
            game_name = port['name']
            with self.subTest(game=game_name):
                android_dir = ports.ROOT / port['path'] / 'android'
                self.assertTrue(
                    android_dir.exists(),
                    f"{game_name}: android/ directory must exist",
                )
                gradlew = android_dir / 'gradlew'
                self.assertTrue(
                    gradlew.exists(),
                    f"{game_name}: android/gradlew must exist",
                )
                build_gradle = android_dir / 'app' / 'build.gradle'
                self.assertTrue(
                    build_gradle.exists(),
                    f"{game_name}: android/app/build.gradle must exist",
                )
                # Build script must use SDL from source (standardized pattern)
                content = build_gradle.read_text(encoding='utf-8')
                self.assertIn(
                    'prepareSdlSource',
                    content,
                    f"{game_name}: build.gradle must auto-download SDL3",
                )
                # Must include SDL3 Java sources so SDLActivity is in the APK
                self.assertIn(
                    'extern/SDL/android-project/app/src/main/java',
                    content,
                    f"{game_name}: build.gradle must include SDL3 Java sources in java.srcDirs",
                )
                # Must have a known package identifier
                self.assertIn(
                    port['android_package'],
                    content,
                    f"{game_name}: build.gradle must reference package '{port['android_package']}'",
                )
                # Must have a custom GameActivity that overrides getLibraries() with the correct library name
                cmake_content = (ports.ROOT / port['path'] / 'CMakeLists.txt').read_text(encoding='utf-8')
                game_target = next(
                    line.split('"')[1]
                    for line in cmake_content.split('\n')
                    if line.strip().startswith('set(GAME_TARGET')
                )
                pkg = port['android_package']
                pkg_path = pkg.replace('.', '/')
                game_activity = (
                    android_dir / 'app' / 'src' / 'main' / 'java' / pkg_path / 'GameActivity.java'
                )
                self.assertTrue(
                    game_activity.exists(),
                    f"{game_name}: {game_activity.relative_to(ports.ROOT)} must exist to override getLibraries()",
                )
                activity_content = game_activity.read_text(encoding='utf-8')
                self.assertIn(
                    f'"{game_target}"',
                    activity_content,
                    f"{game_name}: GameActivity.java must return library '{game_target}' in getLibraries()",
                )
                # AndroidManifest.xml must use the custom GameActivity
                manifest = (android_dir / 'app' / 'src' / 'main' / 'AndroidManifest.xml').read_text(encoding='utf-8')
                self.assertNotIn(
                    'org.libsdl.app.SDLActivity',
                    manifest,
                    f"{game_name}: AndroidManifest.xml must use .GameActivity, not org.libsdl.app.SDLActivity directly",
                )
                self.assertIn(
                    '.GameActivity',
                    manifest,
                    f"{game_name}: AndroidManifest.xml must reference .GameActivity",
                )

    def test_all_games_have_android_apk_flag(self):
        """All 8 games must have android_apk=True in ports.py."""
        for port in ports.PORTS:
            with self.subTest(game=port['name']):
                self.assertTrue(
                    port.get('android_apk', False),
                    f"{port['name']}: android_apk must be True — this repo is exclusively Android/WASM ports",
                )

    def test_android_apk_matrix_includes_all_games(self):
        """android-apk matrix must include all 8 games."""
        import json
        matrix_json = json.loads(
            __import__('subprocess').check_output(
                ['python3', 'scripts/ports.py', 'matrix', 'android-apk'],
                cwd=str(ports.ROOT),
                text=True,
            )
        )
        names = {entry['name'] for entry in matrix_json['include']}
        expected = {port['name'] for port in ports.PORTS}
        self.assertEqual(
            names,
            expected,
            f"android-apk matrix missing: {expected - names}",
        )

    def test_pr_validation_android_smoke_uses_matrix_path(self):
        """pr-validation.yml android-emulator-smoke must use matrix.path, not hardcoded Bugdom2."""
        wf = (ports.ROOT / ".github" / "workflows" / "pr-validation.yml").read_text(encoding="utf-8")
        self.assertIn(
            "matrix.path",
            wf,
            "android-emulator-smoke must use matrix.path so all games get tested",
        )
        # Extract just the android-emulator-smoke section to check it
        # Find the section between 'android-emulator-smoke:' and the next top-level job or EOF
        smoke_start = wf.find("android-emulator-smoke:")
        self.assertGreater(smoke_start, 0, "android-emulator-smoke job not found")
        smoke_section = wf[smoke_start:]
        # The smoke section must use matrix.path, not a hardcoded game path
        self.assertIn(
            'matrix.path',
            smoke_section,
            "android-emulator-smoke must reference matrix.path",
        )
        # The cd command in the smoke build step must NOT hardcode a specific game
        self.assertNotIn(
            "cd games/Bugdom2-Android/android",
            smoke_section,
            "android-emulator-smoke must not hardcode 'cd games/Bugdom2-Android/android'",
        )

    def test_separate_wasm_and_apk_workflow_files_exist(self):
        """Separate CI pipelines: build-wasm.yml (WASM + Pages) and build-android-apk.yml (APK only)."""
        workflows_dir = ports.ROOT / ".github" / "workflows"

        wasm_wf = workflows_dir / "build-wasm.yml"
        self.assertTrue(wasm_wf.exists(), "build-wasm.yml must exist as the dedicated WASM pipeline")

        apk_wf = workflows_dir / "build-android-apk.yml"
        self.assertTrue(apk_wf.exists(), "build-android-apk.yml must exist as the dedicated APK pipeline")

    def test_wasm_workflow_deploys_pages_only_on_main(self):
        """The WASM workflow must deploy Pages only on main/master, not on every tag or dispatch."""
        wasm_wf = (ports.ROOT / ".github" / "workflows" / "build-wasm.yml").read_text(encoding="utf-8")

        # Must contain a deploy-pages job that depends on build-wasm
        self.assertIn("deploy-pages", wasm_wf, "build-wasm.yml must have a deploy-pages job")
        # The deploy-pages job must be gated to main/master (not all tags)
        self.assertIn("refs/heads/main", wasm_wf, "deploy-pages must check for refs/heads/main")
        self.assertIn("refs/heads/master", wasm_wf, "deploy-pages must check for refs/heads/master")

    def test_apk_workflow_does_not_trigger_pages_deploy(self):
        """The APK workflow must not contain any Pages deployment logic."""
        apk_wf = (ports.ROOT / ".github" / "workflows" / "build-android-apk.yml").read_text(encoding="utf-8")
        self.assertNotIn("deploy-pages", apk_wf, "build-android-apk.yml must not deploy Pages")
        self.assertNotIn("actions/deploy-pages", apk_wf, "build-android-apk.yml must not use deploy-pages action")

    def test_apk_workflow_uses_matrix_path(self):
        """The APK build workflow must use matrix.path so all 8 games get built."""
        apk_wf = (ports.ROOT / ".github" / "workflows" / "build-android-apk.yml").read_text(encoding="utf-8")
        self.assertIn(
            "matrix.path",
            apk_wf,
            "build-android-apk.yml must use matrix.path (not hardcoded game dirs)",
        )

    def test_session8_screenshots_present(self):
        """Session-8 screenshots directory must contain skip-to-level and level shots for all 8 games."""
        shots_dir = ports.ROOT / "docs" / "screenshots" / "session-8"
        self.assertTrue(shots_dir.exists(), "docs/screenshots/session-8/ must exist")

        expected_skip = [
            "billyfrontier_skip_to_level.png",
            "bugdom_skip_to_level.png",
            "bugdom2_skip_to_level.png",
            "cromagnrally_skip_to_level.png",
            "mightymike_skip_to_level.png",
            "nanosaur_skip_to_level.png",
            "nanosaur2_skip_to_level.png",
            "ottomatic_skip_to_level.png",
        ]
        for fname in expected_skip:
            with self.subTest(file=fname):
                self.assertTrue((shots_dir / fname).exists(), f"Missing skip-to-level screenshot: {fname}")

        # At least 24 level screenshots must be present (3 per game × 8 games)
        png_files = [f for f in shots_dir.iterdir() if f.suffix == ".png" and "skip_to_level" not in f.name]
        self.assertGreaterEqual(len(png_files), 24, f"Expected ≥24 level screenshots, found {len(png_files)}")

    def test_docs_pages_hero_images_exist(self):
        """
        Docs pages that reference screenshot.webp or screenshot.png must have those files present.
        These are hero images shown on the per-game landing pages and used as og:image metadata.
        """
        expected_screenshots = [
            ("BillyFrontier-Android", "docs/screenshot.webp"),
            ("Bugdom-android", "docs/screenshot.webp"),
            ("CroMagRally-Android", "docs/screenshot.webp"),
            ("Nanosaur-android", "docs/screenshot.png"),
        ]
        for game_name, rel_path in expected_screenshots:
            with self.subTest(game=game_name):
                asset = ports.ROOT / "games" / game_name / rel_path
                self.assertTrue(
                    asset.exists(),
                    f"{game_name}: {rel_path} must exist (referenced from docs/index.html)",
                )


if __name__ == "__main__":
    unittest.main()

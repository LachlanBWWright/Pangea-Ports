#!/usr/bin/env python3
"""
tests/test_jorio_ports_metadata.py

Validates the metadata and infrastructure for the jorio-ports alternative
WASM builds (the LEGACY_GL_EMULATION approach).
"""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import jorio_ports  # noqa: E402


# Expected game names (all 8 Pangea games)
EXPECTED_GAMES = {
    "Bugdom", "Bugdom2", "Nanosaur", "Nanosaur2",
    "OttoMatic", "BillyFrontier", "CroMagRally", "MightyMike",
}


class JorioPortsMetadataTests(unittest.TestCase):

    def test_all_eight_games_present(self):
        """Every Pangea game must have an entry in PORTS."""
        names = {p["name"] for p in jorio_ports.PORTS}
        self.assertEqual(names, EXPECTED_GAMES)

    def test_all_ports_have_required_fields(self):
        required = {"name", "display_name", "jorio_repo", "path",
                    "wasm_build", "wasm_outputs", "wasm_entrypoint",
                    "site_launch_path"}
        for port in jorio_ports.PORTS:
            with self.subTest(port=port["name"]):
                for field in required:
                    self.assertIn(field, port, f"Missing field {field!r} in port {port['name']!r}")

    def test_all_jorio_repos_point_to_jorio_github(self):
        """Every jorio_repo URL must be in the jorio GitHub org."""
        for port in jorio_ports.PORTS:
            with self.subTest(port=port["name"]):
                self.assertIn(
                    "https://github.com/jorio/",
                    port["jorio_repo"],
                    f"{port['name']} jorio_repo does not point to jorio GitHub",
                )

    def test_all_port_paths_exist(self):
        """Every game directory must exist in jorio-ports/."""
        for port in jorio_ports.PORTS:
            with self.subTest(port=port["name"]):
                game_dir = ROOT / port["path"]
                self.assertTrue(
                    game_dir.is_dir(),
                    f"Directory not found: {game_dir}",
                )

    def test_all_build_wasm_scripts_exist(self):
        """Every game directory must contain a build_wasm.py."""
        for port in jorio_ports.PORTS:
            with self.subTest(port=port["name"]):
                build_script = ROOT / port["path"] / "build_wasm.py"
                self.assertTrue(
                    build_script.is_file(),
                    f"build_wasm.py not found: {build_script}",
                )

    def test_all_build_wasm_scripts_reference_correct_repo(self):
        """Each build_wasm.py must reference the correct jorio repo URL."""
        for port in jorio_ports.PORTS:
            with self.subTest(port=port["name"]):
                build_script = (ROOT / port["path"] / "build_wasm.py").read_text(encoding="utf-8")
                self.assertIn(
                    port["jorio_repo"],
                    build_script,
                    f"build_wasm.py for {port['name']} does not reference {port['jorio_repo']}",
                )

    def test_shared_build_common_exists(self):
        """The shared build_common.py module must exist."""
        build_common = ROOT / "jorio-ports" / "shared" / "build_common.py"
        self.assertTrue(build_common.is_file(), f"Not found: {build_common}")

    def test_shared_shell_html_exists(self):
        """The shared HTML shell template must exist."""
        shell = ROOT / "jorio-ports" / "shared" / "shell.html"
        self.assertTrue(shell.is_file(), f"Not found: {shell}")

    def test_shared_shell_has_legacy_gl_badge(self):
        """Shell template must mention LEGACY_GL_EMULATION so it's clearly labelled."""
        shell = (ROOT / "jorio-ports" / "shared" / "shell.html").read_text(encoding="utf-8")
        self.assertIn("LEGACY_GL_EMULATION", shell)

    def test_shared_shell_has_embedded_mode(self):
        """Shell must support embedded-shell mode (embed=1 param) like other shells."""
        shell = (ROOT / "jorio-ports" / "shared" / "shell.html").read_text(encoding="utf-8")
        self.assertIn("embedded-shell", shell)
        self.assertIn("embed') === '1'", shell)

    def test_shared_shell_has_emscripten_hooks(self):
        """Shell must define the Module config with setStatus hook."""
        shell = (ROOT / "jorio-ports" / "shared" / "shell.html").read_text(encoding="utf-8")
        self.assertIn("{{{ SCRIPT }}}", shell)
        self.assertIn("setStatus", shell)
        self.assertIn("Module", shell)

    def test_build_common_uses_legacy_gl_emulation(self):
        """build_common.py must use -sLEGACY_GL_EMULATION=1 as the core flag."""
        bc = (ROOT / "jorio-ports" / "shared" / "build_common.py").read_text(encoding="utf-8")
        self.assertIn("LEGACY_GL_EMULATION=1", bc)

    def test_build_common_patches_opengl_find_package(self):
        """build_common.py must guard find_package(OpenGL) for Emscripten."""
        bc = (ROOT / "jorio-ports" / "shared" / "build_common.py").read_text(encoding="utf-8")
        self.assertIn("find_package(OpenGL REQUIRED)", bc)
        self.assertIn("if(NOT EMSCRIPTEN)", bc)

    def test_build_common_uses_asyncify(self):
        """build_common.py must enable ASYNCIFY so blocking game loops work in browser."""
        bc = (ROOT / "jorio-ports" / "shared" / "build_common.py").read_text(encoding="utf-8")
        self.assertIn("ASYNCIFY=1", bc)

    def test_build_common_clones_from_jorio(self):
        """build_common.py must clone upstream repos from github.com/jorio."""
        bc = (ROOT / "jorio-ports" / "shared" / "build_common.py").read_text(encoding="utf-8")
        self.assertIn("github.com/jorio", bc)

    def test_alternative_ci_workflow_exists(self):
        """build-jorio-wasm.yml must exist as the alternative deployment workflow."""
        workflow = ROOT / ".github" / "workflows" / "build-jorio-wasm.yml"
        self.assertTrue(workflow.is_file(), f"Not found: {workflow}")

    def test_alternative_ci_uses_legacy_gl_emulation_in_comment(self):
        """The CI workflow must document the LEGACY_GL_EMULATION approach."""
        workflow = (ROOT / ".github" / "workflows" / "build-jorio-wasm.yml").read_text(encoding="utf-8")
        self.assertIn("LEGACY_GL_EMULATION", workflow)

    def test_alternative_ci_uses_jorio_ports_script(self):
        """The CI workflow must invoke jorio_ports.py, not ports.py."""
        workflow = (ROOT / ".github" / "workflows" / "build-jorio-wasm.yml").read_text(encoding="utf-8")
        self.assertIn("jorio_ports.py", workflow)

    def test_alternative_ci_does_not_use_original_ports_script(self):
        """build-jorio-wasm.yml must not call the original scripts/ports.py."""
        workflow = (ROOT / ".github" / "workflows" / "build-jorio-wasm.yml").read_text(encoding="utf-8")
        # It should reference jorio_ports.py but NOT scripts/ports.py for the
        # build matrix or run steps (to avoid building the wrong ports).
        import re
        ports_calls = re.findall(r'python3 scripts/ports\.py', workflow)
        self.assertEqual(
            ports_calls, [],
            "build-jorio-wasm.yml should not call scripts/ports.py (use jorio_ports.py)",
        )

    def test_docs_jorio_hub_page_exists(self):
        """The alternative site hub page must exist at docs-jorio/index.html."""
        hub = ROOT / "docs-jorio" / "index.html"
        self.assertTrue(hub.is_file(), f"Not found: {hub}")

    def test_docs_jorio_hub_mentions_all_games(self):
        """The alternative hub must mention all 8 game display names."""
        hub = (ROOT / "docs-jorio" / "index.html").read_text(encoding="utf-8")
        display_names = [p["display_name"] for p in jorio_ports.PORTS]
        for name in display_names:
            with self.subTest(game=name):
                self.assertIn(name, hub, f"Hub page does not mention: {name!r}")

    def test_docs_jorio_hub_mentions_legacy_gl(self):
        """The alternative hub must explain the LEGACY_GL_EMULATION approach."""
        hub = (ROOT / "docs-jorio" / "index.html").read_text(encoding="utf-8")
        self.assertIn("LEGACY_GL_EMULATION", hub)

    def test_docs_jorio_hub_references_d3d9_webgl(self):
        """The hub should cross-reference the d3d9-webgl project concept."""
        hub = (ROOT / "docs-jorio" / "index.html").read_text(encoding="utf-8")
        self.assertIn("d3d9-webgl", hub)

    def test_docs_jorio_hub_mentions_jorio_github(self):
        """The hub must credit jorio's GitHub as the source of the game code."""
        hub = (ROOT / "docs-jorio" / "index.html").read_text(encoding="utf-8")
        self.assertIn("github.com/jorio", hub)

    def test_docs_jorio_hub_links_to_primary_site(self):
        """The alternative hub must link back to the primary site."""
        hub = (ROOT / "docs-jorio" / "index.html").read_text(encoding="utf-8")
        self.assertIn("../", hub)  # relative path to parent (primary site)

    def test_matrix_output_contains_all_games(self):
        """jorio_ports.matrix('wasm') must emit entries for all 8 games."""
        import json
        import io
        from contextlib import redirect_stdout

        output = io.StringIO()
        with redirect_stdout(output):
            sys.argv = ["jorio_ports.py", "matrix", "wasm"]
            # Call the internal function directly instead of subprocess
            matrix = jorio_ports._matrix_for("wasm")

        self.assertIn("include", matrix)
        names = {e["name"] for e in matrix["include"]}
        self.assertEqual(names, EXPECTED_GAMES)

    def test_wasm_outputs_all_reference_build_wasm_subdir(self):
        """All wasm_outputs entries must be in the build-wasm/ subdirectory."""
        for port in jorio_ports.PORTS:
            with self.subTest(port=port["name"]):
                for output in port["wasm_outputs"]:
                    self.assertTrue(
                        output.startswith("build-wasm/"),
                        f"{port['name']} wasm_output {output!r} is not in build-wasm/",
                    )

    def test_readme_exists(self):
        """jorio-ports/README.md must exist."""
        readme = ROOT / "jorio-ports" / "README.md"
        self.assertTrue(readme.is_file(), f"Not found: {readme}")

    def test_readme_mentions_legacy_gl(self):
        """README must explain the LEGACY_GL_EMULATION approach."""
        readme = (ROOT / "jorio-ports" / "README.md").read_text(encoding="utf-8")
        self.assertIn("LEGACY_GL_EMULATION", readme)

    def test_readme_mentions_d3d9_webgl(self):
        """README must cross-reference d3d9-webgl as inspiration."""
        readme = (ROOT / "jorio-ports" / "README.md").read_text(encoding="utf-8")
        self.assertIn("d3d9-webgl", readme)


if __name__ == "__main__":
    unittest.main()

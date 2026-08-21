# Pangea-Ports

This repository is a **monorepo import** of Lachlan Wright's eight public `-Android` Pangea game ports so they can be modified together in a single coding session without using git submodules.

## Imported games

The source trees are copied into [`games/`](./games):

- `games/BillyFrontier-Android`
- `games/Bugdom-android`
- `games/Bugdom2-Android`
- `games/CroMagRally-Android`
- `games/MightyMike-Android`
- `games/Nanosaur-android`
- `games/Nanosaur2-Android`
- `games/OttoMatic-Android`

## Monorepo CI/CD

Repository-level workflows live in [`.github/workflows`](./.github/workflows) and intentionally replace the per-repository CI that existed in some source repos.

### PR validation (`pull_request` to `dev` or `main`)

- Linux native build smoke tests for all eight games
- Static monorepo validation of:
  - `scripts/ports.py` hook/staging metadata
  - GitHub Pages staging layout expectations
  - the root TabView landing page and Billy Frontier hook docs
  - Bugdom 2 Android icon wiring
- Existing browser smoke tests reused where they already exist:
  - `Bugdom-android/test_wasm_browser.py`
  - `Bugdom2-Android/tests/playwright/docs.spec.js`
- Android emulator launch smoke for `Bugdom2-Android`

### Build / publish workflow

The build and release pipeline is split into two dedicated workflows:

#### WASM pipeline (`build-wasm.yml`)
- Triggers automatically on push to `main`/`master` and on tags
- WebAssembly builds for all eight games (matrix strategy, independent per-game jobs)
- WASM artifacts uploaded per game as zipped site bundles
- **GitHub Pages deployment on `main`/`master` only**: deploys the assembled multi-game site
  to GitHub Pages when **all** WASM builds succeed (no deployment if any game fails)
- Optional GitHub release publishing on tag push or `workflow_dispatch`

#### Android APK pipeline (`build-android-apk.yml`)
- Triggers automatically on push to `main`/`master` and on tags
- Currently builds only Bugdom 2 (the only imported game with a checked-in Gradle project)
- APK artifacts (debug + release) uploaded per game
- Optional GitHub release publishing on tag push or `workflow_dispatch`

#### Legacy combined workflow (`build-and-release.yml`)
- Manual-only (`workflow_dispatch`), no automatic triggers
- Retained for backwards compatibility; use the dedicated workflows above instead

> This monorepo now includes a shared upstream `Pomme` dependency checkout at [`extern/Pomme`](./extern/Pomme), with each imported game exposing it through its existing `extern/Pomme` path. This keeps the vendored game trees stable while making the original upstream native/WASM build flows reproducible from the monorepo again.

## Standardized editor/testing terminology

The imported ports do not yet share a single internal implementation for browser/editor controls, so this repo standardizes the **terminology** first at the monorepo layer:

- **skip to level**: launch directly into a playable level, bypassing title/menu flow when the game supports it
- **terrain/map override**: inject custom level data for editor-driven testing
- **browser smoke test**: automated headless validation that the hosted shell loads and does not immediately fail

Current level-skip status:

| Game | Current web entrypoint | Native entrypoint | Status |
| --- | --- | --- | --- |
| BillyFrontier-Android | `?level=N` | `--level N` | Supported |
| Bugdom-android | `?level=N` | — | Supported |
| Bugdom2-Android | `?level=N` | `--level N` | Supported |
| CroMagRally-Android | `?track=N&car=N` | `--track N --car N` | Partial |
| MightyMike-Android | `?level=SCENE:AREA` | `--level SCENE:AREA` | Supported |
| Nanosaur-android | `?level=N&skipMenu=1` | `--level N --skip-menu` | Supported |
| Nanosaur2-Android | `?level=N` | `--level N` | Supported |
| OttoMatic-Android | `?level=N` | `--level N` | Supported |

The repo-level metadata used by CI is stored in [`scripts/ports.py`](./scripts/ports.py).

## Local build workflows

From the repository root that contains `scripts/build-pangea-ports.sh`:

```bash
scripts/build-pangea-ports.sh --list
scripts/build-pangea-ports.sh --target wasm
scripts/build-pangea-ports.sh --target wasm --game ottomatic
scripts/build-pangea-ports.sh --target native --game Nanosaur-android
scripts/build-pangea-ports.sh --target android --dry-run
```

Supported targets are `wasm`, `native`/`desktop`, and `android`. WASM output is
staged under `frontend/public/generated/pangea-ports/wasm/<game>/`. Native builds
use each port's existing CMake build directory. Android builds run the checked-in
Gradle wrapper under each game that has an `android/` project and write APKs under
that project's normal `app/build/outputs/apk/` directory.

Use `--dry-run` to print the planned commands and output paths without building,
`--verbose` for shell tracing, and `--check-env` to validate the toolchain for a
target. WASM builds require an active Emscripten SDK (`emcc` and `emcmake`);
without `--check-env`, the script can bootstrap `emsdk` into `.emsdk` when `emcc`
is missing. Android checks require Java plus `ANDROID_HOME` or `ANDROID_SDK_ROOT`;
an explicit NDK environment variable is recommended.

Profiling and cache-parity scenarios are documented in
[`docs/wasm-performance-profiling.md`](./docs/wasm-performance-profiling.md).
Set `PANGEA_FORCE_CACHE_MISS=1` in the game runtime environment to compare cached
rendering against the streaming miss path.

Lua-authored game scripting is documented in
[`docs/lua-scripting.md`](./docs/lua-scripting.md). All eight adapters expose
the shared Lua runtime behind `PANGEA_ENABLE_SCRIPTING=ON`; the default build
keeps scripting disabled.

## GitHub Pages layout

The build workflow assembles a single Pages site with one subdirectory per game:

- `/BillyFrontier-Android/`
- `/Bugdom-android/`
- `/Bugdom2-Android/`
- `/CroMagRally-Android/`
- `/MightyMike-Android/`
- `/Nanosaur-android/`
- `/Nanosaur2-Android/`
- `/OttoMatic-Android/`

The root landing page source is [`docs/index.html`](./docs/index.html).

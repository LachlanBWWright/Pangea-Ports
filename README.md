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

- WebAssembly builds for all eight games
- WASM artifacts uploaded per game
- GitHub Pages artifact assembled as a multi-game site
- Optional GitHub release publishing on tag push or manual dispatch

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

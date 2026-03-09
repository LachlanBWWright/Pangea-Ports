# Session 7 — 2026-03-09T01:14:24Z

## Summary

This session:
1. Split the CI/CD pipeline into separate workflows for WASM builds and Android APK builds.
2. Fixed visual errors in docs pages (missing `screenshot.webp`/`screenshot.png` references).
3. Captured 'skip to level' screenshots and 3-level screenshots for all 8 games (32 total).

## CI/CD Pipeline Changes

### New workflows

- **`.github/workflows/build-wasm.yml`** — Builds all 8 WASM targets. Deploys to GitHub
  Pages automatically when **all** WASM builds succeed on `main`/`master`. Also supports
  publishing WASM `.zip` artifacts to a GitHub release when triggered by a tag or manual dispatch.
- **`.github/workflows/build-android-apk.yml`** — Builds Bugdom 2 Android APKs (`assembleDebug`
  + `assembleRelease`). Only runs if the `android_apk` matrix is non-empty (currently only Bugdom 2
  has a checked-in Gradle project). Supports publishing APKs to a GitHub release on tag/dispatch.

### Updated workflow

- **`.github/workflows/build-and-release.yml`** — Converted to legacy/manual-only combined
  workflow (`workflow_dispatch` only, no automatic push triggers). The two dedicated workflows
  above are the primary pipelines going forward.

## Visual Error Fixes

The following docs pages referenced missing hero images that showed as broken:

| File fixed | Image added |
|-----------|-------------|
| `games/BillyFrontier-Android/docs/index.html` | `docs/screenshot.webp` (from skip-to-level shot) |
| `games/Bugdom-android/docs/index.html` | `docs/screenshot.webp` (from skip-to-level shot) |
| `games/CroMagRally-Android/docs/index.html` | `docs/screenshot.webp` (from skip-to-level shot) |
| `games/Nanosaur-android/docs/index.html` | `docs/screenshot.png` (from skip-to-level shot) |

## Screenshots (32 total)

### Skip-to-level pages (1 per game = 8 total)
Full-page screenshots of each game's `docs/index.html` skip-to-level UI.

| File | Game |
|------|------|
| `billyfrontier_skip_to_level.png` | Billy Frontier |
| `bugdom_skip_to_level.png` | Bugdom |
| `bugdom2_skip_to_level.png` | Bugdom 2 |
| `cromagnrally_skip_to_level.png` | Cro-Mag Rally |
| `mightymike_skip_to_level.png` | Mighty Mike |
| `nanosaur_skip_to_level.png` | Nanosaur |
| `nanosaur2_skip_to_level.png` | Nanosaur 2 (hub page, Nanosaur 2 tab) |
| `ottomatic_skip_to_level.png` | Otto Matic (hub page, Otto Matic tab) |

### Level screenshots (3 per game = 24 total)
Each shows the docs page scrolled to show level-specific content.
All screenshots are 1280×800 pixels (viewport), except full-page hub screenshots.

| Files | Game | Content shown |
|-------|------|-------------|
| `billyfrontier_lv0/2/4.png` | Billy Frontier | Hero/header w/ screenshot, Level Editor Integration, JavaScript API |
| `bugdom_lv0/1/2.png` | Bugdom | Level Select table (levels 0–9), Editor Integration, JavaScript API |
| `bugdom2_lv0/1/2.png` | Bugdom 2 | Jump-to-Level grid (10 cards), upper dev section, JS Cheat API |
| `cromagnrally_track1/3/5.png` | Cro-Mag Rally | Track/car selector UI, Level Editor API URL params, Custom Level File |
| `mightymike_lv0/1/2.png` | Mighty Mike | Game canvas area, dev panel (Level Navigation), URL reference examples |
| `nanosaur_lv0/1/2.png` | Nanosaur | Top/overview, Level Editor Integration, URL params |
| `nanosaur2_lv0/1/2.png` | Nanosaur 2 | Hub top (tab buttons), game panel header, detail cards |
| `ottomatic_lv0/1/2.png` | Otto Matic | Hub top (tab buttons), game panel header, detail cards |

All screenshot triplets verified to show distinct content (minimum pixel difference > 3.0).

## Visual Review Notes

- **Fixed**: BillyFrontier, Bugdom, CroMag Rally, and Nanosaur docs pages had broken hero image
  references (`screenshot.webp` / `screenshot.png` missing). Fixed by generating WebP/PNG crops
  from the skip-to-level screenshots and placing them in each game's `docs/` directory.
- **Nanosaur 2 and Otto Matic** have no dedicated per-game docs pages (single-site model).
  Screenshots show the hub page with the appropriate tab activated for distinct content.
- All screenshots are 1280px wide; viewport shots are 1280×800, full-page shots are taller.


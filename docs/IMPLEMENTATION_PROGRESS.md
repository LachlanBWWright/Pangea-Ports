# Pangea Ports Follow-up Progress

Started: 2026-03-07T11:14:59Z

## Checklist

- [x] Re-inspect the monorepo, Pages workflow, and current game hook implementations
- [x] Identify current GitHub Pages staging/layout mismatches in imported WASM docs
- [x] Update monorepo Pages deployment and WASM staging so the deployed site works
- [x] Replace the root WASM landing page with a tabbed game selector UI
- [x] Standardize Billy Frontier onto query-string / CLI direct-launch conventions
- [x] Improve current Android app polish for Bugdom 2, including round launcher resources and touch-oriented input tuning
- [x] Run targeted validation and capture fresh screenshots
- [ ] Run final automated review and security checks

## Notes

- The existing monorepo workflow already deployed Pages, but some per-game docs assumed different output layout conventions (`game/` subdir or renamed HTML entrypoint) than the original monorepo staging code.
- `scripts/ports.py` now stages WASM files using per-game layout metadata so GitHub Pages matches each game's existing shell/docs expectations.
- Billy Frontier already had low-level WASM launch and terrain hooks; this follow-up standardizes them onto `?level=` / `?terrainFile=` and `--level` / `--terrain-file`.
- Bugdom 2 already had launcher icons checked in, so this pass focused on explicit round-icon resources plus Android-specific deadzone tuning for better virtual-stick responsiveness.
- Fresh UI screenshot for PR use: https://github.com/user-attachments/assets/c6430c5a-4049-42ce-a870-71658b20ba9b

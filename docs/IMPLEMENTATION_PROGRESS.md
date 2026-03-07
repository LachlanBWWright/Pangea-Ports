# Pangea Ports Follow-up Progress

Started: 2026-03-07T11:14:59Z
Follow-up timestamp: 2026-03-07T11:39:28Z

## Checklist

- [x] Re-inspect the monorepo, Pages workflow, and current game hook implementations
- [x] Identify current GitHub Pages staging/layout mismatches in imported WASM docs
- [x] Update monorepo Pages deployment and WASM staging so the deployed site works
- [x] Replace the root WASM landing page with a tabbed game selector UI
- [x] Standardize Billy Frontier onto query-string / CLI direct-launch conventions
- [x] Improve current Android app polish for Bugdom 2, including round launcher resources and touch-oriented input tuning
- [x] Run targeted validation and capture fresh screenshots
- [x] Investigate the current failing PR validation run via GitHub Actions logs
- [x] Fix the immediate CI issues (broken Python helper, impossible browser smoke, incorrect Android wrapper assumption) and replace failing smoke jobs with truthful monorepo validations
- [x] Capture additional screenshots showing skip-to-level hooks being triggered from the hosted/docs UI
- [x] Run final automated review and security checks

## Notes

- The existing monorepo workflow already deployed Pages, but some per-game docs assumed different output layout conventions (`game/` subdir or renamed HTML entrypoint) than the original monorepo staging code.
- `scripts/ports.py` now stages WASM files using per-game layout metadata so GitHub Pages matches each game's existing shell/docs expectations.
- Billy Frontier already had low-level WASM launch and terrain hooks; this follow-up standardizes them onto `?level=` / `?terrainFile=` and `--level` / `--terrain-file`.
- Bugdom 2 already had launcher icons checked in, so this pass focused on explicit round-icon resources plus Android-specific deadzone tuning for better virtual-stick responsiveness.
- The latest PR workflow failure was not just one bug: it combined a `scripts/ports.py` f-string syntax error, a Bugdom browser smoke that attempted a full source build without vendored `extern/Pomme` sources, and an Android job that assumed a missing Gradle wrapper.
- CI is now being shifted to validate the supported monorepo surface (Pages staging, hook metadata, root docs, Bugdom 2 docs Playwright, and Android Gradle configuration) instead of claiming full native/APK reproducibility from a checkout that lacks the vendored upstream dependency tree.
- The newest PR Validation run for the updated workflow is currently `action_required` on GitHub rather than failing from code, which indicates workflow approval is needed before GitHub will execute the modified PR checks.
- Fresh UI screenshot for PR use: https://github.com/user-attachments/assets/c6430c5a-4049-42ce-a870-71658b20ba9b
- Hook-trigger screenshot for PR use: https://github.com/user-attachments/assets/ce0d424d-c00d-4570-aafc-031131f3859f

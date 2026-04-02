# Level Editor Launch Matrix

This is the current launcher-oriented mapping used by `/docs/index.html`.

| Game | Direct launch mode | Upload strategy | Notes |
| --- | --- | --- | --- |
| Billy Frontier | `--level N` | Write to `/Data/Terrain/...`, pass `--terrain-file :Terrain:...` | 12 duel/shootout presets |
| Bugdom | `--level N` | Write to `/Data/Terrain/...`, pass `--terrain-file :Terrain:...` | 10 campaign levels |
| Bugdom 2 | `--level N` | Replace exact `/Data/Terrain/...` file before boot | 10 levels, no separate override flag in launcher flow |
| Cro-Mag Rally | `--track N --car 0` | Write to `/Data/Terrain/...`, pass `--level-override :Terrain:...` | 17 track presets including battle tracks |
| Mighty Mike | `--level scene:area` | Write to `/Data/Maps/...`, pass `--map-override :Maps:...` | Scene/area launcher presets |
| Nanosaur | `--level 0 --skip-menu` | Write to `/Data/Terrain/...`, pass `--terrain-file /Data/Terrain/...` | Normal boot is also supported |
| Nanosaur 2 | `--level N` | Write to `/Data/Terrain/...`, pass `--terrain-override /Data/Terrain/...` | Includes adventure, race, battle, and capture-the-flag presets |
| Otto Matic | `--level N` | Write to `/Data/Terrain/...`, pass `--terrain /Data/Terrain/...` | 10 campaign presets |

## Recommended editor flow

1. Pick a game in the shared launcher.
2. Choose a preset level/track or use **Start normally**.
3. If the editor has a custom level file, upload it before launch.
4. Set the virtual filesystem path that should be replaced.
5. If the game supports explicit override arguments, keep or adjust the suggested launch override path.
6. Launch the game directly in the shared canvas.

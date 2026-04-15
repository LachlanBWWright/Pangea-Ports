# Level Editor Integration Guide

This document describes how to embed Pangea Ports WebAssembly games into an external
level editor (or any other third-party page) and control them programmatically via the
`window.PangeaGame` JavaScript API.

---

## Quick start

Every game shell page supports an **embed mode** that hides all project UI (header,
footer, debug panels) and makes the canvas fill the iframe:

```html
<iframe id="game"
        src="https://<your-host>/BillyFrontier-Android/game/billyfrontier.html?embed=1"
        allow="fullscreen"
        style="width:100%;height:600px;border:none">
</iframe>
```

Add `?embed=1` to the URL of any per-game shell page (see the table below).  
In this mode only the canvas and the minimal toolbar (mute / fullscreen / fences) are
visible.  Hide the toolbar too by appending `&no-toolbar=1` and adding this to the
shell's `startGame()` block:

```javascript
if (new URLSearchParams(window.location.search).get('no-toolbar') === '1') {
  document.getElementById('toolbar').style.display = 'none';
}
```

---

## Per-game shell URLs

| Game | Shell URL (relative to repo root) | Embed URL |
|------|----------------------------------|-----------|
| Billy Frontier | `games/BillyFrontier-Android/game/billyfrontier.html` | `…?embed=1` |
| Bugdom | `games/Bugdom-android/docs/shell.html` | `…?embed=1` |
| Bugdom 2 | `games/Bugdom2-Android/shell.html` | `…?embed=1` |
| Cro-Mag Rally | `games/CroMagRally-Android/packaging/emscripten/shell.html` | `…?embed=1` |
| Mighty Mike | `games/MightyMike-Android/docs/index.html` | `…?embed=1` |
| Nanosaur | `games/Nanosaur-android/packaging/emscripten/shell.html` | `…?embed=1` |
| Nanosaur 2 | `games/Nanosaur2-Android/packaging/wasm/shell.html` | `…?embed=1` |
| Otto Matic | `games/OttoMatic-Android/docs/shell.html` | `…?embed=1` |

---

## The `window.PangeaGame` API

Once the WASM runtime has finished initialising, every game exposes a `window.PangeaGame`
object with a **consistent method set**.  All methods are safe to call from the browser
console or from a same-origin script injected into the shell page.

### Shared methods (all games)

| Method | Description |
|--------|-------------|
| `PangeaGame.skipToLevel(n)` | Jump to level `n` (0-based integer, or `"scene:area"` for Mighty Mike). Some games perform a full page reload; others switch levels at runtime. |
| `PangeaGame.getCurrentLevel()` | Returns the currently active level number (or URL param if only page-load skip is supported). |
| `PangeaGame.setFenceCollisions(enabled)` | Enable (`true`) or disable (`false`) fence collision. Useful when testing level layout without getting stuck. |
| `PangeaGame.getFenceCollisions()` | Returns `true` if fence collisions are currently enabled. |

### Game-specific methods

#### Billy Frontier

| Method | Description |
|--------|-------------|
| `PangeaGame.setTerrainOverride(path)` | Set a colon-style terrain override path (e.g. `:Terrain:custom.ter`) for the next level load. |
| `PangeaGame.loadTerrainData(data, length)` | Upload raw terrain bytes directly from JavaScript (UInt8Array + byte length). |

#### Bugdom

| Method | Description |
|--------|-------------|
| `PangeaGame.setTerrainOverride(path)` | Set a terrain override path before the next level load. |

#### Bugdom 2

| Method | Description |
|--------|-------------|
| `PangeaGame.winLevel()` | Trigger an immediate level-complete event. |

#### Cro-Mag Rally

`skipToLevel(n)` takes a 1-based track number (1–17, where 10–17 are battle arenas).

#### Mighty Mike

`skipToLevel(s)` accepts a `"scene:area"` string (e.g. `"1:2"`) **or** a flat
0-based integer (automatically converted to scene:area).

#### Nanosaur 2

| Method | Description |
|--------|-------------|
| `PangeaGame.setTerrainPath(path)` | Set a VFS path to a `.ter` override file before the next level load. |

#### Otto Matic

| Method | Description |
|--------|-------------|
| `PangeaGame.setTerrainPath(path)` | Set a VFS path to a `.ter` override file before the next level load. |
| `PangeaGame.setFenceCollisions(enabled)` | Enable/disable fence collision. |
| `PangeaGame.setSpeedMultiplier(mult)` | Multiply player movement speed (1.0 = normal). |
| `PangeaGame.getPlayerHealth()` | Returns current player health (number). |
| `PangeaGame.getPlayerLives()` | Returns current player lives count. |

---

## Hosting requirements

The game assets (`.wasm`, `.data`, JS loader) must be served from the **same origin**
as the embedding page, or the server must supply permissive CORS headers.  Additionally,
games that use Emscripten pthreads require:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

These headers must be present on every page that loads the WASM, including any parent
page that `<iframe>`-embeds the shell.

---

## Injecting custom level files

Write a file into the Emscripten virtual filesystem before the game boots using
`Module.preRun`:

```javascript
// Inside the shell page (or via postMessage + bridge if cross-origin):
Module.preRun = Module.preRun || [];
Module.preRun.push(function() {
  // data is a Uint8Array of your level file
  Module.FS.writeFile('/Data/Terrain/CustomLevel.ter', data);
});
```

Then call the game's terrain override method (`PangeaGame.setTerrainOverride` /
`PangeaGame.setTerrainPath`) with the matching VFS path so the game reads your file
instead of the bundled one.

### Cross-origin iframe approach

If the shell is loaded in a cross-origin iframe, use `postMessage` to pass the file:

```javascript
// Parent page
var frame = document.getElementById('game');
frame.addEventListener('load', function() {
  // Fetch or generate level data
  var data = new Uint8Array(myLevelArrayBuffer);
  frame.contentWindow.postMessage({
    type: 'loadTerrain',
    path: '/Data/Terrain/custom.ter',
    data: data.buffer
  }, '*', [data.buffer]);
});
```

Then add a `message` listener inside the shell:

```javascript
window.addEventListener('message', function(evt) {
  if (!evt.data || typeof evt.data !== 'object') return;
  if (evt.data.type === 'loadTerrain') {
    var bytes = new Uint8Array(evt.data.data);
    Module.FS.writeFile(evt.data.path, bytes);
    PangeaGame.setTerrainPath(evt.data.path);
  }
});
```

---

## Requesting fullscreen from the parent page

When the game is in an `<iframe>`, call `requestFullscreen()` on the **iframe element**
(not on the canvas directly, as cross-origin iframes cannot request fullscreen on inner
elements):

```javascript
document.getElementById('game').requestFullscreen();
```

The shell's `syncCanvasSize` listener fires on `fullscreenchange` and resizes the canvas
to fill the viewport automatically.

---

## Maintaining the 4:3 aspect ratio

Most games render at 4:3 (e.g. 1280×960 or 1280×720).  Size the iframe accordingly:

```css
iframe#game {
  width: 100%;
  aspect-ratio: 4 / 3;
}
```

---

## Per-game level numbering reference

### Billy Frontier (0-based `--level`)
| # | Level |
|---|-------|
| 0 | Town Duel 1 |
| 1 | Town Shootout |
| 2 | Town Duel 2 |
| 3 | Town Stampede |
| 4 | Town Duel 3 |
| 5 | Target Practice 1 |
| 6 | Swamp Duel 1 |
| 7 | Swamp Shootout |
| 8 | Swamp Duel 2 |
| 9 | Swamp Stampede |
| 10 | Swamp Duel 3 |
| 11 | Target Practice 2 |

### Bugdom (0-based)
| # | Level |
|---|-------|
| 0 | Training |
| 1 | Lawn |
| 2 | Pond |
| 3 | Forest |
| 4 | Hive Attack |
| 5 | Bee Hive |
| 6 | Queen Bee |
| 7 | Night Attack |
| 8 | Ant Hill |
| 9 | Ant King |

### Bugdom 2 (0-based)
| # | Level | Terrain file |
|---|-------|-------------|
| 0 | Gnome Garden | `/Data/Terrain/Level1_Garden.ter` |
| 1 | Sidewalk | `/Data/Terrain/Level2_SideWalk.ter` |
| 2 | Fido | `/Data/Terrain/Level3_DogHair.ter` |
| 3 | Plumbing | — |
| 4 | Playroom | `/Data/Terrain/Level5_Playroom.ter` |
| 5 | Closet | `/Data/Terrain/Level6_Closet.ter` |
| 6 | Gutter | — |
| 7 | Garbage | `/Data/Terrain/Level8_Garbage.ter` |
| 8 | Balsa Plane | `/Data/Terrain/Level9_Balsa.ter` |
| 9 | Park | `/Data/Terrain/Level10_Park.ter` |

### Cro-Mag Rally (1-based `--track`)
| # | Level | Type |
|---|-------|------|
| 1–3 | Stone Age | Race |
| 4–6 | Bronze Age | Race |
| 7–9 | Iron Age | Race |
| 10–17 | Various | Battle |

### Mighty Mike (`scene:area`, 0-based)
Scenes 0–4, areas 0–2 within each scene.

### Nanosaur
Level 0 is the only playable level. Pass `--level 0 --skip-menu` to bypass the title sequence.

### Nanosaur 2 (0-based)
| # | Level |
|---|-------|
| 0–2 | Adventure 1–3 |
| 3–4 | Race 1–2 |
| 5–6 | Battle 1–2 |
| 7–8 | Capture the Flag 1–2 |

### Otto Matic (0-based)
| # | Level | Terrain file |
|---|-------|-------------|
| 0 | Earth Farm | `/Data/Terrain/EarthFarm.ter` |
| 1 | Blob World | `/Data/Terrain/BlobWorld.ter` |
| 2 | Blob Boss | `/Data/Terrain/BlobBoss.ter` |
| 3 | Apocalypse | `/Data/Terrain/Apocalypse.ter` |
| 4 | Cloud | `/Data/Terrain/Cloud.ter` |
| 5 | Jungle | `/Data/Terrain/Jungle.ter` |
| 6 | Jungle Boss | `/Data/Terrain/JungleBoss.ter` |
| 7 | Fire & Ice | `/Data/Terrain/FireIce.ter` |
| 8 | Saucer | `/Data/Terrain/Saucer.ter` |
| 9 | Brain Boss | `/Data/Terrain/BrainBoss.ter` |

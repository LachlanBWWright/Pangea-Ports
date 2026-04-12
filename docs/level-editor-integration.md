# Level Editor Integration Plan

This repository now exposes a GitHub Pages launcher flow that is compatible with an external level editor without embedding the repo's pages in an iframe.

## Browser launcher model

- The shared launcher lives in `/docs/index.html`.
- Picking a game no longer boots it immediately.
- Each game now has launcher actions for:
  - starting normally
  - direct-launching known levels/tracks
  - optionally injecting a level file before the WASM module starts
- The launcher passes direct-boot data through `Module.arguments`, so the raw game code receives the same command-line-style inputs used by native builds.

## File injection model

- The launcher reads an uploaded file in JavaScript before the game starts.
- `Module.preRun` writes the file into Emscripten's virtual filesystem before game initialization.
- Games that support an explicit override flag also receive the matching path argument automatically.
- Games that load terrain by filename only, such as Bugdom 2, instead rely on replacing the exact bundled file path in the virtual filesystem.

## Per-game path conventions

- Some games want colon paths like `:Terrain:Custom.ter` or `:Maps:custom.map-1`.
- Others consume direct virtual filesystem paths such as `/Data/Terrain/Custom.ter`.
- The launcher keeps both concepts separate:
  - **Virtual FS target path** = where the file is written
  - **Launch override path** = what gets passed to the game if an override argument is needed

## Resize/fullscreen handling

- The shared launcher canvas uses `ResizeObserver`, viewport/fullscreen listeners, and repeated resize syncs across fullscreen transitions.
- Canvas pixel dimensions are derived from CSS size × device pixel ratio.
- `Module.setCanvasSize` is called when available so SDL/Emscripten stay aligned with the visible canvas.

## Game-specific note

- `games/Nanosaur-android/src/Boot.cpp` no longer forces every web launch to skip directly into gameplay.
- Normal menu boot is now preserved unless a level-editor launch option explicitly requests direct gameplay.

---

## Embedding a game directly into an external site

Each per-game `packaging/shell.html` supports an **embedded mode** that hides all
surrounding content (header, level-editor debug panel, footer) and makes the
canvas fill the available viewport.  This is the recommended integration
mechanism for an external level editor.

### 1. Hosting requirements

The game assets (`.wasm`, `.data`, JS loader) must be hosted on the **same
origin** as the page that loads them, or the server must send permissive CORS
headers.  Additionally, many games rely on Emscripten's pthreads support which
requires:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

These headers are needed on every page that loads the WASM, including any
parent page that `<iframe>`-embeds the shell.

### 2. Activating embedded mode

Add `?embed=1` to the URL of the shell page:

```
https://your-site.example.com/Bugdom2/index.html?embed=1
```

In this mode:
- The header, footer, and debug/API panel are hidden via CSS.
- The canvas expands to fill `100dvh` / `100%` of the surrounding container.
- The minimal toolbar (mute, fullscreen, fences) remains visible at the bottom
  of the canvas; remove it in your own page if desired (see §5 below).

### 3. Injecting game configuration before launch

The shell exposes `window.Module` once the WASM runtime is initialised.  Write
to it *before* the runtime fires using `Module.preRun`:

```html
<!-- In your parent page, load the game shell into an iframe -->
<iframe id="game" src="https://your-site.example.com/Bugdom2/index.html?embed=1"
        allow="fullscreen" style="width:100%;height:600px;border:none"></iframe>

<script>
// Communicate with the iframe via postMessage once it is ready
var frame = document.getElementById('game');
frame.addEventListener('load', function() {
  // Tell the embedded shell which level to start on
  frame.contentWindow.postMessage({ type: 'skipToLevel', level: 3 }, '*');
});
</script>
```

The shell page must implement a `message` listener for these events if you need
them.  The existing `ccall` wrappers in the shell (e.g. `SetStartLevel`,
`UploadTerrainFile`) can be wired up to postMessage in a thin bridge layer:

```javascript
// Add inside shell.html's <script> block if postMessage control is needed
window.addEventListener('message', function(evt) {
  if (!evt.data || typeof evt.data !== 'object') return;
  switch (evt.data.type) {
    case 'skipToLevel':
      try { Module.ccall('SetStartLevel', null, ['number'], [evt.data.level]); }
      catch (e) { console.warn(e); }
      break;
    // … add other cases as required
  }
});
```

### 4. Uploading terrain / map files from the parent page

The virtual filesystem can be written to from within the same JS context:

```javascript
// Inside the shell page (or via postMessage + bridge):
var data = new Uint8Array(arrayBuffer);
Module.FS.writeFile('/Data/Terrain/CustomLevel.ter', data);
```

If loading the shell in a cross-origin `<iframe>`, use `postMessage` to pass
the file contents as a transferable `ArrayBuffer`.

### 5. Hiding the embedded toolbar

The minimal toolbar (mute / fullscreen / fences) is always visible unless you
override the CSS from the parent page.  The simplest approach is to target the
iframe's document directly if same-origin, or to pass a CSS class via
`postMessage`.  Alternatively, the shell already reads `?embed=1`; you can add
a second parameter such as `?embed=1&no-toolbar=1` and extend the script block
in `shell.html` to hide `#toolbar` accordingly:

```javascript
// In shell.html startGame():
if (new URLSearchParams(window.location.search).get('no-toolbar') === '1') {
  document.getElementById('toolbar').style.display = 'none';
}
```

### 6. Fullscreen from the parent page

When the game is embedded in an `<iframe>` the canvas can be made fullscreen
by the parent using the Fullscreen API on the **iframe element** rather than on
the canvas directly (cross-origin frames cannot request fullscreen on behalf of
inner elements):

```javascript
// In the parent page
document.getElementById('game').requestFullscreen();
```

The shell's `syncCanvasSize` listener will fire on the `fullscreenchange` event
inside the iframe and resize the canvas to fill the viewport.

### 7. Canvas sizing in an `<iframe>`

The embedded shell sets `body.embedded-shell #canvas { width:100%; height:100dvh; }`.
Size the `<iframe>` to whatever dimensions you want the game canvas to occupy –
the canvas will fill the iframe automatically.

To maintain the native 4:3 aspect ratio (960 × 720) you can constrain the
iframe height from the parent:

```css
iframe#game {
  width: 100%;
  aspect-ratio: 4 / 3;
}
```


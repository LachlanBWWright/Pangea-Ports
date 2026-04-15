# Cro-Mag Rally WebAssembly Multiplayer — Research Notes

## Background

The original Cro-Mag Rally (2000) shipped with full LAN/internet multiplayer via
Apple's **NetSprocket** API, supporting up to four simultaneous players across four
game modes: Race, Tag, Time Trial, and Battle.  The source code in `Source/System/network.c`
(~1,500 lines) still contains the original NetSprocket logic, but every function body is
wrapped in `#if 0` / `IMPLEMENT_ME_SOFT()` stubs that are unreachable in the current
WebAssembly build.

---

## Original networking architecture

### Protocol style: **deterministic lockstep**

The existing message structures in `Source/Headers/network.h` reveal a
**lockstep deterministic** design — not a state-synchronisation approach:

| Message type | Direction | Contents |
|---|---|---|
| `kNetConfigureMessage` | Host → clients | Game mode, track, difficulty, tag duration, player count |
| `kNetSyncMessage` | Any | Frame-counter based barrier sync |
| `kNetHostControlInfoMessage` | Host → clients | **All** players' input bits + analog steering + RNG seed + FPS |
| `kNetClientControlInfoMessage` | Client → host | Single player's input bits + analog steering |
| `kNetPlayerCharTypeMessage` | Any → all | Vehicle type + sex selection |

Key design points:

- The **host collects every client's input**, bundles all inputs into one authoritative
  broadcast, and clients wait for this packet before advancing the simulation.
- `randomSeed` is included in the host broadcast for error-checking — the game is intended
  to produce identical results on all machines given identical inputs and the same initial
  RNG state.
- `fps` and `fpsFrac` are sent so clients can use the host's delta-time rather than
  their own, keeping physics identical across machines.
- A `frameCounter` on both sides guards against out-of-order or dropped packets.

This architecture means the game simulation code itself does **not** need modification
to support multiplayer — only the transport layer (currently `IMPLEMENT_ME_SOFT()`)
needs to be replaced.

---

## What blocks a straightforward port

### 1. NetSprocket is Mac-only and long dead

NetSprocket was removed from macOS 10.7 (2011).  There is no drop-in replacement.
The `#include <NetSprocket.h>` is commented out in network.h and the code makes
no direct syscalls.

### 2. Raw sockets are unavailable in browser WASM

Standard POSIX `socket()` / `connect()` / `send()` are **not** available in Emscripten
without a custom proxy.  Emscripten does provide a BSD-socket emulation that tunnels
through WebSockets, but it requires a proxy server (`scripts/websocket-proxy.py`) and
adds latency.

### 3. No existing lobby/discovery mechanism for the web

The original discovery used Apple's NBP (Name Binding Protocol) over AppleTalk
(`kNBPType = "CMR5"`).  A browser substitute needs a signalling server.

---

## Recommended approach: WebRTC DataChannel

**WebRTC DataChannel** is the best match for this game's lockstep model:

- Peer-to-peer after initial signalling — no relay server needed during gameplay.
- `RTCDataChannel` with `ordered: false, maxRetransmits: 0` provides **UDP-like**
  semantics, suitable for the per-frame input broadcast.
- `RTCDataChannel` with `ordered: true` (the default) works like TCP for the initial
  configuration exchange.

### High-level flow

```
Player A (Host)          Signalling Server         Player B (Client)
     │                        │                          │
     │──── offer (SDP) ──────►│──────── offer ──────────►│
     │                        │◄── answer (SDP) ─────────│
     │◄─── answer ────────────│                          │
     │◄════════════ ICE candidates (STUN/TURN) ══════════│
     │                                                   │
     │◄══════════════ DataChannel established ═══════════│
     │  kNetConfigureMessage: gameMode, track, players   │
     │──────────────────────────────────────────────────►│
     │                                                   │
     │◄── kNetPlayerCharTypeMessage (vehicle/sex) ───────│
     │                                                   │
     │  [Every frame]                                    │
     │◄── kNetClientControlInfoMessage ──────────────────│
     │─── kNetHostControlInfoMessage ───────────────────►│
```

### Required C-side changes

All the stubbed functions in `network.c` need new bodies.  Each function calls
`IMPLEMENT_ME_SOFT()` and returns early — these should be replaced with calls to
a thin **WebRTC bridge** exposed via Emscripten's JavaScript interop:

```c
// New network.c approach using EM_JS / emscripten_run_script:
#include <emscripten.h>

EM_JS(void, js_net_send_host_control, (const void* data, int len), {
    if (window.__cromagNet && window.__cromagNet.sendToAll)
        window.__cromagNet.sendToAll(HEAPU8.subarray(data, data + len));
});

EM_JS(void, js_net_send_client_control, (const void* data, int len), {
    if (window.__cromagNet && window.__cromagNet.sendToHost)
        window.__cromagNet.sendToHost(HEAPU8.subarray(data, data + len));
});
```

The JavaScript side (`window.__cromagNet`) wraps the WebRTC peer connections and
exposes `sendToAll`, `sendToHost`, and a receive callback queue.

### Signalling server

A minimal Node.js WebSocket signalling server (< 100 lines) is sufficient.  It only
needs to relay SDP offers/answers and ICE candidates — no game data passes through it.

Example minimal server:
```javascript
// server.js (Node.js, using the 'ws' package)
const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 8080 });
const rooms = {};

wss.on('connection', function(ws) {
  ws.on('message', function(raw) {
    const msg = JSON.parse(raw);
    if (msg.type === 'join') {
      rooms[msg.room] = rooms[msg.room] || [];
      rooms[msg.room].push(ws);
    } else {
      // Relay to other members of the same room
      (rooms[msg.room] || []).forEach(function(peer) {
        if (peer !== ws && peer.readyState === WebSocket.OPEN)
          peer.send(raw);
      });
    }
  });
});
```

After ICE negotiation completes, all actual game traffic goes peer-to-peer — the
signalling server can be shut down or reused for the next session.

---

## Alternative: Emscripten WebSocket emulation

Emscripten includes a BSD socket → WebSocket bridge that can be compiled in by adding:

```cmake
target_link_options(CroMagRally PRIVATE
    "-lwebsocket.js"
    "-sWEBSOCKET_URL=ws://localhost:8080"
    "-sPROXY_TO_WORKER"
)
```

This lets you write standard `socket(AF_INET, SOCK_DGRAM, 0)` / `sendto()` C code
and have Emscripten tunnel it through WebSockets.  A companion `websockify` or custom
proxy converts the WebSocket framing back to UDP/TCP on the server.

**Pros:** Less JavaScript glue required; existing POSIX socket code can be re-used.  
**Cons:** Requires a always-on proxy server for all traffic (no P2P); higher latency;
the `PROXY_TO_WORKER` requirement means the game runs on a separate thread which may
conflict with SDL's event loop.

---

## Alternative: Rollback netcode (GGPO-style)

The lockstep model stalls if any player's packet is late.  A **rollback** approach
(as popularised by GGPO) would:

1. Predict remote inputs (repeat last known input).
2. Simulate ahead locally.
3. On receiving late input, detect divergence, roll back saved state, and re-simulate.

This eliminates waiting but requires:
- A full game-state snapshot/restore mechanism (`SaveGameState` / `RestoreGameState`).
- The game currently has no such mechanism — adding it would require capturing all
  dynamically allocated objects, physics state, and RNG state.

Given the game's complexity this is a significant undertaking and is not recommended
for an initial implementation.

---

## Summary of recommended implementation plan

| Step | Work | Notes |
|------|------|-------|
| 1 | Add signalling server (Node.js + ws) | ~80 lines, runs separately from the game |
| 2 | Implement `window.__cromagNet` WebRTC bridge in shell.html | ~200 lines JS; exposes `host()`, `join(roomCode)`, `sendToAll()`, `sendToHost()`, `onReceive(cb)` |
| 3 | Implement `InitNetworkManager`, `SetupNetworkHosting`, `SetupNetworkJoin` | Replace `IMPLEMENT_ME_SOFT()` with `EM_JS` bridge calls |
| 4 | Implement `HostSend_ControlInfoToClients` / `ClientSend_ControlInfoToHost` | Serialise the existing `NetHostControlInfoMessageType` / `NetClientControlInfoMessageType` structs and send via DataChannel |
| 5 | Implement `HostReceive_ControlInfoFromClients` / `ClientReceive_ControlInfoFromHost` | Drain the JS receive queue into the existing C-side buffers |
| 6 | Add a simple lobby UI in shell.html | Input for room code, Host / Join buttons; no changes to C menus required initially |

The lockstep message structures already defined in `network.h` are well-suited for
direct binary serialisation over DataChannel.  The total implementation effort
(excluding lobby UI polish) is estimated at 400–600 lines of new JavaScript and
~300 lines of C replacements in `network.c`.

---

## Limitations and risks

- **Frame-rate parity**: The host broadcasts its `fps` value; all clients use that
  delta-time.  If any client falls below the host's frame rate the lockstep stalls.
  A maximum-wait guard (e.g. 200 ms) before skipping ahead would help.
- **TURN servers**: WebRTC ICE negotiation works peer-to-peer when both sides are
  on the same LAN or have non-symmetric NAT.  For restrictive NATs a TURN relay server
  is needed (e.g. Coturn).  Free TURN services exist but are rate-limited.
- **COOP headers**: Like all pthreads WASM builds, the shell requires
  `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy:
  require-corp`.  These are already set for the GitHub Pages deployment.
- **Player limit**: The existing code supports `MAX_PLAYERS = 4`.  WebRTC mesh
  topology (each peer connected to every other) is fine at 4 players.

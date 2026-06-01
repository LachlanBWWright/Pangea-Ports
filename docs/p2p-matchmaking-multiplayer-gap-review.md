# P2P Matchmaking Multiplayer Gap Review

## Scope

This reviews the recent pangea-ports multiplayer updates against the intended production architecture:

- A dedicated service owns matchmaking, lobby state, participant identity, match configuration, WebRTC signalling, ICE server configuration, and match reporting.
- Gameplay traffic flows peer-to-peer between browsers over WebRTC DataChannels.
- The browser elected as game host runs authoritative simulation.
- The dedicated service does not relay normal gameplay packets.

This document focuses on what is still missing after the recent Cro-Mag Rally and Nanosaur 2 game-side updates.

## Current State

The recent game-side updates are directionally useful:

- Cro-Mag Rally now has an explicit packet header instead of direct raw struct packet types.
- Cro-Mag client inputs and host snapshots moved toward the unreliable channel.
- Cro-Mag no longer sends the old lockstep host-control packet every frame.
- Cro-Mag snapshots now include more gameplay-affecting player state.
- Nanosaur 2 now has explicit packet encode/decode helpers.
- Nanosaur 2 now sends action input bits for fire, jetpack, and weapon switching.
- Nanosaur 2 host input lookup now routes remote-player input through the network input adapter.
- Both games have early local correction and remote smoothing behavior.

These are not yet adequate for production P2P multiplayer. They are a prototype transport/gameplay layer, not a complete matchmade P2P system.

## Key Gap

The main missing piece is the boundary between the dedicated matchmaking/signalling service and the game runtime.

Right now the game runtime mostly derives identity from `localPlayerIndex`, `playerCount`, and `seed`. That is not enough for a P2P service architecture. The dedicated service needs to issue and enforce a match identity, participant identity, slot assignment, host authority, connection permissions, and immutable match configuration. The game and frontend transport need to preserve that identity all the way down to packets and diagnostics.

## Missing Service Contract

The dedicated service should produce one immutable `MatchConfig` at start time containing:

- `matchId`
- `lobbyId`
- `gameId`
- `mode`
- `trackOrLevel`
- `seed`
- `hostParticipantId`
- `hostPlayerIndex`
- Ordered player list with `participantId`, `playerIndex`, display name, and connection state.
- Maximum player count.
- Required protocol version.
- Required game asset/runtime version.
- ICE server configuration reference or short-lived ICE credentials.

The runtime currently does not consume `matchId` directly. Cro-Mag and Nanosaur 2 derive internal match IDs from the seed. That is not sufficient because seed is gameplay configuration, while `matchId` is service identity and should be globally unique.

Required change:

- Extend the JS-to-WASM match config bridge to pass a numeric or split 128-bit `matchId`.
- Stop deriving game packet `matchId` from the seed.
- Keep `seed` only for gameplay determinism/randomization.

## Missing Peer Identity And Routing

The WASM bridge exposes a single `PangeaNet` queue with `sendReliable`, `sendUnreliable`, and `pollMessage`. That works for a two-player prototype, but it is not enough for a multi-peer P2P match.

For Cro-Mag's four-player target, the frontend host may open one WebRTC peer connection per remote participant. The current frontend binding closes/replaces the runtime transport when a new peer channel is bound, so only one peer is effectively connected to the WASM runtime at a time.

Required changes:

- Add a runtime transport multiplexer on the frontend.
- Let the game host broadcast to all connected peers.
- Let clients send only to the authoritative game host.
- Preserve source participant/player index on inbound packets.
- Do not let clients send client-input packets pretending to be another player.
- Keep per-peer reliable and unreliable channels separate.
- Define what happens when one peer disconnects while others remain connected.

## Missing Authority Enforcement

The current packets include `playerIndex`, but the game-side C code trusts that field. In P2P this is not safe enough, even for casual public matches. The frontend/service knows which participant owns which player slot; the game packet alone should not be trusted.

Required changes:

- The dedicated service assigns immutable `participantId -> playerIndex`.
- The frontend transport validates that incoming client input came from the peer assigned to that player index.
- The host game should reject client input for host player index or out-of-slot player indexes.
- Clients should reject host snapshots unless they arrive from the elected host peer.
- Match-start signalling should name the elected authoritative game host explicitly.

## Missing WebRTC Matchmaking Requirements

The pangea-ports game code assumes `globalThis.PangeaNet` already works. The production system still needs service-backed WebRTC setup:

- Lobby creation and public/private matchmaking.
- Join-code resolution.
- Capacity checks.
- Ready checks.
- Host-only start.
- Immutable match config publication.
- SignalR or WebSocket group membership per lobby.
- SDP offer/answer relay.
- ICE candidate relay.
- Short-lived STUN/TURN config endpoint.
- TURN credential minting for production.
- Host migration policy, even if the first version says host migration is unsupported.
- Lobby expiry and cleanup.
- Match result and disconnect reporting.

If gameplay should remain peer-to-peer, the dedicated service must only relay signalling and metadata, not normal input/snapshot packets.

## Missing Tick Model

Both games still increment network tick counters from send paths. That does not define a stable simulation tick.

Problems:

- Cro-Mag increments `gPangeaLocalTick` when clients send input and when the host sends snapshots, so host and client tick meanings differ.
- Nanosaur 2 increments `gNS2Tick` in both client input sending and host snapshot sending.
- The gameplay loops still run at render cadence, not an explicit fixed network tick.
- Snapshot ticks are not guaranteed to map to a particular simulation step.

Required changes:

- Add a shared fixed network tick clock.
- Capture local input for tick N.
- Host applies the accepted input set for tick N.
- Host snapshots authoritative state for tick N after simulation.
- Clients reconcile against the authoritative tick they receive.
- Keep render interpolation separate from simulation tick.

## Missing Real Prediction/Reconciliation

The current client correction blends or snaps the local player toward the host snapshot. That is useful for a prototype but is not full prediction/reconciliation.

Missing pieces:

- Local input history ring buffer.
- Predicted local state history by tick.
- Snapshot tick lookup against predicted history.
- Rewind to authoritative state.
- Replay unacknowledged local inputs.
- Tuned thresholds for snap versus smooth correction.
- Debug visibility into correction distance and replay cost.

Without this, local clients will feel responsive briefly but can diverge after collisions, pickups, weapon hits, terrain interactions, or packet loss.

## Missing Remote Interpolation Buffer

Remote smoothing currently blends from current state toward the latest received target. That is not the same as snapshot interpolation.

Required changes:

- Keep a per-remote-player snapshot buffer.
- Render remote players at `hostTime - interpolationDelay`.
- Interpolate between two known snapshots.
- Extrapolate only for short gaps.
- Freeze, fade, or show disconnected state after longer gaps.
- Drop stale snapshots but keep enough recent snapshots to interpolate cleanly.

## Missing Gameplay Event Authority

Snapshots now include more player fields, but gameplay outcomes still need authoritative event coverage.

Cro-Mag needs host-owned reliable events or object replication for:

- Powerup pickup.
- Powerup use.
- Projectile spawn.
- Projectile hit.
- Explosion/effect that changes gameplay.
- Damage.
- Player elimination.
- Race finish.
- Lap/checkpoint validation.
- Battle/tag/capture-the-flag mode state.
- Dynamic object create/update/destroy for gameplay-affecting objects.

Nanosaur 2 needs host-owned reliable events or object replication for:

- Shot fired.
- Projectile spawn.
- Projectile hit.
- Pickup collection.
- Damage.
- Death.
- Respawn, if applicable.
- Level completion.
- Game over.
- Race/checkpoint progress.
- Weapon inventory changes.

State snapshots alone are not enough unless they include all authoritative gameplay objects and all outcome transitions.

## Missing Match Lifecycle Separation

There are packet enum values for pause, resume, disconnect, reliable event, and protocol error, but they are not implemented as a complete lifecycle.

Required changes:

- Define reliable lifecycle packets.
- Define who may send each lifecycle packet.
- Define service reports for each lifecycle transition.
- Define client behavior for host disconnect.
- Define behavior for non-host disconnect.
- Define pause behavior during temporary network disruption.
- Define match abort behavior when WebRTC cannot establish.
- Define match completion and result submission.

## Missing Dedicated-Service Reporting

The service should receive operational reports from the frontend, not from raw game packets:

- WebRTC connected.
- WebRTC failed.
- TURN relay used.
- Host disconnected.
- Participant disconnected.
- Match started.
- Match ended.
- Timeout.
- Desync or correction threshold exceeded.
- Protocol version mismatch.
- Runtime asset version mismatch.

The recent game-side updates track dropped/reordered packets internally, but those diagnostics are not exposed clearly enough to the frontend or service.

Required changes:

- Add debug exports for packet drop count, reorder count, last correction distance, tick, and snapshot age.
- Add frontend polling or event reporting for those metrics.
- Attach reports to `matchId` and `participantId`.

## Missing Protocol Version And Compatibility Checks

The packet headers have protocol version fields, but the service and frontend do not appear to enforce compatibility before launch.

Required changes:

- Service match config includes required protocol version and runtime build/version.
- Frontend validates the loaded WASM exports and expected protocol before signalling ready.
- Peers exchange a reliable protocol hello before gameplay packets.
- Mismatch aborts the match before the game starts.

## Missing Security And Abuse Controls

Even casual public P2P matchmaking needs basic safeguards:

- Participant token or session identity from the service.
- Hub method authorization by lobby membership.
- SDP/ICE relay only between active lobby participants.
- Host-only start enforcement.
- Rate limits for lobby creation, joining, signalling, chat, and reports.
- Validation of display names and chat.
- No long-term storage of raw SDP, ICE candidates, IP addresses, or gameplay packets.
- Clear disconnect/report moderation hooks.

Game-side packet checks do not replace service-side authorization.

## Missing TURN Production Path

P2P WebRTC will not connect for some users without TURN. A production path needs:

- `GET /api/multiplayer/ice-servers`.
- Short-lived TURN credentials minted server-side.
- TURN secret kept server-side.
- Deployment configuration for Coturn or a managed TURN provider.
- Metrics for direct, STUN-assisted, and TURN-relayed sessions.
- A forced-TURN smoke test.

Without TURN, matchmaking will appear broken for users behind restrictive NAT, carrier-grade NAT, or corporate networks.

## Missing Tests

The recent code should not be considered adequate until these tests exist:

- Unit tests for packet encode/decode success and malformed packet rejection.
- Tests for cross-version protocol rejection.
- Tests for host rejecting spoofed client player indexes.
- Tests for clients rejecting non-host snapshots.
- Two-browser P2P smoke test through the dedicated signalling service.
- Multi-peer host test for Cro-Mag with at least three participants.
- Latency/jitter/loss tests for input, snapshots, and lifecycle packets.
- Host disconnect test.
- Non-host disconnect test.
- TURN-forced connection test.
- Match reporting test tied to service `matchId`.

## Adequacy Assessment

The recent updates are adequate as a game-side prototype milestone. They are not adequate as the final implementation for a P2P service matchmade by a dedicated host.

The highest-priority missing work is:

1. Service-issued `matchId` and participant identity propagated into WASM packets.
2. Frontend runtime transport multiplexing for more than one peer.
3. Source-peer validation and host authority enforcement outside the C packet payload.
4. Fixed network tick semantics.
5. Real local prediction/reconciliation with input history.
6. Snapshot interpolation buffers for remote players.
7. Reliable authoritative gameplay events and object replication for outcomes.
8. STUN/TURN production support through the dedicated service.
9. Lifecycle/reporting integration with the service.
10. End-to-end tests under real latency, loss, reordering, disconnect, and TURN relay.

## Recommended Next Steps

1. Extend the match config bridge to pass service `matchId`, `lobbyId`, `participantId`, and immutable slot assignments.
2. Replace the singleton frontend runtime transport with a host/client P2P multiplexer.
3. Add peer-source validation before packets enter the WASM inbound queue.
4. Implement fixed tick ownership in both games.
5. Add input and predicted-state history for client reconciliation.
6. Add remote snapshot buffers and render-time interpolation.
7. Define and implement reliable event packets for gameplay outcomes.
8. Add service endpoints and hub enforcement for ICE, signalling, lifecycle, reports, and TURN credentials.
9. Add the production test matrix before widening public matchmaking.

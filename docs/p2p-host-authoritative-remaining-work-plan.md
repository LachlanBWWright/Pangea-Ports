# P2P Host-Authoritative Multiplayer Remaining Work Plan

## Review Verdict

The recent updates are a useful prototype step, but they are not adequate yet for production web multiplayer.

They improve the game-side model by moving Cro-Mag Rally and Nanosaur 2 toward explicit binary packets, unreliable input/snapshot traffic, host-side remote input application, and basic snapshot correction. They also add frontend/backend concepts for match config, protocol versioning, host player identity, TURN credentials, and a host runtime transport multiplexer.

The biggest remaining issue is that the new game-side packets and the frontend transport filtering do not yet agree on the wire format. Cro-Mag and Nanosaur 2 now emit raw `PNET` gameplay packets, but the host runtime multiplexer tries to decode incoming peer packets as the older frontend `MultiplayerPacket` envelope and silently drops packets it cannot decode. With the current shape, remote client input can fail before it reaches the host WASM runtime.

This should still be treated as a partial implementation, not as the final online multiplayer path.

## Blocking Findings

### 1. Host multiplexer drops raw game packets

Frontend file: `frontend/src/multiplayer/runtimeTransportMultiplexer.ts`

The multiplexer calls `decodeMultiplayerPacket(bytes)` and returns immediately when decoding fails. Raw Cro-Mag/Nanosaur `PNET` packets are not encoded with the frontend `MultiplayerPacket` envelope, so they will normally fail this decode and be dropped.

Required fix:

- Add a shared parser for the raw `PNET` header.
- Allow raw `PNET` packets through after validating magic, version, packet type, match id, and sender player index.
- Keep the old frontend envelope path only if it is still needed by mock transports or future control messages.
- Add tests proving raw client input from participant N reaches only the host runtime and is accepted only when `header.playerIndex` matches the participant's assigned player index.

### 2. Authority checks rely on packet self-identification

Game files:

- `games/CroMagRally-Android/Source/System/network.c`
- `games/Nanosaur2-Android/Source/System/WebCommands.c`

Both games validate `header.playerIndex`, but they do not know which WebRTC peer supplied the packet. In a P2P match, the frontend must bind each data channel to a participant and reject packets whose claimed player index does not match the service-issued participant mapping.

Required fix:

- Enforce participant-to-player mapping in the frontend before packets enter WASM.
- Validate host-only packet types on clients using the actual peer identity, not only packet contents.
- Reject client packets from the host channel and host packets from client channels unless they match the authoritative host participant.

### 3. Local correction is smoothing, not prediction and reconciliation

Game files:

- Cro-Mag: `ClientApplyPendingSnapshot`
- Nanosaur 2: `PangeaNet_ClientApplySnapshot`

The current client logic blends or snaps local player position toward the latest host snapshot. That is better than ignoring host authority, but it is not proper high-latency prediction/reconciliation.

Required fix:

- Store local input history by client input sequence/tick.
- Include the last processed client input sequence in host snapshots.
- On clients, restore authoritative state, replay unacknowledged local inputs, and only blend residual visual error.
- Keep immediate input responsiveness local, but make gameplay truth come from the host.

### 4. Remote interpolation has no time buffer

The current remote player smoothing blends directly toward the latest received snapshot. This will jitter under WAN packet delay variation because render interpolation is tied to packet arrival timing.

Required fix:

- Store a short snapshot ring buffer per remote player.
- Render remote players at an interpolation delay, for example 100-150 ms behind host time.
- Interpolate between bracketing snapshots by authoritative tick/time.
- Extrapolate only briefly when packets are late, then hold or degrade cleanly.

### 5. Tick semantics are still loose

Both games increment local counters when sending input or snapshots, but there is no shared fixed network tick contract between host simulation, client input sampling, snapshot cadence, and ack handling.

Required fix:

- Define one network tick rate per game.
- Stamp every input and snapshot with that tick.
- Send input at the network tick rate, optionally bundling several recent inputs for loss tolerance.
- Send host snapshots at a separate fixed cadence, for example 10-20 Hz.
- Track RTT/jitter from ack timing and expose it in debug telemetry.

### 6. Reliable gameplay events are still not modeled

The packet enums reserve reliable event packet types, but the games still mostly replicate player state snapshots. Race results, pickups, weapon fire, damage, item use, lap/checkpoint decisions, deaths, and match end need explicit host-owned event replication.

Required fix:

- Define reliable event packets per game.
- Make host authoritative for all scoring, pickups, item effects, race completion, eliminations, and match end.
- Add event ids and acknowledgement/resend or replay-from-snapshot behavior.
- Keep high-frequency transforms on unreliable channels, but send outcome-changing events reliably.

### 7. Match id is only partially used

The frontend now derives `matchIdLow` and `matchIdHigh`, but the C packet header carries only a 32-bit `matchId`. That is better than using only the seed, but it is weaker than the service-issued UUID and increases collision risk.

Required fix:

- Put both low and high id fields in the raw game packet header, or define a compact 64-bit/128-bit match id representation consistently.
- Use the same representation in Cro-Mag, Nanosaur 2, frontend validation, and tests.
- Stop treating seed as a fallback match identity except in local/mock modes.

## Remaining Plan

### Phase 1: Make The Wire Contract Consistent

Deliverables:

- One documented raw `PNET` packet header shared by Cro-Mag, Nanosaur 2, and the frontend.
- A TypeScript decoder for raw `PNET` headers using Zod or typed binary parsing without `any`.
- Frontend tests for raw packet accept/reject behavior.
- C packet headers updated to include full match identity.

Acceptance criteria:

- A raw Cro-Mag/Nanosaur client input packet from player 1 reaches host WASM.
- The same packet is rejected when it arrives on player 2's WebRTC peer channel.
- Raw host snapshots reach clients only from the service-appointed host participant.

### Phase 2: Finish P2P Host Transport Routing

Deliverables:

- Host multiplexer routes inbound packets from each peer to the single host WASM runtime.
- Host outbound snapshots/events fan out to all connected peers.
- Client runtime binds only to the host peer.
- Disconnects detach the correct peer without destroying the whole host runtime unless the host itself leaves.

Acceptance criteria:

- Cro-Mag host can maintain three remote peer transports at the same time.
- Nanosaur 2 host can maintain one remote peer transport.
- A peer reconnect can replace only that peer's transport.
- Host and client runtime startup waits for required game-specific peer count or a clear ready state.

### Phase 3: Implement Real Prediction And Reconciliation

Deliverables:

- Client input history ring buffer.
- Host snapshots include `lastProcessedInputSequence` per player.
- Clients replay unacknowledged inputs after applying authoritative local state.
- Visual-only correction smoothing layered after simulation reconciliation.

Acceptance criteria:

- With 150-250 ms latency, the local player remains responsive.
- Host corrections do not cause regular visible snapping during normal movement.
- A deliberate divergence is corrected within a bounded number of ticks.

### Phase 4: Add Remote Snapshot Interpolation

Deliverables:

- Per-remote-player snapshot buffers.
- Render interpolation delayed behind host time.
- Late/lost packet handling with short extrapolation and hold behavior.
- Debug overlay for interpolation buffer depth and dropped/reordered packet counts.

Acceptance criteria:

- Remote players move smoothly under latency, jitter, and 1-5% packet loss.
- Out-of-order snapshots do not move players backward.
- A large authoritative discontinuity still snaps when appropriate.

### Phase 5: Replicate Authoritative Events

Deliverables:

- Cro-Mag event packets for lap/checkpoint, place/race complete, pickup/use, damage/status effects, elimination, and match end.
- Nanosaur 2 event packets for weapon changes, shots, pickups, health/shield changes, lap/checkpoint, race complete, and match end.
- Reliable delivery or snapshot-replay behavior for each event class.

Acceptance criteria:

- Clients cannot decide final race results locally.
- Clients cannot grant themselves pickups, weapon inventory, damage, or win state.
- A lost snapshot does not permanently lose outcome-changing gameplay state.

### Phase 6: Service Integration And Lifecycle

Deliverables:

- Backend stores service-issued match identity, host participant, player mapping, protocol version, runtime version, and connection state.
- TURN credentials are production-configured and short-lived.
- Clients report ready, connected, timeout, desync, disconnect, and match end states.
- Host migration is explicitly unsupported for now, with clear match failure behavior if the host leaves.

Acceptance criteria:

- A browser client can create/join through the service, exchange WebRTC signaling, receive TURN credentials, and start the game with the same match config on all peers.
- The service can tell whether a match is waiting, active, degraded, ended, or abandoned.
- Host disconnect ends the match cleanly.

### Phase 7: Test Under Real Network Conditions

Deliverables:

- Unit tests for raw packet parsing, participant validation, multiplexer fan-in/fan-out, and bridge queue behavior.
- Browser integration tests for two-player Nanosaur 2 and two-to-four-player Cro-Mag session setup.
- Network-condition tests for latency, jitter, packet loss, reordering, disconnect, reconnect, and TURN-only connectivity.
- Game smoke tests that verify input reaches the host and host snapshots reach clients.

Acceptance criteria:

- Nanosaur 2 remains playable at 150-250 ms latency and 2% packet loss.
- Cro-Mag remains playable with at least two browser clients under the same network profile before expanding to four.
- Packet validation tests prove spoofed player indexes and non-host authoritative packets are rejected before WASM.

## Implementation Priority

1. Fix raw `PNET` frontend parsing and host multiplexer forwarding.
2. Enforce participant identity before packets enter WASM.
3. Add full match identity to raw packet headers.
4. Add input sequence acknowledgement and client replay.
5. Add remote interpolation buffers.
6. Add reliable host-owned gameplay events.
7. Run latency/loss/reordering tests and tune per game.

Do not continue adding snapshot fields until the transport and authority contract is correct. More state in snapshots will not solve spoofing, jitter, or high-latency reconciliation.

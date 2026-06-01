# Host-Authoritative Web Multiplayer Plan

## Goal

Build production web multiplayer for Cro-Mag Rally and Nanosaur 2 using a host-authoritative model that tolerates internet latency, jitter, packet loss, and packet reordering.

The original games were designed around local play and low-latency LAN-era assumptions. For browser multiplayer, clients should predict for responsiveness, but the host must remain the source of truth for gameplay outcomes.

## Core Model

- The host runs the authoritative gameplay simulation.
- Clients send compact input commands to the host.
- The host broadcasts authoritative snapshots.
- Clients predict only their own local player.
- Clients reconcile local prediction against host snapshots.
- Remote players are rendered from buffered host snapshots using interpolation or short extrapolation.
- Reliable ordered packets are used for durable events.
- Unreliable unordered packets are used for high-frequency input and state.

## Shared Foundation

### Protocol

Create a shared `PangeaNet` gameplay protocol with explicit packet types:

- `MatchConfig`
- `ClientInput`
- `HostSnapshot`
- `ReliableEvent`
- `ClientAck`
- `Pause`
- `Resume`
- `Disconnect`
- `ProtocolError`

Every packet should include:

- `magic`
- `version`
- `packetType`
- `matchId`
- `tick`
- `sequence`
- `playerIndex`, where applicable

Do not send raw C structs directly over the network. Add explicit fixed-width encode/decode helpers with deliberate little-endian reads and writes. Reject malformed packets at the boundary before they reach game state.

### Channels

Use WebRTC DataChannels by packet purpose:

- Reliable ordered channel: lobby, match config, ready/start, character or vehicle selection, match end, pause, resume, disconnect, and durable gameplay events.
- Unreliable unordered channel: per-tick input commands and host snapshots.

High-frequency state is disposable. If a newer snapshot is available, stale snapshots should be dropped rather than queued.

### Network Tick

Add a fixed network tick, initially 30 Hz or 60 Hz.

The host advances authoritative gameplay state on this tick. Rendering can remain browser-frame-rate based, but network packets need stable tick identity for prediction, reconciliation, interpolation, and diagnostics.

### Connection State

Track per-player state:

- Last received input tick.
- Last acknowledged snapshot tick.
- Last sent packet sequence.
- Last received packet sequence.
- RTT estimate.
- Jitter estimate.
- Input buffer length.
- Timeout/disconnect state.

### Diagnostics

Add debug instrumentation before tuning:

- Local predicted position.
- Last authoritative position.
- Correction distance.
- Snapshot age.
- Dropped packet count.
- Reordered packet count.
- RTT and jitter.
- Host tick.
- Client tick.
- Client drift from host.

## Cro-Mag Rally Plan

### Replace The Hybrid Flow

The current preliminary implementation mixes the old lockstep-style control packet path with host snapshots. Replace that with a clear host-authoritative flow:

- Remove reliable per-frame host snapshots.
- Stop making clients wait for a host control packet before every simulation step.
- Stop treating snapshots as a sidecar correction on top of lockstep.
- Make the host the only authority for race state, collisions, powerups, weapons, damage, and match end.

### Client Input Packet

Create a Cro-Mag `ClientInput` packet containing:

- `tick`
- `sequence`
- `playerIndex`
- `controlBits`
- `controlBitsNew`
- `analogSteering.x`
- `analogSteering.y`

Clients send this on the unreliable unordered channel every network tick.

### Host Input Handling

The host should:

- Read local host input directly.
- Receive remote client input packets.
- Apply inputs by tick.
- Repeat the last known input for a short missing-input window.
- Mark a player idle, paused, or disconnected after timeout.

### Host Snapshot

Create a Cro-Mag `HostSnapshot` packet with enough state to reconstruct authoritative gameplay:

- Player position.
- Player rotation.
- Player velocity.
- Steering.
- Current thrust.
- Control state.
- Lap number.
- Checkpoint number.
- Place.
- Race completion state.
- Health.
- Elimination state.
- Powerup type and quantity.
- Gameplay-affecting timers such as frozen, greased tires, nitro, invisibility, sticky tires, and similar status effects.

For the first milestone, player-car snapshots plus reliable events for pickups, weapon use, hits, and race finish are acceptable. Longer term, the host should own all gameplay objects that affect outcomes and publish create/update/destroy events or object snapshots.

### Client Prediction

Clients should:

- Apply local input immediately to the local car.
- Keep a ring buffer of local inputs by tick.
- Keep a ring buffer of predicted local states by tick.
- Apply host snapshots to remote cars through interpolation.
- Compare host authoritative local-player state against predicted local-player state.

For correction:

- Smooth small errors over a few frames.
- Snap large errors.
- Prefer rewind/replay once enough state can be captured reliably.

### Remote Player Rendering

Remote cars should be rendered from a snapshot buffer, slightly behind real time. Start with around 100 ms of interpolation delay and tune from diagnostics.

Avoid directly teleporting remote cars every received snapshot unless the error exceeds a large threshold.

## Nanosaur 2 Plan

### Finish The Existing Direction

The current preliminary implementation already sends client inputs and host snapshots. Keep that direction, but make it complete:

- Apply all remote inputs on the host.
- Add local-player reconciliation on clients.
- Expand snapshots to cover gameplay state, not only position and a few race fields.
- Add authoritative events for actions and outcomes.

### Client Input Packet

Expand Nanosaur 2 `ClientInput` to include:

- `tick`
- `sequence`
- `playerIndex`
- Analog control.
- Fire.
- Jetpack.
- Next weapon.
- Previous weapon.
- Any other action that can change gameplay state.

### Host Input Application

The host must apply every gameplay input field for remote players. Analog movement alone is not enough.

Add a network input adapter so input queries for remote players resolve from the latest accepted network input. This should cover action paths such as firing, jetpack use, and weapon switching.

### Host Snapshot

Expand Nanosaur 2 `HostSnapshot` to include:

- Position.
- Rotation.
- Velocity.
- Health.
- Shield state.
- Jetpack fuel.
- Jetpack active state.
- Current weapon.
- Ammo or weapon resource state.
- Race checkpoint state.
- Place.
- Race completion state.
- Action or animation state needed for remote rendering.

Use reliable authoritative events for shots, hits, pickups, deaths, level completion, game over, and other durable outcomes.

### Client Reconciliation

Clients should not permanently preserve their local predicted state when the host disagrees.

Add local-player reconciliation:

- Predict local player immediately.
- Store input history by tick.
- Store predicted state history by tick.
- When a host snapshot arrives, compare authoritative local-player state against predicted state for that snapshot tick.
- Smooth small corrections.
- Snap large corrections.
- Add rewind/replay when the required state capture is reliable enough.

### Remote Player Rendering

Remote Nanosaur 2 players should be interpolated from host snapshots with a small render delay. Short extrapolation is acceptable for brief packet gaps, but long gaps should freeze or fade into a disconnected/paused state rather than inventing gameplay.

## Implementation Order

1. Add the shared explicit packet codec.
2. Replace raw struct packet sending with encoded packets.
3. Add channel selection by packet kind.
4. Add tick, sequence, and ack metadata.
5. Move Cro-Mag snapshots to the unreliable unordered state channel.
6. Finish Nanosaur 2 remote input application.
7. Add Nanosaur 2 local prediction reconciliation.
8. Convert Cro-Mag from hybrid lockstep/snapshot flow to host-authoritative input and snapshot flow.
9. Add Cro-Mag local prediction reconciliation.
10. Add remote-player interpolation for both games.
11. Add authoritative reliable events for pickups, weapons, hits, deaths, race completion, level completion, and match end.
12. Add diagnostics overlays and automated tests for latency, jitter, packet loss, packet reordering, disconnect, and reconnect.

## Test Matrix

Test both games under:

- 0 ms latency, 0% packet loss.
- 80 ms latency, 0% packet loss.
- 150 ms latency, 1% packet loss.
- 250 ms latency, 3% packet loss.
- 100 ms latency with jitter spikes.
- Unreliable packet reordering.
- Reliable channel delay while state channel continues.
- Host disconnect.
- Client disconnect.
- Client reconnect or rejoin, if supported.

The pass condition is convergence to host-authoritative state without permanent divergence, plus acceptable local responsiveness for the predicted player.

## Design Rule

Clients may predict for feel, but only the host decides truth. Snapshots are authoritative state, reliable packets are durable events, and stale high-frequency state is disposable.

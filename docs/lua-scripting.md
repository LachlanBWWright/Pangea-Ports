# Lua scripting

Pangea Ports embeds Lua 5.4 in native, Android, and WebAssembly builds whenever
`PANGEA_ENABLE_SCRIPTING=ON`. The same shared backend and API bindings are used
on every platform.

The editor and runtime share scripting contract version 1 and API version 1.
Packages carry the contract version in `Data/Scripts/config/project.json`; a
package with an unsupported version is rejected before it can be previewed or
exported. Newly built packages also contain
`Data/Scripts/config/manifest.json`, which records the runtime version and a
deterministic content hash over the package. Its network policy is explicitly
`disabled`; a networked runtime must reject the package until peer agreement,
authority, and deterministic command-stream checks are available.
The shared native boundary also exposes an explicit network-mode switch; when
enabled by a host, every script hook is rejected until the host returns to local
mode.
The editor can compare a typed peer manifest containing the game, contract,
runtime, content hash, file count, and network policy; mismatches are rejected
before the runtime safety gate is considered. This is package validation only,
not a claim that networked script execution is enabled.

```sh
cmake -S games/Bugdom2-Android -B games/Bugdom2-Android/build-scripted -DPANGEA_ENABLE_SCRIPTING=ON
cmake --build games/Bugdom2-Android/build-scripted
```

Scripts are authored and packaged as Lua:

```text
Data/Scripts/dist/main.lua
Data/Scripts/dist/modules/*.lua
Data/Scripts/config/levels.json
```

`main.lua` must return an entry table. Supported callbacks are selected by each
game adapter and include lifecycle, frame, terrain, spline, map-item, and object
events.

```lua
local pangea = require("pangea")
local entry = {}

function entry.onLevelStart(ctx)
    pangea.log.info("Level " .. ctx.levelNum .. " started")
end

return entry
```

The Lua VM enforces an instruction budget on every callback. Native objects are
accessed through opaque generation-checked handles exposed by `pangea.object`.
Native terrain items can be created with `pangea.spawn.native` or the structured
`pangea.spawn.nativeResult` API.

Object mutation has both legacy boolean APIs and structured result APIs. Prefer
`setPositionResult`, `setVelocityResult`, `setRotationResult`, `setScaleResult`,
`setAnimationResult`, `setActiveResult`, and `deleteResult` when a script needs diagnostics. Each
returns `{ ok, code, reason, message, primary }`; failed commands leave the
native object unchanged, and stale generation-checked handles report
`bad-argument` instead of mutating a replacement object.

The typed object commands and gameplay event payloads declare their policy in
the shared contract. The native boundary exports command, event, and object
lifecycle descriptors, and CI compares all three descriptor sets with the
frontend contract:
they require a generation-checked handle, validate finite numeric inputs, run
in the callback phase, and remain disabled for network simulation until an
authority and deterministic command-order protocol exists. The native runtime
exports the same descriptors to its conformance tests, so the editor metadata,
LuaLS declarations, and native command boundary cannot silently drift.

Each VM has a 16 MiB allocation limit. Script loading, gameplay events, and
per-frame callbacks use separate instruction budgets, and runtime failures
include Lua stack traces. The standard `math.random` functions are unavailable;
use `pangea.random` for deterministic random numbers that behave consistently
in native and WebAssembly builds.

`pangea.level.current()`, `pangea.time.frame()`, `pangea.time.delta()`, and
`pangea.time.level()` expose the active runtime state. `pangea.api.capabilities()`
reports game-specific terrain, spline, map-item, and spawn support. Terrain,
spline, and map-item flags are declared by each native adapter and exercised by
the eight-game conformance fixtures; they are not inferred from display names
or game-ID string matching.

Calling an unavailable terrain, spline, or map-item entry point is rejected at
the shared runtime boundary with `incompatible-item`; it never reaches a Lua
hook that the current adapter did not advertise.

One-shot callbacks can be scheduled with `pangea.time.after(delaySeconds,
callback)`. Repeating work uses `pangea.time.every(intervalSeconds, callback)`;
both return IDs accepted by `pangea.time.cancel(timerId)`. Repeating timers
advance from their prior deadline, so uneven frames do not accumulate drift or
cause unbounded catch-up bursts. Timers use level time, are cleared between
levels, reject non-finite durations, and run under the event instruction budget.
`pangea.api.diagnostics()` reports current Lua memory usage, the memory limit,
pending timer count, frame number, and the deterministic local command trace.
The trace is an FNV-1a hash over the ordered command identifier, target handle,
and result status. `diagnostics().commandTrace` also exposes up to 256 bounded
entries containing the command ID, target identity, and result status;
`commandTraceOverflow` reports when more entries were produced. This is
diagnostic evidence only until a network authority protocol exists. Hosts can
compare an exchanged aggregate trace and bounded entries with
`PangeaScript_CompareCommandTrace`; a mismatch reports the first divergent
entry and both command IDs/statuses without enabling network execution.

Longer sequences can be written as budgeted coroutines instead of nested timer
callbacks. `pangea.task.start(function() ... end)` starts a task immediately;
inside it, `pangea.task.wait(delaySeconds)` suspends execution until level time
reaches the next deadline. `pangea.task.cancel(taskId)` cancels a suspended task.
Tasks share the event instruction budget, are capped at 64 concurrent tasks,
are cleared between levels, and appear as `activeTasks` in diagnostics.

Registered native and scripted objects can be discovered with
`pangea.object.all()`, `pangea.object.findByTag(tag)`, and
`pangea.object.nearest(position, optionalTag)`. Returned values remain opaque,
generation-checked handles; discovery does not grant mutation capabilities.
Source-backed replacement objects also expose a read-only
`pangea.object.source(handle)` record with its terrain, spline, or map identity
and validated coordinates. It returns `nil` for ordinary scripted objects and
stale handles. Native replacement entry points use the same validated identity
to find an existing replacement before creating another one when a source
callback is replayed; a later stream-in still receives a new generation after
stream-out.
Map-item replacements use a dedicated `mapReplacements` configuration record
with map coordinates, rather than overloading terrain replacement fields. The
Mighty Mike adapter resolves that record before creating the scripted object
and associates the resulting handle with a `map` source identity.
For example: `{"mapReplacements":[{"itemIndex":4,"nativeType":9,"x":12,"y":24,"customObjectId":"custom.map-box","strict":false}]}`.
The editor rejects a replacement when the selected native type has no audited
replacement path for the selected surface, including when a type number is
shared by native and generic entries.

Modules can communicate without global variables through `pangea.events`.
`on(name, callback)` returns a subscription ID, `off(id)` removes it, and
`emit(name, payload)` synchronously returns the number of listeners invoked.
`once(name, callback)` automatically removes its listener before invocation,
so nested emissions cannot invoke it twice. Timer and task IDs can be inspected
with their respective `isActive(id)` functions.
Payload tables are read-only inside listeners, newly added listeners do not run
during the current emission, callbacks are individually budgeted, and all
subscriptions disappear on reload and level unload. Object-owned timers, tasks,
subscriptions, and state are also removed when an object is unregistered or
its generation is invalidated. Diagnostics expose `activeSubscriptions`.

Scripts can call `pangea.api.requireVersion(minimum, maximum)` while loading to
fail early with a clear compatibility diagnostic. Callback context tables are
deeply read-only while retaining normal indexing, length, and `pairs` behavior.
Vectors, timer durations, and numeric hook results reject NaN and infinity
before they can cross the native boundary.

All eight adapters expose normalized read-only player lookup through
`pangea.player.count()` and `pangea.player.get(playerNum)`. A player snapshot
always contains its index and position; `health` is optional because the games
use incompatible health models. The `playerLookup` capability reports whether
an adapter supplies this data.

Hooks that return results must return a table or `nil`. Returning another Lua
type is treated as a runtime error with the hook name in the diagnostic. Object
state created with `pangea.object.state` is removed when its generation-checked
native handle is released.

Custom objects using the pickup collision preset now dispatch
`onPickupCollected` from their real game-side trigger callback. Returning
`consumePickup = true` requests deletion through the object's cleanup-safe
native operation.

Scripts may use bounded host-backed persistence for small scalar values:

```lua
local saved = pangea.persistence.get("coins", 1) or 0
pangea.persistence.set("coins", 1, saved + 10)
```

Keys are ASCII letters, digits, `_`, `-`, or `.`, with a maximum encoded size
of 63 bytes. Values are strings, booleans, integers, or finite numbers and are
limited to 4096 encoded bytes per value and 16384 bytes total per runtime.
Persistence is unavailable unless the host supplies both callbacks. A missing
value, unavailable store, malformed record, or version mismatch returns `nil`
from `get`; a version mismatch is discarded rather than migrated implicitly.
`set` and `delete` return `false` when the host or bounds reject the request.
The encoded record format is versioned, so a host can migrate data explicitly
without allowing old bytes to be interpreted as a new schema.

  Adapters use the shared object lifecycle boundary for checkpointed objects and
  terrain/spline-backed objects that leave the native streaming window. Source
  replacements receive `streamIn` after their native source record is associated
  and spline replacements join the native spline object list for culling;
  `activate`, `deactivate`, `streamIn`, `streamOut`, and `checkpointReset` are delivered
  through the object context event field. Bugdom, Bugdom 2, Cro-Mag Rally,
  Nanosaur, Nanosaur 2, Billy Frontier, and Otto Matic classify deletion of a source-backed
  scripted object as `streamOut`; Bugdom, Bugdom 2, Nanosaur, Nanosaur 2, and
  Otto Matic also deliver checkpoint transitions at their native respawn
  functions. If a `streamIn` callback fails, the shared boundary removes the
  partially created object before the adapter applies its strict or native
  fallback policy.
Deactivation clears object-owned state, timers, tasks, subscriptions, and any
checkpoint snapshot. The first checkpoint reset captures a private-state
baseline using an atomic deep copy of supported scalar/table values; later
resets restore that baseline before calling the object's checkpoint handler,
then clear object-owned timers, tasks, and subscriptions. Unsupported or cyclic
values are never published as a partial checkpoint snapshot.
The object keeps its generation across checkpoint resets. Stream-out and
destruction invalidate the handle generation after the transition callback; a
later stream-in must register a new handle and therefore starts with fresh
private state and no checkpoint snapshot. This prevents a stale script
reference from mutating a recreated native object while making repeated
checkpoint resets deterministic for an object that actually survived.
Scripted objects spawned during an object callback are owned by that callback's
object. Destructive parent teardown recursively delivers child destruction and
invalidates the child generations before releasing the parent registry entry.

Object animation marker callbacks receive `ctx.event == "animationEvent"` and
the native marker integer in `ctx.eventValue`. Completion callbacks omit
`eventValue`; scripts should branch on the event name before reading it.

Custom-object modules may implement `onSpawn`, `onUpdate`, `onTriggerEnter`,
`onTriggerStay`, `onAnimationEvent`, `onAnimationComplete`, `onActivate`, `onDeactivate`,
`onStreamIn`, `onStreamOut`, `onCheckpointReset`, and `onDestroy`. Each receives
`(self, ctx)`, where `self.handle` is the generation-checked object handle.
`onTriggerEnter` is delivered once for a contact and `onTriggerStay` is delivered
for repeated callbacks for the same handle pair in the same or immediately
following frame. Exit delivery remains unavailable until an adapter exposes a
real collision-exit call site.

Replacement validation uses the adapter's registered numeric native type, not a
display label. Terrain and pickup families are classified as
`replaceable-with-native-hooks`; NPCs, objectives, and transition triggers are
native-only unless their native initializer remains available. A non-strict
replacement falls back to that initializer when custom creation fails. A strict
replacement rejects audited native-only families before packaging and stops the
native item from being initialized when creation fails.

Script packages may include BG3D, Shapes, glTF 2.0 source, and skeleton
resource assets under `Data/Scripts/assets/`. Paths use forward slashes, contain
no traversal or empty segments, and are validated before bytes are parsed. The
editor accepts constrained `.gltf` and `.glb` uploads, normalizes supported
features, emits a deterministic BG3D runtime asset, and retains the normalized
source alongside it. Preview, export, and package import repeat the same
preflight. Skeleton resources must contain a readable header and bone table;
malformed or unsupported assets fail with a path-specific diagnostic before
they can enter an object registry or preview bundle. Asynchronous workspace
validation also checks that every custom-skeleton animation mapping and the
selected initial animation refer to an animation index present in the parsed
skeleton header. Editor actions that handle skeleton or glTF assets use the
async package and preview APIs; synchronous helpers conservatively reject
those formats instead of allowing unparsed bytes to bypass validation.

## Contract identifier index

This index is intentionally kept in the production scripting documentation.
CI checks it against the frontend contract so an API, hook, lifecycle event,
failure code, or game adapter cannot be added without a corresponding
documentation update.

```text
Hooks:
onGameStart onGameShutdown onLevelLoad onLevelStart onLevelComplete onLevelUnload
onFrame onTerrainItem onSplineItem onMapItem onObjectFrame onPickupCollected
onWeaponHit onTriggerEnter onAreaLoad onAreaStart onAreaFrame onAreaComplete
onAreaUnload onRaceLoad onRaceStart onRaceFrame onRaceComplete onRaceUnload

Events:
onTriggerEnter onPickupCollected onWeaponHit onDamage onDamageApplied onDeath onPlayerSpawn
onPlayerRespawn animationComplete destroy

Object events:
spawn update triggerEnter animationEvent animationComplete activate deactivate
triggerStay streamIn streamOut checkpointReset destroy

Failure codes:
ok not-enabled file-not-found parse-error runtime-error bad-argument
budget-exceeded incompatible-item config-error

API functions:
pangea.log.info pangea.log.warn pangea.log.error pangea.level.current
pangea.level.setting pangea.api.capabilities pangea.api.diagnostics
pangea.api.requireVersion pangea.time.frame pangea.time.delta pangea.time.level
pangea.time.after pangea.time.every pangea.time.cancel pangea.time.isActive
pangea.task.start pangea.task.wait pangea.task.cancel pangea.task.isActive
pangea.events.on pangea.events.once pangea.events.off pangea.events.emit
pangea.random.number pangea.random.integer pangea.random.seed
pangea.persistence.get pangea.persistence.set pangea.persistence.delete
pangea.player.count pangea.player.get pangea.spawn.native
pangea.spawn.nativeResult pangea.spawn.scripted pangea.object.position
pangea.object.all pangea.object.findByTag pangea.object.nearest
pangea.object.exists pangea.object.tags pangea.object.hasTag pangea.object.state
pangea.object.source
pangea.object.setPosition pangea.object.setPositionResult
pangea.object.setVelocity pangea.object.setVelocityResult
pangea.object.setRotation pangea.object.setRotationResult
pangea.object.setScale pangea.object.setScaleResult
pangea.object.setAnimation pangea.object.setAnimationResult
pangea.object.setActive pangea.object.setActiveResult
pangea.object.delete pangea.object.deleteResult

Result shapes:
ItemSpawnResult TriggerResult PickupResult WeaponHitResult ObjectCommandResult
NativeSpawnResult PangeaCapabilities PangeaDiagnostics PangeaPlayerSnapshot

Game adapters:
OttoMatic-Android Bugdom-android Bugdom2-Android Nanosaur-android
Nanosaur2-Android CroMagRally-Android BillyFrontier-Android MightyMike-Android
```

## Testing

The standalone shared-runtime build registers the Lua integration suite with
CTest:

```sh
cmake -S games/pangea-ports/shared/script -B build/pangea-script -DBUILD_TESTING=ON
cmake --build build/pangea-script
ctest --test-dir build/pangea-script --output-on-failure
```

The same CTest targets can be compiled and executed through Emscripten. The
Emscripten CMake wrapper supplies Node as CTest's cross-compiling emulator:

```sh
emcmake cmake -S games/pangea-ports/shared/script -B build/pangea-script-wasm -DBUILD_TESTING=ON
cmake --build build/pangea-script-wasm
ctest --test-dir build/pangea-script-wasm --output-on-failure
```

This runs the identical Lua packages, lifecycle fixtures, gameplay result
fixtures, object-handle tests, and eight game sample cases against the WASM
runtime rather than substituting a browser-only smoke test.

The suite covers sandboxing, all lifecycle naming families, context fields,
player lookup, deterministic randomness, timers and timer limits, reload
isolation, structured object-command and gameplay results, malformed results, tracebacks, memory
and instruction limits, failure suspension, and successful reload recovery.

CTest also registers a separately reported scripting sample test for each of
the eight games. Each game case executes every lifecycle phase using that
game's level, race, or area vocabulary; its supported terrain, spline, or map
item hooks; structured trigger, pickup, and weapon hooks; player, level, time,
task, random, event, spawn, query, mutation, deletion, diagnostics, sandbox,
traceback, and reload behavior; its tagged sample motion and negative filter;
and custom-object position and rotation through the native object-handle
boundary.

Global `onTriggerEnter`, `onPickupCollected`, and `onWeaponHit` hooks have typed
native entry points and structured results. Scripted trigger objects in all
eight games dispatch the global trigger hook and object-local trigger behavior;
object-local `onTriggerEnter` is followed by `onTriggerStay` for a continuing
contact. Pickup dispatch is available through that shared
object-trigger path. `onWeaponHit` is advertised only by Bugdom 2, whose
projectile-versus-enemy native collision path now provides the required
adapter call site; other adapters keep it unavailable.

Billy Frontier, Bugdom, Bugdom 2, Nanosaur, Nanosaur 2, Otto Matic, and Cro-Mag Rally expose
real player damage callbacks: `onDamage` runs before the native health entry
point applies damage and may return a non-negative replacement `damage` value
or `applyDamage = false`. All seven damage-capable adapters additionally expose
`onDamageApplied` after their native health decrement. Bugdom 2 exposes
`onWeaponHit` at its native projectile collision path; its
typed result may override or suppress damage and request target destruction.
Otto Matic, Bugdom, Bugdom 2, Nanosaur, and Nanosaur 2 expose `onPlayerSpawn` when the native
player object is registered, and `onPlayerRespawn` after their native
checkpoint reset has restored the player. All seven
damage-capable adapters dispatch `onDeath` at their native death or elimination
boundary where that mode has one. Their
typed contexts are `DamageContext`, `WeaponHitContext`, and `PlayerEventContext`, and the damage
result is `DamageResult` and weapon-hit results use `WeaponHitResult`. A script failure falls back to the native damage
path. Nanosaur 2 preserves its multiplayer safety gate; these local callbacks
do not make networked scripting eligible.

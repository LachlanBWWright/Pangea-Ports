# Lua scripting

Pangea Ports embeds Lua 5.4 in native, Android, and WebAssembly builds whenever
`PANGEA_ENABLE_SCRIPTING=ON`. The same shared backend and API bindings are used
on every platform.

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

Each VM has a 16 MiB allocation limit. Script loading, gameplay events, and
per-frame callbacks use separate instruction budgets, and runtime failures
include Lua stack traces. The standard `math.random` functions are unavailable;
use `pangea.random` for deterministic random numbers that behave consistently
in native and WebAssembly builds.

`pangea.level.current()`, `pangea.time.frame()`, `pangea.time.delta()`, and
`pangea.time.level()` expose the active runtime state. `pangea.api.capabilities()`
reports game-specific terrain, spline, map-item, and spawn support.

One-shot callbacks can be scheduled with `pangea.time.after(delaySeconds,
callback)`. Repeating work uses `pangea.time.every(intervalSeconds, callback)`;
both return IDs accepted by `pangea.time.cancel(timerId)`. Repeating timers
advance from their prior deadline, so uneven frames do not accumulate drift or
cause unbounded catch-up bursts. Timers use level time, are cleared between
levels, reject non-finite durations, and run under the event instruction budget.
`pangea.api.diagnostics()` reports current Lua memory usage, the memory limit,
pending timer count, and frame number.

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

Modules can communicate without global variables through `pangea.events`.
`on(name, callback)` returns a subscription ID, `off(id)` removes it, and
`emit(name, payload)` synchronously returns the number of listeners invoked.
`once(name, callback)` automatically removes its listener before invocation,
so nested emissions cannot invoke it twice. Timer and task IDs can be inspected
with their respective `isActive(id)` functions.
Payload tables are read-only inside listeners, newly added listeners do not run
during the current emission, callbacks are individually budgeted, and all
subscriptions disappear on reload. Diagnostics expose `activeSubscriptions`.

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

## Testing

The standalone shared-runtime build registers the Lua integration suite with
CTest:

```sh
cmake -S games/pangea-ports/shared/script -B build/pangea-script -DBUILD_TESTING=ON
cmake --build build/pangea-script
ctest --test-dir build/pangea-script --output-on-failure
```

The suite covers sandboxing, all lifecycle naming families, context fields,
player lookup, deterministic randomness, timers and timer limits, reload
isolation, structured gameplay results, malformed results, tracebacks, memory
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
eight games dispatch both the global trigger hook and their object-local
`onTriggerEnter` behavior.

# Lua Scripting

Lua scripting is the shared runtime path for all eight Pangea Ports games.
Scripts are authored as Lua source, loaded from `Data/Scripts/dist/main.lua`, and
executed by the shared `PangeaScript` host through an embedded Lua 5.4 runtime.

## Build

```sh
cmake -S games/Bugdom2-Android -B games/Bugdom2-Android/build-scripted -DPANGEA_ENABLE_SCRIPTING=ON
cmake --build games/Bugdom2-Android/build-scripted
```

## Runtime files

```text
Data/Scripts/dist/main.lua
Data/Scripts/config/levels.json
```

Each game uses the same host for lifecycle, frame, item, and object-frame hooks.
Nanosaur 2 and Cro-Mag Rally continue to gate simulation-affecting scripting to
non-networked modes only.

## Lua API

Scripts can import the shared runtime table with:

```lua
local pangea = require("pangea")
```

The embedded runtime currently exposes:

- `pangea.api.capabilities()`
- `pangea.game.id` / `pangea.game.name`
- `pangea.log.info|warn|error`
- `pangea.spawn.native(id, position, options)`
- `pangea.spawn.scripted(id, position)`
- `pangea.object.position(handle)`
- `pangea.object.setPosition(handle, position)`
- `pangea.object.setVelocity(handle, velocity)`
- `pangea.object.delete(handle)`

`pangea.experimental.spawn.scripted(...)` remains available as a compatibility
alias for existing object-spawn flows.

## Hook layout

The startup module should return a table of hook functions:

```lua
local pangea = require("pangea")
local module = {}

function module.onLevelLoad(ctx)
    pangea.log.info("Loaded level " .. ctx.levelNum)
end

return module
```

Supported hook names remain:

- `onGameStart`
- `onLevelLoad`
- `onLevelStart`
- `onFrame`
- `onLevelComplete`
- `onLevelUnload`
- `onGameShutdown`
- `onTerrainItem`
- `onSplineItem`
- `onMapItem`
- `onObjectFrame`

## Runtime safety

The embedded Lua VM is sandboxed:

- no `io`, `os`, or `debug` libraries
- no unrestricted package/module loading
- script modules are limited to `Data/Scripts/`
- protected calls wrap all script execution
- instruction budgets and memory limits keep failures non-fatal

# Script Examples

Converted Lua examples live under `shared/script/examples/`.

## Bugdom 2 Level Log

```lua
local pangea = require("pangea")
local module = {}

function module.onLevelLoad(ctx)
    pangea.log.info("Loaded level " .. ctx.levelNum)
end

return module
```

## Bugdom 2 Terrain Item

```lua
local pangea = require("pangea")
local module = {}

function module.onTerrainItem(item)
    local handle = pangea.spawn.scripted("custom.bouncingPickup", {
        x = item.x,
        y = 0,
        z = item.z,
    })

    if handle then
        return { handled = true, markInUse = true }
    end

    return { handled = false }
end

return module
```

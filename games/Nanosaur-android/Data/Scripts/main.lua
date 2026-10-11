-- Shipped starter module. Optional player bobbing is visual only.
-- Set levelSettings.scriptBobHeight to a positive number to enable it.
local bobHeight = 0
local bobSpeed = 5
local module = {}

function module.onLevelLoad(ctx)
    local height = pangea.level.setting("scriptBobHeight")
    local speed = pangea.level.setting("scriptBobSpeed")
    bobHeight = type(height) == "number" and math.max(0, height) or 0
    bobSpeed = type(speed) == "number" and speed or 5
    pangea.log.info(ctx.gameName .. " Lua loaded for level " .. ctx.levelNum)
end

function module.onLevelStart(ctx)
    pangea.log.info(ctx.gameName .. " Lua started for level " .. ctx.levelNum)
end

function module.onObjectFrame(ctx)
    if ctx.event ~= "update" or bobHeight == 0 then return end
    for _, tag in ipairs(ctx.tags) do
        if tag == "nanosaur.player" then
            return { positionOffset = {
                x = 0,
                y = math.abs(math.sin(ctx.levelTimeSeconds * bobSpeed)) * bobHeight,
                z = 0,
            } }
        end
    end
end

return module

local helpers = require("helpers")
local module = {}

local PLAYER_TAG = "mightymike.player"
local PLAYER_SWAY_SPEED = 6
local PLAYER_SWAY_DISTANCE = 6

function module.onObjectFrame(ctx)
    if not helpers.hasTag(ctx.tags, PLAYER_TAG) then
        return nil
    end

    return helpers.makeHorizontalSwayOffset(
        ctx.levelTimeSeconds,
        PLAYER_SWAY_SPEED,
        PLAYER_SWAY_DISTANCE
    )
end

return module

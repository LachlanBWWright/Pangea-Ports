local helpers = require("helpers")
local module = {}

local PLAYER_TAG = "bugdom2.player"
local PLAYER_BOB_SPEED = 5
local PLAYER_BOB_HEIGHT = 18

function module.onObjectFrame(ctx)
    if not helpers.hasTag(ctx.tags, PLAYER_TAG) then
        return nil
    end

    return helpers.makeVerticalBobOffset(
        ctx.levelTimeSeconds,
        PLAYER_BOB_SPEED,
        PLAYER_BOB_HEIGHT
    )
end

return module

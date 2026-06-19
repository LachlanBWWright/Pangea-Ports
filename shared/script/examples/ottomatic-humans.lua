local helpers = require("helpers")
local module = {}

local HUMAN_TAG = "ottomatic.human"
local SCIENTIST_TAG = "ottomatic.human.scientist"
local HUMAN_BOB_SPEED = 8
local DEFAULT_HUMAN_BOB_HEIGHT = 32
local SCIENTIST_BOB_HEIGHT = 56

local function getOttoHumanBobHeight(tags)
    if helpers.hasTag(tags, SCIENTIST_TAG) then
        return SCIENTIST_BOB_HEIGHT
    end

    return DEFAULT_HUMAN_BOB_HEIGHT
end

function module.onObjectFrame(ctx)
    if not helpers.hasTag(ctx.tags, HUMAN_TAG) then
        return nil
    end

    return helpers.makeVerticalBobOffset(
        ctx.levelTimeSeconds,
        HUMAN_BOB_SPEED,
        getOttoHumanBobHeight(ctx.tags)
    )
end

return module

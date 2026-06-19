local module = {}

local function hasTag(tags, tag)
    for _, candidate in ipairs(tags or {}) do
        if candidate == tag then
            return true
        end
    end

    return false
end

local function getOttoHumanBobHeight(tags)
    if hasTag(tags, "ottomatic.human.scientist") then
        return 56
    end

    return 32
end

function module.onObjectFrame(ctx)
    if not hasTag(ctx.tags, "ottomatic.human") then
        return nil
    end

    return {
        positionOffset = {
            x = 0,
            y = math.abs(math.sin(ctx.levelTimeSeconds * 8)) * getOttoHumanBobHeight(ctx.tags),
            z = 0,
        },
    }
end

return module

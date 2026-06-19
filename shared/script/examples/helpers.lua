local helpers = {}

function helpers.hasTag(tags, tag)
    for _, candidate in ipairs(tags or {}) do
        if candidate == tag then
            return true
        end
    end

    return false
end

function helpers.makeVerticalBobOffset(levelTimeSeconds, speed, amplitude)
    return {
        positionOffset = {
            x = 0,
            y = math.abs(math.sin(levelTimeSeconds * speed)) * amplitude,
            z = 0,
        },
    }
end

function helpers.makeHorizontalSwayOffset(levelTimeSeconds, speed, amplitude)
    return {
        positionOffset = {
            x = math.sin(levelTimeSeconds * speed) * amplitude,
            y = 0,
            z = 0,
        },
    }
end

return helpers

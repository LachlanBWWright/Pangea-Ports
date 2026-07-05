local module = {}
local helpers = require("pangea/helpers")

local HUMAN_TAG = "ottomatic.human"
local SCIENTIST_TAG = "ottomatic.human.scientist"

local function getBobHeight(tags)
  if helpers.hasTag(tags, SCIENTIST_TAG) then
    return 56
  end

  return 32
end

function module.onObjectFrame(ctx)
  if not helpers.hasTag(ctx.tags, HUMAN_TAG) then
    return nil
  end

  return {
    positionOffset = {
      x = 0,
      y = math.sin(ctx.levelTimeSeconds * 8) * getBobHeight(ctx.tags),
      z = 0,
    },
  }
end

return module

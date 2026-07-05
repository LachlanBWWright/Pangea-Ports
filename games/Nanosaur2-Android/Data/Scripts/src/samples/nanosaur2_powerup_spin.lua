local module = {}
local helpers = require("pangea/helpers")

local POWERUP_TAG = "nanosaur2.powerup"

function module.onObjectFrame(ctx)
  if not helpers.hasTag(ctx.tags, POWERUP_TAG) then
    return nil
  end

  return {
    positionOffset = {
      x = 0,
      y = math.sin(ctx.levelTimeSeconds * 6) * 18,
      z = 0,
    },
  }
end

return module

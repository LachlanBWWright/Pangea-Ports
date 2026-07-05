local module = {}
local helpers = require("pangea/helpers")

local PICKUP_TAG = "cromag.pickup"

function module.onObjectFrame(ctx)
  if not helpers.hasTag(ctx.tags, PICKUP_TAG) then
    return nil
  end

  return {
    positionOffset = {
      x = 0,
      y = math.sin(ctx.levelTimeSeconds * 7) * 25,
      z = 0,
    },
  }
end

return module

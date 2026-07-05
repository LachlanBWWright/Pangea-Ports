local module = {}
local helpers = require("pangea/helpers")

local COLLECTIBLE_TAG = "bugdom2.collectible"

function module.onObjectFrame(ctx)
  if not helpers.hasTag(ctx.tags, COLLECTIBLE_TAG) then
    return nil
  end

  return {
    positionOffset = {
      x = 0,
      y = math.sin(ctx.levelTimeSeconds * 5) * 15,
      z = 0,
    },
  }
end

return module

local module = {}
local helpers = require("pangea/helpers")

local EGG_TAG = "nanosaur.egg"

function module.onObjectFrame(ctx)
  if not helpers.hasTag(ctx.tags, EGG_TAG) then
    return nil
  end

  return {
    positionOffset = {
      x = 0,
      y = math.sin(ctx.levelTimeSeconds * 4) * 12,
      z = 0,
    },
  }
end

return module

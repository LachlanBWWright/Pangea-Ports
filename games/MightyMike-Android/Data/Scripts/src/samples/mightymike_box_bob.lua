local module = {}
local helpers = require("pangea/helpers")

local BOX_TAG = "mightymike.box"

function module.onObjectFrame(ctx)
  if not helpers.hasTag(ctx.tags, BOX_TAG) then
    return nil
  end

  return {
    positionOffset = {
      x = 0,
      y = math.sin(ctx.levelTimeSeconds * 5) * 16,
      z = 0,
    },
  }
end

return module

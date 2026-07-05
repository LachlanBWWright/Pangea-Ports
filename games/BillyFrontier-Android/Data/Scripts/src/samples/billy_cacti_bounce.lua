local module = {}
local helpers = require("pangea/helpers")

local CACTI_TAG = "billy.cacti"

function module.onObjectFrame(ctx)
  if not helpers.hasTag(ctx.tags, CACTI_TAG) then
    return nil
  end

  return {
    positionOffset = {
      x = 0,
      y = math.sin(ctx.levelTimeSeconds * 5) * 14,
      z = 0,
    },
  }
end

return module

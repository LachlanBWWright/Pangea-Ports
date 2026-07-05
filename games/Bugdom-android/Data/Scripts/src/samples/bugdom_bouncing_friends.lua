local module = {}
local helpers = require("pangea/helpers")

local BUDDY_TAG = "bugdom.buddy"

function module.onObjectFrame(ctx)
  if not helpers.hasTag(ctx.tags, BUDDY_TAG) then
    return nil
  end

  return {
    positionOffset = {
      x = 0,
      y = math.sin(ctx.levelTimeSeconds * 6) * 20,
      z = 0,
    },
  }
end

return module

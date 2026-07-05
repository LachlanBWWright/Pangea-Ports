local module = {}
local helpers = require("pangea/helpers")

function module.onTriggerEnter(ctx)
  if ctx.triggerId ~= "mightymike.teleport" then
    return { handled = false }
  end

  local state = helpers.state(ctx.self)
  state.touchCount = (state.touchCount or 0) + 1
  pangea.effects.spawn(1, { position = ctx.position, quantity = 3 })

  return {
    handled = false,
  }
end

function module.onWeaponHit(ctx)
  if ctx.weaponId ~= "mightymike.weaponHit" then
    return { handled = false }
  end

  local targetState = helpers.state(ctx.target)
  targetState.scriptedHitCount = (targetState.scriptedHitCount or 0) + 1
  pangea.effects.spawn(3, { position = ctx.position, quantity = 4 })

  return {
    handled = true,
    applyDamage = true,
    damage = ctx.damage,
  }
end

return module

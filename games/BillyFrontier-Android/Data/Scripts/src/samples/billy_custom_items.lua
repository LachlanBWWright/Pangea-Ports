local module = {}
local helpers = require("pangea/helpers")

function module.onTriggerEnter(ctx)
  if ctx.triggerId ~= "billy.peso" then
    return { handled = false }
  end

  local state = helpers.state(ctx.self)
  state.touchCount = (state.touchCount or 0) + 1

  return {
    handled = false,
  }
end

function module.onWeaponHit(ctx)
  if ctx.weaponId ~= "billy.weaponHit" and ctx.weaponId ~= "billy.particleHit" then
    return { handled = false }
  end

  local targetState = helpers.state(ctx.target)
  targetState.scriptedHitCount = (targetState.scriptedHitCount or 0) + 1

  pangea.effects.spawn(1, {
    position = ctx.position,
    scale = 0.6,
  })

  return {
    handled = true,
    applyDamage = true,
    damage = ctx.damage,
  }
end

return module
